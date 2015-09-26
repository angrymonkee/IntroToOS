#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

#include "workload.h"
#include "gfclient.h"
#include "steque.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -p [server_port]    Server port (Default: 8888)\n"                         \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \
"  -t [nthreads]       Number of threads (Default 1)\n"                       \
"  -n [num_requests]   Requests download per thread (Default: 1)\n"           \
"  -h                  Show this help message\n"                              \

pthread_t _tid[100];
pthread_mutex_t _fileLock;

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"server",        required_argument,      NULL,           's'},
  {"port",          required_argument,      NULL,           'p'},
  {"workload-path", required_argument,      NULL,           'w'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"nrequests",     required_argument,      NULL,           'n'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

typedef struct ServerSettings
{
    char *Server;
    unsigned short Port;
    int NumOfRequest;
    int ThreadID;
} ServerSettings;

static void Usage() {
	fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while(NULL != (cur = strchr(prev+1, '/'))){
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)){
      if (errno != EEXIST){
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if( NULL == (ans = fopen(&path[0], "w"))){
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
  FILE *file = (FILE*) arg;

  fwrite(data, 1, data_len, file);
}

/* Threading =========================================================*/
void ProcessRequest(ServerSettings *settings);

void InitializeThreadConstructs()
{
    if (pthread_mutex_init(&_fileLock, NULL) != 0)
    {
        printf("\n File lock mutex init failed\n");
        exit(1);
    }
}

void ProcessRequest(ServerSettings *settings)
{
    int i;
    for(i=0;i < settings->NumOfRequest; i++)
    {
        char *req_path = workload_get_path();

        if(strlen(req_path) > 256)
        {
          fprintf(stderr, "Request path exceeded maximum of 256 characters\n");
          exit(EXIT_FAILURE);
        }

        if(req_path == NULL)
        {
            fprintf(stderr, "Request path cannot be NULL or empty\n");
            exit(EXIT_FAILURE);
        }

        printf("Processing request\n");

        FILE *file;
        char local_path[512];
        int returncode;

        localPath(req_path, local_path);

        pthread_mutex_lock(&_fileLock);

        file = openFile(local_path);

        gfcrequest_t *gfr;
        gfr = gfc_create();
        gfc_set_server(gfr, settings->Server);
        gfc_set_path(gfr, req_path);
        gfc_set_port(gfr, settings->Port);
        gfc_set_writefunc(gfr, writecb);
        gfc_set_writearg(gfr, file);

        fprintf(stdout, "Requesting %s%s\n", settings->Server, req_path);

        if ( 0 > (returncode = gfc_perform(gfr)))
        {
            fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
            fclose(file);
            if ( 0 > unlink(local_path))
            {
                fprintf(stderr, "unlink failed on %s\n", local_path);
            }
        }
        else
        {
            fclose(file);
        }

        pthread_mutex_unlock(&_fileLock);

        if ( gfc_get_status(gfr) != GF_OK)
        {
            if ( 0 > unlink(local_path))
            {
                fprintf(stderr, "unlink failed on %s\n", local_path);
            }
        }

        fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
        fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));
    }

    pthread_exit(NULL);
    free(settings);
}

/* Main ========================================================= */
int main(int argc, char **argv)
{
    /* COMMAND LINE OPTIONS ============================================= */
    char *server = "localhost";
    unsigned short port = 8888;
    char *workload_path = "workload.txt";

    int i;
    int option_char = 0;
    int nrequests = 1;
    int nthreads = 1;

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:w:n:t:h", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
            case 's': // server
                server = optarg;
                break;
            case 'p': // port
                port = atoi(optarg);
                break;
            case 'w': // workload-path
                workload_path = optarg;
                break;
            case 'n': // nrequests
                nrequests = atoi(optarg);
                break;
            case 't': // nthreads
                nthreads = atoi(optarg);
                break;
            case 'h': // help
                Usage();
                exit(0);
                break;
            default:
                Usage();
                exit(1);
        }
    }

    if( EXIT_SUCCESS != workload_init(workload_path))
    {
        fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
        exit(EXIT_FAILURE);
    }

    gfc_global_init();

    InitializeThreadConstructs();

    /*Making the threads for requests...*/
    for(i = 0; i < nthreads; i++)
    {
        printf("Creating thread: %d\n", i);

        ServerSettings *settings = malloc(sizeof(ServerSettings));
        settings->Port = port;
        settings->Server = server;
        settings->NumOfRequest = nrequests;
        settings->ThreadID = _tid[i];

        int err = 0;
        err = pthread_create(&(_tid[i]), NULL, (void*)&ProcessRequest, settings);
        if (err != 0)
        {
            printf("Can't create thread :[%s]\n", strerror(err));
        }
    }

    // Wait for processing to complete
    for(i=0;i < nthreads; i++)
    {
        pthread_join(_tid[i], NULL);
        printf("Joined thread: %d\n", i);
    }

    gfc_global_cleanup();

    return 0;
}
