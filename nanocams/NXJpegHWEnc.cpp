#define LOG_TAG "NXJpegHW"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef ALOGE
#define ALOGE	printf
#endif

//#include <utils/Log.h>

//#include <nx_fourcc.h>
#include <nx_video_api.h>
#include <libnxjpeghw.h>
#include <nx_align.h>

//#include "gralloc_priv.h"

/**
 * return jpeg Size
 */
int NX_JpegHWEncoding(void *dstVirt, int dstSize,
        int width, int height, unsigned int fourcc,
        unsigned int yPhy, unsigned int yVirt, unsigned int yStride,
        unsigned int cbPhy, unsigned int cbVirt, unsigned int cbStride,
        unsigned int crPhy, unsigned int crVirt, unsigned int crStride,
        bool copySOI)
{
    NX_VID_ENC_HANDLE hEnc;
    NX_VID_ENC_OUT encOut;
    NX_VID_MEMORY_INFO memInfo;
    NX_VID_ENC_INIT_PARAM encInitParam;
    int ret = 0;

    unsigned char *dst = (unsigned char *)dstVirt;
    unsigned char *jpegHeader = (unsigned char *)dst;

    memset(&memInfo, 0, sizeof(NX_VID_MEMORY_INFO));
    memInfo.fourCC = fourcc;
    memInfo.imgWidth = width;
    memInfo.imgHeight = height;
    memInfo.luPhyAddr = yPhy;
    memInfo.luVirAddr = yVirt;
    //memInfo.luStride = yStride;
    memInfo.luStride = ALIGN(width, 16);  //YUV_STRIDE(width);
    memInfo.cbPhyAddr = cbPhy;
    memInfo.cbVirAddr = cbVirt;
    //memInfo.cbStride = cbStride;
    memInfo.cbStride = YUV_STRIDE(width/2);
    memInfo.crPhyAddr = crPhy;
    memInfo.crVirAddr = crVirt;
    //memInfo.crStride = crStride;
    memInfo.crStride = YUV_STRIDE(width/2);

    memset( &encInitParam, 0, sizeof(encInitParam) );
    encInitParam.width = width;
    encInitParam.height = height;
    encInitParam.rotAngle = 0;
    encInitParam.mirDirection = 0;
    encInitParam.jpgQuality = 100;

    hEnc = NX_VidEncOpen(NX_JPEG_ENC, NULL);
    if (NX_VidEncInit(hEnc, &encInitParam) != 0) {
        ALOGE("NX_VidEncInit failed!");
        return -EIO;
    }

    int size;
    NX_VidEncJpegGetHeader(hEnc, jpegHeader, &size);
    if (size <= 0) {
        ALOGE("Invalid JPEG Header Size %d", size);
        return -EINVAL;
    }
    if (!copySOI) {
        memcpy(jpegHeader, jpegHeader + 2, size - 2);
        size -= 2;
    }
    dst += size;
    ret = size;

    NX_VidEncJpegRunFrame(hEnc, &memInfo, &encOut);
    memcpy(dst, encOut.outBuf, encOut.bufSize);
    ret += encOut.bufSize;

    NX_VidEncClose(hEnc);

    return ret;
}

