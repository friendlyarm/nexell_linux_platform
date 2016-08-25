//
//  Nexel Video Rate Control API
//


#ifndef __NX_VIDEO_RATE_CTRL_H__
#define __NX_VIDEO_RATE_CTRL_H__

#include <nx_video_api.h>


typedef struct
{
    int32_t                    iMajor;
    int32_t                    iMinor;
    int32_t                    iPatch;
} NX_RC_VERSION;


#ifdef __cplusplus
extern "C" {
#endif

//
//  Video Rate Control API Functions
//
void *NX_VidRateCtrlInit( int32_t iCodecType, NX_VID_ENC_INIT_PARAM *pstPara );
VID_ERROR_E NX_VidRateCtrlGetFrameQp( void *hRateCtrl, int32_t *piFrmQp, int32_t *piFrmType );
VID_ERROR_E NX_VidRateCtrlUpdate( void *hRateCtrl, uint32_t uFrmByte );
VID_ERROR_E NX_VidRateCtrlChangePara( void *hRateCtrl, NX_VID_ENC_CHG_PARAM *pstChgPara );
VID_ERROR_E NX_VidRateCtrlGetVersion( NX_RC_VERSION *pstVersion );


#ifdef __cplusplus
}
#endif

#endif  //  __NX_VIDEO_RATE_CTRL_H__c
