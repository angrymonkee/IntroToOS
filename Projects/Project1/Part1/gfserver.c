#include <unistd.h>
#include "gfserver.h"

/* 
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len)
{
	// TODO: Send header
	// consider sending something to mark this as a header being sent
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len)
{
	// TODO: Send
}

void gfs_abort(gfcontext_t *ctx)
{
	
}

gfserver_t* gfserver_create()
{
	gfserver_t ret = malloc(sizeof(gfserver_t));
	return ret;
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port)
{
	gfs->Port = port;
}

void gfserver_set_maxpending(gfserver_t *gfs, int max_npending)
{
	gfs->MaxNumberConnections = max_npending;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*))
{
	gfs->Handler = handler();
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg)
{
	gfs->HandlerArg = arg();
}

void gfserver_serve(gfserver_t *gfs)
{
	
}
