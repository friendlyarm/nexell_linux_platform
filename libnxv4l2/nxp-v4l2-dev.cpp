#include "nxp-v4l2-dev.h"

#ifdef ANDROID
#define LOG_TAG "nxp-v4l2-dev"
#include <utils/Log.h>
#else
#define ALOGE(x...) fprintf(stderr, x)
#define ALOGD(x...) fprintf(stderr, x)
#define ALOGV(x...)
#endif

/* V4l2Device */
bool V4l2Device::init(char *name, int entityId,
        int padNum, struct media_pad_desc *pDesc,
        int linkNum, struct media_link_desc *lDesc)
{
#if 0
    int fd = open(name, O_RDWR);
    if (fd < 0) {
        ALOGE("Error open %s", name);
        return false;
    }
#endif

    Name = new char[strlen(name) + 1];
    if (!Name) {
        ALOGE("Error alloc for name %d", strlen(name) + 1);
        return false;
    }

    PadDesc = new struct media_pad_desc[padNum];
    if (!PadDesc) {
        ALOGE("Error alloc for PadDesc %d", sizeof(*pDesc) * padNum);
        delete[] Name;
    }

    LinkDesc = new struct media_link_desc[linkNum];
    if (!LinkDesc) {
        ALOGE("Error alloc for LinkDesc %d", sizeof(*lDesc) * linkNum);
        delete[] PadDesc;
        delete[] Name;
    }

    //FD = fd;
    strcpy(Name, name);
    memcpy(PadDesc, pDesc, sizeof(*pDesc) * padNum);
    memcpy(LinkDesc, lDesc, sizeof(*lDesc) * linkNum);
    EntityID = entityId;
    PadNum = padNum;
    LinkNum = linkNum;

    return true;
}

bool V4l2Device::activate()
{
    //printf("%s: %s %d\n", __func__, Name, EntityID);
    if (Name && FD < 0) {
        int fd = open(Name, O_RDWR);
        if (fd < 0) {
            ALOGE("Error open %s", Name);
            return false;
        }

        FD = fd;
        return true;
    } else {
        ALOGE("invalid device!!!");
        return false;
    }
}

int V4l2Device::setCtrl(int ctrlId, int value)
{
    struct v4l2_control ctrl;
    bzero(&ctrl, sizeof(ctrl));
    ctrl.id = ctrlId;
    ctrl.value = value;
    return ioctl(FD, VIDIOC_S_CTRL, &ctrl);
}

int V4l2Device::getCtrl(int ctrlId, int *value)
{
    struct v4l2_control ctrl;
    bzero(&ctrl, sizeof(ctrl));
    ctrl.id = ctrlId;
    int ret = ioctl(FD, VIDIOC_G_CTRL, &ctrl);
    if (ret)
        return ret;
    *value = ctrl.value;
    return 0;
}

V4l2Device::LinkStatus
V4l2Device::checkLink(int myPad, int remoteEntity, int remotePad)
{
    // ALOGD("%s: myEntity(%d), myPad(%d), remoteEntity(%d), remotePad(%d)", __func__,
    //         EntityID, myPad, remoteEntity, remotePad);
    if (myPad >= PadNum)
        return LINK_INVALID;

    /* loop LinkDesc */
    struct media_link_desc *ldesc = LinkDesc;
    struct media_pad_desc *source, *sink;
    for (int i = 0; i < LinkNum; i++) {
        source = &ldesc->source;
        sink = &ldesc->sink;
        if (sink->entity == EntityID && sink->index == myPad &&
            source->entity == remoteEntity && source->index == remotePad)
            if (ldesc->flags & MEDIA_LNK_FL_ENABLED)
                return LINK_CONNECTED;
            else
                return LINK_DISCONNECTED;
        else if (sink->entity == remoteEntity && sink->index == remotePad &&
                 source->entity == EntityID && source->index == myPad)
            if (ldesc->flags & MEDIA_LNK_FL_ENABLED)
                return LINK_CONNECTED;
            else
                return LINK_DISCONNECTED;
        ldesc++;
    }

    return LINK_INVALID;
}

int V4l2Device::getSinkPadFor(int remoteEntity, int remotePad)
{
    struct media_link_desc *ldesc = LinkDesc;
    struct media_pad_desc *source;
    struct media_pad_desc *sink;
    for (int i = 0; i < LinkNum; i++) {
        source = &ldesc->source;
        sink = &ldesc->sink;
        if (source->entity == remoteEntity)  {
            if (remotePad == -1)
                return sink->index;
            if (source->index == remotePad)
                return sink->index;
        }
        ldesc++;
    }
    return -EINVAL;
}

int V4l2Device::getSourcePadFor(int remoteEntity, int remotePad)
{
    struct media_link_desc *ldesc = LinkDesc;
    struct media_pad_desc *source;
    struct media_pad_desc *sink;
    // ALOGD("%s: remoteEntity(%d), remotePad(%d)", __func__, remoteEntity, remotePad);
    for (int i = 0; i < LinkNum; i++) {
        source = &ldesc->source;
        sink = &ldesc->sink;
        if (sink->entity == remoteEntity) {
            if (remotePad == -1)
                return source->index;
            if (sink->index == remotePad)
                return source->index;
        }
        ldesc++;
    }
    return -EINVAL;
}

/* V4l2Subdev */
int V4l2Subdev::setFormat(int w, int h, int format, int index)
{
    struct v4l2_subdev_format fmt;
    bzero(&fmt, sizeof(fmt));
    fmt.pad = index;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = format;
    fmt.format.width = w;
    fmt.format.height = h;
    fmt.format.field = V4L2_FIELD_NONE;
    return ioctl(FD, VIDIOC_SUBDEV_S_FMT, &fmt);
}

int V4l2Subdev::getFormat(int *w, int *h, int *format, int index)
{
    struct v4l2_subdev_format fmt;
    bzero(&fmt, sizeof(fmt));
    fmt.pad = index;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    int ret = ioctl(FD, VIDIOC_SUBDEV_G_FMT, &fmt);
    if (ret)
        return ret;
    *w = fmt.format.width;
    *h = fmt.format.height;
    *format =  fmt.format.code;
    return 0;
}

int V4l2Subdev::setCrop(int l, int t, int w, int h, int index)
{
    struct v4l2_subdev_crop crop;
    bzero(&crop, sizeof(crop));
    crop.pad = index;
    crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    crop.rect.left = l;
    crop.rect.top = t;
    crop.rect.width = w;
    crop.rect.height = h;
    return ioctl(FD, VIDIOC_SUBDEV_S_CROP, &crop);
}

int V4l2Subdev::getCrop(int *l, int *t, int *w, int *h, int index)
{
    struct v4l2_subdev_crop crop;
    bzero(&crop, sizeof(crop));
    crop.pad = index;
    crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    int ret = ioctl(FD, VIDIOC_SUBDEV_G_CROP, &crop);
    if (ret)
        return ret;
    *l = crop.rect.left;
    *t = crop.rect.top;
    *w = crop.rect.width;
    *h = crop.rect.height;
    return 0;
}

int V4l2Subdev::setPreset(uint32_t preset)
{
    struct v4l2_dv_preset p;
    bzero(&p, sizeof(p));
    p.preset = preset;
    return ioctl(FD, VIDIOC_S_DV_PRESET, &p);
}

/* V4l2Video */
int V4l2Video::setFormat(int w, int h, int format, int index)
{
    struct v4l2_format v4l2_fmt;
    bzero(&v4l2_fmt, sizeof(v4l2_fmt));

    if (!IsM2M) {
        v4l2_fmt.type = static_cast<enum v4l2_buf_type>(BufType);
        v4l2_fmt.fmt.pix_mp.width = w;
        v4l2_fmt.fmt.pix_mp.height = h;
        v4l2_fmt.fmt.pix_mp.pixelformat = format;
        v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        return ioctl(FD, VIDIOC_S_FMT, &v4l2_fmt);
    } else {
        v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        v4l2_fmt.fmt.pix_mp.width = w;
        v4l2_fmt.fmt.pix_mp.height = h;
        v4l2_fmt.fmt.pix_mp.pixelformat = format;
        v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        int ret =  ioctl(FD, VIDIOC_S_FMT, &v4l2_fmt);
        if (ret)
            return ret;
        v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        v4l2_fmt.fmt.pix_mp.width = w;
        v4l2_fmt.fmt.pix_mp.height = h;
        v4l2_fmt.fmt.pix_mp.pixelformat = format;
        v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        return ioctl(FD, VIDIOC_S_FMT, &v4l2_fmt);
    }
}

int V4l2Video::getFormat(int *w, int *h, int *format, int index)
{
    struct v4l2_format v4l2_fmt;
    bzero(&v4l2_fmt, sizeof(v4l2_fmt));
    v4l2_fmt.type = static_cast<enum v4l2_buf_type>(BufType);
    v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    int ret = ioctl(FD, VIDIOC_G_FMT, &v4l2_fmt);
    if (ret)
        return ret;
    *w = v4l2_fmt.fmt.pix_mp.width;
    *h = v4l2_fmt.fmt.pix_mp.height;
    *format = v4l2_fmt.fmt.pix_mp.pixelformat;
    return 0;
}

int V4l2Video::setCrop(int l, int t, int w, int h, int index)
{
    struct v4l2_crop crop;
    bzero(&crop, sizeof(crop));
    crop.type = static_cast<enum v4l2_buf_type>(BufType);
    crop.pad = index;
    crop.c.left = l;
    crop.c.top = t;
    crop.c.width = w;
    crop.c.height = h;
    return ioctl(FD, VIDIOC_S_CROP, &crop);
}

int V4l2Video::getCrop(int *l, int *t, int *w, int *h, int index)
{
    struct v4l2_crop crop;
    bzero(&crop, sizeof(crop));
    crop.type = static_cast<enum v4l2_buf_type>(BufType);
    int ret = ioctl(FD, VIDIOC_G_CROP, &crop);
    if (ret)
        return ret;
    *l = crop.c.left;
    *t = crop.c.top;
    *w = crop.c.width;
    *h = crop.c.height;
    return 0;
}

int V4l2Video::reqBuf(int count)
{
    struct v4l2_requestbuffers req;
    bzero(&req, sizeof(req));
    req.count = count;
    req.memory = static_cast<enum v4l2_memory>(MemoryType);

    if (!IsM2M) {
        req.type = static_cast<enum v4l2_buf_type>(BufType);
        return ioctl(FD, VIDIOC_REQBUFS, &req);
    } else {
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        int ret = ioctl(FD, VIDIOC_REQBUFS, &req);
        if (ret)
            return ret;
        req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        return ioctl(FD, VIDIOC_REQBUFS, &req);
    }
}

int V4l2Video::qBuf(int planeNum, int index0, int *fds0, int *sizes0,
            int index1, int *fds1, int *sizes1, int *syncfd0, int *syncfd1)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[NXP_VIDEO_MAX_BUFFER_PLANES];
    int i;

    bzero(&v4l2_buf, sizeof(v4l2_buf));
    if (!IsM2M) {
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = static_cast<enum v4l2_buf_type>(BufType);
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.index = index0;
        v4l2_buf.length = planeNum;
        if (syncfd0 != NULL) {
            v4l2_buf.flags = V4L2_BUF_FLAG_USE_SYNC;
            v4l2_buf.reserved = -1; // acquire fence fd
        }

        for (i = 0; i < planeNum; i++) {
            v4l2_buf.m.planes[i].m.fd = fds0[i];
            v4l2_buf.m.planes[i].length = sizes0[i];
        }

        int ret =  ioctl(FD, VIDIOC_QBUF, &v4l2_buf);
        if (ret == 0 && syncfd0 != NULL) {
            *syncfd0 = v4l2_buf.reserved;
        }
        return ret;
    } else {
        /* M2M device */
        /* first q out : output */
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.index = index0;
        v4l2_buf.length = planeNum;
        if (syncfd0 != NULL) {
            v4l2_buf.flags = V4L2_BUF_FLAG_USE_SYNC;
            v4l2_buf.reserved = -1; // acquire fence fd
        }

        for (i = 0; i < planeNum; i++) {
            v4l2_buf.m.planes[i].m.fd = fds0[i];
            v4l2_buf.m.planes[i].length = sizes0[i];
        }

        int ret = ioctl(FD, VIDIOC_QBUF, &v4l2_buf);
        if (ret)
            return ret;

        if (syncfd0 != NULL)
            *syncfd0 = v4l2_buf.reserved;

        /* second q in : capture */
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.index = index1;
        v4l2_buf.length = planeNum;
        if (syncfd1 != NULL) {
            v4l2_buf.flags = V4L2_BUF_FLAG_USE_SYNC;
            v4l2_buf.reserved = -1; // acquire fence fd
        }

        for (i = 0; i < planeNum; i++) {
            v4l2_buf.m.planes[i].m.fd = fds1[i];
            v4l2_buf.m.planes[i].length = sizes1[i];
        }

        ret = ioctl(FD, VIDIOC_QBUF, &v4l2_buf);
        if (ret)
            return ret;

        if (syncfd1 != NULL)
            *syncfd1 = v4l2_buf.reserved;

        return 0;
    }
}

int V4l2Video::qBuf(int planeNum, int index0, int const *fds0, int const *sizes0,
            int index1, int const *fds1, int const *sizes1, int *syncfd0, int *syncfd1)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[NXP_VIDEO_MAX_BUFFER_PLANES];
    int i;

    bzero(&v4l2_buf, sizeof(v4l2_buf));
    if (!IsM2M) {
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = static_cast<enum v4l2_buf_type>(BufType);
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.index = index0;
        v4l2_buf.length = planeNum;
        if (syncfd0 != NULL) {
            v4l2_buf.flags = V4L2_BUF_FLAG_USE_SYNC;
            v4l2_buf.reserved = -1; // acquire fence fd
        }

        for (i = 0; i < planeNum; i++) {
            v4l2_buf.m.planes[i].m.fd = fds0[i];
            v4l2_buf.m.planes[i].length = sizes0[i];
        }

        int ret =  ioctl(FD, VIDIOC_QBUF, &v4l2_buf);
        if (ret == 0 && syncfd0 != NULL) {
            *syncfd0 = v4l2_buf.reserved;
        }
        return ret;
    } else {
        /* M2M device */
        /* first q out : output */
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.index = index0;
        v4l2_buf.length = planeNum;
        if (syncfd0 != NULL) {
            v4l2_buf.flags = V4L2_BUF_FLAG_USE_SYNC;
            v4l2_buf.reserved = -1; // acquire fence fd
        }

        for (i = 0; i < planeNum; i++) {
            v4l2_buf.m.planes[i].m.fd = fds0[i];
            v4l2_buf.m.planes[i].length = sizes0[i];
        }

        int ret = ioctl(FD, VIDIOC_QBUF, &v4l2_buf);
        if (ret)
            return ret;

        if (syncfd0 != NULL)
            *syncfd0 = v4l2_buf.reserved;

        /* second q in : capture */
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.index = index1;
        v4l2_buf.length = planeNum;
        if (syncfd1 != NULL) {
            v4l2_buf.flags = V4L2_BUF_FLAG_USE_SYNC;
            v4l2_buf.reserved = -1; // acquire fence fd
        }

        for (i = 0; i < planeNum; i++) {
            v4l2_buf.m.planes[i].m.fd = fds1[i];
            v4l2_buf.m.planes[i].length = sizes1[i];
        }

        ret = ioctl(FD, VIDIOC_QBUF, &v4l2_buf);
        if (ret)
            return ret;

        if (syncfd1 != NULL)
            *syncfd1 = v4l2_buf.reserved;

        return 0;
    }
}

int V4l2Video::dqBuf(int planeNum, int *index0, int *index1)
{
    int ret;
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[NXP_VIDEO_MAX_BUFFER_PLANES];

    if (!IsM2M) {
        v4l2_buf.type = static_cast<enum v4l2_buf_type>(BufType);
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.m.planes = planes;
        v4l2_buf.length = planeNum;
        ret = ioctl(FD, VIDIOC_DQBUF, &v4l2_buf);
        if (ret < 0)
            return ret;
        *index0 = v4l2_buf.index;
    } else {
        /* M2M device */
        /* first dq in : capture */
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.m.planes = planes;
        v4l2_buf.length = planeNum;
        ret = ioctl(FD, VIDIOC_DQBUF, &v4l2_buf);
        if (ret < 0)
            return ret;
        *index0 = v4l2_buf.index;
        /* second dq out : output */
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        v4l2_buf.memory = static_cast<enum v4l2_memory>(MemoryType);
        v4l2_buf.m.planes = planes;
        v4l2_buf.length = planeNum;
        ret = ioctl(FD, VIDIOC_DQBUF, &v4l2_buf);
        if (ret < 0)
            return ret;
        *index1 = v4l2_buf.index;
    }

    TimeStamp = v4l2_buf.timestamp.tv_sec * 1000000000LL + v4l2_buf.timestamp.tv_usec * 1000LL;

    return 0;
}

int V4l2Video::streamOn()
{
    if (!IsM2M) {
        ALOGV("%s: name %s", __func__, getName());
        return ioctl(FD, VIDIOC_STREAMON, &BufType);
    } else {
        /* first on out */
        unsigned int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        int ret = ioctl(FD, VIDIOC_STREAMON, &type);
        if (ret)
            return ret;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        return ioctl(FD, VIDIOC_STREAMON, &type);
    }
}

int V4l2Video::streamOff()
{
    if (!IsM2M) {
        ALOGV("%s: name %s", __func__, getName());
        return ioctl(FD, VIDIOC_STREAMOFF, &BufType);
    } else {
        /* first off in: capture */
        unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        int ret = ioctl(FD, VIDIOC_STREAMOFF, &type);
        if (ret)
            return ret;
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        return ioctl(FD, VIDIOC_STREAMOFF, &type);
    }
}

/* V4l2Composite */
bool V4l2Composite::activate()
{
#if 0
    bool ret;
    ALOGV("%s", __func__);
    if (VideoDev) {
        ret = VideoDev->activate();
        if (ret == false) {
            ALOGE("%s: VideoDev activate failed", __func__);
            return ret;
        }
    }

    if (SubDev) {
        return SubDev->activate();
    }
#endif
    return true;
}

bool V4l2Composite::linkDefault(int srcPad, int sinkPad)
{
    // TODO
    if (Type == capture) {
        int myPad;
        if (srcPad != -1) {
            myPad = srcPad;
        } else {
            myPad = getSourcePadFor(VideoDev->getEntityID(), 0);
            if (myPad < 0) {
                ALOGE("pad invalid");
                return false;
            }
        }
        int remotePad = 0;
        if (sinkPad != -1) {
            remotePad = sinkPad;
        }
        V4l2Device::LinkStatus lstat = checkLink(myPad, VideoDev->getEntityID(), 0);
        if (lstat == LINK_INVALID) {
            ALOGE("link [%d,%d] to [%d,%d] is invalid",
                    SubDev->getEntityID(), myPad,
                    VideoDev->getEntityID(), 0);
            return false;
        }
        if (lstat == LINK_CONNECTED)
            return true;

        struct media_link_desc setupLink;
        memset(&setupLink, 0, sizeof(setupLink));

        setupLink.flags |= MEDIA_LNK_FL_ENABLED;

        setupLink.source.entity = SubDev->getEntityID();
        setupLink.source.index = myPad;
        setupLink.source.flags = MEDIA_PAD_FL_SOURCE;

        setupLink.sink.entity = VideoDev->getEntityID();
        setupLink.sink.index = 0;
        setupLink.sink.flags = MEDIA_PAD_FL_SINK;

        int ret = ioctl(MediaFD, MEDIA_IOC_SETUP_LINK, &setupLink);
        if (ret) {
            ALOGE("%s: failed to IOC_SETUP_LINK(ret: %d", __func__, ret);
            return false;
        }
        return true;
    } else if (Type == out) {
        int myPad;
        if (srcPad != -1) {
            myPad = srcPad;
        } else {
            myPad = VideoDev->getSourcePadFor(SubDev->getEntityID(), 1);
            if (myPad < 0) {
                ALOGE("pad invalid");
                return false;
            }
        }
        int remotePad = 0;
        if (sinkPad != -1) {
            remotePad = sinkPad;
        }
        V4l2Device::LinkStatus lstat = VideoDev->checkLink(myPad, SubDev->getEntityID(), remotePad);
        if (lstat == LINK_INVALID) {
            ALOGE("link [%d,%d] to [%d,%d] is invalid",
                    VideoDev->getEntityID(), myPad,
                    SubDev->getEntityID(), 0);
            return false;
        }
        if (lstat == LINK_CONNECTED)
            return true;

        struct media_link_desc setupLink;
        memset(&setupLink, 0, sizeof(setupLink));

        setupLink.flags |= MEDIA_LNK_FL_ENABLED;

        setupLink.source.entity = VideoDev->getEntityID();
        setupLink.source.index = myPad;
        setupLink.source.flags = MEDIA_PAD_FL_SOURCE;

        setupLink.sink.entity = SubDev->getEntityID();
        setupLink.sink.index = remotePad;
        setupLink.sink.flags = MEDIA_PAD_FL_SINK;

        int ret = ioctl(MediaFD, MEDIA_IOC_SETUP_LINK, &setupLink);
        if (ret) {
            ALOGE("%s: failed to IOC_SETUP_LINK(ret: %d", __func__, ret);
            return false;
        }
        return true;
    } else {
        // m2m, TODO
        ALOGV("%s: type m2m", __func__);
        return true;
    }
}
