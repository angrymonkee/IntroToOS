#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gfserver.h"
#include "content.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                      \
"options:\n"                                                                  \
"  -n [segment_count]  Number of segments to use in communication with cache\n"  \
"  -z [segment_size]   Size (in bytes) of the segments\n"                        \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"\
"  -p                  Listen port (Default: 8888)\n"                         \
"  -t                  Number of threads (Default: 1)\n"                      \
"  -c                  Content file mapping keys to content files\n"          \
"  -h                  Show this help message\n"                              \

extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);
extern void InitializeThreadPool(int numberOfThreads);
extern void InitializeThreadConstructs();
extern void ThreadCleanup();

extern void InitializeCurl();
extern void CurlCleanup();

extern void InitializeSharedSegmentPool(int nsegments, int segmentSize);
extern void InitializeSynchronizationQueues();
extern void CleanupSharedSegmentPool();
extern void CleanupSynchronizationQueues();

/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  unsigned short port = 8888;
  char *content = "content.txt";
  gfserver_t *gfs;
  int threads = 2;
  unsigned short nsegments = 1;
  unsigned short segmentSize = 1024;
  char *server = "http://s3.amazonaws.com/content.udacity-data.com";

  // Parse and set command line arguments
  while ((option_char = getopt(argc, argv, "p:t:c:h")) != -1) {
    switch (option_char) {
	  case 'n': // segment-count
		nsegments = atoi(optarg);
		break;
	  case 'z': // segnment-size
		segmentSize = atoi(optarg);
		break;
      case 's': // file-path
        server = optarg;
        printf("set server to %s\n", server);
        break;
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 't': // number of threads
        threads = atoi(optarg);
        break;
      case 'c': // file-path
        content = optarg;
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

  content_init(content);

  InitializeThreadConstructs();
  InitializeThreadPool(threads);
  InitializeCurl();
  InitializeSharedSegmentPool(nsegments, segmentSize);
  InitializeSynchronizationQueues();


  /*Initializing server*/
  gfs = gfserver_create();

  /*Setting options*/
  gfserver_set_port(gfs, port);
  gfserver_set_maxpending(gfs, 100);
  gfserver_set_handler(gfs, handle_with_cache);
  gfserver_set_handlerarg(gfs, NULL);

  /*Loops forever*/
  gfserver_serve(gfs);
  
  
  CleanupSharedSegmentPool();
  CleanupSynchronizationQueues();
  CurlCleanup();
  ThreadCleanup();
}
