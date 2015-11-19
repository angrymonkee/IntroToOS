#include <stdio.h>
#include "minifyjpeg.h"
#include "magickminify.h"


//typedef struct image_descriptor
//{
//    long Size;
//    void Buffer[];
//}image_descriptor


image_descriptor *CompressImage_1(image_descriptor **imgDescriptor)
{
    printf("In CompressImage Service RPC...\n");

    static image_descriptor destDescriptor;

    destDescriptor.ImgBuffer = magickminify(*imgDescriptor->Buffer, *imgDescriptor->Size, &(destDescriptor.Size));

    return &destDescriptor;
}
