#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include "nxp-v4l2-dev.h"

#define V4L2_SYS_PATH                   "/sys/class/video4linux"

//#define MEDIA_ENTITY_COUNT              23
#define MEDIA_ENTITY_COUNT              24

/* media entity name */
/* E_ : entity */
#define E_MIPI_CSI_NAME                 "NXP MIPI CSI"
#define E_CLIPPER0_NAME                 "NXP VIN CLIPPER0"
#define E_CLIPPER1_NAME                 "NXP VIN CLIPPER1"
#define E_VIDEO_CLIPPER0_NAME           "VIDEO CLIPPER0"
#define E_VIDEO_CLIPPER1_NAME           "VIDEO CLIPPER1"
#define E_DECIMATOR0_NAME               "NXP DECIMATOR0"
#define E_VIDEO_DECIMATOR0_NAME         "VIDEO DECIMATOR0"
#define E_DECIMATOR1_NAME               "NXP DECIMATOR1"
#define E_VIDEO_DECIMATOR1_NAME         "VIDEO DECIMATOR1"
#define E_SCALER_NAME                   "NXP SCALER"
#define E_VIDEO_SCALER_NAME             "VIDEO SCALER"
#define E_DEINTERLACER_NAME             "NXP DEINTERLACER"
#define E_VIDEO_DEINTERLACER_NAME       "VIDEO DEINTERLACER"
#define E_MLC0_NAME                     "NXP MLC0"
#define E_VIDEO_MLC0_RGB_NAME           "VIDEO MLC RGB0"
#define E_VIDEO_MLC0_VIDEO_NAME         "VIDEO MLC VID0"
#define E_MLC1_NAME                     "NXP MLC1"
#define E_VIDEO_MLC1_RGB_NAME           "VIDEO MLC RGB1"
#define E_VIDEO_MLC1_VIDEO_NAME         "VIDEO MLC VID1"
#define E_RESOL_NAME                    "NXP RESC"
#define E_HDMI_NAME                     "NXP HDMI"
#define E_TVOUT_NAME                    "NXP TVOUT"

#ifndef ANDROID
#if 1
#define ALOGD(x...)
#define ALOGE(x...)
#else
#define ALOGD(x...) do { \
        fprintf(stderr, x); \
        fprintf(stderr, "\n"); \
    } while (0);
#define ALOGE(x...) do { \
        fprintf(stderr, x); \
        fprintf(stderr, "\n"); \
    } while (0);
#endif
#define ALOGV(x...)
#endif
/* private class for nexell v4l2 interface */
class V4l2NexellPrivate {
#ifndef NXP_V4L2_MAX_CAMERA
#define NXP_V4L2_MAX_CAMERA         2
#endif
#define NXP_V4L2_MAX_NAME_SIZE      32
#define NXP_V4L2_MAX_DEVNAME_SIZE   32

    typedef struct {
        bool IsMIPI;
        char SensorEntityName[NXP_V4L2_MAX_NAME_SIZE];
    } NexellCameraInfo;

    struct DeviceInfo {
        DeviceInfo() {
            memset(Devnode, 0, NXP_V4L2_MAX_DEVNAME_SIZE);
            Device = NULL;
        }
        ~DeviceInfo() {
            if (Device != NULL) {
                 delete Device;
                 Device = NULL;
            }
        }
        int id; /* entity id */
        int pads;
        int links;
        char Devnode[NXP_V4L2_MAX_DEVNAME_SIZE];
        V4l2Device *Device;
        bool isM2M() {
            return (id == nxp_v4l2_scaler || id == nxp_v4l2_deinterlacer);
        }
    };

    typedef enum {
        Sensor0             = 0,
        Sensor1             = 1,
        MipiCSI             = 2,
        Clipper0            = 3,
        Clipper0Video       = 4,
        Clipper0Composite   = 5,
        Clipper1            = 6,
        Clipper1Video       = 7,
        Clipper1Composite   = 8,
        Decimator0          = 9,
        Decimator0Video     = 10,
        Decimator0Composite = 11,
        Decimator1          = 12,
        Decimator1Video     = 13,
        Decimator1Composite = 14,
        Scaler              = 15,
        ScalerVideo         = 16,
        ScalerComposite     = 17,
        Deinterlacer        = 18,
        DeinterlacerVideo   = 19,
        DeinterlacerComposite    = 20,
        Mlc0                = 21,
        Mlc0Rgb             = 22,
        Mlc0RgbComposite    = 23,
        Mlc0Video           = 24,
        Mlc0VideoComposite  = 25,
        Mlc1                = 26,
        Mlc1Rgb             = 27,
        Mlc1RgbComposite    = 28,
        Mlc1Video           = 29,
        Mlc1VideoComposite  = 30,
        Resol               = 31,
        Hdmi                = 32,
        Tvout               = 33,
        INTERNAL_ID_MAX
    } InternalID;

    typedef struct {
        bool useSensor0;
        bool useSensor1;
        bool useMipiCSI;
        bool useClipper0;
        bool useClipper1;
        bool useDecimator0;
        bool useDecimator1;
        bool useScaler;
        bool useDeinterlacer;
        bool useMlc0;
        bool useMlc1;
    } SubdevUsageScheme;

public:
    V4l2NexellPrivate() : EntityCount(0), MediaFD(0) {
    }

    virtual ~V4l2NexellPrivate() {
        if( MediaFD ) close(MediaFD);
    }

    int init(const struct V4l2UsageScheme *);
    int link(int sourceID, int sinkID);
    int unlink(int sourceID, int sinkID);
    int setFormat(int id, int w, int h, int f, int pad = 0);
    int getFormat(int id, int *w, int *h, int *f, int pad = 0);
    int setCrop(int id, int l, int t, int w, int h, int pad = 0);
    int getCrop(int id, int *l, int *t, int *w, int *h, int pad = 0);
    int setCtrl(int id, int ctrlID, int value);
    int getCtrl(int id, int ctrlID, int *value);
    int reqBuf(int id, int bufCount);
    int qBuf(int id, int planeNum, int index0, int *fds0, int *sizes0, int *syncfd0 = NULL, int index1 = -1, int *fds1 = NULL, int *sizes1 = NULL, int *syncfd1 = NULL);
    int qBuf(int id, int planeNum, int index0, int const *fds0, int const *sizes0, int *syncfd0 = NULL, int index1 = -1, int const *fds1 = NULL, int const *sizes1 = NULL, int *syncfd1 = NULL);
    int dqBuf(int id, int planeNum, int *index0, int *index1 = NULL);
    int streamOn(int id);
    int streamOff(int id);
    int getTimeStamp(int id, long long *timestamp);
    int setPreset(int id, uint32_t preset);
    int getDeviceFD(char *name);
    int getDeviceFD(int id);

private: // data
    /* media device fd */
    int MediaFD;
    NexellCameraInfo CameraInfo[NXP_V4L2_MAX_CAMERA];
    struct V4l2UsageScheme UsageScheme;
    SubdevUsageScheme SubdevUsage;
    /* V4l2Device */
    int EntityCount;
    DeviceInfo Devices[INTERNAL_ID_MAX];

private: // funcs
    int getCameraInfo();
    int createSubDevice(int internalID, V4l2Device **ppDevice);
    int createVideoDevice(int internalID, V4l2Device **ppDevice);
    int createCompositeDevice(int id, V4l2Device **ppDevice);
    void checkUsageScheme();
    DeviceInfo *getDevice(char *entityName);
    DeviceInfo *getDevice(int id);
    int enumEntities();
    int enumDevices();
    int enumLink(DeviceInfo *pInfo, struct media_links_enum &enumLink);
    int createDevices();
    bool activateDevices();
    int linkDefault();
};

/* TODO : smart check */
void V4l2NexellPrivate::checkUsageScheme()
{
    memset(&SubdevUsage, 0, sizeof(SubdevUsage));

    if (UsageScheme.useClipper0 || UsageScheme.useDecimator0) {
        SubdevUsage.useSensor0 = true;
        if (CameraInfo[0].IsMIPI)
            SubdevUsage.useMipiCSI = true;
        SubdevUsage.useClipper0 = true;
    }

    if (UsageScheme.useClipper1 || UsageScheme.useDecimator1) {
        SubdevUsage.useSensor1 = true;
        if (CameraInfo[1].IsMIPI)
            SubdevUsage.useMipiCSI = true;
        SubdevUsage.useClipper1 = true;
    }

    if (UsageScheme.useDecimator0)
        SubdevUsage.useDecimator0 = true;

    if (UsageScheme.useDecimator1)
        SubdevUsage.useDecimator1 = true;

    if (UsageScheme.useScaler)
        SubdevUsage.useScaler = true;

    if (UsageScheme.useDeinterlacer)
        SubdevUsage.useDeinterlacer = true;

    if (UsageScheme.useMlc0Rgb || UsageScheme.useMlc0Video)
        SubdevUsage.useMlc0 = true;

    if (UsageScheme.useMlc1Rgb || UsageScheme.useMlc1Video)
        SubdevUsage.useMlc1 = true;
}

int V4l2NexellPrivate::getCameraInfo()
{
    /* TODO */
#if 0
    CameraInfo[0].IsMIPI = false;
    strcpy(CameraInfo[0].SensorEntityName, "S5K5CAGX 0-003c");
    CameraInfo[1].IsMIPI = true;
    strcpy(CameraInfo[1].SensorEntityName, "S5K4ECGX 0-002d");
#else
    char sysfsEntry[128];
    strcpy(sysfsEntry, "/sys/devices/platform/camera sensor/sensor.0");
    NexellCameraInfo *info;
    char buf[512] = {0, };
    int ret;
    char *c;
    int sysfsFd;
    for (int i = 0; i < 2; i++) {
        info = &CameraInfo[i];
        sysfsEntry[strlen(sysfsEntry) - 1] = i + '0';
        // ALOGV("===>%s", sysfsEntry);
        sysfsFd = open(sysfsEntry, O_RDONLY);
        if (sysfsFd < 0) {
            ALOGE("%s: can't open sysfs entry!!!", __func__);
            info->SensorEntityName[0] = 0;
        } else {
			memset(buf, 0, sizeof(buf));
            ret  = read(sysfsFd, buf, sizeof(buf));
            if (ret < 0) {
                ALOGE("error read sysfs entry");
                return -EINVAL;
            }
            close(sysfsFd);
            // ALOGV("%s: buf %s", __func__, buf);
            //"is_mipi:1,name:XXX"
            c = &buf[strlen("is_mipi:")];
            info->IsMIPI = *c - '0';
            c += 7; // ,name:
            strcpy(info->SensorEntityName, c);
        }
    }
    ALOGV("sensor0: IsMIPI %d, name %s", CameraInfo[0].IsMIPI, CameraInfo[0].SensorEntityName);
    ALOGV("sensor1: IsMIPI %d, name %s", CameraInfo[1].IsMIPI, CameraInfo[1].SensorEntityName);
#endif
    return 0;
}

int V4l2NexellPrivate::enumEntities()
{
    EntityCount = 0;
    struct media_entity_desc entity[MEDIA_ENTITY_COUNT];
    memset(entity, 0, sizeof(struct media_entity_desc) * MEDIA_ENTITY_COUNT);

    int index = 0;
    int ret;
    struct media_entity_desc *pEntity;
    ALOGV("+++++++++++++++++++++++++++++++++++++++++++");
    ALOGV("enumeration entities");
    ALOGV("+++++++++++++++++++++++++++++++++++++++++++");
    do {
        pEntity = &entity[index];
        pEntity->id = index | MEDIA_ENT_ID_FLAG_NEXT;
        ret = ioctl(MediaFD, MEDIA_IOC_ENUM_ENTITIES, pEntity);
        if (!ret) {
            DeviceInfo *pDevice = getDevice(pEntity->name);
            if (pDevice) {
                pDevice->id = pEntity->id;
                pDevice->pads = pEntity->pads;
                pDevice->links = pEntity->links;
                ALOGV("[%d]\tname:%s\tpads:%d\t%p", pEntity->id, pEntity->name, pEntity->pads, pDevice);
            }
        }
        index++;
    } while (ret == 0 && index < MEDIA_ENTITY_COUNT);
    ALOGV("total number of entities: %d", index);

    if (index > 0) {
        EntityCount = index;
        ret = 0;
    }

    return ret;
}

int V4l2NexellPrivate::getDeviceFD(char *name)
{
    V4l2NexellPrivate::DeviceInfo *info = getDevice(name);
    if (info != NULL && info->Device != NULL) {
        return info->Device->getFD();
    }
    return -1;
}

int V4l2NexellPrivate::getDeviceFD(int id)
{
    V4l2NexellPrivate::DeviceInfo *info = getDevice(id);
    if (info != NULL && info->Device != NULL) {
        return info->Device->getFD();
    }
    return -1;
}

V4l2NexellPrivate::DeviceInfo *V4l2NexellPrivate::getDevice(char *name)
{
    DeviceInfo *pDevice = NULL;
    ALOGV("%s: %s", __func__, name);
    if (SubdevUsage.useSensor0 == true &&
            !strncmp(name, CameraInfo[0].SensorEntityName, strlen(CameraInfo[0].SensorEntityName))) {
        ALOGV("find sensor0: %s", CameraInfo[0].SensorEntityName);
        pDevice = &Devices[Sensor0];
    } else if (SubdevUsage.useSensor1 == true &&
            (!strncmp(name, CameraInfo[1].SensorEntityName, strlen(CameraInfo[1].SensorEntityName)))) {
        ALOGV("find sensor1");
        pDevice = &Devices[Sensor1];
    } else if (!strncmp(name, E_MIPI_CSI_NAME, strlen(E_MIPI_CSI_NAME))) {
        ALOGV("find mipicsi");
        pDevice = &Devices[MipiCSI];
    } else if (!strncmp(name, E_CLIPPER0_NAME, strlen(E_CLIPPER0_NAME))) {
        ALOGV("find clipper0");
        pDevice = &Devices[Clipper0];
    } else if (!strncmp(name, E_CLIPPER1_NAME, strlen(E_CLIPPER1_NAME))) {
        ALOGV("find clipper1");
        pDevice = &Devices[Clipper1];
    } else if (!strncmp(name, E_DECIMATOR0_NAME, strlen(E_DECIMATOR0_NAME))) {
        ALOGV("find decimator0");
        pDevice = &Devices[Decimator0];
    } else if (!strncmp(name, E_DECIMATOR1_NAME, strlen(E_DECIMATOR1_NAME))) {
        ALOGV("find decimator1");
        pDevice = &Devices[Decimator1];
    } else if (!strncmp(name, E_SCALER_NAME, strlen(E_SCALER_NAME))) {
        ALOGV("find scaler");
        pDevice = &Devices[Scaler];
    } else if (!strncmp(name, E_DEINTERLACER_NAME, strlen(E_DEINTERLACER_NAME))) {
        ALOGV("find deinterlacer");
        pDevice = &Devices[Deinterlacer];
    } else if (!strncmp(name, E_MLC0_NAME, strlen(E_MLC0_NAME))) {
        ALOGV("find mlc0");
        pDevice = &Devices[Mlc0];
    } else if (!strncmp(name, E_MLC1_NAME, strlen(E_MLC1_NAME))) {
        ALOGV("find mlc1");
        pDevice = &Devices[Mlc1];
    } else if (!strncmp(name, E_RESOL_NAME, strlen(E_RESOL_NAME))) {
        ALOGV("find resc");
        pDevice = &Devices[Resol];
    } else if (!strncmp(name, E_HDMI_NAME, strlen(E_HDMI_NAME))) {
        ALOGV("find hdmi");
        pDevice = &Devices[Hdmi];
    } else if (!strncmp(name, E_VIDEO_CLIPPER0_NAME, strlen(E_VIDEO_CLIPPER0_NAME))) {
        ALOGV("find video clipper0");
        pDevice = &Devices[Clipper0Video];
    } else if (!strncmp(name, E_VIDEO_CLIPPER1_NAME, strlen(E_VIDEO_CLIPPER1_NAME))) {
        ALOGV("find video clipper1");
        pDevice = &Devices[Clipper1Video];
    } else if (!strncmp(name, E_VIDEO_DECIMATOR0_NAME, strlen(E_VIDEO_DECIMATOR0_NAME))) {
        ALOGV("find video decimator0");
        pDevice = &Devices[Decimator0Video];
    } else if (!strncmp(name, E_VIDEO_DECIMATOR1_NAME, strlen(E_VIDEO_DECIMATOR1_NAME))) {
        ALOGV("find video decimator1");
        pDevice = &Devices[Decimator1Video];
    } else if (!strncmp(name, E_VIDEO_SCALER_NAME, strlen(E_VIDEO_SCALER_NAME))) {
        ALOGV("find video scaler");
        pDevice = &Devices[ScalerVideo];
    } else if (!strncmp(name, E_VIDEO_DEINTERLACER_NAME, strlen(E_VIDEO_DEINTERLACER_NAME))) {
        ALOGV("find video deinterlacer");
        pDevice = &Devices[DeinterlacerVideo];
    } else if (!strncmp(name, E_VIDEO_MLC0_RGB_NAME, strlen(E_VIDEO_MLC0_RGB_NAME))) {
        ALOGV("find video mlc0 rgb");
        pDevice = &Devices[Mlc0Rgb];
    } else if (!strncmp(name, E_VIDEO_MLC0_VIDEO_NAME, strlen(E_VIDEO_MLC0_VIDEO_NAME))) {
        ALOGV("find video mlc0 video");
        pDevice = &Devices[Mlc0Video];
    } else if (!strncmp(name, E_VIDEO_MLC1_RGB_NAME, strlen(E_VIDEO_MLC1_RGB_NAME))) {
        ALOGV("find video mlc1 rgb");
        pDevice = &Devices[Mlc1Rgb];
    } else if (!strncmp(name, E_VIDEO_MLC1_VIDEO_NAME, strlen(E_VIDEO_MLC1_VIDEO_NAME))) {
        ALOGV("find video mlc1 video");
        pDevice = &Devices[Mlc1Video];
    } else if (!strncmp(name, E_TVOUT_NAME, strlen(E_TVOUT_NAME))) {
        ALOGV("find tvout");
        pDevice = &Devices[Tvout];
    } else {
        ALOGE("Unknown name %s", name);
    }

    ALOGV("p: %p", pDevice);
    return pDevice;
}

V4l2NexellPrivate::DeviceInfo *V4l2NexellPrivate::getDevice(int id)
{
    if (id < INTERNAL_ID_MAX)
        return &Devices[id];
    return NULL;
}

int V4l2NexellPrivate::enumDevices()
{
    char *curDir = getcwd(NULL, 0);
    if (chdir(V4L2_SYS_PATH) < 0) {
        ALOGE("failed to chdir to %s", V4L2_SYS_PATH);
        return -EINVAL;
    }

    ALOGV("+++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    ALOGV("enum devices");
    ALOGV("+++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    struct dirent **items;
#ifdef ANDROID
    int nitems = scandir(".", &items, NULL, (int (*)(const dirent**, const dirent **))alphasort);
#else
    int nitems = scandir(".", &items, NULL, alphasort);
#endif

    for (int i = 0; i < nitems; i++) {
        if ((!strcmp(items[i]->d_name, ".")) || (!strcmp(items[i]->d_name, "..")))
            continue;

        struct stat fstat;
        lstat(items[i]->d_name, &fstat);

        char entrySyspath[128];
        char entryName[NXP_V4L2_MAX_DEVNAME_SIZE];

        memset(entrySyspath, 0, 128);
        memset(entryName, 0, NXP_V4L2_MAX_DEVNAME_SIZE);

        sprintf(entrySyspath, "%s/name", items[i]->d_name);

        int fd = open(entrySyspath, O_RDONLY);
        if (fd < 0) {
            ALOGE("can't open %s", entrySyspath);
            continue;
        }
        int readCnt = read(fd, entryName, NXP_V4L2_MAX_DEVNAME_SIZE - 1);
        close(fd);
        if (readCnt <= 0) {
            ALOGE("can't read %s", entrySyspath);
            continue;
        }
        DeviceInfo *pDevice = getDevice(entryName);
        if (!pDevice) {
            ALOGE("%s: can't get device for %s", __func__, entryName);
            continue;
        }
        sprintf(pDevice->Devnode, "/dev/%s", items[i]->d_name);
        ALOGV("%s\t------> %s", entryName, pDevice->Devnode);
    }

    if (curDir) {
        chdir(curDir);
        free(curDir);
    }

    return 0;
}

int V4l2NexellPrivate::enumLink(DeviceInfo *pDeviceInfo, struct media_links_enum &enumlink)
{
    memset(&enumlink, 0, sizeof(enumlink));
    enumlink.entity = pDeviceInfo->id;
    enumlink.pads = (struct media_pad_desc *)malloc(sizeof(struct media_pad_desc) * pDeviceInfo->pads);
    enumlink.links = (struct media_link_desc *)malloc(sizeof(struct media_link_desc) * pDeviceInfo->links);
    int ret = ioctl(MediaFD, MEDIA_IOC_ENUM_LINKS, &enumlink);
    if (ret < 0) {
        ALOGE("failed to enum link for %d", pDeviceInfo->id);
        free(enumlink.pads);
        free(enumlink.links);
        return -EINVAL;
    }

    struct media_pad_desc *padDesc = enumlink.pads;
    struct media_link_desc *linkDesc = enumlink.links;
    ALOGV("+++++++++++++++++++++++++++++++++++");
    ALOGV("enum link for %d", enumlink.entity);
    ALOGV("+++++++++++++++++++++++++++++++++++");
    ALOGV("pads for entity %d= ", enumlink.entity);
    int i;
    for (i = 0; i < pDeviceInfo->pads; i++) {
        ALOGV("(%d, %s) ", padDesc->index, (padDesc->flags & MEDIA_PAD_FL_SINK) ? "INPUT":"OUTPUT");
        padDesc++;
    }
    for (i = 0; i < pDeviceInfo->links; i++) {
        ALOGV("\t[%x:%x] --------------> [%x:%x]", linkDesc->source.entity,
                linkDesc->source.index, linkDesc->sink.entity, linkDesc->sink.index);
        if (linkDesc->flags & MEDIA_LNK_FL_ENABLED) {
            ALOGV("\tACTIVE");
        } else {
            ALOGV("\tINACTIVE");
        }
        linkDesc++;
    }
    return 0;
}

int V4l2NexellPrivate::createSubDevice(int internalID, V4l2Device **ppDevice)
{
    DeviceInfo *pDeviceInfo = getDevice(internalID);
    if (pDeviceInfo->id == -1 || pDeviceInfo->Devnode[0] == '\0') {
        ALOGE("%s: can't get valid entity id for device %p, %d(%d, %s)", __func__, pDeviceInfo, internalID, pDeviceInfo->id, pDeviceInfo->Devnode);
        return -EINVAL;
    }

    if (pDeviceInfo->Device) {
        *ppDevice = pDeviceInfo->Device;
        return 0;
    }

    /* enum link */
    struct media_links_enum enumlink;
    int ret = enumLink(pDeviceInfo, enumlink);
    if (ret < 0)
        return ret;

    V4l2Device *device;
#ifdef ANDROID
    device = new V4l2Subdev(pDeviceInfo->Devnode, pDeviceInfo->id,
            pDeviceInfo->pads, enumlink.pads, pDeviceInfo->links, enumlink.links);
#else
    try {
        device = new V4l2Subdev(pDeviceInfo->Devnode, pDeviceInfo->id,
                pDeviceInfo->pads, enumlink.pads, pDeviceInfo->links, enumlink.links);
    } catch (int errorNum) {
        free(enumlink.pads);
        free(enumlink.links);
        ALOGE("failed to create subdev for %d", internalID);
        return -EINVAL;
    }
#endif
    free(enumlink.pads);
    free(enumlink.links);

    pDeviceInfo->Device = device;
    *ppDevice = device;
    ALOGV("createSubdevice %d success!!!\n", internalID);
    return 0;
}

int V4l2NexellPrivate::createVideoDevice(int internalID, V4l2Device **ppDevice)
{
    DeviceInfo *pDeviceInfo = getDevice(internalID);
    if (pDeviceInfo->id == -1 || pDeviceInfo->Devnode[0] == '\0') {
        ALOGE("%s: can't get valid entity id for device %d", __func__, internalID);
        return -EINVAL;
    }

    if (pDeviceInfo->Device) {
        *ppDevice = pDeviceInfo->Device;
        return 0;
    }

    /* enum link */
    struct media_links_enum enumlink;
    int ret = enumLink(pDeviceInfo, enumlink);
    if (ret < 0)
        return ret;

    bool isM2M = (internalID == ScalerVideo || internalID == DeinterlacerVideo) ? true : false;
    unsigned int bufType;
    if (internalID == Clipper0Video || internalID == Clipper1Video ||
        internalID == Decimator0Video || internalID == Decimator1Video)
        bufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else
        bufType = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

    V4l2Device *device;
#ifdef ANDROID
    device = new V4l2Video(pDeviceInfo->Devnode, pDeviceInfo->id,
            pDeviceInfo->pads, enumlink.pads, pDeviceInfo->links, enumlink.links,
            isM2M, bufType, V4L2_MEMORY_DMABUF);
#else
    try {
        device = new V4l2Video(pDeviceInfo->Devnode, pDeviceInfo->id,
                pDeviceInfo->pads, enumlink.pads, pDeviceInfo->links, enumlink.links,
                isM2M, bufType, V4L2_MEMORY_DMABUF);
    } catch (int errorNum) {
        free(enumlink.pads);
        free(enumlink.links);
        ALOGE("failed to create videodev for %d", internalID);
        return -EINVAL;
    }
#endif
    free(enumlink.pads);
    free(enumlink.links);

    pDeviceInfo->Device = device;
    *ppDevice = device;
    ALOGV("createVideoDevice %d success!!!\n", internalID);
    return 0;
}

int V4l2NexellPrivate::createCompositeDevice(int id, V4l2Device **ppDevice)
{
    DeviceInfo *pDeviceInfo = getDevice(id);
    if (pDeviceInfo->Device) {
        *ppDevice = pDeviceInfo->Device;
        return 0;
    }

    int subId, videoId, compositeId;
    V4l2Composite::CompositionType type;
    int srcPad = -1, sinkPad = -1;

    switch (id) {
    case nxp_v4l2_clipper0:
        subId = Clipper0;
        videoId = Clipper0Video;
        compositeId = Clipper0Composite;
        type = V4l2Composite::capture;
        break;
    case nxp_v4l2_clipper1:
        subId = Clipper1;
        videoId = Clipper1Video;
        compositeId = Clipper1Composite;
        type = V4l2Composite::capture;
        break;
    case nxp_v4l2_decimator0:
        subId = Decimator0;
        videoId = Decimator0Video;
        compositeId = Decimator0Composite;
        type = V4l2Composite::capture;
        break;
    case nxp_v4l2_decimator1:
        subId = Decimator1;
        videoId = Decimator1Video;
        compositeId = Decimator1Composite;
        type = V4l2Composite::capture;
        break;
    case nxp_v4l2_scaler:
        subId = Scaler;
        videoId = ScalerVideo;
        compositeId = ScalerComposite;
        type = V4l2Composite::m2m;
        break;
    case nxp_v4l2_deinterlacer:
        subId = Deinterlacer;
        videoId = DeinterlacerVideo;
        compositeId = DeinterlacerComposite;
        type = V4l2Composite::m2m;
        break;
    case nxp_v4l2_mlc0_rgb:
        subId = Mlc0;
        videoId = Mlc0Rgb;
        compositeId = Mlc0RgbComposite;
        type = V4l2Composite::out;
        srcPad = 1;
        sinkPad = 0;
        break;
    case nxp_v4l2_mlc0_video:
        subId = Mlc0;
        videoId = Mlc0Video;
        compositeId = Mlc0VideoComposite;
        type = V4l2Composite::out;
        srcPad = 1;
        sinkPad = 1;
        break;
    case nxp_v4l2_mlc1_rgb:
        subId = Mlc1;
        videoId = Mlc1Rgb;
        compositeId = Mlc1RgbComposite;
        type = V4l2Composite::out;
        srcPad = 1;
        sinkPad = 0;
        break;
    case nxp_v4l2_mlc1_video:
        subId = Mlc1;
        videoId = Mlc1Video;
        compositeId = Mlc1VideoComposite;
        type = V4l2Composite::out;
        srcPad = 1;
        sinkPad = 1;
        break;
    default:
        ALOGE("invalid composite device id(%d)", id);
        return -EINVAL;
    }

    V4l2Device *sub, *video, *composite;

    int ret = createSubDevice(subId, &sub);
    if (ret < 0)
        return ret;
    ret = createVideoDevice(videoId, &video);
    if (ret < 0)
        return ret;
#ifdef ANDROID
    composite = new V4l2Composite(MediaFD, sub, video, type);
#else
    try {
        composite = new V4l2Composite(MediaFD, sub, video, type);
    } catch (int errorNum) {
        delete sub;
        delete video;
        ALOGE("can't create V4l2Composite for %d", id);
        return ret;
    }
#endif

    composite->linkDefault(srcPad, sinkPad);

    pDeviceInfo->Device = composite;
    *ppDevice = composite;

    ALOGV("createCompositeDevice %d success!!!\n", id);
    return 0;
}

int V4l2NexellPrivate::createDevices()
{
    V4l2Device *device;
    int ret;

    /* subdevices */
    /* sensor0 */
    if (SubdevUsage.useSensor0) {
        ret = createSubDevice(Sensor0, &device);
        if (ret < 0) {
            ALOGE("can't create sensor0");
            return ret;
        }
        //Devices[Sensor0].Device = device;
    }

    /* sensor1 */
    if (SubdevUsage.useSensor1) {
        ret = createSubDevice(Sensor1, &device);
        if (ret < 0) {
            ALOGE("can't create sensor1");
            return ret;
        }
        //Devices[Sensor1].Device = device;
    }

    /* mipicsi */
    if (SubdevUsage.useMipiCSI) {
        ret = createSubDevice(MipiCSI, &device);
        if (ret < 0) {
            ALOGE("can't create mipicsi");
            return ret;
        }
        //Devices[MipiCSI].Device = device;
    }

    /* clipper0 */
    if (SubdevUsage.useClipper0 && !UsageScheme.useClipper0) {
        ret = createSubDevice(Clipper0, &device);
        // ret = createSubDevice(nxp_v4l2_clipper0, &device);
        if (ret < 0) {
            ALOGE("can't create clipper0");
            return ret;
        }
        //Devices[Clipper0].Device = device;
    }

    /* clipper1 */
    if (SubdevUsage.useClipper1 && !UsageScheme.useClipper1) {
        ret = createSubDevice(Clipper1, &device);
        if (ret < 0) {
            ALOGE("can't create clipper1");
            return ret;
        }
        //Devices[Clipper1].Device = device;
    }

    /* decimator0 */
    if (SubdevUsage.useDecimator0 && !UsageScheme.useDecimator0) {
        ret = createSubDevice(Decimator0, &device);
        if (ret < 0) {
            ALOGE("can't create decimator0");
            return ret;
        }
        //Devices[Decimator0].Device = device;
    }

    /* decimator1 */
    if (SubdevUsage.useDecimator1 && !UsageScheme.useDecimator1) {
        ret = createSubDevice(Decimator1, &device);
        if (ret < 0) {
            ALOGE("can't create decimator1");
            return ret;
        }
        //Devices[Decimator1].Device = device;
    }

    /* scaler */
    if (SubdevUsage.useScaler && !UsageScheme.useScaler) {
        ret = createSubDevice(Scaler, &device);
        if (ret < 0) {
            ALOGE("can't create scaler");
            return ret;
        }
        //Devices[Scaler].Device = device;
    }

    /* deinterlacer */
    if (SubdevUsage.useDeinterlacer && !UsageScheme.useDeinterlacer) {
        ret = createSubDevice(Deinterlacer, &device);
        if (ret < 0) {
            ALOGE("can't create deinterlacer");
            return ret;
        }
        //Devices[Deinterlacer].Device = device;
    }

    /* mlc0 */
    if (SubdevUsage.useMlc0 && !UsageScheme.useMlc0Rgb && !UsageScheme.useMlc0Video) {
        ret = createSubDevice(Mlc0, &device);
        if (ret < 0) {
            ALOGE("can't create mlc0");
            return ret;
        }
        //Devices[Mlc0].Device = device;
    }

    /* mlc1 */
    if (SubdevUsage.useMlc1 && !UsageScheme.useMlc1Rgb && !UsageScheme.useMlc1Video) {
        ret = createSubDevice(Mlc1, &device);
        if (ret < 0) {
            ALOGE("can't create mlc1");
            return ret;
        }
        //Devices[Mlc1].Device = device;
    }

    /* resolution converter */
    if (UsageScheme.useResol) {
        ret = createSubDevice(Resol, &device);
        if (ret < 0) {
            ALOGE("can't create resol");
            return ret;
        }
        //Devices[Resol].Device = device;
    }

    /* HDMI */
    if (UsageScheme.useHdmi) {
        ret = createSubDevice(Hdmi, &device);
        if (ret < 0) {
            ALOGE("can't create hdmi");
            return ret;
        }
        //Devices[Hdmi].Device = device;
    }

    /* TVOUT */
    if (UsageScheme.useTvout) {
        ret = createSubDevice(Tvout, &device);
        if (ret < 0) {
            ALOGE("can't create tvout");
            return ret;
        }
        //Devices[Tvout].Device = device;
    }

    /* compositie : all video devices have their subdevices */
    /* clipper0 */
    if (UsageScheme.useClipper0) {
        ret = createCompositeDevice(nxp_v4l2_clipper0, &device);
        if (ret < 0) {
            ALOGE("can't create clipper0 composite");
            return ret;
        }
        //Devices[nxp_v4l2_clipper0].Device = device;
    }

    /* clipper1 */
    if (UsageScheme.useClipper1) {
        ret = createCompositeDevice(nxp_v4l2_clipper1, &device);
        if (ret < 0) {
            ALOGE("can't create clipper1 composite");
            return ret;
        }
        //Devices[nxp_v4l2_clipper1].Device = device;
    }

    /* decimator0 */
    if (UsageScheme.useDecimator0) {
        ret = createCompositeDevice(nxp_v4l2_decimator0, &device);
        if (ret < 0) {
            ALOGE("can't create decimator0 composite");
            return ret;
        }
        //Devices[nxp_v4l2_decimator0].Device = device;
    }

    /* decimator1 */
    if (UsageScheme.useDecimator1) {
        ret = createCompositeDevice(nxp_v4l2_decimator1, &device);
        if (ret < 0) {
            ALOGE("can't create decimator1 composite");
            return ret;
        }
        //Devices[nxp_v4l2_decimator1].Device = device;
    }

    /* scaler */
    if (UsageScheme.useScaler) {
        ret = createCompositeDevice(nxp_v4l2_scaler, &device);
        if (ret < 0) {
            ALOGE("can't create scaler composite");
            return ret;
        }
        //Devices[nxp_v4l2_scaler].Device = device;
    }

    /* deinterlacer */
    if (UsageScheme.useDeinterlacer) {
        ret = createCompositeDevice(nxp_v4l2_deinterlacer, &device);
        if (ret < 0) {
            ALOGE("can't create deinterlacer composite");
            return ret;
        }
        //Devices[nxp_v4l2_deinterlacer].Device = device;
    }

    /* mlc0rgb */
    if (UsageScheme.useMlc0Rgb) {
        ret = createCompositeDevice(nxp_v4l2_mlc0_rgb, &device);
        if (ret < 0) {
            ALOGE("can't create mlc0rgb composite");
            return ret;
        }
        //Devices[nxp_v4l2_mlc0_rgb].Device = device;
    }

    /* mlc0video */
    if (UsageScheme.useMlc0Video) {
        ret = createCompositeDevice(nxp_v4l2_mlc0_video, &device);
        if (ret < 0) {
            ALOGE("can't create mlc0video composite");
            return ret;
        }
        //Devices[nxp_v4l2_mlc0_video].Device = device;
    }

    /* mlc1rgb */
    if (UsageScheme.useMlc1Rgb) {
        ret = createCompositeDevice(nxp_v4l2_mlc1_rgb, &device);
        if (ret < 0) {
            ALOGE("can't create mlc1rgb composite");
            return ret;
        }
        //Devices[nxp_v4l2_mlc1_rgb].Device = device;
    }

    /* mlc1video */
    if (UsageScheme.useMlc1Video) {
        ret = createCompositeDevice(nxp_v4l2_mlc1_video, &device);
        if (ret < 0) {
            ALOGE("can't create mlc1video composite");
            return ret;
        }
        //Devices[nxp_v4l2_mlc1_video].Device = device;
    }

    return 0;
}

bool V4l2NexellPrivate::activateDevices()
{
    bool ret = true;
    DeviceInfo *pDeviceInfo = NULL;
    for (int i = 0, count = 0; i < INTERNAL_ID_MAX; i++) {
        pDeviceInfo = &Devices[i];
        if (pDeviceInfo->Device) {
            count++;
            ret = pDeviceInfo->Device->activate();
            if (ret == false) {
                ALOGE("%s: failed to activate %s", __func__, pDeviceInfo->Devnode);
                //return ret;
            }
        }
    }

    //return ret;
    return true;
}

int V4l2NexellPrivate::linkDefault()
{
    int ret;

    if (SubdevUsage.useSensor0) {
        if (CameraInfo[0].IsMIPI) {
            // link sensor0 -> mipicsi
            if (!SubdevUsage.useMipiCSI) {
                ALOGE("%s: invalid SubDevUsageScheme!!!, sensor0 is mipi, but mipi not included", __func__);
                return -EINVAL;
            }
            ret = link(Sensor0, MipiCSI);
            if (ret) {
                ALOGE("%s: failed to link Sensor0 to MipiCSI", __func__);
                return ret;
            }
            ret = link(MipiCSI, Clipper0);
            if (ret) {
                ALOGE("%s: failed to link MipiCSI to Clipper0", __func__);
                return ret;
            }
        } else {
            ret = link(Sensor0, Clipper0);
            if (ret) {
                ALOGE("%s: failed to link Sensor0 to Clipper0", __func__);
                return ret;
            }
        }
    }

    if (SubdevUsage.useSensor1) {
        if (CameraInfo[1].IsMIPI) {
            // link sensor1 -> mipicsi
            if (!SubdevUsage.useMipiCSI) {
                ALOGE("%s: invalid SubDevUsageScheme!!!, sensor1 is mipi, but mipi not included", __func__);
                return -EINVAL;
            }
            ret = link(Sensor1, MipiCSI);
            if (ret) {
                ALOGE("%s: failed to link Sensor1 to MipiCSI", __func__);
                return ret;
            }
            ret = link(MipiCSI, Clipper1);
            if (ret) {
                ALOGE("%s: failed to link MipiCSI to Clipper1", __func__);
                return ret;
            }
        } else {
            ret = link(Sensor1, Clipper1);
            if (ret) {
                ALOGE("%s: failed to link Sensor1 to Clipper1", __func__);
                return ret;
            }
        }
    }

    if (UsageScheme.useDecimator0) {
        ret = link(Clipper0, Decimator0);
        if (ret)
            return ret;
    }

    if (UsageScheme.useDecimator1) {
        ret = link(Clipper1, Decimator1);
        if (ret)
            return ret;
    }

    return 0;
}

/**
 * public
 */
int V4l2NexellPrivate::init(const struct V4l2UsageScheme *s)
{
    MediaFD = open("/dev/media0", O_RDWR);
    if (MediaFD < 0) {
        ALOGE("can't open media device");
        return MediaFD;
    }

    if (s->useClipper0 || s->useDecimator0 || s->useClipper1 || s->useDecimator1) {
        if (getCameraInfo()) {
            close(MediaFD); MediaFD = -1;
            ALOGE("can't get camera info");
            return -EINVAL;
        }
    }

    UsageScheme = *s;
    checkUsageScheme();

    int ret = enumEntities();
    if (ret < 0) {
        ALOGE("failed to enumEntities()");
        return ret;
    }

    ret = enumDevices();
    if (ret < 0) {
        ALOGE("failed to enumDevices()");
        return ret;
    }

    ret = createDevices();
    if (ret < 0) {
        ALOGE("failed to createDevices()");
        return ret;
    }

    ret = linkDefault();
    if (ret < 0) {
        ALOGE("failed to linkDefault()");
        return ret;
    }

    if (activateDevices() == false) {
        ret = -EINVAL;
    }

    return ret;
}

int V4l2NexellPrivate::link(int sourceID, int sinkID)
{
    DeviceInfo *pSrcInfo = getDevice(sourceID);
    if (!pSrcInfo) {
        ALOGE("link: invalid source id(%d)", sourceID);
        return -EINVAL;
    }
    DeviceInfo *pSinkInfo = getDevice(sinkID);
    if (!pSinkInfo) {
        ALOGE("link: invalid source id(%d)", sinkID);
        return -EINVAL;
    }

    V4l2Device *pSrcDevice = pSrcInfo->Device;
    V4l2Device *pSinkDevice = pSinkInfo->Device;
    if (!pSrcDevice || !pSinkDevice) {
        ALOGE("link: uninitialized!!!(src:%p, sink:%p)", pSrcDevice, pSinkDevice);
        return -EINVAL;
    }

    int srcPad = pSrcDevice->getSourcePadFor(pSinkInfo->id);
    // TODO
    //int sinkPad = pSinkDevice->getSinkPadFor(pSrcInfo->id);
    int sinkPad = 0;

    int ret = pSrcDevice->checkLink(srcPad, pSinkInfo->id, sinkPad);
    if (ret == V4l2Device::LINK_CONNECTED) {
        ALOGV("link: already connected(%d->%d)", pSrcInfo->id, pSinkInfo->id);
        return 0;
    }
    if (ret == V4l2Device::LINK_INVALID) {
        ALOGE("link: invalid connect(%d->%d)", pSrcInfo->id, pSinkInfo->id);
        return -EINVAL;
    }

    struct media_link_desc setupLink;
    memset(&setupLink, 0, sizeof(setupLink));
    setupLink.flags |= MEDIA_LNK_FL_ENABLED;

    setupLink.source.entity = pSrcInfo->id;
    setupLink.source.index = srcPad;
    setupLink.source.flags = MEDIA_PAD_FL_SOURCE;

    setupLink.sink.entity = pSinkInfo->id;
    setupLink.sink.index = sinkPad;
    setupLink.sink.flags = MEDIA_PAD_FL_SINK;

    ret = ioctl(MediaFD, MEDIA_IOC_SETUP_LINK, &setupLink);
    if (ret) {
        ALOGE("%s: failed to MEDIA_IOC_SETUP_LINK[%d %d] --> [%d %d]",
                __func__, pSrcInfo->id, srcPad, pSinkInfo->id, sinkPad);
        return ret;
    }

    return 0;
}

int V4l2NexellPrivate::unlink(int sourceID, int sinkID)
{
    ALOGV("unlink %d --> %d", sourceID, sinkID);
    DeviceInfo *pSrcInfo = getDevice(sourceID);
    if (!pSrcInfo) {
        ALOGE("link: invalid source id(%d)", sourceID);
        return -EINVAL;
    }
    DeviceInfo *pSinkInfo = getDevice(sinkID);
    if (!pSinkInfo) {
        ALOGE("link: invalid source id(%d)", sinkID);
        return -EINVAL;
    }

    V4l2Device *pSrcDevice = pSrcInfo->Device;
    V4l2Device *pSinkDevice = pSinkInfo->Device;
    if (!pSrcDevice || !pSinkDevice) {
        ALOGE("link: uninitialized!!!(src:%p, sink:%p)", pSrcDevice, pSinkDevice);
        return -EINVAL;
    }

    int srcPad = pSrcDevice->getSourcePadFor(pSinkInfo->id);
    // TODO
    // int sinkPad = pSinkDevice->getSinkPadFor(pSrcInfo->id);
    int sinkPad = 0;

#if 0
    int ret = pSrcDevice->checkLink(srcPad, pSinkInfo->id, sinkPad);
    if (ret == V4l2Device::LINK_DISCONNECTED) {
        ALOGV("unlink: already disconnected(%d->%d)", sourceID, sinkID);
        return 0;
    }
    if (ret == V4l2Device::LINK_INVALID) {
        ALOGE("unlink: invalid connect(%d->%d)", sourceID, sinkID);
        return -EINVAL;
    }
#endif

    struct media_link_desc setupLink;
    memset(&setupLink, 0, sizeof(setupLink));
    setupLink.flags &= ~MEDIA_LNK_FL_ENABLED;

    setupLink.source.entity = pSrcInfo->id;
    setupLink.source.index = srcPad;
    setupLink.source.flags = MEDIA_PAD_FL_SOURCE;

    setupLink.sink.entity = pSinkInfo->id;
    setupLink.sink.index = sinkPad;
    setupLink.sink.flags = MEDIA_PAD_FL_SINK;

    int ret = ioctl(MediaFD, MEDIA_IOC_SETUP_LINK, &setupLink);
    if (ret) {
        ALOGE("unlink: failed to MEDIA_IOC_SETUP_LINK");
        return ret;
    }

    return 0;
}

int V4l2NexellPrivate::setFormat(int id, int w, int h, int f, int pad)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

#if 0
    /* for M2M, set,getFormat involves input */
    if (!pInfo->isM2M())
        return pInfo->Device->setFormat(w, h, f);
    else
        return pInfo->Device->setFormat(w, h, f, 0);
#else
    return pInfo->Device->setFormat(w, h, f, pad);
#endif
}

int V4l2NexellPrivate::getFormat(int id, int *w, int *h, int *f, int pad)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

#if 0
    /* for M2M, set,getFormat involves input */
    if (!pInfo->isM2M())
        return pInfo->Device->getFormat(w, h, f);
    else
        return pInfo->Device->getFormat(w, h, f, 0);
#else
    return pInfo->Device->getFormat(w, h, f, pad);
#endif
}

int V4l2NexellPrivate::setCrop(int id, int l, int t, int w, int h, int pad)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    //printf("%s: id %d, pad %d, %d:%d-%d%d\n", __func__, id, pad, l, t, w, h);
#if 1
    /* for M2M, set,getCrop involves output */
    if (!pInfo->isM2M())
        return pInfo->Device->setCrop(l, t, w, h, pad);
    else
        return pInfo->Device->setCrop(l, t, w, h, 1);
#else
    return pInfo->Device->setCrop(l, t, w, h, pad);
#endif
}

int V4l2NexellPrivate::getCrop(int id, int *l, int *t, int *w, int *h, int pad)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

#if 1
    /* for M2M, set,getCrop involves output */
    if (!pInfo->isM2M())
        return pInfo->Device->getCrop(l, t, w, h, pad);
    else
        return pInfo->Device->getCrop(l, t, w, h, 1);
#else
    return pInfo->Device->getCrop(l, t, w, h, pad);
#endif
}

int V4l2NexellPrivate::setCtrl(int id, int ctrlID, int value)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    return pInfo->Device->setCtrl(ctrlID, value);
}

int V4l2NexellPrivate::getCtrl(int id, int ctrlID, int *value)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    return pInfo->Device->getCtrl(ctrlID, value);
}

int V4l2NexellPrivate::reqBuf(int id, int bufCount)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    return pInfo->Device->reqBuf(bufCount);
}

int V4l2NexellPrivate::qBuf(int id, int planeNum, int index0, int *fds0, int *sizes0, int *syncfd0, int index1, int *fds1, int *sizes1, int *syncfd1)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    if (!pInfo->isM2M())
        return pInfo->Device->qBuf(planeNum, index0, fds0, sizes0, -1, NULL, NULL, syncfd0, NULL);
    else
        return pInfo->Device->qBuf(planeNum, index0, fds0, sizes0, index1, fds1, sizes1, syncfd0, syncfd1);
}

int V4l2NexellPrivate::qBuf(int id, int planeNum, int index0, int const *fds0, int const *sizes0, int *syncfd0, int index1, int const *fds1, int const *sizes1, int *syncfd1)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    if (!pInfo->isM2M())
        return pInfo->Device->qBuf(planeNum, index0, fds0, sizes0, -1, NULL, NULL, syncfd0, NULL);
    else
        return pInfo->Device->qBuf(planeNum, index0, fds0, sizes0, index1, fds1, sizes1, syncfd0, syncfd1);
}

int V4l2NexellPrivate::dqBuf(int id, int planeNum, int *index0, int *index1)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    if (!pInfo->isM2M())
        return pInfo->Device->dqBuf(planeNum, index0);
    else
        return pInfo->Device->dqBuf(planeNum, index0, index1);
}

int V4l2NexellPrivate::streamOn(int id)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    return pInfo->Device->streamOn();
}

int V4l2NexellPrivate::streamOff(int id)
{
    DeviceInfo *pInfo = getDevice(id);
    ALOGV("%s: %d", __func__, id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    return pInfo->Device->streamOff();
}

int V4l2NexellPrivate::getTimeStamp(int id, long long *timestamp)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    long long val = pInfo->Device->getTimeStamp();
    *timestamp = val;
    return 0;
}

int V4l2NexellPrivate::setPreset(int id, uint32_t preset)
{
    DeviceInfo *pInfo = getDevice(id);
    if (!pInfo || !pInfo->Device) {
        ALOGE("%s: can't get device for %d", __func__, id);
        return -EINVAL;
    }

    return pInfo->Device->setPreset(preset);
}
