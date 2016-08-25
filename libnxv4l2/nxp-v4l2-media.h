//------------------------------------------------------------------------------
//
//  Copyright (C) 2013 Nexell Co. All Rights Reserved
//  Nexell Co. Proprietary & Confidential
//
//  NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//  Module      :
//  File        :
//  Description :
//  Author      :
//  Export      :
//  History     :
//
//------------------------------------------------------------------------------

#ifndef _NXP_V4L2_H
#define _NXP_V4L2_H

#ifdef ANDROID
#include <gralloc_priv.h>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#define MAX_BUFFER_PLANES 3
struct nxp_vid_buffer {
    int plane_num;
    int fds[MAX_BUFFER_PLANES];
    char *virt[MAX_BUFFER_PLANES];
    unsigned long phys[MAX_BUFFER_PLANES];
    int sizes[MAX_BUFFER_PLANES];
};

/* pixel code */
#define PIXCODE_YUV422_PACKED       V4L2_MBUS_FMT_YUYV8_2X8
#define PIXCODE_YUV420_PLANAR       V4L2_MBUS_FMT_YUYV8_1_5X8
#define PIXCODE_YUV422_PLANAR       V4L2_MBUS_FMT_YUYV8_1X16
#define PIXCODE_YUV444_PLANAR       V4L2_MBUS_FMT_YUV8_1X24

/* pixel format */
#define PIXFORMAT_YUV422_PACKED     V4L2_PIX_FMT_YUYV
#define PIXFORMAT_YUV420_PLANAR     V4L2_PIX_FMT_YUV420M        //  3 plane
#define PIXFORMAT_YUV420_YV12       V4L2_PIX_FMT_YUV420         //  1 Plane
#define PIXFORMAT_YUV422_PLANAR     V4L2_PIX_FMT_YUV422P
#define PIXFORMAT_YUV444_PLANAR     V4L2_PIX_FMT_YUV444

enum {
    YUV422_PACKED = 0,
    YUV420_PLANAR,      //  3 Plane
    YUV420_YV12,        //  1 Plane
    YUV422_PLANAR,
    YUV444_PLANAR,
    MAX_PIXFORMAT
};

struct PixFormatPixCode {
    uint32_t format;
    uint32_t code;
};

extern struct PixFormatPixCode PixelArray[MAX_PIXFORMAT];

#ifndef ANDROID
#ifdef __cplusplus
extern "C" {
#endif
#endif

/* user control interface */
typedef enum {
    nxp_v4l2_sensor0        = 0,
    nxp_v4l2_sensor1        = 1,
    nxp_v4l2_mipicsi        = 2,
    nxp_v4l2_clipper0       = 5, /* include camera sensor, mipicsi */
    nxp_v4l2_clipper1       = 8,
    nxp_v4l2_decimator0     = 11,
    nxp_v4l2_decimator1     = 14,
    nxp_v4l2_scaler         = 17,
    nxp_v4l2_deinterlacer   = 20,
    nxp_v4l2_mlc0           = 21,
    nxp_v4l2_mlc0_rgb       = 23,
    nxp_v4l2_mlc0_video     = 25,
    nxp_v4l2_mlc1           = 26,
    nxp_v4l2_mlc1_rgb       = 28,
    nxp_v4l2_mlc1_video     = 30,
    nxp_v4l2_resol          = 31,
    nxp_v4l2_hdmi           = 32,
    nxp_v4l2_tvout          = 33,
    nxp_v4l2_id_max,
} nxp_v4l2_id;

struct V4l2UsageScheme {
    bool useClipper0;
    bool useDecimator0;
    bool useClipper1;
    bool useDecimator1;
    bool useScaler;
    bool useDeinterlacer;
    bool useMlc0Rgb;
    bool useMlc0Video;
    bool useMlc1Rgb;
    bool useMlc1Video;
    bool useResol;
    bool useHdmi;
    bool useTvout;
};

typedef void* V4L2_PRIVATE_HANDLE;

V4L2_PRIVATE_HANDLE	v4l2_init		(const struct V4l2UsageScheme *scheme);
void v4l2_exit(V4L2_PRIVATE_HANDLE pHandle);
int v4l2_link(V4L2_PRIVATE_HANDLE pHandle, int src_id, int dst_id);
int v4l2_unlink(V4L2_PRIVATE_HANDLE pHandle, int src_id, int dst_id);
int v4l2_set_format(V4L2_PRIVATE_HANDLE pHandle, int id, int w, int h, int f);
int v4l2_get_format(V4L2_PRIVATE_HANDLE pHandle, int id, int *w, int *h, int *f);
int v4l2_set_crop(V4L2_PRIVATE_HANDLE pHandle, int id, int l, int t, int w, int h);
int v4l2_get_crop(V4L2_PRIVATE_HANDLE pHandle, int id, int *l, int *t, int *w, int *h);
int v4l2_set_format_with_pad(V4L2_PRIVATE_HANDLE pHandle, int id, int pad, int w, int h, int f);
int v4l2_get_format_with_pad(V4L2_PRIVATE_HANDLE pHandle, int id, int pad, int *w, int *h, int *f);
int v4l2_set_crop_with_pad(V4L2_PRIVATE_HANDLE pHandle, int id, int pad, int l, int t, int w, int h);
int v4l2_get_crop_with_pad(V4L2_PRIVATE_HANDLE pHandle, int id, int pad, int *l, int *t, int *w, int *h);
int v4l2_set_ctrl(V4L2_PRIVATE_HANDLE pHandle, int id, int ctrl_id, int value);
int v4l2_get_ctrl(V4L2_PRIVATE_HANDLE pHandle, int id, int ctrl_id, int *value);
int v4l2_reqbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int buf_count);
#ifdef ANDROID
//int v4l2_qbuf(int id, int plane_num, int index0, struct private_handle_t *b0, int index1, struct private_handle_t *b1);
int v4l2_qbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int plane_num, int index0, struct private_handle_t const *b0, int index1,
struct private_handle_t const *b1, int *syncfd0 = NULL, int *syncfd1 = NULL);
#endif
int	v4l2_qbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int plane_num, int index0, struct nxp_vid_buffer *b0, int index1, struct nxp_vid_buffer *b1);
int	v4l2_dqbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int plane_num, int *index0, int *index1);
int	v4l2_streamon(V4L2_PRIVATE_HANDLE pHandle, int id);
int	v4l2_streamoff(V4L2_PRIVATE_HANDLE pHandle, int id);
int	v4l2_get_timestamp(V4L2_PRIVATE_HANDLE pHandle, int id, long long *timestamp);
int	v4l2_set_preset(V4L2_PRIVATE_HANDLE pHandle, int id, uint32_t preset);
int v4l2_get_device_fd(V4L2_PRIVATE_HANDLE pHandle, int id);
#ifndef ANDROID
#ifdef __cplusplus
}
#endif
#endif

#endif
