/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _MINIFYJPEG_H_RPCGEN
#define _MINIFYJPEG_H_RPCGEN

#include <rpc/rpc.h>

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif


struct image_descriptor {
	long Size;
	struct {
		u_int Buffer_len;
		char *Buffer_val;
	} Buffer;
};
typedef struct image_descriptor image_descriptor;

#define MINIFY_PROG 0x31234567
#define COMPRESS_VERS 1

#if defined(__STDC__) || defined(__cplusplus)
#define Compress_Image 1
extern  enum clnt_stat compress_image_1(image_descriptor , image_descriptor *, CLIENT *);
extern  bool_t compress_image_1_svc(image_descriptor , image_descriptor *, struct svc_req *);
extern int minify_prog_1_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define Compress_Image 1
extern  enum clnt_stat compress_image_1();
extern  bool_t compress_image_1_svc();
extern int minify_prog_1_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_image_descriptor (XDR *, image_descriptor*);

#else /* K&R C */
extern bool_t xdr_image_descriptor ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_MINIFYJPEG_H_RPCGEN */
