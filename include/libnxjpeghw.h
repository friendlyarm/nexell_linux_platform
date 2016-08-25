#ifndef _LIBNXJPEGHW_H
#define _LIBNXJPEGHW_H

int NX_JpegHWEncoding(void *dstVirt, int dstSize,
        int width, int height, unsigned int fourcc,
        unsigned int yPhy, unsigned int yVirt, unsigned int yStride,
        unsigned int cbPhy, unsigned int cbVirt, unsigned int cbStride,
        unsigned int crPhy, unsigned int crVirt, unsigned int crStride,
        bool copySOI = true);

#endif
