/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "minifyjpeg.h"

bool_t
xdr_image_descriptor (XDR *xdrs, image_descriptor *objp)
{
	register int32_t *buf;

	 if (!xdr_long (xdrs, &objp->Size))
		 return FALSE;
	 if (!xdr_bytes (xdrs, (char **)&objp->Buffer.Buffer_val, (u_int *) &objp->Buffer.Buffer_len, ~0))
		 return FALSE;
	return TRUE;
}