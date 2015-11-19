/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */


struct image_descriptor
{
    long Size;
    char Buffer<200>;
};

program MINIFY_PROG {
    version COMPRESS_VERS {
        image_descriptor CompressImage(image_descriptor imgDescriptor) = 1;    /* procedure number = 1 */
    } = 1;                          /* version number = 1 */
} = 0x31234567;                     /* program number = 0x31234567 */
