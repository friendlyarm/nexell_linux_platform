#include "parser_vld.h"

int vld_count_leading_zero(unsigned long dwWord)
{
    int   iLZ = 0;

    if ( (dwWord >> (32 - 16)) == 0 )       iLZ  = 16;
    if ( (dwWord >> (32 - 8 - iLZ)) == 0 )  iLZ  += 8;
    if ( (dwWord >> (32 - 4 - iLZ)) == 0 )  iLZ  += 4;
    if ( (dwWord >> (32 - 2 - iLZ)) == 0 )  iLZ  += 2;
    if ( (dwWord >> (32 - 1 - iLZ)) == 0 )  iLZ  += 1;
    return iLZ;
}
unsigned long vld_show_bits(VLD_STREAM *pstVldStm, int iBits)
{
    unsigned long   dwUsedBits  = pstVldStm->dwUsedBits;
    int             iBitCnt     = 8 - (dwUsedBits & 0x7);
    unsigned char   *pbyRead    = (unsigned char *)pstVldStm->pbyStart + (dwUsedBits>>3);
    unsigned long   dwRead;

    dwRead  = *pbyRead++ << 24;
    if ( iBits > iBitCnt ) {
        dwRead  += *pbyRead++ << 16;
        if ( iBits > iBitCnt + 8 ) {
            dwRead  += *pbyRead++ << 8;
            if ( iBits > iBitCnt + 16 ) {
                dwRead  += *pbyRead++;
            }
        }
    }

    return ( dwRead << (8 - iBitCnt)) >> (32 - iBits);
}

unsigned long vld_get_bits(VLD_STREAM *pstVldStm, int iBits)
{
    unsigned long   dwUsedBits  = pstVldStm->dwUsedBits;
    int             iBitCnt     = 8 - (dwUsedBits & 0x7);
    unsigned char   *pbyRead    = (unsigned char *)pstVldStm->pbyStart + (dwUsedBits>>3);
    unsigned long   dwRead;

    pstVldStm->dwUsedBits     += iBits;

    dwRead  = *pbyRead++ << 24;
    if ( iBits > iBitCnt ) {
        dwRead  += *pbyRead++ << 16;
        if ( iBits > iBitCnt + 8 ) {
            dwRead  += *pbyRead++ << 8;
            if ( iBits > iBitCnt + 16 ) {
                dwRead  += *pbyRead++;
            }
        }
    }

    return ( dwRead << (8 - iBitCnt)) >> (32 - iBits);
}

void vld_flush_bits(VLD_STREAM *pstVldStm, int iBits)
{
    pstVldStm->dwUsedBits     += iBits;
}

unsigned int vld_get_uev(VLD_STREAM *pstVldStm)
{
    int     iLZ = vld_count_leading_zero(vld_show_bits(pstVldStm, 32));
    vld_flush_bits(pstVldStm, iLZ);
    return (vld_get_bits(pstVldStm, iLZ + 1) - 1);
}

int vld_get_sev(VLD_STREAM *pstVldStm)
{
    int     iUev;
    int     iLZ = vld_count_leading_zero(vld_show_bits(pstVldStm, 32));
    vld_flush_bits(pstVldStm, iLZ);
    iUev    = (vld_get_bits(pstVldStm, iLZ + 1) - 1);
    return (iUev & 1 ? -(iUev >> 1) : (iUev >> 1));
}
