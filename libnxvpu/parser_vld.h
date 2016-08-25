#ifndef _PARSER_VLD_H_
#define _PARSER_VLD_H_

typedef struct {
    unsigned long           dwUsedBits;
    unsigned char           *pbyStart;
    unsigned long           dwPktSize;
} VLD_STREAM;

unsigned long   vld_show_bits (VLD_STREAM *pstVldStm, int iBits);
unsigned long   vld_get_bits (VLD_STREAM *pstVldStm, int iBits);
void            vld_flush_bits (VLD_STREAM *pstVldStm, int iBits);
unsigned int    vld_get_uev(VLD_STREAM *pstVldStm);
int             vld_get_sev(VLD_STREAM *pstVldStm);


#endif  // #ifndef _PARSER_VLD_H_
