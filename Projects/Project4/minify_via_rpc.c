#include <stdlib.h>
#include <stdio.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"

CLIENT* get_minify_client(char *server)
{
    CLIENT cl = clnt_create(server, MINIFY_PROG, COMPRESS_VERS, "tcp");

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

	/*Your code here */

    image_descriptor imgDescriptor;
    imgDescriptor.Size = src_len;
    imgDescriptor.Buffer_val = src_val;


    /*
     * Call the remote procedure "printmessage" on the server.
     */
    result = printmessage_1(&message, cl);
    if (result == NULL) {
        /*
         * An error occurred while calling the server.
         * Print error message and die
         */
        clnt_perror(cl, server);
        exit(1);
    }
    /*
     * Okay, we successfully called the remote procedure
     */
    if (*result == 0) {
        /*
         * Server was unable to print our message.
         * Print error message and die.
         */
        fprintf(stderr, "%s: %s couldn't print your message\n",
        argv[0], server);
        exit(1);
    }
    /*
     * Message got printed at server
     */
     printf("Message delivered to %s!\n", server);
     exit(0);


}
