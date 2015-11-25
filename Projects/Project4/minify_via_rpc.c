#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"

// Client-side code

CLIENT* get_minify_client(char *server)
{
    printf("Creating client...\n");
    CLIENT *cl = clnt_create(server, MINIFY_PROG, COMPRESS_VERS, "tcp");
    if (cl == NULL)
    {
        printf("Couldn't establish connection with server\n");
        clnt_pcreateerror(server);
        exit(1);
    }

    clnt_control(cl, CLSET_TIMEOUT, "60");

    printf("Client created successfully...\n");
    return cl;
}

void* minify_via_rpc(CLIENT *cl, void* src_val, size_t src_len, size_t *dst_len)
{
    printf("create image descriptor...\n");
    image_descriptor *descriptor = malloc(sizeof(image_descriptor));
    descriptor->Size = src_len;
    descriptor->Buffer.Buffer_len = src_len;
    descriptor->Buffer.Buffer_val = src_val;

    image_descriptor *result = malloc(sizeof(image_descriptor));
    result->Size = 0;
    result->Buffer.Buffer_len = 0;
    result->Buffer.Buffer_val = NULL;

    printf("calling rpc service...\n");
    compress_image_1(*descriptor, result, cl);
    printf("result image size %ld\n", result->Size);

    *dst_len = result->Size;
    char *retBuffer = result->Buffer.Buffer_val;

    free(result);
    free(descriptor);

    return retBuffer;
}
