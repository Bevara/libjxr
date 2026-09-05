/*
 *			GPAC - Multimedia Framework C SDK
 *
 *  This file is part of GPAC / JPEG XR decoder filter, based on jxrlib
 *  (https://github.com/4creators/jxrlib), the reference implementation
 *  Microsoft contributed to ITU-T T.832.
 *
 *  Takes a whole .jxr/.wdp/.hdp file and outputs one raw RGB frame. The
 *  jxrlib calls themselves live in jxr_decode.c, whose headers cannot be
 *  mixed with GPAC's - see jxr_decode.h.
 */

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <string.h>
#include <stdlib.h>

#include "jxr_decode.h"

typedef struct
{
	GF_FilterPid *ipid, *opid;
	Bool is_playing;
} GF_JXRDecCtx;

static GF_Err jxrdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_JXRDecCtx *ctx = (GF_JXRDecCtx *)gf_filter_get_udta(filter);

	if (is_remove)
	{
		if (ctx->opid)
		{
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_RGB));

	return GF_OK;
}

static Bool jxrdec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_JXRDecCtx *ctx = (GF_JXRDecCtx *)gf_filter_get_udta(filter);
	switch (evt->base.type)
	{
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		return GF_FALSE;
	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
}

static GF_Err jxrdec_process(GF_Filter *filter)
{
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 size;
	unsigned char *rgb = NULL;
	unsigned int width = 0, height = 0;
	int res;
	GF_JXRDecCtx *ctx = (GF_JXRDecCtx *)gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck)
	{
		if (gf_filter_pid_is_eos(ctx->ipid))
		{
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (u8 *)gf_filter_pck_get_data(pck, &size);
	if (!data)
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_IO_ERR;
	}

	res = jxr_decode_rgb(data, size, &rgb, &width, &height);
	gf_filter_pid_drop_packet(ctx->ipid);

	switch (res)
	{
	case JXR_DEC_OK:
		break;
	case JXR_DEC_ERR_MEMORY:
		return GF_OUT_OF_MEM;
	case JXR_DEC_ERR_PIXFMT:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JXRDec] No conversion path from this pixel format to 24bpp RGB\n"));
		return GF_NOT_SUPPORTED;
	case JXR_DEC_ERR_DECODE:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JXRDec] Failed to decode image data\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JXRDec] Not a valid JPEG XR bitstream\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(height));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(width * 3));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_RGB));

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, width * height * 3, &output);
	if (!dst_pck)
	{
		jxr_decode_free(rgb);
		return GF_OUT_OF_MEM;
	}
	memcpy(output, rgb, width * height * 3);
	jxr_decode_free(rgb);

	gf_filter_pck_set_cts(dst_pck, 0);
	gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
	gf_filter_pck_send(dst_pck);

	gf_filter_pid_set_eos(ctx->opid);
	return GF_EOS;
}

static void jxrdec_finalize(GF_Filter *filter)
{
}

static const GF_FilterCapability JXRDecCaps[] =
	{
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "jxr|wdp|hdp|wmp"),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "image/jxr|image/vnd.ms-photo"),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister JXRDecoderRegister = {
	.name = "jxrdec",
	GF_FS_SET_DESCRIPTION("JPEG XR decoder")
		GF_FS_SET_HELP("This filter decodes JPEG XR (ITU-T T.832, also known as HD Photo / Windows Media Photo) images using jxrlib.")
			.private_size = sizeof(GF_JXRDecCtx),
	SETCAPS(JXRDecCaps),
	.configure_pid = jxrdec_configure_pid,
	.process = jxrdec_process,
	.process_event = jxrdec_process_event,
	.finalize = jxrdec_finalize,
};

const GF_FilterRegister *EMSCRIPTEN_KEEPALIVE jxrdec_register(GF_FilterSession *session)
{
	return &JXRDecoderRegister;
}

#include "filter_register.h"
__attribute__((constructor))
void register_jxrdec(void) {
    gf_filter_auto_register("jxrdec", jxrdec_register);
}
