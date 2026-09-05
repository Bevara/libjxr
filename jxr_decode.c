/*
 *  jxrlib-facing half of the JPEG XR filter - see jxr_decode.h for why it is a
 *  translation unit of its own.
 */

/* jxrlib's headers only compile with __ANSI__ defined: without it, and off
 * Windows, guiddef.h expands FAR to the 16-bit "_far" keyword and every
 * DEFINE_GUID declaration becomes a syntax error. The library itself is built
 * with the same define. The headers still publish real typedefs for
 * PKImageDecode & co - the macro forms only exist inside the struct bodies, so
 * they can refer to themselves, and are undefined again just before the
 * closing brace. */
#define __ANSI__ 1
#define DISABLE_PERF_MEASUREMENT 1

#include <stdlib.h>
#include <string.h>

#include <JXRGlue.h>

#include "jxr_decode.h"

void jxr_decode_free(unsigned char *buffer)
{
	free(buffer);
}

int jxr_decode_rgb(const unsigned char *data, size_t size,
                   unsigned char **out, unsigned int *width, unsigned int *height)
{
	PKFactory *pFactory = NULL;
	PKCodecFactory *pCodecFactory = NULL;
	PKImageDecode *pDecoder = NULL;
	PKFormatConverter *pConverter = NULL;
	struct WMPStream *pStream = NULL;
	PKPixelInfo PI;
	PKRect rect;
	I32 w = 0, h = 0;
	size_t src_bytes, scratch_stride, y;
	unsigned char *scratch = NULL, *rgb = NULL;
	int ret = JXR_DEC_ERR_BITSTREAM;

	if (PKCreateFactory(&pFactory, PK_SDK_VERSION) != WMP_errSuccess)
		return JXR_DEC_ERR_MEMORY;
	if (PKCreateCodecFactory(&pCodecFactory, WMP_SDK_VERSION) != WMP_errSuccess)
	{
		ret = JXR_DEC_ERR_MEMORY;
		goto cleanup;
	}
	/* the stream only borrows the caller's bytes; nothing here outlives the call */
	if (pFactory->CreateStreamFromMemory(&pStream, (void *)data, size) != WMP_errSuccess)
	{
		ret = JXR_DEC_ERR_MEMORY;
		goto cleanup;
	}
	if (PKImageDecode_Create_WMP(&pDecoder) != WMP_errSuccess)
	{
		ret = JXR_DEC_ERR_MEMORY;
		goto cleanup;
	}
	if (pDecoder->Initialize(pDecoder, pStream) != WMP_errSuccess)
		goto cleanup;
	if (pDecoder->GetSize(pDecoder, &w, &h) != WMP_errSuccess || w <= 0 || h <= 0)
		goto cleanup;

	memset(&PI, 0, sizeof(PI));
	PI.pGUIDPixFmt = &pDecoder->guidPixFormat;
	if (PixelFormatLookup(&PI, LOOKUP_FORWARD) != WMP_errSuccess)
	{
		ret = JXR_DEC_ERR_PIXFMT;
		goto cleanup;
	}

	/* The decode parameters JxrDecApp fills in before touching the image; left
	 * at zero they would yield a thumbnail-sized, region-of-interest-clipped
	 * image instead of the full frame. uAlphaMode 0 makes the decoder skip the
	 * alpha plane, which is what we want: the filter's output pid is RGB,
	 * since an RGBA pid has no adaptation path to writegen in this build. */
	pDecoder->WMP.wmiSCP.uAlphaMode = 0;
	pDecoder->WMP.wmiSCP.bVerbose = FALSE;
	pDecoder->WMP.bIgnoreOverlap = FALSE;
	pDecoder->WMP.wmiI.cfColorFormat = PI.cfColorFormat;
	pDecoder->WMP.wmiI.bdBitDepth = PI.bdBitDepth;
	pDecoder->WMP.wmiI.cBitsPerUnit = PI.cbitUnit;
	pDecoder->WMP.wmiI.bRGB = 1;
	pDecoder->WMP.wmiI.cThumbnailWidth = pDecoder->WMP.wmiI.cWidth;
	pDecoder->WMP.wmiI.cThumbnailHeight = pDecoder->WMP.wmiI.cHeight;
	pDecoder->WMP.wmiI.bSkipFlexbits = FALSE;
	pDecoder->WMP.wmiI.cROILeftX = 0;
	pDecoder->WMP.wmiI.cROITopY = 0;
	pDecoder->WMP.wmiI.cROIWidth = w;
	pDecoder->WMP.wmiI.cROIHeight = h;
	pDecoder->WMP.wmiI.oOrientation = O_NONE;
	pDecoder->WMP.wmiI.cPostProcStrength = 0;

	if (pCodecFactory->CreateFormatConverter(&pConverter) != WMP_errSuccess)
	{
		ret = JXR_DEC_ERR_MEMORY;
		goto cleanup;
	}
	/* the extension argument only steers which conversions the converter is
	 * willing to offer; ".bmp" is the one JxrDecApp uses for 24bpp RGB output */
	if (pConverter->Initialize(pConverter, pDecoder, ".bmp", GUID_PKPixelFormat24bppRGB) != WMP_errSuccess)
	{
		ret = JXR_DEC_ERR_PIXFMT;
		goto cleanup;
	}

	/* PKFormatConverter_Copy decodes into the caller's buffer in the *source*
	 * pixel format and then converts it in place, so the buffer must be wide
	 * enough for whichever of the two formats is larger - a 32bpp or 48bpp
	 * source would overrun a buffer sized for 24bpp RGB. Decode into a scratch
	 * buffer of that width, then lift the RGB rows out of it. */
	src_bytes = (PI.cbitUnit + 7) / 8;
	if (src_bytes < 3)
		src_bytes = 3;
	scratch_stride = (size_t)w * src_bytes;
	scratch = (unsigned char *)malloc(scratch_stride * (size_t)h);
	rgb = (unsigned char *)malloc((size_t)w * (size_t)h * 3);
	if (!scratch || !rgb)
	{
		ret = JXR_DEC_ERR_MEMORY;
		goto cleanup;
	}

	rect.X = 0;
	rect.Y = 0;
	rect.Width = w;
	rect.Height = h;
	if (pConverter->Copy(pConverter, &rect, scratch, (U32)scratch_stride) != WMP_errSuccess)
	{
		ret = JXR_DEC_ERR_DECODE;
		goto cleanup;
	}

	for (y = 0; y < (size_t)h; y++)
		memcpy(rgb + y * (size_t)w * 3, scratch + y * scratch_stride, (size_t)w * 3);

	*out = rgb;
	*width = (unsigned int)w;
	*height = (unsigned int)h;
	rgb = NULL;
	ret = JXR_DEC_OK;

cleanup:
	free(scratch);
	free(rgb);
	if (pConverter)
		pConverter->Release(&pConverter);
	if (pDecoder)
		pDecoder->Release(&pDecoder);
	if (pCodecFactory)
		pCodecFactory->Release(&pCodecFactory);
	if (pFactory)
		pFactory->Release(&pFactory);
	return ret;
}
