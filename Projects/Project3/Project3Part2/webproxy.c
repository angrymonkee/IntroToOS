#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gfserver.h"

#define USAGE                                                                    \
"usage:\n"                                                                       \
"  webproxy [options]\n"                                                         \
"options:\n"                                                                     \
"  -n [segment_count]  Number of segments to use in communication with cache\n"  \
"  -z [segment_size]   Size (in bytes) of the segments\n"                        \
"  -p [listen_port]    Listen port (Default: 8888)\n"                            \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"         \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"\
"  -h                  Show this help message\n"                                 \
"special options:\n"                                                             \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] =
{
    {"segment-count", required_argument,      NULL,           'n'},
    {"segment-size",  required_argument,      NULL,           'z'},
    {"port",          required_argument,      NULL,           'p'},
    {"thread-count",  required_argument,      NULL,           't'},
    {"server",        required_argument,      NULL,           's'},
    {"help",          no_argument,            NULL,           'h'},
    {NULL,            0,                      NULL,             0}
};

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);
extern void InitializeSharedSegmentPool(int nsegments, int segmentSize);
extern void InitializeSynchronizationQueues();

extern void CleanupSharedSegmentPool();
extern void CleanupSynchronizationQueues();

static gfserver_t gfs;

static void _sig_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        gfserver_stop(&gfs);
        CleanupSynchronizationQueues();
        CleanupSharedSegmentPool();
        exit(signo);
    }
}

int _debuggingLevel;

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int i, option_char = 0;
    unsigned short port = 8888;
    unsigned short nworkerthreads = 1;
    unsigned short nsegments = 1;
    unsigned short segmentSize = 1024;

    _debuggingLevel = 1;

    char *server = "http://s3.amazonaws.com/content.udacity-data.com";

    if (signal(SIGINT, _sig_handler) == SIG_ERR)
    {
        fprintf(stderr,"Can't catch SIGINT...exiting.\n");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, _sig_handler) == SIG_ERR)
    {
        fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
        exit(EXIT_FAILURE);
    }

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "n:z:p:t:s:h", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        case 'n': // segment-count
            nsegments = atoi(optarg);
            break;
        case 'z': // segnment-size
            segmentSize = atoi(optarg);
            break;
        case 'p': // listen-port
            port = atoi(optarg);
            break;
        case 't': // thread-count
            nworkerthreads = atoi(optarg);
            break;
        case 's': // file-path
            server = optarg;
            printf("set server to %s\n", server);
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        }
    }

    /* SHM initialization...*/
    InitializeSharedSegmentPool(nsegments, segmentSize);
    InitializeSynchronizationQueues();

    /*Initializing server*/
    gfserver_init(&gfs, nworkerthreads);

    /*Setting options*/
    gfserver_setopt(&gfs, GFS_PORT, port);
    gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
    gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
    printf("server set to %s\n", server);
    for(i = 0; i < nworkerthreads; i++)
        gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);

    /*Loops forever*/
    gfserver_serve(&gfs);

    CleanupSynchronizationQueues();
    CleanupSharedSegmentPool();

    return 0;
}
