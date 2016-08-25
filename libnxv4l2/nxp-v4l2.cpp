#include "nxp-v4l2.h"

#ifdef ANDROID
#define LOG_TAG "nxp-v4l2"
#include <utils/Log.h>
#include <system/graphics.h>
#include <nexell_format.h>
#endif

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
int v4l2_init(const struct V4l2UsageScheme *scheme)
{
    if (!_priv) {
        _priv = new V4l2NexellPrivate();
        if (!_priv) {
            ALOGE("Fatal Error: can't allocate context");
            return -ENOMEM;
        }

        int ret = _priv->init(scheme);
        if (ret < 0) {
            ALOGE("Fatal Error: init failed");
            delete _priv;
            _priv = NULL;
            return -EINVAL;
        }
    }

    return 0;
}

void v4l2_exit(void)
{
    if (_priv) {
        delete _priv;
        _priv = NULL;
    }
}

int v4l2_link(int src_id, int dst_id)
{
    return _priv->link(src_id, dst_id);
}

int v4l2_unlink(int src_id, int dst_id)
{
    return _priv->unlink(src_id, dst_id);
}

int v4l2_set_format(int id, int w, int h, int f)
{
    return _priv->setFormat(id, w, h, f);
}

int v4l2_get_format(int id, int *w, int *h, int *f)
{
    return _priv->getFormat(id, w, h, f);
}

int v4l2_set_crop(int id, int l, int t, int w, int h)
{
    return _priv->setCrop(id, l, t, w, h);
}

int v4l2_get_crop(int id, int *l, int *t, int *w, int *h)
{
    return _priv->getCrop(id, l, t, w, h);
}

int v4l2_set_format_with_pad(int id, int pad, int w, int h, int f)
{
    return _priv->setFormat(id, w, h, f, pad);
}

int v4l2_get_format_with_pad(int id, int pad, int *w, int *h, int *f)
{
    return _priv->getFormat(id, w, h, f, pad);
}

int v4l2_set_crop_with_pad(int id, int pad, int l, int t, int w, int h)
{
    return _priv->setCrop(id, l, t, w, h, pad);
}

int v4l2_get_crop_with_pad(int id, int pad, int *l, int *t, int *w, int *h)
{
    return _priv->getCrop(id, l, t, w, h, pad);
}

int v4l2_set_ctrl(int id, int ctrl_id, int value)
{
    return _priv->setCtrl(id, ctrl_id, value);
}

int v4l2_get_ctrl(int id, int ctrl_id, int *value)
{
    return _priv->getCtrl(id, ctrl_id, value);
}

int v4l2_reqbuf(int id, int buf_count)
{
    return _priv->reqBuf(id, buf_count);
}

#ifdef ANDROID
int v4l2_qbuf(int id, int plane_num, int index0, struct private_handle_t *b0, int index1, struct private_handle_t *b1, int *syncfd0, int *syncfd1)
{
    if (b1) {
        if (plane_num == 1)
            return _priv->qBuf(id, plane_num, index0, &b0->share_fd, &b0->size, syncfd0, index1, &b1->share_fd, &b1->size, syncfd1);
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
            return _priv->qBuf(id, plane_num, index0, srcFds0, sizes0, syncfd0, index1, srcFds1, sizes1, syncfd1);
        }
    } else {
        if (plane_num == 1)
            return _priv->qBuf(id, plane_num, index0, &b0->share_fd, &b0->size, syncfd0);
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
            return _priv->qBuf(id, plane_num, index0, srcFds0, sizes0, syncfd0);
        }
    }
}

int v4l2_qbuf(int id, int plane_num, int index0, struct private_handle_t const *b0, int index1, struct private_handle_t const *b1,
        int *syncfd0, int *syncfd1)
{
    if (b1) {
        if (plane_num == 1)
            return _priv->qBuf(id, plane_num, index0, &b0->share_fd, &b0->size, syncfd0, index1, &b1->share_fd, &b1->size, syncfd1);
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
            return _priv->qBuf(id, plane_num, index0, srcFds0, sizes0, syncfd0, index1, srcFds1, sizes1, syncfd1);
        }
    } else {
        if (plane_num == 1)
            return _priv->qBuf(id, plane_num, index0, &b0->share_fd, &b0->size, syncfd0);
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
            return _priv->qBuf(id, plane_num, index0, srcFds0, sizes0, syncfd0);
        }
    }
}
#endif

int v4l2_qbuf(int id, int plane_num, int index0, struct nxp_vid_buffer *b0, int index1, struct nxp_vid_buffer *b1)
{
    if (b1)
        return _priv->qBuf(id, plane_num, index0, b0->fds, b0->sizes, NULL, index1, b1->fds, b1->sizes, NULL);
    else
        return _priv->qBuf(id, plane_num, index0, b0->fds, b0->sizes, NULL);
}

int v4l2_dqbuf(int id, int plane_num, int *index0, int *index1)
{
    if (index1)
        return _priv->dqBuf(id, plane_num, index0, index1);
    else
        return _priv->dqBuf(id, plane_num, index0);
}

int v4l2_streamon(int id)
{
    return _priv->streamOn(id);
}

int v4l2_streamoff(int id)
{
    return _priv->streamOff(id);
}

int v4l2_get_timestamp(int id, long long *timestamp)
{
    return _priv->getTimeStamp(id, timestamp);
}

int v4l2_set_preset(int id, uint32_t preset)
{
    return _priv->setPreset(id, preset);
}

int v4l2_get_device_fd(int id)
{
     return _priv->getDeviceFD(id);
}

