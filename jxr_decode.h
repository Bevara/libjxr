/*
 *  Minimal C surface over jxrlib, deliberately free of both jxrlib and GPAC
 *  types.
 *
 *  jxrlib's windowsmediaphoto.h does "typedef int Bool;" while gpac/setup.h
 *  declares Bool as an enum, so the two header sets cannot be included in the
 *  same translation unit. Everything that touches jxrlib therefore lives in
 *  jxr_decode.c, and the filter itself only sees this header.
 */

#ifndef _JXR_DECODE_H_
#define _JXR_DECODE_H_

#include <stddef.h>

/* Decodes a whole JPEG XR file to a tightly packed 24-bit RGB buffer.
 *
 * On success returns 0, stores a buffer allocated with malloc() in *out (the
 * caller releases it with jxr_decode_free) and its dimensions in *width and
 * *height. On failure returns one of the negative codes below and leaves the
 * output parameters untouched. */
int jxr_decode_rgb(const unsigned char *data, size_t size,
                   unsigned char **out, unsigned int *width, unsigned int *height);

void jxr_decode_free(unsigned char *buffer);

#define JXR_DEC_OK              0
#define JXR_DEC_ERR_MEMORY     -1
#define JXR_DEC_ERR_BITSTREAM  -2   /* not a JPEG XR file, or a broken one */
#define JXR_DEC_ERR_PIXFMT     -3   /* no conversion path to 24bpp RGB */
#define JXR_DEC_ERR_DECODE     -4   /* the image data itself failed to decode */

#endif
