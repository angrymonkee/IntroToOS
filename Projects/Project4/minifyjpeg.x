/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */



struct image_descriptor
{
    long Size;
    string Buffer<4096>;
};

program MINIFY_PROG {
    version COMPRESS_VERS {
        image_descriptor Compress_Image(image_descriptor) = 1;
    } = 1;
} = 0x31234567;
