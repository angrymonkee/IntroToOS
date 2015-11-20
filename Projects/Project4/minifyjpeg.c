#include <stdio.h>
#include <stdlib.h>
#include "minifyjpeg.h"
#include "magickminify.h"

// Server-side functions here
int minify_prog_1_freeresult (SVCXPRT * transp, xdrproc_t xdr_result, caddr_t result)
{
	(void) xdr_free(xdr_result, result);
	return 1;
}

bool_t compress_image_1_svc(image_descriptor descriptor, image_descriptor *result, struct svc_req *req)
{
    printf("In Compress_Image_1 Server RPC call...\n");

    result->Buffer = magickminify(descriptor.Buffer, descriptor.Size, &(result->Size));

    return 1;
}
