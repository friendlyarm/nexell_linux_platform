#ifndef _NXP_V4L2_DEV_H
#define _NXP_V4L2_DEV_H

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#ifdef ANDROID
#include <utils/Log.h>
#endif

class V4l2Device {
public:
    typedef enum {
        LINK_INVALID  = 0,
        LINK_DISCONNECTED,
        LINK_CONNECTED,
    } LinkStatus;

public:
    V4l2Device()
        : FD(-1),
          EntityID(-1),
          PadNum(0),
          LinkNum(0),
          Name(NULL),
          PadDesc(NULL),
          LinkDesc(NULL)
    {
    }

    virtual ~V4l2Device() {
        if (FD >= 0)
            close(FD);
        if (Name)
            delete[] Name;
        if (PadDesc)
            delete[] PadDesc;
        if (LinkDesc)
            delete[] LinkDesc;
    }

    /* for link */
    virtual LinkStatus checkLink(int myPad, int remoteEntity, int remotePad);
    virtual int getSinkPadFor(int remoteEntity, int remotePad = -1);
    virtual int getSourcePadFor(int remoteEntity, int remotePad = -1);

    /* pure virtual */
    virtual int setFormat(int w, int h, int format, int index = 0) = 0;
    virtual int getFormat(int *w, int *h, int *format, int index = 0) = 0;
    virtual int setCrop(int l, int t, int w, int h, int index = 0) = 0;
    virtual int getCrop(int *l, int *t, int *w, int *h, int index = 0) = 0;
    virtual int reqBuf(int count) = 0;
    virtual int qBuf(int planeNum, int index0, int *fds0, int *sizes0,
            int index1 = 0, int *fds1 = NULL, int *sizes1 = NULL, int *syncfd0 = NULL, int *syncfd1 = NULL) = 0;
    virtual int qBuf(int planeNum, int index0, int const *fds0, int const *sizes0,
            int index1 = 0, int const *fds1 = NULL, int const *sizes1 = NULL, int *syncfd0 = NULL, int *syncfd1 = NULL) = 0;
    virtual int dqBuf(int planeNum, int *index0, int *index1 = NULL) = 0;
    virtual int streamOn() = 0;
    virtual int streamOff() = 0;
    virtual long long getTimeStamp() = 0;
    virtual int setPreset(uint32_t preset) = 0;

    /* common */
    virtual int setCtrl(int ctrlId, int value);
    virtual int getCtrl(int ctrlId, int *value);

    virtual bool activate();

    virtual bool linkDefault(int srcPad=-1, int sinkPad=-1) = 0;
public:
    /* accessors */
    char *getName() {
        return Name;
    }
    int getFD() {
        return FD;
    }
    int getEntityID() {
        return EntityID;
    }
    int getPadNum() {
        return PadNum;
    }
    struct media_pad_desc *getPadDesc() {
        return PadDesc;
    }
    int getLinkNum() {
        return LinkNum;
    }
    struct media_link_desc *getLinkDesc() {
        return LinkDesc;
    }

protected:
    bool init(char *name, int entityId, int padNum, struct media_pad_desc *pDesc,
            int linkNum, struct media_link_desc *lDesc);

protected:
    /* device node name */
    int FD;
    int EntityID;
    int PadNum;
    int LinkNum;
    char *Name;
    struct media_pad_desc *PadDesc;
    struct media_link_desc *LinkDesc;
};

class V4l2Subdev : public V4l2Device {
public:
    V4l2Subdev(char *name, int entityId, int padNum, struct media_pad_desc *pDesc,
            int linkNum, struct media_link_desc *lDesc) {
        if (!init(name, entityId, padNum, pDesc, linkNum, lDesc))
#ifdef ANDROID
            return;
#else
            throw -EINVAL;
#endif
    }
    virtual ~V4l2Subdev() {
    }

    virtual int setFormat(int w, int h, int format, int index = 0);
    virtual int getFormat(int *w, int *h, int *format, int index = 0);
    virtual int setCrop(int l, int t, int w, int h, int index = 0);
    virtual int getCrop(int *l, int *t, int *w, int *h, int index = 0);
    virtual int setPreset(uint32_t preset);
    virtual int reqBuf(int count) {
        return -EINVAL;
    }
    virtual int qBuf(int planeNum, int index0, int *fds0, int *sizes0,
            int index1 = 0, int *fds1 = NULL, int *sizes1 = NULL, int *syncfd0 = NULL, int *syncfd1 = NULL) {
        return -EINVAL;
    }
    virtual int qBuf(int planeNum, int index0, int const *fds0, int const *sizes0,
            int index1 = 0, int const *fds1 = NULL, int const *sizes1 = NULL, int *syncfd0 = NULL, int *syncfd1 = NULL) {
        return -EINVAL;
    }
    virtual int dqBuf(int planeNum, int *index0, int *index1) {
        return -EINVAL;
    }
    virtual int streamOn() {
        return -EINVAL;
    }
    virtual int streamOff() {
        return -EINVAL;
    }
    virtual long long getTimeStamp() {
        return 0;
    }
    virtual bool linkDefault(int srcPad=-1, int sinkPad=-1) {
        return true;
    }
};

#define NXP_VIDEO_MAX_BUFFER_PLANES 3
class V4l2Video : public V4l2Device {
public:
    V4l2Video(char *name, int entityId, int padNum, struct media_pad_desc *pDesc,
            int linkNum, struct media_link_desc *lDesc,
            bool isM2M, unsigned int bufType, unsigned int memoryType)
        : IsM2M(isM2M),
          BufType(bufType),
          MemoryType(memoryType)
    {
        if (!init(name, entityId, padNum, pDesc, linkNum, lDesc))
#ifdef ANDROID
            return;
#else
            throw -EINVAL;
#endif
        if (isM2M)
            BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }
    virtual ~V4l2Video() {
    }

    virtual int setFormat(int w, int h, int format, int index = 0);
    virtual int getFormat(int *w, int *h, int *format, int index = 0);
    virtual int setCrop(int l, int t, int w, int h, int index = 0);
    virtual int getCrop(int *l, int *t, int *w, int *h, int index = 0);
    virtual int reqBuf(int count);
    virtual int qBuf(int planeNum, int index0, int *fds0, int *sizes0,
            int index1 = 0, int *fds1 = NULL, int *sizes1 = NULL, int *syncfd0 = NULL, int *syncfd1 = NULL);
    virtual int qBuf(int planeNum, int index0, int const *fds0, int const *sizes0,
            int index1 = 0, int const *fds1 = NULL, int const *sizes1 = NULL, int *syncfd0 = NULL, int *syncfd1 = NULL);
    virtual int dqBuf(int planeNum, int *index0, int *index1 = NULL);
    virtual int streamOn();
    virtual int streamOff();
    virtual long long getTimeStamp() {
        return TimeStamp;
    }
    virtual bool linkDefault(int srcPad=-1, int sinkPad=-1) {
        return true;
    }
    virtual int setPreset(uint32_t preset) {
        return -EINVAL;
    }

private:
    bool IsM2M;
    /*
     * V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
     * V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
     */
    unsigned int BufType;
    /*
     * V4L2_MEMORY_DMABUF
	 * V4L2_MEMORY_MMAP
	 * V4L2_MEMORY_USERPTR
     */
    unsigned int MemoryType;

    long long TimeStamp;
};

class V4l2Composite : public V4l2Device {
public:
    typedef enum {
        capture,
        m2m,
        out
    } CompositionType;

    V4l2Composite() {
    }
    V4l2Composite(int mediaFD, V4l2Device *subDev, V4l2Device *videoDev, CompositionType type = capture)
        : MediaFD(mediaFD),
          SubDev(subDev),
          VideoDev(videoDev),
          Type(type)
    {
    }
    virtual ~V4l2Composite() {
    }

    virtual int setFormat(int w, int h, int format, int index = 0) {
        return VideoDev->setFormat(w, h, format);
    }
    virtual int getFormat(int *w, int *h, int *format, int index = 0) {
        return VideoDev->getFormat(w, h, format);
    }
    virtual int setCrop(int l, int t, int w, int h, int index = 0) {
        return VideoDev->setCrop(l, t, w, h, index);
    }
    virtual int getCrop(int *l, int *t, int *w, int *h, int index = 0) {
        return VideoDev->getCrop(l, t, w, h);
    }
    virtual int setCtrl(int ctrlId, int value) {
        return VideoDev->setCtrl(ctrlId, value);
    }
    virtual int getCtrl(int ctrlId, int *value) {
        return VideoDev->getCtrl(ctrlId, value);
    }
    virtual int reqBuf(int count) {
        return VideoDev->reqBuf(count);
    }
    virtual int qBuf(int planeNum, int index0, int *fds0, int *sizes0,
            int index1 = 0, int *fds1 = NULL, int *sizes1 = NULL, int *syncfd0 = NULL, int *syncfd1 = NULL) {
        return VideoDev->qBuf(planeNum,
                index0, fds0, sizes0, index1, fds1, sizes1, syncfd0, syncfd1);
    }
    virtual int qBuf(int planeNum, int index0, int const *fds0, int const *sizes0,
            int index1 = 0, int const *fds1 = NULL, int const *sizes1 = NULL, int *syncfd0 = NULL, int *syncfd1 = NULL) {
        return VideoDev->qBuf(planeNum,
                index0, fds0, sizes0, index1, fds1, sizes1, syncfd0, syncfd1);
    }
    virtual int dqBuf(int planeNum, int *index0, int *index1 = NULL) {
        return VideoDev->dqBuf(planeNum, index0, index1);
    }
    virtual int streamOn() {
        return VideoDev->streamOn();
    }
    virtual int streamOff() {
        return VideoDev->streamOff();
    }

    virtual LinkStatus checkLink(int myPad, int remoteEntity, int remotePad) {
        return SubDev->checkLink(myPad, remoteEntity, remotePad);
    }

    virtual int getSinkPadFor(int remoteEntity, int remotePad = -1) {
        return SubDev->getSinkPadFor(remoteEntity, remotePad);
    }

    virtual int getSourcePadFor(int remoteEntity, int remotePad = -1) {
        return SubDev->getSourcePadFor(remoteEntity, remotePad);
    }

    virtual bool activate();

    virtual long long getTimeStamp() {
        return VideoDev->getTimeStamp();
    }

    virtual int setPreset(uint32_t preset) {
        return SubDev->setPreset(preset);
    }

    virtual bool linkDefault(int srcPad=-1, int sinkPad=-1);

private:
    int MediaFD;
    V4l2Device *SubDev;
    V4l2Device *VideoDev;
    CompositionType Type;

private:
};

#endif
