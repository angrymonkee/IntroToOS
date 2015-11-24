#include <stdio.h>
#include <stdlib.h>
#include "minifyjpeg.h"
#include "magickminify.h"

// Server-side functions here
int minify_prog_1_freeresult (SVCXPRT * transp, xdrproc_t xdr_result, caddr_t result)
{
    printf("Freeing result\n");
	(void) xdr_free(xdr_result, result);
	printf("Result freed\n");
	return 1;
}

bool_t compress_image_1_svc(image_descriptor descriptor, image_descriptor *result, struct svc_req *req)
{
    printf("In Compress_Image_1 Server RPC call...\n");

    result->Buffer.Buffer_val = magickminify(descriptor.Buffer.Buffer_val, descriptor.Size, &(result->Size));
    result->Buffer.Buffer_len = result->Size;

    printf("compression done..\n");
    return 1;
}
