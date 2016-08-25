#include "nxp-v4l2-media.h"

#ifdef ANDROID
#define LOG_TAG "nxp-v4l2"
#include <utils/Log.h>
#include <system/graphics.h>
#include <nexell_format.h>
#endif
#include "assert.h"
#include "nxp-v4l2-private.cpp"

/**
 * static interface class
 */
static V4l2NexellPrivate *_priv = NULL;

/**
 * format
 */
struct PixFormatPixCode PixelArray[MAX_PIXFORMAT] = {
    {PIXFORMAT_YUV422_PACKED, PIXCODE_YUV422_PACKED},
    {PIXFORMAT_YUV420_PLANAR, PIXCODE_YUV420_PLANAR},       //  3 Plane
    {PIXFORMAT_YUV420_YV12,   PIXCODE_YUV420_PLANAR},       //  1 Plane
    {PIXFORMAT_YUV422_PLANAR, PIXCODE_YUV422_PLANAR},
    {PIXFORMAT_YUV444_PLANAR, PIXCODE_YUV444_PLANAR}
};

/**
 * public api
 */
V4L2_PRIVATE_HANDLE v4l2_init(const struct V4l2UsageScheme *scheme)
{
	V4l2NexellPrivate *_priv = NULL;
    
	if (!_priv) {
        _priv = new V4l2NexellPrivate();
        if (!_priv) {
            ALOGE("Fatal Error: can't allocate context");
            return _priv;
        }

        int ret = _priv->init(scheme);
        if (ret < 0) {
            ALOGE("Fatal Error: init failed");
            delete _priv;
            _priv = NULL;
            return _priv;
        }
    }
    return (V4L2_PRIVATE_HANDLE*)_priv;
}

void v4l2_exit(V4L2_PRIVATE_HANDLE pHandle)
{
    if (pHandle) {
        delete (V4l2NexellPrivate*)pHandle;
        pHandle = NULL;
    }
}

int v4l2_link(V4L2_PRIVATE_HANDLE pHandle, int src_id, int dst_id)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->link(src_id, dst_id);
}

int v4l2_unlink(V4L2_PRIVATE_HANDLE pHandle, int src_id, int dst_id)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->unlink(src_id, dst_id);
}

int v4l2_set_format(V4L2_PRIVATE_HANDLE pHandle, int id, int w, int h, int f)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->setFormat(id, w, h, f);
}

int v4l2_get_format(V4L2_PRIVATE_HANDLE pHandle, int id, int *w, int *h, int *f)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->getFormat(id, w, h, f);
}

int v4l2_set_crop(V4L2_PRIVATE_HANDLE pHandle, int id, int l, int t, int w, int h)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->setCrop(id, l, t, w, h);
}

int v4l2_get_crop(V4L2_PRIVATE_HANDLE pHandle, int id, int *l, int *t, int *w, int *h)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->getCrop(id, l, t, w, h);
}

int v4l2_set_format_with_pad(V4L2_PRIVATE_HANDLE pHandle, int id, int pad, int w, int h, int f)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->setFormat(id, w, h, f, pad);
}

int v4l2_get_format_with_pad(V4L2_PRIVATE_HANDLE pHandle, int id, int pad, int *w, int *h, int *f)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->getFormat(id, w, h, f, pad);
}

int v4l2_set_crop_with_pad(V4L2_PRIVATE_HANDLE pHandle, int id, int pad, int l, int t, int w, int h)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->setCrop(id, l, t, w, h, pad);
}

int v4l2_get_crop_with_pad(V4L2_PRIVATE_HANDLE pHandle, int id, int pad, int *l, int *t, int *w, int *h)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->getCrop(id, l, t, w, h, pad);
}

int v4l2_set_ctrl(V4L2_PRIVATE_HANDLE pHandle, int id, int ctrl_id, int value)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->setCtrl(id, ctrl_id, value);
}

int v4l2_get_ctrl(V4L2_PRIVATE_HANDLE pHandle, int id, int ctrl_id, int *value)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->getCtrl(id, ctrl_id, value);
}

int v4l2_reqbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int buf_count)
{
	assert(pHandle);
    return ((V4l2NexellPrivate*)pHandle)->reqBuf(id, buf_count);
}

#ifdef ANDROID
int v4l2_qbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int plane_num, int index0, struct private_handle_t *b0, int index1, struct private_handle_t *b1, 
	int *syncfd0, int *syncfd1)
{
	assert(pHandle);

    if (b1) {
        if (plane_num == 1)
            return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, &b0->share_fd, &b0->size, syncfd0, index1, &b1->share_fd, &b1->size, syncfd1);
        else {
            int srcFds0[3] = { b0->share_fd, b0->share_fd1, b0->share_fd2 };
            int srcFds1[3] = { b1->share_fd, b1->share_fd1, b1->share_fd2 };
            int sizes0[3];
            sizes0[0] = b0->stride * ALIGN(b0->height, 16);
            switch (b0->format) {
            case HAL_PIXEL_FORMAT_YV12:
                sizes0[1] = ALIGN(b0->stride >> 1, 16) * ALIGN(b0->height >> 1, 16);
                sizes0[2] = sizes0[1];
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                sizes0[1] = b0->stride * ALIGN(b0->height >> 1, 16);
                sizes0[2] = 0;
                break;
            }
            int sizes1[3];
            sizes1[0] = b1->stride * ALIGN(b1->height, 16);
            switch (b1->format) {
            case HAL_PIXEL_FORMAT_YV12:
                sizes1[1] = ALIGN(b1->stride >> 1, 16) * ALIGN(b1->height >> 1, 16);
                sizes1[2] = sizes1[1];
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                sizes1[1] = b1->stride * ALIGN(b1->height >> 1, 16);
                sizes1[2] = 0;
                break;
            }
            return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, srcFds0, sizes0, syncfd0, index1, srcFds1, sizes1, syncfd1);
		}
    } else {
        if (plane_num == 1)
            return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, &b0->share_fd, &b0->size, syncfd0);
        else {
            int srcFds0[3] = { b0->share_fd, b0->share_fd1, b0->share_fd2 };
            int sizes0[3];
            sizes0[0] = b0->stride * ALIGN(b0->height, 16);
            switch (b0->format) {
            case HAL_PIXEL_FORMAT_YV12:
                sizes0[1] = ALIGN(b0->stride >> 1, 16) * ALIGN(b0->height >> 1, 16);
                sizes0[2] = sizes0[1];
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                sizes0[1] = b0->stride * ALIGN(b0->height >> 1, 16);
                sizes0[2] = 0;
                break;
            }
            return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, srcFds0, sizes0, syncfd0);
		}
    }
}

int v4l2_qbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int plane_num, int index0, struct private_handle_t const *b0, int index1, struct private_handle_t const *b1, 
	int *syncfd0, int *syncfd1)
{
	assert(pHandle);

    if (b1) {
        if (plane_num == 1)
            return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, &b0->share_fd, &b0->size, syncfd0, index1, &b1->share_fd, &b1->size, syncfd1);
        else {
            int srcFds0[3] = { b0->share_fd, b0->share_fd1, b0->share_fd2 };
            int srcFds1[3] = { b1->share_fd, b1->share_fd1, b1->share_fd2 };
            int sizes0[3];
            sizes0[0] = b0->stride * ALIGN(b0->height, 16);
            switch (b0->format) {
            case HAL_PIXEL_FORMAT_YV12:
                sizes0[1] = ALIGN(b0->stride >> 1, 16) * ALIGN(b0->height >> 1, 16);
                sizes0[2] = sizes0[1];
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                sizes0[1] = b0->stride * ALIGN(b0->height >> 1, 16);
                sizes0[2] = 0;
                break;
            }
            int sizes1[3];
            sizes1[0] = b1->stride * ALIGN(b1->height, 16);
            switch (b1->format) {
            case HAL_PIXEL_FORMAT_YV12:
                sizes1[1] = ALIGN(b1->stride >> 1, 16) * ALIGN(b1->height >> 1, 16);
                sizes1[2] = sizes1[1];
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                sizes1[1] = b1->stride * ALIGN(b1->height >> 1, 16);
                sizes1[2] = 0;
                break;
            }
            return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, srcFds0, sizes0, syncfd0, index1, srcFds1, sizes1, syncfd1);
		}
    } else {
        if (plane_num == 1)
            return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, &b0->share_fd, &b0->size, syncfd0);
        else {
            int srcFds0[3] = { b0->share_fd, b0->share_fd1, b0->share_fd2 };
            int sizes0[3];
            sizes0[0] = b0->stride * ALIGN(b0->height, 16);
            switch (b0->format) {
            case HAL_PIXEL_FORMAT_YV12:
                sizes0[1] = ALIGN(b0->stride >> 1, 16) * ALIGN(b0->height >> 1, 16);
                sizes0[2] = sizes0[1];
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                sizes0[1] = b0->stride * ALIGN(b0->height >> 1, 16);
                sizes0[2] = 0;
                break;
            }
            return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, srcFds0, sizes0, syncfd0);
		}
    }
}
#endif

int v4l2_qbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int plane_num, int index0, struct nxp_vid_buffer *b0, int index1, struct nxp_vid_buffer *b1)
{
	assert(pHandle);

    if (b1)
        return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, b0->fds, b0->sizes, NULL, index1, b1->fds, b1->sizes, NULL);
    else
        return ((V4l2NexellPrivate*)pHandle)->qBuf(id, plane_num, index0, b0->fds, b0->sizes, NULL);
}

int v4l2_dqbuf(V4L2_PRIVATE_HANDLE pHandle, int id, int plane_num, int *index0, int *index1)
{
	assert(pHandle);

    if (index1)
        return ((V4l2NexellPrivate*)pHandle)->dqBuf(id, plane_num, index0, index1);
    else
        return ((V4l2NexellPrivate*)pHandle)->dqBuf(id, plane_num, index0);
}

int v4l2_streamon(V4L2_PRIVATE_HANDLE pHandle, int id)
{
	assert(pHandle);

    return ((V4l2NexellPrivate*)pHandle)->streamOn(id);
}

int v4l2_streamoff(V4L2_PRIVATE_HANDLE pHandle, int id)
{
	assert(pHandle);

    return ((V4l2NexellPrivate*)pHandle)->streamOff(id);
}

int v4l2_get_timestamp(V4L2_PRIVATE_HANDLE pHandle, int id, long long *timestamp)
{
	assert(pHandle);

    return ((V4l2NexellPrivate*)pHandle)->getTimeStamp(id, timestamp);
}

int v4l2_set_preset(V4L2_PRIVATE_HANDLE pHandle, int id, uint32_t preset)
{
	assert(pHandle);

    return ((V4l2NexellPrivate*)pHandle)->setPreset(id, preset);
}

int v4l2_get_device_fd(V4L2_PRIVATE_HANDLE pHandle, int id)
{
	assert(pHandle);

	return ((V4l2NexellPrivate*)pHandle)->getDeviceFD(id);
}
