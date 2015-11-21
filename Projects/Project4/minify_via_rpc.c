#include <stdlib.h>
#include <stdio.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"

// Client-side code

CLIENT* get_minify_client(char *server)
{
    CLIENT *cl = clnt_create(server, MINIFY_PROG, COMPRESS_VERS, "tcp");

    if (cl == NULL)
    {
        printf("Couldn't establish connection with server\n");
        clnt_pcreateerror(server);
        exit(1);
    }

    return cl;
}

void* minify_via_rpc(CLIENT *cl, void* src_val, size_t src_len, size_t *dst_len)
{
    image_descriptor descriptor;
    descriptor.Size = src_len;
    descriptor.Buffer.Buffer_len = src_len;
    descriptor.Buffer.Buffer_val = src_val;

    image_descriptor *result = malloc(sizeof(image_descriptor));

    compress_image_1(descriptor, result, cl);

    *dst_len = result->Size;

    return result->Buffer.Buffer_val;
}
