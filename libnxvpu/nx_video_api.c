//
//	Nexel Video En/Decoder API
//
#include <stdlib.h>		//	malloc & free
#include <string.h>		//	memset
#include <unistd.h>		//	close

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <nx_video_api.h>
#include <nx_video_rate_ctrl.h>

#include <nx_fourcc.h>
#include <vpu_drv_ioctl.h>		//	Device Driver IOCTL
#include <nx_alloc_mem.h>		//	Memory Allocation Information
#include "parser_vld.h"

#ifdef HEVC_DEC
#include <ihevcd_cxa.h>
#include <iv.h>
#endif


// 14.12.05 initial version = 0.9.0
#define NX_VID_VER_MAJOR		0
#define NX_VID_VER_MINOR		9
#define NX_VID_VER_PATCH		0

#define	WORK_BUF_SIZE		(  80*1024)
#define	STREAM_BUF_SIZE		(1024*1024*4)
#define	PS_SAVE_SIZE		( 320*1024)


//
//	Debug Message Configuration
//
#define	NX_DTAG		"[VPU|API] "		//
#include "api_osapi.h"
#define	DBG_BUF_ALLOC		0
#define	DBG_ENC_OUT			0
#define DBG_BUF_INFO		0
#define	DBG_VBS				0
#define	DBG_WARNING			1
#define	DBG_USER_DATA		0

#define	DEV_NAME		"/dev/nx_vpu"
#define	RECON_CHROMA_INTERLEAVED	0

#define SLICE_SAVE_SIZE                 (MAX_DEC_PIC_WIDTH*MAX_DEC_PIC_HEIGHT*3/4)

//----------------------------------------------------------------------------
//	define static functions
static int32_t AllocateEncoderMemory( NX_VID_ENC_HANDLE hEnc );
static int32_t FreeEncoderMemory( NX_VID_ENC_HANDLE hEnc );
static int32_t AllocateDecoderMemory( NX_VID_DEC_HANDLE hDec);
static int32_t FreeDecoderMemory( NX_VID_DEC_HANDLE hDec );
static void DecoderFlushDispInfo( NX_VID_DEC_HANDLE hDec );
static void DecoderPutDispInfo( NX_VID_DEC_HANDLE hDec, int32_t iIndex, VPU_DEC_DEC_FRAME_ARG *pDecArg, uint64_t lTimeStamp, int32_t Reliable );
//static uint64_t DecoderGetTimeStamp( NX_VID_DEC_HANDLE hDec, int32_t iIndex, int32_t *piPicType );

static void Mp4DecParserVideoCfg( NX_VID_DEC_HANDLE hdec );
static void Mp4DecParserFrameHeader ( NX_VID_DEC_HANDLE hDec, VPU_DEC_DEC_FRAME_ARG *pDecArg );

#ifdef HEVC_DEC
static int32_t NX_HevcDecOpen( NX_VID_DEC_HANDLE hDec, int32_t iOptions );
static VID_ERROR_E NX_HevcDecParseVideoCfg( NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn, NX_VID_SEQ_OUT *pstSeqOut);
static VID_ERROR_E NX_HevcDecInit (NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn);
static VID_ERROR_E NX_HevcDecDecodeFrame( NX_VID_DEC_HANDLE hDec, NX_VID_DEC_IN *pstDecIn, NX_VID_DEC_OUT *pstDecOut );
#endif

static int32_t DecoderJpegHeader(	NX_VID_DEC_HANDLE hDec, uint8_t           *pbyStream, int32_t iSize );
static int32_t DecoderJpegSOF( NX_VID_DEC_HANDLE hDec, VLD_STREAM *pstStrm );
static void DecoderJpegDHT( NX_VID_DEC_HANDLE hDec, VLD_STREAM *pstStrm );
static int32_t DecoderJpegDQT( NX_VID_DEC_HANDLE hDec, VLD_STREAM *pstStrm );
static void	DecoderJpegSOS( NX_VID_DEC_HANDLE hDec, VLD_STREAM *pstStrm );
static void GenerateJpegHuffmanTable( NX_VID_DEC_HANDLE hDec, int32_t iTabNum );


//////////////////////////////////////////////////////////////////////////////
//
//		Video Encoder APIs
//

struct NX_VIDEO_ENC_INFO
{
	// open information
	int32_t hEncDrv;		                    // Device Driver Handle
	int32_t codecMode;		                    // (AVC_ENC = 0x10 / MP4_ENC = 0x12 / NX_JPEG_ENC=0x20 )
	int32_t instIndex;		                    // Instance Index

	NX_MEMORY_HANDLE hInstanceBuf;				// Encoder Instance Memory Buffer

	// Frame Buffer Information ( for Initialization )
	int32_t refChromaInterleave;				// Reconstruct & Referernce Buffer Chroma Interleaved
	NX_VID_MEMORY_HANDLE hRefReconBuf[2];		// Reconstruct & Referernce Buffer Information
	NX_MEMORY_HANDLE hSubSampleBuf[2];			// Sub Sample Buffer Address
	NX_MEMORY_HANDLE hBitStreamBuf;				// Bit Stream Buffer
	int32_t isInitialized;

	// Initialize Output Informations
	VPU_ENC_GET_HEADER_ARG seqInfo;

	// Encoder Options ( Default CBR Mode )
	int32_t width;
	int32_t height;
	int32_t gopSize;							// Group Of Pictures' Size
	int32_t frameRateNum;						// Framerate numerator
	int32_t frameRateDen;						// Framerate denominator
	int32_t bitRate;							// BitRate
	int32_t enableSkip;							// Enable skip frame

	int32_t userQScale;							// Default User Qunatization Scale

	int32_t GopFrmCnt;							// GOP frame counter

	// JPEG Specific
	uint32_t frameIndex;
	int32_t rstIntval;

	void *hRC;									// Rate Control Handle
};

NX_VID_ENC_HANDLE NX_VidEncOpen( VID_TYPE_E eCodecType, int32_t *piInstanceIdx )
{
	VPU_OPEN_ARG openArg;
	int32_t ret;

	//	Create Context
	NX_VID_ENC_HANDLE hEnc = (NX_VID_ENC_HANDLE)malloc( sizeof(struct NX_VIDEO_ENC_INFO) );
	memset( hEnc, 0, sizeof(struct NX_VIDEO_ENC_INFO) );
	memset( &openArg, 0, sizeof(openArg) );

	FUNC_IN();

	//	Open Device Driver
	hEnc->hEncDrv = open(DEV_NAME, O_RDWR);
	if( hEnc->hEncDrv < 0 )
	{
		NX_ErrMsg( ("Cannot open device(%s)!!!\n", DEV_NAME) );
		goto ERROR_EXIT;
	}

	switch( eCodecType )
	{
		case NX_MP4_ENC:
			openArg.codecStd = CODEC_STD_MPEG4;
			break;
		case NX_AVC_ENC:
			openArg.codecStd = CODEC_STD_AVC;
			break;
		case NX_JPEG_ENC:
			openArg.codecStd = CODEC_STD_MJPG;
			break;
		case NX_H263_ENC:
			openArg.codecStd = CODEC_STD_H263;
			break;
		default:
			NX_ErrMsg( ("Invalid codec type (%d)!!!\n", eCodecType) );
			goto ERROR_EXIT;
	}

	//	Allocate Instance Memory & Stream Buffer
	hEnc->hInstanceBuf =  NX_AllocateMemory( WORK_BUF_SIZE, 4096 );		//	x16 aligned
	if( 0 == hEnc->hInstanceBuf ){
		NX_ErrMsg(("hInstanceBuf allocation failed.\n"));
		goto ERROR_EXIT;
	}

	openArg.instIndex = -1;
	openArg.isEncoder = 1;
	openArg.instanceBuf = *hEnc->hInstanceBuf;

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_OPEN_INSTANCE, &openArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("NX_VidEncOpen() : IOCTL_VPU_OPEN_INSTANCE ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	hEnc->instIndex = openArg.instIndex;
	hEnc->codecMode = eCodecType;
	hEnc->refChromaInterleave = RECON_CHROMA_INTERLEAVED;

	if ( piInstanceIdx )
		*piInstanceIdx = hEnc->instIndex;

	FUNC_OUT();
	return hEnc;

ERROR_EXIT:
	if( hEnc )
	{
		if( hEnc->hEncDrv > 0 )
		{
			close( hEnc->hEncDrv );
		}
		free( hEnc );
	}
	return NULL;
}

VID_ERROR_E NX_VidEncClose( NX_VID_ENC_HANDLE hEnc )
{
	int32_t ret;
	FUNC_IN();

	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if ( hEnc->hRC )
	{
		free( hEnc->hRC );
		hEnc->hRC = NULL;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		free( hEnc );
		return VID_ERR_PARAM;
	}

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_CLOSE_INSTANCE, 0 );
	if( ret < 0 )
	{
		ret = VID_ERR_FAIL;
		NX_ErrMsg( ("NX_VidEncClose() : IOCTL_VPU_CLOSE_INSTANCE ioctl failed!!!\n") );
	}

	FreeEncoderMemory( hEnc );
	close( hEnc->hEncDrv );
	free( hEnc );

	FUNC_OUT();
	return VID_ERR_NONE;
}

VID_ERROR_E NX_VidEncInit( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_INIT_PARAM *pstParam )
{
	int32_t ret = VID_ERR_NONE;
	VPU_ENC_SEQ_ARG seqArg;
	VPU_ENC_SET_FRAME_ARG frameArg;
	VPU_ENC_GET_HEADER_ARG *pHdrArg = &hEnc->seqInfo;

	FUNC_IN();

	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	memset( &seqArg, 0, sizeof( seqArg ) );
	memset( &frameArg, 0, sizeof( frameArg ) );
	memset( pHdrArg, 0, sizeof(VPU_ENC_GET_HEADER_ARG) );

	//	Initialize Encoder
	if( hEnc->isInitialized  )
	{
		NX_ErrMsg( ("Already initialized\n") );
		return VID_ERR_FAIL;
	}

	hEnc->width = pstParam->width;
	hEnc->height = pstParam->height;
	hEnc->gopSize = pstParam->gopSize;
	hEnc->frameRateNum = pstParam->fpsNum;
	hEnc->frameRateDen = pstParam->fpsDen;
	hEnc->bitRate = pstParam->bitrate;
	hEnc->enableSkip = !pstParam->disableSkip;

	if( 0 != AllocateEncoderMemory( hEnc ) )
	{
		NX_ErrMsg( ("AllocateEncoderMemory() failed!!\n") );
		return VID_ERR_NOT_ALLOC_BUFF;
	}

	seqArg.srcWidth = pstParam->width;
	seqArg.srcHeight = pstParam->height;

	seqArg.chromaInterleave = pstParam->chromaInterleave;
	seqArg.refChromaInterleave = hEnc->refChromaInterleave;

	seqArg.rotAngle = pstParam->rotAngle;
	seqArg.mirDirection = pstParam->mirDirection;

	seqArg.strmBufPhyAddr = hEnc->hBitStreamBuf->phyAddr;
	seqArg.strmBufVirAddr = hEnc->hBitStreamBuf->virAddr;
	seqArg.strmBufSize = hEnc->hBitStreamBuf->size;

	if( hEnc->codecMode != NX_JPEG_ENC )
	{
		seqArg.gopSize = pstParam->gopSize;
		seqArg.frameRateNum = pstParam->fpsNum;
		seqArg.frameRateDen = pstParam->fpsDen;

		//	Rate Control
		if ( pstParam->enableRC )
		{
			if (pstParam->RCAlgorithm == 1)
			{
				seqArg.RCModule = 2;
	    		seqArg.bitrate = 0;
	    		seqArg.disableSkip = 0;
				seqArg.initialDelay = 0;
				seqArg.vbvBufferSize = 0;
				seqArg.gammaFactor = 0;

				hEnc->hRC = NX_VidRateCtrlInit( hEnc->codecMode, pstParam );
				if( hEnc->hRC == NULL ) goto ERROR_EXIT;
			}
			else
			{
				hEnc->hRC = NULL;
				seqArg.RCModule = 1;
	    		seqArg.bitrate = pstParam->bitrate;
	    		seqArg.disableSkip = !pstParam->disableSkip;
				seqArg.initialDelay = pstParam->RCDelay;
				seqArg.vbvBufferSize = pstParam->rcVbvSize;
				seqArg.gammaFactor = ( pstParam->gammaFactor ) ? ( pstParam->gammaFactor ) : ((int)(0.75 * 32768));
			}
		}
		else
		{
			seqArg.RCModule = 0;
			hEnc->userQScale = (pstParam->initialQp == 0) ? (23) : (pstParam->initialQp);
		}

		seqArg.searchRange = (hEnc->codecMode != NX_H263_ENC) ? (pstParam->searchRange) : (3);       // ME Search Range
		seqArg.intraRefreshMbs = pstParam->numIntraRefreshMbs;

		if( hEnc->codecMode == NX_AVC_ENC )
		{
			if ( pstParam->enableAUDelimiter != 0 )
				seqArg.enableAUDelimiter = 1;
			seqArg.maxQP = ( pstParam->maximumQp > 0 ) ? ( pstParam->maximumQp ) : (51);
		}
		else
		{
			seqArg.maxQP = ( pstParam->maximumQp > 0 ) ? ( pstParam->maximumQp ) : (31);
		}
	}
	else
	{
		seqArg.frameRateNum = 1;
		seqArg.frameRateDen = 1;
		seqArg.gopSize = 1;
		seqArg.quality = pstParam->jpgQuality;
	}

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_SET_SEQ_PARAM, &seqArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_ENC_SET_SEQ_PARAM ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	if( hEnc->codecMode != NX_JPEG_ENC )
	{
		frameArg.numFrameBuffer = 2;		//	We use always 2 frame
		frameArg.frameBuffer[0] = *hEnc->hRefReconBuf[0];
		frameArg.frameBuffer[1] = *hEnc->hRefReconBuf[1];
		frameArg.subSampleBuffer[0] = *hEnc->hSubSampleBuf[0];
		frameArg.subSampleBuffer[1] = *hEnc->hSubSampleBuf[1];

		//	data partition mode always disabled ( for MPEG4 )
		frameArg.dataPartitionBuffer.phyAddr = 0;
		frameArg.dataPartitionBuffer.virAddr = 0;

		ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_SET_FRAME_BUF, &frameArg );
		if( ret < 0 )
		{
			ret = VID_ERR_INIT;
			NX_ErrMsg( ("IOCTL_VPU_ENC_SET_FRAME_BUF ioctl failed!!!\n") );
			goto ERROR_EXIT;
		}

		ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_GET_HEADER, pHdrArg );
		if( ret < 0 )
		{
			NX_ErrMsg( ("IOCTL_VPU_ENC_GET_HEADER ioctl failed!!!\n") );
			goto ERROR_EXIT;
		}
	}
	else
	{
		frameArg.numFrameBuffer = 0;
	}

	hEnc->isInitialized = 1;
	ret = VID_ERR_NONE;
	FUNC_OUT();

ERROR_EXIT:
	return ret;
}

VID_ERROR_E NX_VidEncGetSeqInfo( NX_VID_ENC_HANDLE hEnc, uint8_t *pbySeqBuf, int32_t *piSeqBufSize )
{
	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( !hEnc->isInitialized )
	{
		NX_ErrMsg( ("Invalid encoder operation initialize first!!!\n") );
		return VID_ERR_INIT;
	}

	if( hEnc->codecMode == NX_AVC_ENC )
	{
		memcpy( pbySeqBuf, hEnc->seqInfo.avcHeader.spsData, hEnc->seqInfo.avcHeader.spsSize );
		memcpy( pbySeqBuf+hEnc->seqInfo.avcHeader.spsSize, hEnc->seqInfo.avcHeader.ppsData, hEnc->seqInfo.avcHeader.ppsSize );
		*piSeqBufSize = hEnc->seqInfo.avcHeader.spsSize + hEnc->seqInfo.avcHeader.ppsSize;
	}
	else if ( hEnc->codecMode == NX_MP4_ENC )
	{
		memcpy( pbySeqBuf, hEnc->seqInfo.mp4Header.vosData, hEnc->seqInfo.mp4Header.vosSize );
		memcpy( pbySeqBuf+hEnc->seqInfo.mp4Header.vosSize, hEnc->seqInfo.mp4Header.volData, hEnc->seqInfo.mp4Header.volSize );
		*piSeqBufSize = hEnc->seqInfo.mp4Header.vosSize + hEnc->seqInfo.mp4Header.volSize;
	}
	else
	{
		*piSeqBufSize = 0;
		return VID_ERR_NONE;
	}

	FUNC_OUT();
	return VID_ERR_NONE;
}

VID_ERROR_E NX_VidEncEncodeFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_IN *pstEncIn, NX_VID_ENC_OUT *pstEncOut )
{
	int32_t ret;
	int32_t iFrmType = PIC_TYPE_UNKNOWN;
	VPU_ENC_RUN_FRAME_ARG runArg;

	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	memset( &runArg, 0, sizeof(runArg) );

	runArg.inImgBuffer = *(pstEncIn->pImage);
	//runArg.changeFlag = 0;
	//runArg.enableRc = 1;					//	N/A
	runArg.quantParam = pstEncIn->quantParam;
	runArg.skipPicture = pstEncIn->forcedSkipFrame;
	//pstEncIn->timeStamp;

	//printf("forcedIFrame = %d, GopFrmCnt = %d, gopSize = %d \n", pstEncIn->forcedIFrame, hEnc->GopFrmCnt, hEnc->gopSize);

	if( (pstEncIn->forcedIFrame) || (hEnc->GopFrmCnt >= hEnc->gopSize) || (hEnc->GopFrmCnt == 0) )
	{
		runArg.forceIPicture = 1;
		hEnc->GopFrmCnt = 0;
	}
	hEnc->GopFrmCnt += 1;

	if ( hEnc->hRC )
	{
#if 1
		if ( runArg.forceIPicture == 1 )
			iFrmType = PIC_TYPE_I;
		else if ( runArg.skipPicture == 1 )
			iFrmType = PIC_TYPE_SKIP;
		else
			iFrmType = PIC_TYPE_P;
#else
		iFrmType = PIC_TYPE_UNKNOWN;
#endif
		NX_VidRateCtrlGetFrameQp( hEnc->hRC, &runArg.quantParam, &iFrmType );

		if ( iFrmType == PIC_TYPE_SKIP )
			runArg.skipPicture = 1;

#if 1
		pstEncIn->quantParam = runArg.quantParam;
#endif
	}

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_RUN_FRAME, &runArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_ENC_RUN_FRAME ioctl failed!!!\n") );
		return ret;
	}

	if ( hEnc->hRC )
	{
		NX_VidRateCtrlUpdate( hEnc->hRC, runArg.outStreamSize );
	}

	pstEncOut->width = hEnc->width;
	pstEncOut->height = hEnc->height;
	pstEncOut->frameType = ( iFrmType != PIC_TYPE_SKIP ) ? (runArg.frameType) : (PIC_TYPE_SKIP);
	pstEncOut->bufSize = runArg.outStreamSize;
	pstEncOut->outBuf = runArg.outStreamAddr;
	pstEncOut->ReconImg = *hEnc->hRefReconBuf[runArg.reconImgIdx];

//	NX_DbgMsg( DBG_ENC_OUT, ("Encoder Output : Success(outputSize = %d, isKey=%d)\n", pEncOut->bufSize, pEncOut->isKey) );
	FUNC_OUT();
	return 0;
}

VID_ERROR_E NX_VidEncChangeParameter( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_CHG_PARAM *pstChgParam )
{
	int32_t ret;
	VPU_ENC_CHG_PARA_ARG chgArg;
	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	memset( &chgArg, 0, sizeof(chgArg) );

	//printf("chgFlg = %x, gopSize = %d, bitrate = %d, fps = %d/%d, maxQp = %d, skip = %d, vbv = %d, mb = %d \n",
	//	pstChgParam->chgFlg, pstChgParam->gopSize, pstChgParam->bitrate, pstChgParam->fpsNum, pstChgParam->fpsDen, pstChgParam->maximumQp, pstChgParam->disableSkip, pstChgParam->rcVbvSize, pstChgParam->numIntraRefreshMbs);

	chgArg.chgFlg = pstChgParam->chgFlg;
	chgArg.gopSize = pstChgParam->gopSize;
	chgArg.bitrate = pstChgParam->bitrate;
	chgArg.frameRateNum = pstChgParam->fpsNum;
	chgArg.frameRateDen = pstChgParam->fpsDen;
	chgArg.intraRefreshMbs = pstChgParam->numIntraRefreshMbs;

	if ( chgArg.chgFlg & VID_CHG_GOP )
		hEnc->gopSize = chgArg.gopSize;

	if ( hEnc->hRC )
	{
		if ( NX_VidRateCtrlChangePara( hEnc->hRC, pstChgParam ) != VID_ERR_NONE )
			return VID_ERR_CHG_PARAM;

		chgArg.chgFlg = pstChgParam->chgFlg & 0x18;
		chgArg.gopSize = 0;
		chgArg.bitrate = 0;
		chgArg.frameRateNum = pstChgParam->fpsNum;
		chgArg.frameRateDen = pstChgParam->fpsDen;
		chgArg.intraRefreshMbs = pstChgParam->numIntraRefreshMbs;
	}

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_CHG_PARAM, &chgArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_ENC_CHG_PARAM ioctl failed!!!\n") );
		return ret;
	}

	FUNC_OUT();
	return VID_ERR_NONE;
}

//
//		End of Encoder APIs
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//	Jpeg Encoder APIs
//
VID_ERROR_E NX_VidEncJpegGetHeader( NX_VID_ENC_HANDLE hEnc, uint8_t *pbyJpgHeader, int32_t *piHeaderSize )
{
	int32_t ret;
	VPU_ENC_GET_HEADER_ARG *pHdrArg = (VPU_ENC_GET_HEADER_ARG *)calloc(sizeof(VPU_ENC_GET_HEADER_ARG), 1);
	FUNC_IN();
	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_JPG_GET_HEADER, pHdrArg );

	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_JPG_GET_HEADER ioctl failed!!!\n") );
	}
	else
	{
		memcpy( pbyJpgHeader, pHdrArg->jpgHeader.jpegHeader, pHdrArg->jpgHeader.headerSize );
		*piHeaderSize = pHdrArg->jpgHeader.headerSize;
	}
	FUNC_OUT();
	return ret;
}

VID_ERROR_E NX_VidEncJpegRunFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_MEMORY_HANDLE hInImage, NX_VID_ENC_OUT *pstEncOut )
{
	int32_t ret;
	VPU_ENC_RUN_FRAME_ARG runArg;
	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	memset( &runArg, 0, sizeof(runArg) );
	runArg.inImgBuffer = *hInImage;

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_JPG_RUN_FRAME, &runArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_JPG_RUN_FRAME ioctl failed!!!\n") );
		return -1;
	}

	pstEncOut->width = hEnc->width;
	pstEncOut->height = hEnc->height;
	pstEncOut->bufSize = runArg.outStreamSize;
	pstEncOut->outBuf = runArg.outStreamAddr;
	FUNC_OUT();
	return 0;
}

//
//	Jpeg Encoder APIs
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//	Video Decoder APIs
//

#define	PIC_FLAG_KEY		0x0001
#define	PIC_FLAG_INTERLACE	0x0010

#ifdef HEVC_DEC
#define HEVC_MAX_FRAME_WIDTH			1920	// 2560
#define HEVC_MAX_FRAME_HEIGHT			1088	// 1600
#define HEVC_MAX_REF_FRAMES			16
#define HEVC_MAX_REORDER_FRAMES		16
#define HEVC_MAX_NUM_CORES				4

#define HEVC_DEFAULT_WIDTH				1920
#define HEVC_DEFAULT_HEIGHT			1080
#endif


struct NX_VIDEO_DEC_INFO
{
	// open information
	int32_t hDecDrv;                                       // Device Driver Handle
	int32_t codecStd;                                      // NX_VPU_CODEC_MODE 	( AVC_DEC = 0, MP2_DEC = 2, MP4_DEC = 3, DV3_DEC = 3, RV_DEC = 4  )
	int32_t instIndex;                                     // Instance Index

	int32_t width;
	int32_t height;

	// Frame Buffer Information ( for Initialization )
	int32_t numFrameBuffers;
	NX_MEMORY_HANDLE hInstanceBuf;								// Decoder Instance Memory Buffer
	NX_MEMORY_HANDLE hBitStreamBuf;								// Bit Stream Buffer
	NX_VID_MEMORY_HANDLE hFrameBuffer[MAX_DEC_FRAME_BUFFERS];	// Reconstruct & Referernce Buffer Information
	NX_MEMORY_HANDLE hColMvBuffer;								// All Codecs
	NX_MEMORY_HANDLE hSliceBuffer;								// AVC codec
	NX_MEMORY_HANDLE hPvbSliceBuffer;							// PVX codec

	int32_t enableUserData;										// User Data Mode Enable/Disable
	NX_MEMORY_HANDLE hUserDataBuffer;							// User Data ( MPEG2 Only )

	int32_t isInitialized;

	int32_t useExternalFrameBuffer;
	int32_t numBufferableBuffers;

	// Initialize Output Information
	uint8_t	pSeqData[2048];										// SPS PPS (H.264) or Decoder Specific Information(for MPEG4)
	int32_t seqDataSize;

	uint64_t savedTimeStamp;
	uint32_t prevStrmReadPos;

	uint64_t timeStamp[MAX_DEC_FRAME_BUFFERS][2];
	int32_t picType[MAX_DEC_FRAME_BUFFERS];
	int32_t picFlag[MAX_DEC_FRAME_BUFFERS];

	int32_t multiResolution[MAX_DEC_FRAME_BUFFERS];

	// For Display Frame Information
	int32_t isInterlace[MAX_DEC_FRAME_BUFFERS];
	int32_t topFieldFirst[MAX_DEC_FRAME_BUFFERS];
	int32_t FrmReliable_0_100[MAX_DEC_FRAME_BUFFERS];
	int32_t upSampledWidth[MAX_DEC_FRAME_BUFFERS];
	int32_t upSampledHeight[MAX_DEC_FRAME_BUFFERS];

	// For MPEG4
	int32_t vopTimeBits;

	// For Jpeg Decoder
	uint32_t headerSize;
	int32_t thumbnailMode;
	int32_t frmBufferValid[MAX_DEC_FRAME_BUFFERS];
	int32_t decodeIdx;
	int32_t fstFrame;

	int32_t imgFourCC;
	int32_t rstInterval;
	int32_t userHuffTable;

	uint8_t  huffBits[4][16];
	uint8_t  huffPtr[4][16];
	uint32_t huffMin[4][16];
	uint32_t huffMax[4][16];

	uint8_t huffValue[4][162];
	uint8_t infoTable[4][6];
	uint8_t quantTable[4][64];

	int32_t huffDcIdx;
	int32_t huffAcIdx;
	int32_t qIdx;

	int32_t busReqNum;
	int32_t mcuBlockNum;
	int32_t compNum;
	int32_t compInfo[3];
    int32_t mcuWidth;
    int32_t mcuHeight;

	int32_t pagePtr;
	int32_t wordPtr;
	int32_t bitPtr;

#ifdef HEVC_DEC
	iv_obj_t *codec_obj;				// HEVC Handle
	iv_mem_rec_t *pv_mem_rec_location;	// memory table pointer
	uint32_t u4_num_mem_recs;			// memory table #
	//ivd_out_bufdesc_t *ps_out_buf;
    //ivd_out_bufdesc_t s_disp_buffers[MAX_DEC_FRAME_BUFFERS];
	int32_t validFrame[MAX_DEC_FRAME_BUFFERS];
	int32_t curFrmIdx;
#endif
};

NX_VID_DEC_HANDLE NX_VidDecOpen( VID_TYPE_E eCodecType, uint32_t uMp4Class, int32_t iOptions, int32_t *piInstanceIdx  )
{
	int32_t ret;
	VPU_OPEN_ARG openArg;
	int32_t workBufSize = WORK_BUF_SIZE;
	FUNC_IN();

	//	Create Context
	NX_VID_DEC_HANDLE hDec = (NX_VID_DEC_HANDLE)malloc( sizeof(struct NX_VIDEO_DEC_INFO) );
	memset( hDec, 0, sizeof(struct NX_VIDEO_DEC_INFO) );

	if ( eCodecType != NX_HEVC_DEC )
	{
		memset( &openArg, 0, sizeof(openArg) );

		//	Open Device Driver
		hDec->hDecDrv = open(DEV_NAME, O_RDWR);
		if( hDec->hDecDrv < 0 )
		{
			NX_ErrMsg(("Cannot open device(%s)!!!\n", DEV_NAME));
			goto ERROR_EXIT;
		}

		if( eCodecType == NX_AVC_DEC || eCodecType == NX_AVC_ENC )
		{
			workBufSize += PS_SAVE_SIZE;
		}
		hDec->hBitStreamBuf = NX_AllocateMemory( STREAM_BUF_SIZE, 4096 );	//	x16 aligned
		if( 0 == hDec->hBitStreamBuf ){
			NX_ErrMsg(("hBitStreamBuf allocation failed.\n"));
			goto ERROR_EXIT;
		}

		//	Allocate Instance Memory & Stream Buffer
		hDec->hInstanceBuf =  NX_AllocateMemory( workBufSize, 4096 );		//	x16 aligned
		if( 0 == hDec->hInstanceBuf ){
			NX_ErrMsg(("hInstanceBuf allocation failed.\n"));
			goto ERROR_EXIT;
		}

		switch( eCodecType )
		{
			case NX_AVC_DEC:
				openArg.codecStd = CODEC_STD_AVC;
				break;
			case NX_MP2_DEC:
				openArg.codecStd = CODEC_STD_MPEG2;
				break;
			case NX_MP4_DEC:
				openArg.codecStd = CODEC_STD_MPEG4;
				openArg.mp4Class = uMp4Class;
				break;
			case NX_H263_DEC:	//
				openArg.codecStd = CODEC_STD_H263;
				break;
			case NX_DIV3_DEC:	//
				openArg.codecStd = CODEC_STD_DIV3;
				break;
			case NX_RV_DEC:		// Real Video
				openArg.codecStd = CODEC_STD_RV;
				break;
			case NX_VC1_DEC:	//	WMV
				openArg.codecStd = CODEC_STD_VC1;
				break;
			case NX_THEORA_DEC:	//	Theora
				openArg.codecStd = CODEC_STD_THO;
				break;
			case NX_VP8_DEC:	//	VP8
				openArg.codecStd = CODEC_STD_VP8;
				break;
			case NX_JPEG_DEC:
				openArg.codecStd = CODEC_STD_MJPG;
				break;
			default:
				NX_ErrMsg( ("IOCTL_VPU_OPEN_INSTANCE codec Type\n") );
				goto ERROR_EXIT;
		}

		openArg.instIndex = -1;
		openArg.instanceBuf = *hDec->hInstanceBuf;
		openArg.streamBuf = *hDec->hBitStreamBuf;

		if( iOptions && DEC_OPT_CHROMA_INTERLEAVE )
		{
			openArg.chromaInterleave = 1;
		}

		ret = ioctl( hDec->hDecDrv, IOCTL_VPU_OPEN_INSTANCE, &openArg );
		if( ret < 0 )
		{
			NX_ErrMsg( ("IOCTL_VPU_OPEN_INSTANCE ioctl failed!!!\n") );
			goto ERROR_EXIT;
		}
		hDec->instIndex = openArg.instIndex;
		hDec->codecStd = openArg.codecStd;
	}
#ifdef HEVC_DEC
	else
	{
		if ( NX_HevcDecOpen( hDec, iOptions ) < 0 )
			goto ERROR_EXIT;
	}
#endif

	if ( piInstanceIdx )
		*piInstanceIdx = hDec->instIndex;

	DecoderFlushDispInfo( hDec );

	FUNC_OUT();
	return hDec;

ERROR_EXIT:
	if ( hDec )
	{
		if( hDec->hDecDrv > 0 )
		{
			if( hDec->hInstanceBuf )
			{
				NX_FreeMemory(hDec->hInstanceBuf);
			}
			if( hDec->hBitStreamBuf )
			{
				NX_FreeMemory(hDec->hBitStreamBuf);
			}
			close( hDec->hDecDrv );
		}
		free( hDec );
	}
	return 0;
}

VID_ERROR_E NX_VidDecClose( NX_VID_DEC_HANDLE hDec )
{
	int32_t ret;
	FUNC_IN();

	if( !hDec )
	{
		NX_ErrMsg( ("Invalid decoder handle or driver handle!!!\n") );
		return -1;
	}

	if ( hDec->codecStd != CODEC_STD_HEVC )
	{
		if( hDec->hDecDrv <= 0 )
		{
			NX_ErrMsg( ("Invalid decoder handle or driver handle!!!\n") );
			return -1;
		}

		ret = ioctl( hDec->hDecDrv, IOCTL_VPU_CLOSE_INSTANCE, 0 );
		if( ret < 0 )
		{
			NX_ErrMsg( ("IOCTL_VPU_CLOSE_INSTANCE ioctl failed!!!\n") );
		}

		FreeDecoderMemory( hDec );

		close( hDec->hDecDrv );
	}
#ifdef HEVC_DEC
	else
	{
		uint32_t i;
        iv_retrieve_mem_rec_ip_t s_retrieve_dec_ip;
        iv_retrieve_mem_rec_op_t s_retrieve_dec_op;

        s_retrieve_dec_ip.pv_mem_rec_location = hDec->pv_mem_rec_location;
        s_retrieve_dec_ip.e_cmd = IV_CMD_RETRIEVE_MEMREC;
        s_retrieve_dec_ip.u4_size = sizeof(iv_retrieve_mem_rec_ip_t);
        s_retrieve_dec_op.u4_size = sizeof(iv_retrieve_mem_rec_op_t);

        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_retrieve_dec_ip, (void *)&s_retrieve_dec_op) )
        {
            NX_ErrMsg( ("Error in Retrieve Memrec\n") );
            return -1;
        }

        {
            iv_mem_rec_t *ps_mem_rec = hDec->pv_mem_rec_location;

            for( i = 0; i < s_retrieve_dec_op.u4_num_mem_rec_filled ; i++ )
            {
                free(ps_mem_rec->pv_base);
                ps_mem_rec++;
            }

			free(hDec->pv_mem_rec_location);
        }

#if 0
	    for(i = 0; i < hDec->numFrameBuffers ; i++)
            free( hDec->s_disp_buffers[i].pu1_bufs[0] );

		free ( hDec->ps_out_buf->pu1_bufs[0] );
	    free ( hDec->ps_out_buf );
#else
		//	Free Frame Buffer
		if( !hDec->useExternalFrameBuffer )
		{
			for( i=0 ; i<hDec->numFrameBuffers ; i++ )
			{
				if( hDec->hFrameBuffer[i] )
				{
					NX_FreeVideoMemory( hDec->hFrameBuffer[i] );
					hDec->hFrameBuffer[i] = 0;
				}
			}
		}
#endif
    }
#endif

	free( hDec );

	FUNC_OUT();
	return 0;
}

VID_ERROR_E NX_VidDecParseVideoCfg(NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn, NX_VID_SEQ_OUT *pstSeqOut)
{
	int32_t ret = -1;
	FUNC_IN();
	memset( pstSeqOut, 0, sizeof(NX_VID_SEQ_OUT) );

	if( !hDec )
	{
		NX_ErrMsg( ("Invalid decoder handle or driver handle!!!\n") );
		goto ERROR_EXIT;
	}

	if ( hDec->codecStd != CODEC_STD_HEVC )
	{
		VPU_DEC_SEQ_INIT_ARG seqArg;
		memset( &seqArg, 0, sizeof(seqArg) );

		if( hDec->hDecDrv <= 0 )
		{
			NX_ErrMsg( ("Invalid decoder handle or driver handle!!!\n") );
			goto ERROR_EXIT;
		}

		//	Initialize Decoder
		if( hDec->isInitialized  )
		{
			int32_t i = 0;
			int32_t iSeqSize = ( pstSeqIn->seqSize < 2048 ) ? ( pstSeqIn->seqSize ) : ( 2048 );

			if ( iSeqSize == hDec->seqDataSize )
			{
				uint8_t *pbySrc = hDec->pSeqData;
				uint8_t *pbyDst = pstSeqIn->seqInfo;
				for (i=0 ; i<iSeqSize ; i++)
				{
					if ( *pbySrc++ != *pbyDst++ )	break;
				}
			}

			if ( (iSeqSize == 0) || (i == iSeqSize) )
			{
				NX_ErrMsg( ("Already initialized\n") );
				goto ERROR_EXIT;
			}
			hDec->isInitialized = 0;
		}

		seqArg.seqData        	= pstSeqIn->seqInfo;
		seqArg.seqDataSize    	= pstSeqIn->seqSize;
		seqArg.outWidth 		= pstSeqIn->width;
		seqArg.outHeight 		= pstSeqIn->height;

		if ( hDec->codecStd != CODEC_STD_MJPG )
		{
			if( pstSeqIn->disableOutReorder )
			{
				NX_DbgMsg( DBG_WARNING, ("Diable Reordering!!!!\n") );
				seqArg.disableOutReorder = 1;
			}

			seqArg.enablePostFilter = pstSeqIn->enablePostFilter;
			seqArg.enableUserData   = (pstSeqIn->enableUserData) && (hDec->codecStd == CODEC_STD_MPEG2);
			if( seqArg.enableUserData )
			{
				NX_DbgMsg(DBG_USER_DATA, ("Enabled user data\n"));
				hDec->enableUserData = 1;
				hDec->hUserDataBuffer = NX_AllocateMemory( 0x10000, 4096 );		//	x16 aligned
				if( 0 == hDec->hUserDataBuffer ){
					NX_ErrMsg(("hUserDataBuffer allocation failed.(size=%d,align=%d)\n", 0x10000, 4096));
					goto ERROR_EXIT;
				}
				seqArg.userDataBuffer = *hDec->hUserDataBuffer;
			}

			if ( hDec->codecStd == CODEC_STD_AVC )
			{
				unsigned char *pbyTmp = seqArg.seqData;
				if ( (pbyTmp[2] == 0) && (pbyTmp[7] > 51) )
					pbyTmp[7] = 51;
				else if ( (pbyTmp[2] == 1) && (pbyTmp[6] > 51) )
					pbyTmp[6] = 51;
			}

			ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_SET_SEQ_INFO, &seqArg );
			if( ret == VID_NEED_STREAM )
				goto ERROR_EXIT;
			if( ret < 0 )
			{
				NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed!!!\n") );
				goto ERROR_EXIT;
			}

			if( seqArg.minFrameBufCnt < 1 || seqArg.minFrameBufCnt > MAX_DEC_FRAME_BUFFERS )
			{
				NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed(nimFrameBufCnt = %d)!!!\n", seqArg.minFrameBufCnt) );
				goto ERROR_EXIT;
			}
		}
		else
		{
			int32_t iRead;
			int32_t iThumbFlag = 0;
			VLD_STREAM stStrm = { 0, seqArg.seqData, seqArg.seqDataSize };

			iRead = vld_get_bits( &stStrm, 16 );
			if ( iRead != 0xFFD8 ) 			return -1;

			do {
				iRead = vld_get_bits( &stStrm, 16 );
				if ( iRead == 0xFFC0 )			// baseline DCT (SOF)
				{
					if ( DecoderJpegSOF( hDec, &stStrm ) < 0 )
					{
						pstSeqOut->unsupportedFeature = 1;
						NX_ErrMsg( ("DecoderJpegSOF() Error : not supported\n") );
						return -1;
					}

					if ( iThumbFlag == 0 )
					{
						seqArg.outWidth = hDec->width;
						seqArg.outHeight = hDec->height;
						pstSeqOut->imgFourCC = hDec->imgFourCC;
					}
					else
					{
						pstSeqOut->thumbnailWidth = hDec->width;
						pstSeqOut->thumbnailHeight = hDec->height;
					}

					ret = VID_ERR_NONE;

					if ( iThumbFlag == 0 )
						break;
					else
						iThumbFlag = 0;
				}
				else if ( ((iRead >= 0xFFC1) && (iRead <= 0xFFC3)) || ((iRead >= 0xFFC5) && (iRead <= 0xFFCF)) )
				{
					NX_ErrMsg( ("Profile is not supported\n") );
					pstSeqOut->unsupportedFeature = 1;
					return -1;
				}
				else if ( iRead == 0xFFD8 )		// SOI(Start of Image), Thumbnail
				{
					iThumbFlag = 1;
					pstSeqOut->thumbnailWidth;
					pstSeqOut->thumbnailHeight;
				}
				else
				{
					stStrm.dwUsedBits -= 8;
				}
			} while( 1 );

			seqArg.cropRight = seqArg.outWidth;
			seqArg.cropBottom = seqArg.outHeight;
			seqArg.minFrameBufCnt = 1;
		}

		hDec->seqDataSize = ( pstSeqIn->seqSize < 2048 ) ? ( pstSeqIn->seqSize ) : ( 2048 );
		memcpy (hDec->pSeqData, pstSeqIn->seqInfo, hDec->seqDataSize);

		if ( (hDec->codecStd == CODEC_STD_MPEG4) && ( hDec->pSeqData != NULL) && (hDec->seqDataSize > 0) )
			Mp4DecParserVideoCfg( hDec );

		if ( pstSeqIn->seqSize == 0 )
		{
			seqArg.cropRight = pstSeqIn->width;
			seqArg.cropBottom = pstSeqIn->height;
		}

		hDec->width = seqArg.cropRight;
		hDec->height = seqArg.cropBottom;
		hDec->numFrameBuffers = seqArg.minFrameBufCnt;

		pstSeqOut->minBuffers 		= seqArg.minFrameBufCnt;
		pstSeqOut->numBuffers 		= seqArg.minFrameBufCnt + pstSeqIn->addNumBuffers;
		pstSeqOut->width        	= seqArg.cropRight;
		pstSeqOut->height       	= seqArg.cropBottom;
		pstSeqOut->frameBufDelay	= seqArg.frameBufDelay;
		pstSeqOut->isInterlace 	= seqArg.interlace;
		pstSeqOut->frameRateNum 	= seqArg.frameRateNum;
		pstSeqOut->frameRateDen 	= seqArg.frameRateDen;

		if (seqArg.vp8HScaleFactor == 0)		pstSeqOut->vp8ScaleWidth  = 0;
		else if (seqArg.vp8HScaleFactor == 1) 	pstSeqOut->vp8ScaleWidth  = seqArg.vp8ScaleWidth * 5 / 4;
		else if (seqArg.vp8HScaleFactor == 2) 	pstSeqOut->vp8ScaleWidth  = seqArg.vp8ScaleWidth * 5 / 3;
		else if (seqArg.vp8HScaleFactor == 3) 	pstSeqOut->vp8ScaleWidth  = seqArg.vp8ScaleWidth * 2;

		if (seqArg.vp8VScaleFactor == 0)		pstSeqOut->vp8ScaleHeight  = 0;
		else if (seqArg.vp8VScaleFactor == 1) 	pstSeqOut->vp8ScaleHeight  = seqArg.vp8ScaleHeight * 5 / 4;
		else if (seqArg.vp8VScaleFactor == 2) 	pstSeqOut->vp8ScaleHeight  = seqArg.vp8ScaleHeight * 5 / 3;
		else if (seqArg.vp8VScaleFactor == 3) 	pstSeqOut->vp8ScaleHeight  = seqArg.vp8ScaleHeight * 2;
	}
#ifdef HEVC_DEC
	else
	{
		hDec->seqDataSize = ( pstSeqIn->seqSize < 2048 ) ? ( pstSeqIn->seqSize ) : ( 2048 );
		memcpy (hDec->pSeqData, pstSeqIn->seqInfo, hDec->seqDataSize);

		ret = NX_HevcDecParseVideoCfg( hDec, pstSeqIn, pstSeqOut );
	}
#endif

	// TBD.
	pstSeqOut->userDataNum = 0;
	pstSeqOut->userDataSize = 0;
	pstSeqOut->userDataBufFull = 0;
	pstSeqOut->unsupportedFeature = 0;

ERROR_EXIT:
	FUNC_OUT();
	return ret;
}

VID_ERROR_E NX_VidDecInit(NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn)
{
	int32_t i, ret=-1;
	VPU_DEC_REG_FRAME_ARG frameArg;

	FUNC_IN();
	memset( &frameArg, 0, sizeof(frameArg) );

	if( !hDec )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		goto ERROR_EXIT;
	}

	if ( hDec->codecStd != CODEC_STD_HEVC )
	{
		if( hDec->hDecDrv <= 0 )
		{
			NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
			goto ERROR_EXIT;
		}
		//	Initialize Encoder
		if( hDec->isInitialized  )
		{
			NX_ErrMsg( ("Already initialized\n") );
			goto ERROR_EXIT;
		}

		if( pstSeqIn->numBuffers > 0 )
		{
			hDec->useExternalFrameBuffer = 1;

			if ( pstSeqIn->numBuffers - hDec->numFrameBuffers < 2 )
			{
				NX_DbgMsg( DBG_WARNING, ("[Warning] External Buffer too small.(min=%d, buffers=%d)\n", hDec->numFrameBuffers, pstSeqIn->numBuffers) );
			}

			hDec->numFrameBuffers = pstSeqIn->numBuffers;
		}
		else
		{
			hDec->numFrameBuffers += pstSeqIn->addNumBuffers;
		}

		NX_ErrMsg( ("Frame Buffer Number = %d, useExternalFrameBuffer = %x \n", hDec->numFrameBuffers, hDec->useExternalFrameBuffer ) );

		if ( hDec->codecStd != CODEC_STD_MJPG )
		{
			//	Allocation & Save Parameter in the decoder handle.
			if( 0 != AllocateDecoderMemory( hDec ) )
			{
				ret = VID_ERR_NOT_ALLOC_BUFF;
				NX_ErrMsg(("AllocateDecoderMemory() Failed!!!\n"));
				goto ERROR_EXIT;
			}

			//	Set Frame Argement Valiable
			frameArg.numFrameBuffer = hDec->numFrameBuffers;
			for( i=0 ; i< hDec->numFrameBuffers ; i++ )
			{
				if( hDec->useExternalFrameBuffer )
					hDec->hFrameBuffer[i] = pstSeqIn->pMemHandle[i];
				frameArg.frameBuffer[i] = *hDec->hFrameBuffer[i];
			}
			if( hDec->hSliceBuffer )
				frameArg.sliceBuffer = *hDec->hSliceBuffer;
			if( hDec->hColMvBuffer)
				frameArg.colMvBuffer = *hDec->hColMvBuffer;
			if( hDec->hPvbSliceBuffer )
				frameArg.pvbSliceBuffer = *hDec->hPvbSliceBuffer;

			ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_REG_FRAME_BUF, &frameArg );
			if( ret < 0 )
			{
				NX_ErrMsg( ("IOCTL_VPU_DEC_REG_FRAME_BUF ioctl failed!!!\n") );
				goto ERROR_EXIT;
			}
		}
		else
		{
			if ( (pstSeqIn->width) && (pstSeqIn->height) )
			{
				if ( (pstSeqIn->width != hDec->width) || (pstSeqIn->height != hDec->height) )
				{
					hDec->thumbnailMode = 1;
					hDec->width = pstSeqIn->width;
					hDec->height = pstSeqIn->height;
				}
			}

			ret = DecoderJpegHeader( hDec, pstSeqIn->seqInfo, pstSeqIn->seqSize );
			if ( ret < 0 )
			{
				NX_ErrMsg( ("JpegDecodeHeader() failed(Return Error = %d)!!!\n", ret) );
				goto ERROR_EXIT;
			}

			//	Set Frame Argument Valiable
			for( i=0 ; i<hDec->numFrameBuffers ; i++ )
			{
				hDec->hFrameBuffer[i] = (!hDec->useExternalFrameBuffer) ? (NX_VideoAllocateMemory(4096, hDec->width, hDec->height, NX_MEM_MAP_LINEAR, hDec->imgFourCC)) : (pstSeqIn->pMemHandle[i]);
				if( hDec->hFrameBuffer[i] == NULL ){
					NX_ErrMsg(("Frame memory allocation(%d x %d) failed.(i=%d)\n", hDec->width, hDec->height, i));
					goto ERROR_EXIT;
				}
				hDec->frmBufferValid[i] = VID_ERR_NONE;
			}
		}
	}
#ifdef HEVC_DEC
	else
	{
		ret = NX_HevcDecInit( hDec, pstSeqIn );
	}
#endif

	hDec->isInitialized = 1;

ERROR_EXIT:
	FUNC_OUT();
	return ret;
}

VID_ERROR_E NX_VidDecDecodeFrame( NX_VID_DEC_HANDLE hDec, NX_VID_DEC_IN *pstDecIn, NX_VID_DEC_OUT *pstDecOut )
{
	int32_t ret = 0;
	uint64_t timeStamp = -1;
	VPU_DEC_DEC_FRAME_ARG decArg;

	FUNC_IN();

	//	Initialize Decoder
	if( !hDec->isInitialized  )
	{
		NX_ErrMsg( ("%s Line(%d) : Not initialized!!!\n", __func__, __LINE__));
		return -1;
	}

	memset( pstDecOut, 0, sizeof(NX_VID_DEC_OUT) );
	pstDecOut->outImgIdx = -1;

	if ( hDec->codecStd != CODEC_STD_HEVC )
	{
		memset( &decArg, 0, sizeof(decArg) );
		decArg.strmData = pstDecIn->strmBuf;
		decArg.strmDataSize = pstDecIn->strmSize;
		decArg.eos = pstDecIn->eos;
		decArg.iFrameSearchEnable = 0;
		decArg.skipFrameMode = 0;
		decArg.decSkipFrameNum = 0;

		if ( (hDec->codecStd == CODEC_STD_MPEG4) && (hDec->vopTimeBits > 0) && (decArg.strmDataSize > 0) )
			Mp4DecParserFrameHeader( hDec, &decArg );

		if ( (hDec->codecStd == CODEC_STD_AVC) && (pstDecIn->strmSize > 8) )
		{
			unsigned char *pbyTmp = decArg.strmData;
			if ( (pbyTmp[2] == 0) && ((pbyTmp[4]&0x1F) == 7) && (pbyTmp[7] > 51) )
				pbyTmp[7] = 51;
			else if ( (pbyTmp[2] == 1) && ((pbyTmp[3]&0x1F) == 7) && (pbyTmp[6] > 51) )
				pbyTmp[6] = 51;
		}

		if( hDec->codecStd == CODEC_STD_MJPG )
		{
			decArg.downScaleWidth = pstDecIn->downScaleWidth;
			decArg.downScaleHeight = pstDecIn->downScaleHeight;

			if ( hDec->fstFrame )
			{
				if ( DecoderJpegHeader( hDec, pstDecIn->strmBuf, pstDecIn->strmSize ) < 0 )
					return VID_ERR_WRONG_SEQ;
			}
			else
			{
				hDec->fstFrame = 1;
			}

			decArg.strmData  += hDec->headerSize;
			decArg.strmDataSize -= hDec->headerSize;

			//decArg.imgFourCC = hDec->imgFourCC;
			decArg.rstInterval = hDec->rstInterval;
			decArg.userHuffTable = hDec->userHuffTable;

			decArg.huffBits = hDec->huffBits;
			decArg.huffPtr = hDec->huffPtr;
			decArg.huffMin = hDec->huffMin;
			decArg.huffMax = hDec->huffMax;

			decArg.huffValue = hDec->huffValue;
			decArg.infoTable = hDec->infoTable;
			decArg.quantTable = hDec->quantTable;

			decArg.huffAcIdx = hDec->huffAcIdx;
			decArg.huffDcIdx = hDec->huffDcIdx;
			//hDec->qIdx;

			decArg.busReqNum = hDec->busReqNum;
			decArg.mcuBlockNum = hDec->mcuBlockNum;
			decArg.compNum = hDec->compNum;
			decArg.compInfo = hDec->compInfo;
			//hDec->mcuWidth;
			//hDec->mcuHeight;

			decArg.width = hDec->width;
			decArg.height = hDec->height;

			decArg.outRect.left = 0;
			decArg.outRect.right = hDec->width;
			decArg.outRect.top = 0;
			decArg.outRect.bottom = hDec->height;

			decArg.pagePtr = hDec->pagePtr;
			decArg.wordPtr = hDec->wordPtr;
			decArg.bitPtr = hDec->bitPtr;

			{
				int32_t i;
				for ( i = 1 ; i <= hDec->numFrameBuffers ; i++ )
				{
					int32_t Idx = hDec->decodeIdx + i;
					if ( Idx >= hDec->numFrameBuffers )
						Idx -= hDec->numFrameBuffers;

					if ( hDec->frmBufferValid[ Idx ] == VID_ERR_NONE )
					{
						hDec->decodeIdx = Idx;
						decArg.hCurrFrameBuffer = hDec->hFrameBuffer[Idx];
						decArg.indexFrameDecoded = Idx;
						decArg.indexFrameDisplay = Idx;
						break;
					}
				}

				if ( i > hDec->numFrameBuffers )
				{
					NX_ErrMsg( ("Frame Buffer for Decoding is not sufficient!!!\n" ) );
					return VID_ERR_NO_DEC_FRAME;
				}
			}
		}

#if 0
		if ( pstDecIn->strmSize > 0 )
		{
			uint8_t *pbyStrm = pstDecIn->strmBuf;
			uint32_t uPreFourByte = (uint32_t)-1;
			int32_t  iFrmNum;

			do
			{
				if ( pbyStrm >= (pstDecIn->strmBuf + pstDecIn->strmSize) )		return VID_ERR_NOT_ENOUGH_STREAM;
				uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;

				if ( uPreFourByte == 0x00000001 || uPreFourByte<<8 == 0x00000100 )
				{
					int32_t iNaluType = pbyStrm[0] & 0x1F;

					// Slice start code
					if ( iNaluType == 5 || iNaluType == 1 )
					{
						VLD_STREAM stStrm = { 8, pbyStrm, pstDecIn->strmSize };

						vld_get_uev(&stStrm);        // First_mb_in_slice
						vld_get_uev(&stStrm);        // Slice type
						vld_get_uev(&stStrm);        // PPS ID
						iFrmNum = vld_get_bits( &stStrm, 6 );
						if ( vld_get_bits( &stStrm, 1 ) )
							vld_flush_bits( &stStrm, 1 );

						if ( iNaluType == 5 )
							vld_get_uev(&stStrm);

						printf("FrmNum = %d, Lsb = %d, PTS = %lld \n", iFrmNum,  vld_get_bits( &stStrm, 7 ), pstDecIn->timeStamp );
						break;
					}
				}
			} while(1);
		}
#endif

		ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_RUN_FRAME, &decArg );
		if( ret != VID_ERR_NONE )
		{
			NX_ErrMsg( ("IOCTL_VPU_DEC_RUN_FRAME ioctl failed!!!(%d) \n", decArg.iRet ) );
			return (decArg.iRet);
		}

		if( hDec->codecStd == CODEC_STD_MJPG )
		{
			hDec->frmBufferValid[decArg.indexFrameDecoded] = VID_ERR_FAIL;
		}

		pstDecOut->outImgIdx = decArg.indexFrameDisplay;
		pstDecOut->outDecIdx = decArg.indexFrameDecoded;
		pstDecOut->width     = decArg.outRect.right;
		pstDecOut->height    = decArg.outRect.bottom;
		pstDecOut->picType[DECODED_FRAME] = ( decArg.picType != 7 ) ? ( decArg.picType ) : ( PIC_TYPE_UNKNOWN );

		pstDecOut->strmReadPos  = decArg.strmReadPos;
		pstDecOut->strmWritePos = decArg.strmWritePos;

		if ( pstDecOut->outDecIdx >= 0 )
		{
			if ( decArg.numOfErrMBs == 0 )
				pstDecOut->outFrmReliable_0_100[DECODED_FRAME] = ( pstDecOut->outDecIdx < 0 ) ? ( 0 ) : ( 100 );
			else
			{
				if ( hDec->codecStd != CODEC_STD_MJPG )
				{
					int32_t TotalMbNum = ( (decArg.outWidth + 15) >> 4 ) * ( (decArg.outHeight + 15) >> 4 );
					pstDecOut->outFrmReliable_0_100[DECODED_FRAME] = (TotalMbNum - decArg.numOfErrMBs) * 100 / TotalMbNum;
				}
				else
				{
					int32_t PosX = ((decArg.numOfErrMBs >> 12) & 0xFFF) * hDec->mcuWidth;
					int32_t PosY = (decArg.numOfErrMBs & 0xFFF) * hDec->mcuHeight;
					int32_t PosRst= ((decArg.numOfErrMBs >> 24) & 0xF) * hDec->mcuWidth * hDec->mcuHeight;
					pstDecOut->outFrmReliable_0_100[DECODED_FRAME] = (PosRst + (PosY * hDec->width) + PosX) * 100 / (hDec->width * hDec->height);
				}
			}
		}

		if ( hDec->codecStd != CODEC_STD_MJPG )
		{
			if ( pstDecIn->strmSize > 0 )
			{
				uint32_t strmWritePos = ( decArg.strmWritePos >= (uint32_t)pstDecIn->strmSize ) ? ( decArg.strmWritePos ) : ( decArg.strmWritePos + hDec->hBitStreamBuf->size );
				uint32_t strmReadPos  = ( decArg.strmReadPos >= hDec->prevStrmReadPos ) ? ( decArg.strmReadPos ) : ( decArg.strmReadPos + hDec->hBitStreamBuf->size );

				if ( strmWritePos < strmReadPos )
					strmWritePos += hDec->hBitStreamBuf->size;

				if ( (strmReadPos + pstDecIn->strmSize) > strmWritePos )
				{
					timeStamp = pstDecIn->timeStamp;
				}
				else
				{
					timeStamp = hDec->savedTimeStamp;
					hDec->savedTimeStamp = pstDecIn->timeStamp;
				}

				hDec->prevStrmReadPos = decArg.strmReadPos;
			}
			else
			{
				//if ( (decArg.strmWritePos - decArg.strmReadPos < 4) || (hDec->hBitStreamBuf->size + decArg.strmWritePos - decArg.strmReadPos < 4) )
				if ( (hDec->savedTimeStamp == (uint64_t)-1) && (decArg.strmWritePos - decArg.strmReadPos < 4) )
				{
					timeStamp = pstDecIn->timeStamp;
				}
				else
				{
					timeStamp = hDec->savedTimeStamp;
					hDec->savedTimeStamp = pstDecIn->timeStamp;
				}
			}
		}
		else
			timeStamp = pstDecIn->timeStamp;

		DecoderPutDispInfo( hDec, pstDecOut->outDecIdx, &decArg, timeStamp, pstDecOut->outFrmReliable_0_100[DECODED_FRAME] );

		if( (pstDecOut->outImgIdx >= 0) && (pstDecOut->outImgIdx < hDec->numFrameBuffers) )
		{
			int32_t iIdx = pstDecOut->outImgIdx;
			pstDecOut->outImg = *hDec->hFrameBuffer[ iIdx ];
			pstDecOut->timeStamp[FIRST_FIELD] = hDec->timeStamp[ iIdx ][ FIRST_FIELD ];
			pstDecOut->timeStamp[SECOND_FIELD] = ( hDec->timeStamp[ iIdx ][ SECOND_FIELD ] != (uint64_t)(-10) ) ? ( hDec->timeStamp[ iIdx ][ SECOND_FIELD ] ) : ( (uint64_t)(-1) );
			pstDecOut->picType[DISPLAY_FRAME] = hDec->picType[ iIdx ];
			pstDecOut->outFrmReliable_0_100[DISPLAY_FRAME] = hDec->FrmReliable_0_100[ iIdx ];
			pstDecOut->isInterlace = hDec->isInterlace[ iIdx ];
			pstDecOut->topFieldFirst = hDec->topFieldFirst[ iIdx ];
			pstDecOut->multiResolution = hDec->multiResolution[ iIdx ];
			pstDecOut->upSampledWidth = hDec->upSampledWidth[ iIdx ];
			pstDecOut->upSampledHeight = hDec->upSampledHeight[ iIdx ];

			if ( hDec->codecStd == CODEC_STD_VC1 )
			{
				if ( pstDecOut->timeStamp[ FIRST_FIELD] == (uint64_t)-10 )
					pstDecOut->timeStamp[ FIRST_FIELD] = (uint64_t)-1;
				if ( pstDecOut->timeStamp[SECOND_FIELD] == (uint64_t)-10 )
					pstDecOut->timeStamp[SECOND_FIELD] = (uint64_t)-1;
			}

			hDec->timeStamp[ iIdx ][ FIRST_FIELD] = (uint64_t)-10;
			hDec->timeStamp[ iIdx ][SECOND_FIELD] = (uint64_t)-10;
			hDec->FrmReliable_0_100[ iIdx ] = 0;

#if DBG_BUF_INFO
			// {
			// 	int32_t j=0;
			// 	NX_MEMORY_INFO *memInfo;
			// 	for( j=0 ; j<3 ; j++ )
			// 	{
			// 		memInfo = (NX_MEMORY_INFO *)pDecOut->outImg.privateDesc[j];
			// 		NX_DbgMsg( DBG_BUF_INFO, ("privateDesc = 0x%.8x\n", memInfo->privateDesc ) );
			// 		NX_DbgMsg( DBG_BUF_INFO, ("align       = 0x%.8x\n", memInfo->align       ) );
			// 		NX_DbgMsg( DBG_BUF_INFO, ("size        = 0x%.8x\n", memInfo->size        ) );
			// 		NX_DbgMsg( DBG_BUF_INFO, ("virAddr     = 0x%.8x\n", memInfo->virAddr     ) );
			// 		NX_DbgMsg( DBG_BUF_INFO, ("phyAddr     = 0x%.8x\n", memInfo->phyAddr     ) );
			// 	}
			// }
			{
				NX_VID_MEMORY_INFO *memInfo = &pDecOut->outImg;
				NX_DbgMsg( DBG_BUF_INFO, ("Phy(0x%08x,0x%08x,0x%08x), Vir(0x%08x,0x%08x,0x%08x), Stride(0x%08x,0x%08x,0x%08x)\n",
					memInfo->luPhyAddr, memInfo->cbPhyAddr, memInfo->crPhyAddr,
					memInfo->luVirAddr, memInfo->cbVirAddr, memInfo->crVirAddr,
					memInfo->luStride , memInfo->cbStride , memInfo->crStride) );
			}
#endif
		}
		else
		{
			pstDecOut->timeStamp[FIRST_FIELD] = -1;
			pstDecOut->timeStamp[SECOND_FIELD] = -1;
		}
	}
#ifdef HEVC_DEC
	else
	{
		ret = NX_HevcDecDecodeFrame( hDec, pstDecIn, pstDecOut );
	}
#endif

	NX_RelMsg( 0, ("NX_VidDecDecodeFrame() Resol:%dx%d, picType=%d, %d, imgIdx = %d\n", pstDecOut->width, pstDecOut->height, pstDecOut->picType[DECODED_FRAME], pstDecOut->picType[DISPLAY_FRAME], pstDecOut->outImgIdx) );
	FUNC_OUT();
	return ret;
}

VID_ERROR_E NX_VidDecFlush( NX_VID_DEC_HANDLE hDec )
{
	int32_t ret;
	FUNC_IN();

	if( !hDec->isInitialized  )
	{
		NX_ErrMsg( ("%s Line(%d) : Not initialized!!!\n", __func__, __LINE__));
		return -1;
	}

	if ( hDec->codecStd != CODEC_STD_HEVC )
	{
		if ( hDec->codecStd != CODEC_STD_MJPG )
		{
			ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_FLUSH, NULL );
			if( ret < 0 )
			{
				NX_ErrMsg( ("IOCTL_VPU_DEC_FLUSH ioctl failed!!!\n") );
				return -1;
			}
		}
		else
		{
			int32_t i;
			for ( i = 0 ; i < hDec->numFrameBuffers ; i++ )
				hDec->frmBufferValid[ i ] = VID_ERR_NONE;
		}

		DecoderFlushDispInfo( hDec );
	}
#ifdef HEVC_DEC	
	else
	{
		int32_t i;
		NX_VID_MEMORY_HANDLE hRaminBuffer = NULL;;

		//
		//	HEVC Flush
		//
		IV_API_CALL_STATUS_T status;
		ivd_ctl_flush_ip_t s_video_flush_ip;
		ivd_ctl_flush_op_t s_video_flush_op;

		ivd_video_decode_ip_t s_video_decode_ip;
		ivd_video_decode_op_t s_video_decode_op;

		s_video_flush_ip.e_cmd = IVD_CMD_VIDEO_CTL;
		s_video_flush_ip.e_sub_cmd = IVD_CMD_CTL_FLUSH;
		s_video_flush_ip.u4_size = sizeof(ivd_ctl_flush_ip_t);
		s_video_flush_op.u4_size = sizeof(ivd_ctl_flush_op_t);
		
		/* Set the decoder in Flush mode, subsequent decode() calls will flush */
		status = ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_video_flush_ip, (void *)&s_video_flush_op );

		if (status != IV_SUCCESS) {
			NX_ErrMsg( ("Error in setting the decoder in flush mode: (%d) 0x%x\n", 
				status, s_video_flush_op.u4_error_code) );
			return -1;
		}

		//	
		//	Dummy Decoding
		//
		for( i = 0; i < hDec->numFrameBuffers; i++ )
		{
			if( hDec->validFrame[ i ] == 0 )
			{
				hRaminBuffer = hDec->hFrameBuffer[i];
				break;
			}
		}

		if( hRaminBuffer == NULL )
		{
			NX_ErrMsg( ("Error, Valid Remain Buffer.\n") );
			hRaminBuffer = hDec->hFrameBuffer[0];
		}

		while( 1 )
		{
			s_video_decode_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
			s_video_decode_ip.u4_ts = 0;
			s_video_decode_ip.pv_stream_buffer = NULL;
			s_video_decode_ip.u4_num_Bytes = 0;
			s_video_decode_ip.u4_size = sizeof(ivd_video_decode_ip_t);

			s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[0] = hRaminBuffer->luStride * hRaminBuffer->imgHeight;
			s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[1] = hRaminBuffer->cbStride * hRaminBuffer->imgHeight/2;
			s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[2] = hRaminBuffer->crStride * hRaminBuffer->imgHeight/2;
			s_video_decode_ip.s_out_buffer.pu1_bufs[0] = (uint8_t *)hRaminBuffer->luVirAddr;
			s_video_decode_ip.s_out_buffer.pu1_bufs[1] = (uint8_t *)hRaminBuffer->cbVirAddr;
			s_video_decode_ip.s_out_buffer.pu1_bufs[2] = (uint8_t *)hRaminBuffer->crVirAddr;
			s_video_decode_ip.s_out_buffer.u4_num_bufs = 3;

			s_video_decode_op.u4_size = sizeof(ivd_video_decode_op_t);

			ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_video_decode_ip, (void *)&s_video_decode_op);
			if( 0 == s_video_decode_op.u4_output_present )
				break;
		}

		//
		//	Reset ValidFrame
		//
		for ( i = 0 ; i < hDec->numFrameBuffers ; i++ )
		{
			if( hDec->validFrame[ i ] )
			{
				hDec->validFrame[ i ] = 0;
			}
		}
	}
#endif

	FUNC_OUT();
	return VID_ERR_NONE;
}

VID_ERROR_E NX_VidDecClrDspFlag( NX_VID_DEC_HANDLE hDec, NX_VID_MEMORY_HANDLE hFrameBuf, int32_t iFrameIdx )
{
	int32_t ret;
	FUNC_IN();
	VPU_DEC_CLR_DSP_FLAG_ARG clrFlagArg;

	if( !hDec->isInitialized  )
	{
		return -1;
	}

	if ( hDec->codecStd == CODEC_STD_MJPG )
	{
		hDec->frmBufferValid[ iFrameIdx ] = VID_ERR_NONE;
		return VID_ERR_NONE;
	}

	if ( hDec->codecStd != CODEC_STD_HEVC )
	{
		clrFlagArg.indexFrameDisplay = iFrameIdx;
		if( NULL != hFrameBuf )
		{
			//	Optional
			clrFlagArg.frameBuffer = *hFrameBuf;
		}
		else
		{
			memset( &clrFlagArg.frameBuffer, 0, sizeof(clrFlagArg.frameBuffer) );
		}

		ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_CLR_DSP_FLAG, &clrFlagArg );
		if( ret < 0 )
		{
			NX_ErrMsg( ("IOCTL_VPU_DEC_CLR_DSP_FLAG ioctl failed!!!\n") );
			return -1;
		}
	}
#ifdef HEVC_DEC
	else
	{
		int32_t iIdx;

		if ( iFrameIdx >= 0 )
		{
			iIdx = iFrameIdx;
		}
		else
		{
			int32_t i;
			for ( i=0 ; i<hDec->numFrameBuffers ; i++ )
			{
				if ( hDec->hFrameBuffer[ i ]->luVirAddr == hFrameBuf->luVirAddr )
					break;
			}
			iIdx = i;
		}

		if ( hDec->validFrame[ iIdx ] == 1 )
		{
			hDec->validFrame[ iIdx ] = 0;
		}
		else
		{
			NX_ErrMsg( ("CLR_DSP_FLAG ioctl failed!!!\n") );
			return VID_ERR_PARAM;
		}
	}
#endif

	FUNC_OUT();
	return VID_ERR_NONE;
}

// Optional Function
VID_ERROR_E NX_VidDecGetFrameType( VID_TYPE_E eCodecType, NX_VID_DEC_IN *pstDecIn, int32_t *piFrameType )
{
	uint8_t *pbyStrm = pstDecIn->strmBuf;
	uint32_t uPreFourByte = (uint32_t)-1;
	int32_t  iFrmType = PIC_TYPE_UNKNOWN;

	if ( (pbyStrm == NULL) || (piFrameType == NULL) )
		return VID_ERR_PARAM;

	if ( eCodecType == NX_AVC_DEC )
	{
		do
		{
			if ( pbyStrm >= (pstDecIn->strmBuf + pstDecIn->strmSize) )		return VID_ERR_NOT_ENOUGH_STREAM;
			uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;

			if ( uPreFourByte == 0x00000001 || uPreFourByte<<8 == 0x00000100 )
			{
				int32_t iNaluType = pbyStrm[0] & 0x1F;

				// Slice start code
				if ( iNaluType == 5 )
				{
					//vld_get_uev(&stStrm);                 // First_mb_in_slice
					iFrmType = PIC_TYPE_IDR;
					break;
				}
				else if ( iNaluType == 1 )
				{
					VLD_STREAM stStrm = { 8, pbyStrm, pstDecIn->strmSize };
					vld_get_uev(&stStrm);                   // First_mb_in_slice
					iFrmType = vld_get_uev(&stStrm);        // Slice type

					if ( iFrmType == 0 || iFrmType == 5 ) 		iFrmType = PIC_TYPE_P;
					else if ( iFrmType == 1 || iFrmType == 6 ) iFrmType = PIC_TYPE_B;
					else if ( iFrmType == 2 || iFrmType == 7 ) iFrmType = PIC_TYPE_I;
					break;
				}
			}
		} while(1);
	}
	else if ( eCodecType == NX_MP2_DEC )
	{
		do
		{
			if ( pbyStrm >= (pstDecIn->strmBuf + pstDecIn->strmSize) )		return VID_ERR_NOT_ENOUGH_STREAM;
			uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;

			// Picture start code
			if ( uPreFourByte == 0x00000100 )
			{
				VLD_STREAM stStrm = { 0, pbyStrm, pstDecIn->strmSize };

				vld_flush_bits( &stStrm, 10 );				// tmoporal_reference
				iFrmType = vld_get_bits( &stStrm, 3 );		// picture_coding_type

				if ( iFrmType == 1 ) 		iFrmType = PIC_TYPE_I;
				else if ( iFrmType == 2 ) 	iFrmType = PIC_TYPE_P;
				else if ( iFrmType == 3 ) 	iFrmType = PIC_TYPE_B;
				break;
			}
		} while(1);
	}
	else
	{
		return VID_ERR_NOT_SUPPORT;
	}

	*piFrameType = iFrmType;
	return VID_ERR_NONE;
}


//
//	Video Decoder APIs
//
//////////////////////////////////////////////////////////////////////////////

VID_ERROR_E NX_VidGetVersion( NX_VID_VERSION *pstVersion )
{
	pstVersion->iMajor = NX_VID_VER_MAJOR;
	pstVersion->iMinor = NX_VID_VER_MINOR;
	pstVersion->iPatch = NX_VID_VER_PATCH;
	pstVersion->iReserved = (int32_t)NULL;
	return VID_ERR_NONE;
}


//////////////////////////////////////////////////////////////////////////////
//
//	Static Internal Functions
//

static int32_t AllocateEncoderMemory( NX_VID_ENC_HANDLE hEnc )
{
	int32_t width, height;

	if( !hEnc || hEnc->hEncDrv<=0 )
	{
		NX_ErrMsg(("invalid encoder handle or driver handle!!!\n"));
		return -1;
	}

	//	Make alligned x16
	width  = ((hEnc->width  + 15) >> 4)<<4;
	height = ((hEnc->height + 15) >> 4)<<4;

	if( hEnc->codecMode == NX_JPEG_ENC )
	{
		int32_t jpegStreamBufSize = width * height * 1.5;
		hEnc->hRefReconBuf[0] = NULL;
		hEnc->hRefReconBuf[1] = NULL;
		hEnc->hSubSampleBuf[0] = NULL;
		hEnc->hSubSampleBuf[1] = NULL;
		hEnc->hBitStreamBuf = NX_AllocateMemory( jpegStreamBufSize, 16 );		//	x16 aligned
		if( 0 == hEnc->hBitStreamBuf ){
			NX_ErrMsg(("hBitStreamBuf allocation failed.(size=%d,align=%d)\n", ENC_BITSTREAM_BUFFER, 16));
			goto ERROR_EXIT;
		}
	}
	else
	{
		int32_t fourcc = FOURCC_MVS0;
		if( hEnc->refChromaInterleave ){
			fourcc = FOURCC_NV12;	//	2 Planar 420( Luminunce Plane + Cb/Cr Interleaved Plane )
		}
		else
		{
			fourcc = FOURCC_MVS0;	//	3 Planar 420( Luminunce plane + Cb Plane + Cr Plane )
		}

		hEnc->hRefReconBuf[0] = NX_VideoAllocateMemory( 64, width, height, NX_MEM_MAP_LINEAR, fourcc );
		if( 0 == hEnc->hRefReconBuf[0] ){
			NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(recon0)\n", width, height));
			goto ERROR_EXIT;
		}

		hEnc->hRefReconBuf[1] = NX_VideoAllocateMemory( 64, width, height, NX_MEM_MAP_LINEAR, fourcc );
		if( 0 == hEnc->hRefReconBuf[1] ){
			NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(recon1)\n", width, height));
			goto ERROR_EXIT;
		}

		hEnc->hSubSampleBuf[0] = NX_AllocateMemory( width*height/4, 16 );	//	x16 aligned
		if( 0 == hEnc->hSubSampleBuf[0] ){
			NX_ErrMsg(("hSubSampleBuf allocation failed.(size=%d,align=%d)\n", width*height, 16));
			goto ERROR_EXIT;
		}

		hEnc->hSubSampleBuf[1] = NX_AllocateMemory( width*height/4, 16 );	//	x16 aligned
		if( 0 == hEnc->hSubSampleBuf[1] ){
			NX_ErrMsg(("hSubSampleBuf allocation failed.(size=%d,align=%d)\n", width*height, 16));
			goto ERROR_EXIT;
		}

		hEnc->hBitStreamBuf = NX_AllocateMemory( ENC_BITSTREAM_BUFFER, 16 );		//	x16 aligned
		if( 0 == hEnc->hBitStreamBuf ){
			NX_ErrMsg(("hBitStreamBuf allocation failed.(size=%d,align=%d)\n", ENC_BITSTREAM_BUFFER, 16));
			goto ERROR_EXIT;
		}
	}

#if (DBG_BUF_ALLOC)
	NX_DbgMsg( DBG_BUF_ALLOC, ("Allocate Encoder Memory\n") );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hRefReconBuf[0]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", hEnc->hRefReconBuf[0]->luPhyAddr, hEnc->hRefReconBuf[0]->cbPhyAddr, hEnc->hRefReconBuf[0]->crPhyAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hRefReconBuf[1]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", hEnc->hRefReconBuf[1]->luPhyAddr, hEnc->hRefReconBuf[1]->cbPhyAddr, hEnc->hRefReconBuf[1]->crPhyAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hSubSampleBuf[0] : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hSubSampleBuf[0]->phyAddr, hEnc->hSubSampleBuf[0]->virAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hSubSampleBuf[1] : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hSubSampleBuf[1]->phyAddr, hEnc->hSubSampleBuf[1]->virAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hBitStreamBuf    : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hBitStreamBuf->phyAddr, hEnc->hBitStreamBuf->virAddr) );
#endif	//	DBG_BUF_ALLOC

	return 0;

ERROR_EXIT:
	FreeEncoderMemory( hEnc );
	return -1;
}

static int32_t FreeEncoderMemory( NX_VID_ENC_HANDLE hEnc )
{
	if( !hEnc )
	{
		NX_ErrMsg(("invalid encoder handle!!!\n"));
		return -1;
	}

	//	Free Reconstruct Buffer & Reference Buffer
	if( hEnc->hRefReconBuf[0] )
	{
		NX_FreeVideoMemory( hEnc->hRefReconBuf[0] );
		hEnc->hRefReconBuf[0] = 0;
	}
	if( hEnc->hRefReconBuf[1] )
	{
		NX_FreeVideoMemory( hEnc->hRefReconBuf[1] );
		hEnc->hRefReconBuf[1] = 0;
	}

	//	Free SubSampleb Buffer
	if( hEnc->hSubSampleBuf[0] )
	{
		NX_FreeMemory( hEnc->hSubSampleBuf[0] );
		hEnc->hSubSampleBuf[0] = 0;
	}
	if( hEnc->hSubSampleBuf[1] )
	{
		NX_FreeMemory( hEnc->hSubSampleBuf[1] );
		hEnc->hSubSampleBuf[1] = 0;
	}

	//	Free Bitstream Buffer
	if( hEnc->hBitStreamBuf )
	{
		NX_FreeMemory( hEnc->hBitStreamBuf );
		hEnc->hBitStreamBuf = 0;
	}

	if( hEnc->hInstanceBuf )
	{
		NX_FreeMemory(hEnc->hInstanceBuf);
		hEnc->hInstanceBuf = 0;
	}

	return 0;
}

static int32_t AllocateDecoderMemory( NX_VID_DEC_HANDLE hDec )
{
	int32_t i, width, height, mvSize;

	if( !hDec || !hDec->hDecDrv )
	{
		NX_ErrMsg(("invalid encoder handle or driver handle!!!\n"));
		return -1;
	}

	//	Make alligned x16
	width  = ((hDec->width  + 15) >> 4)<<4;
	height = ((hDec->height + 15) >> 4)<<4;

	//
	mvSize = ((hDec->width+31)&~31)*((hDec->height+31)&~31);
	mvSize = (mvSize*3)/2;
	mvSize = (mvSize+4) / 5;
	mvSize = ((mvSize+7)/ 8) * 8;
	mvSize = ((mvSize + 4096-1)/4096) * 4096;

	if( width==0 || height==0 || mvSize==0 )
	{
		NX_ErrMsg(("Invalid memory parameters!!!(width=%d, height=%d, mvSize=%d)\n", width, height, mvSize));
		return -1;
	}

	if( !hDec->useExternalFrameBuffer )
	{
		NX_RelMsg( 1, ( "resole : %dx%d, numFrameBuffers=%d\n", width, height, hDec->numFrameBuffers ));
		for( i=0 ; i<hDec->numFrameBuffers ; i++ )
		{
			hDec->hFrameBuffer[i] = NX_VideoAllocateMemory( 4096, width, height, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );	//	Planar Lu/Cb/Cr
			if( 0 == hDec->hFrameBuffer[i] ){
				NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(i=%d)\n", width, height, i));
				goto ERROR_EXIT;
			}
		}
	}

	hDec->hColMvBuffer = NX_AllocateMemory( mvSize*hDec->numFrameBuffers, 4096 );	//	Planar Lu/Cb/Cr
	if( 0 == hDec->hColMvBuffer ){
		NX_ErrMsg(("hColMvBuffer allocation failed(size=%d, align=%d)\n", mvSize*hDec->numFrameBuffers, 4096));
		goto ERROR_EXIT;
	}

	if( hDec->codecStd == CODEC_STD_AVC )
	{
		hDec->hSliceBuffer = NX_AllocateMemory( 2048*2048*3/4, 4096 );		//	x16 aligned
		if( 0 == hDec->hSliceBuffer ){
			NX_ErrMsg(("hSliceBuffer allocation failed.(size=%d,align=%d)\n",  2048*2048*3/4, 4096));
			goto ERROR_EXIT;
		}
	}

	if( hDec->codecStd == CODEC_STD_THO || hDec->codecStd == CODEC_STD_VP3 || hDec->codecStd == CODEC_STD_VP8 )
	{
		hDec->hPvbSliceBuffer = NX_AllocateMemory( 17*4*(2048*2048/256), 4096 );		//	x16 aligned
		if( 0 == hDec->hPvbSliceBuffer ){
			NX_ErrMsg(("hPvbSliceBuffer allocation failed.(size=%d,align=%d)\n", 17*4*(2048*2048/256), 4096));
			goto ERROR_EXIT;
		}
	}

#if DBG_BUF_ALLOC
	NX_DbgMsg( DBG_BUF_ALLOC, ("Allocate Decoder Memory\n") );
	for( i=0 ; i<hDec->numFrameBuffers ; i++ )
	{
		NX_DbgMsg( DBG_BUF_ALLOC, ("    hFrameBuffer[%d]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", i, hDec->hFrameBuffer[i]->luPhyAddr, hDec->hFrameBuffer[i]->cbPhyAddr, hDec->hFrameBuffer[i]->crPhyAddr) );
		NX_DbgMsg( DBG_BUF_ALLOC, ("    hFrameBuffer[%d]  : LuVir(0x%08x), CbVir(0x%08x), CrVir(0x%08x)\n", i, hDec->hFrameBuffer[i]->luVirAddr, hDec->hFrameBuffer[i]->cbVirAddr, hDec->hFrameBuffer[i]->crVirAddr) );
	}
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hBitStreamBuf    : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hDec->hBitStreamBuf->phyAddr, hDec->hBitStreamBuf->virAddr) );
#endif	//	DBG_BUF_ALLOC

	return 0;

ERROR_EXIT:
	FreeDecoderMemory( hDec );
	return -1;
}

static int32_t FreeDecoderMemory( NX_VID_DEC_HANDLE hDec )
{
	int32_t i;
	if( !hDec )
	{
		NX_ErrMsg(("invalid encoder handle!!!\n"));
		return -1;
	}

	if( !hDec->useExternalFrameBuffer )
	{
		//	Free Frame Buffer
		for( i=0 ; i<hDec->numFrameBuffers ; i++ )
		{
			if( hDec->hFrameBuffer[i] )
			{
				NX_FreeVideoMemory( hDec->hFrameBuffer[i] );
				hDec->hFrameBuffer[i] = 0;
			}
		}
	}

	if( hDec->hColMvBuffer )
	{
		NX_FreeMemory( hDec->hColMvBuffer );
		hDec->hColMvBuffer = 0;
	}

	if( hDec->hSliceBuffer )
	{
		NX_FreeMemory( hDec->hSliceBuffer );
		hDec->hSliceBuffer = 0;
	}

	if( hDec->hPvbSliceBuffer )
	{
		NX_FreeMemory( hDec->hPvbSliceBuffer );
		hDec->hPvbSliceBuffer = 0;
	}

	//	Allocate Instance Memory & Stream Buffer
	if( hDec->hInstanceBuf )
	{
		NX_FreeMemory( hDec->hInstanceBuf );
		hDec->hInstanceBuf = 0;
	}

	//	Free Bitstream Buffer
	if( hDec->hBitStreamBuf )
	{
		NX_FreeMemory( hDec->hBitStreamBuf );
		hDec->hBitStreamBuf = 0;
	}

	//	Free USerdata Buffer
	if( hDec->hUserDataBuffer )
	{
		NX_FreeMemory( hDec->hUserDataBuffer );
		hDec->hUserDataBuffer = 0;
	}

	return 0;
}

static void DecoderFlushDispInfo( NX_VID_DEC_HANDLE hDec )
{
	int32_t i;
	for( i=0 ; i<MAX_DEC_FRAME_BUFFERS ; i++ )
	{
		hDec->timeStamp[i][FIRST_FIELD] = -10;
		hDec->timeStamp[i][SECOND_FIELD] = -10;
		hDec->picType[i] = -1;
		hDec->picFlag[i] = -1;
		hDec->multiResolution[i] = 0;
		hDec->isInterlace[i] = -1;
		hDec->topFieldFirst[i] = -1;
		hDec->FrmReliable_0_100[i] = 0;
		hDec->upSampledWidth[i] = 0;
		hDec->upSampledHeight[i] = 0;
	}
	hDec->savedTimeStamp = -1;
}

static void DecoderPutDispInfo( NX_VID_DEC_HANDLE hDec, int32_t iIndex, VPU_DEC_DEC_FRAME_ARG *pDecArg, uint64_t lTimeStamp, int32_t FrmReliable_0_100 )
{
	int32_t iPicStructure;

	if ( iIndex < 0 )
		return;

	iPicStructure = ( hDec->codecStd != CODEC_STD_MPEG2 ) ? ( 0 ) : ( pDecArg->picStructure );

	hDec->picType[ iIndex ] = pDecArg->picType;
	hDec->isInterlace[ iIndex ] = pDecArg->isInterace;

	if( (pDecArg->isInterace == 0) || (iPicStructure == 3) )
	{
		hDec->topFieldFirst[ iIndex ] = pDecArg->topFieldFirst;
		hDec->timeStamp[ iIndex ][ NONE_FIELD ] = lTimeStamp;
	}
	else
	{
		hDec->picFlag[ iIndex ] |= PIC_FLAG_INTERLACE;

		if ( hDec->timeStamp[iIndex][FIRST_FIELD] == (uint64_t)(-10) )
		{
			hDec->topFieldFirst[ iIndex ] = pDecArg->topFieldFirst;
			hDec->timeStamp[ iIndex ][ FIRST_FIELD ] = lTimeStamp;
		}
		else
		{
			hDec->timeStamp[ iIndex ][ SECOND_FIELD ] = lTimeStamp;
		}
	}

	if ( hDec->FrmReliable_0_100[ iIndex ] != 0 )
	{
		hDec->FrmReliable_0_100[ iIndex ] += ( FrmReliable_0_100 >> 1 );
	}
	else if ( (hDec->codecStd == CODEC_STD_AVC) || (hDec->codecStd == CODEC_STD_MPEG2) )
	{
		hDec->FrmReliable_0_100[ iIndex ] = ( (pDecArg->isInterace == 0) || (pDecArg->npf) || (iPicStructure == 3) ) ? ( FrmReliable_0_100 ) : ( FrmReliable_0_100 >> 1 );
	}
	else
	{
		hDec->FrmReliable_0_100[ iIndex ] = FrmReliable_0_100;
	}

	if( hDec->codecStd == CODEC_STD_AVC )
	{
		if( pDecArg->picTypeFirst == 6 || pDecArg->picType == 0 || pDecArg->picType == 6 )
		{
			hDec->picFlag[ iIndex ] |= PIC_FLAG_KEY;
		}
	}
	else if( hDec->codecStd == CODEC_STD_VC1 )
	{
		hDec->multiResolution[ iIndex ] = pDecArg->multiRes;
	}
	else if (hDec->codecStd == CODEC_STD_VP8)
	{
		if (pDecArg->vp8ScaleInfo.hScaleFactor == 0)       hDec->upSampledWidth[ iIndex ] = 0;
		else if (pDecArg->vp8ScaleInfo.hScaleFactor == 1) hDec->upSampledWidth[ iIndex ] = pDecArg->vp8ScaleInfo.picWidth * 5 / 4;
		else if (pDecArg->vp8ScaleInfo.hScaleFactor == 2) hDec->upSampledWidth[ iIndex ] = pDecArg->vp8ScaleInfo.picWidth * 5 / 3;
		else if (pDecArg->vp8ScaleInfo.hScaleFactor == 3) hDec->upSampledWidth[ iIndex ] = pDecArg->vp8ScaleInfo.picWidth * 2;

		if (pDecArg->vp8ScaleInfo.vScaleFactor == 0)       hDec->upSampledHeight[ iIndex ] = 0;
		else if (pDecArg->vp8ScaleInfo.vScaleFactor == 1) hDec->upSampledHeight[ iIndex ] = pDecArg->vp8ScaleInfo.picHeight * 5 / 4;
		else if (pDecArg->vp8ScaleInfo.vScaleFactor == 2) hDec->upSampledHeight[ iIndex ] = pDecArg->vp8ScaleInfo.picHeight * 5 / 3;
		else if (pDecArg->vp8ScaleInfo.vScaleFactor == 3) hDec->upSampledHeight[ iIndex ] = pDecArg->vp8ScaleInfo.picHeight * 2;
	}
}

static void Mp4DecParserVideoCfg( NX_VID_DEC_HANDLE hDec )
{
	uint8_t *pbyStrm = hDec->pSeqData;
	uint32_t uPreFourByte = (uint32_t)-1;

	hDec->vopTimeBits = 0;

	do
	{
		if ( pbyStrm >= (hDec->pSeqData + hDec->seqDataSize) )
		{
			break;
		}
		uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;

		if ( uPreFourByte >= 0x00000120 && uPreFourByte <= 0x0000012F )
		{
			VLD_STREAM stStrm = { 0, pbyStrm, hDec->seqDataSize };
			int32_t    i;

			vld_flush_bits( &stStrm, 1+8 );									// random_accessible_vol, video_object_type_indication
			if (vld_get_bits( &stStrm, 1 ))									// is_object_layer_identifier
				vld_flush_bits( &stStrm, 4 + 3 );							// video_object_layer_verid, video_object_layer_priority

			if (vld_get_bits( &stStrm, 4 ) == 0xF )							// aspect_ratio_info
				vld_flush_bits( &stStrm, 8+8 );								// par_width, par_height

			if (vld_get_bits( &stStrm, 1)) 									// vol_control_parameters
			{
				if (vld_get_bits( &stStrm, 2+1+1 ) & 1) 					// chroma_format, low_delay, vbv_parameters
				{
	                vld_flush_bits( &stStrm, 15+1 );						// first_half_bit_rate, marker_bit
	                vld_flush_bits( &stStrm, 15+1 );						// latter_half_bit_rate, marker_bit
	                vld_flush_bits( &stStrm, 15+1 );						// first_half_vbv_buffer_size, marker_bit
	                vld_flush_bits( &stStrm, 3+11+1 );						// latter_half_vbv_buffer_size, first_half_vbv_occupancy, marker_bit
	                vld_flush_bits( &stStrm, 15+1 );						// latter_half_vbv_occupancy, marker_bit
	            }
	        }

			vld_flush_bits( &stStrm, 2+1);									// video_object_layer_shape, marker_bit

			for (i=0 ; i<16 ; i++)											// vop_time_increment_resolution
				if ( vld_get_bits( &stStrm, 1) )
					break;
			hDec->vopTimeBits = 16 - i;
			break;
		}
	} while(1);
}

static void Mp4DecParserFrameHeader ( NX_VID_DEC_HANDLE hDec, VPU_DEC_DEC_FRAME_ARG *pDecArg )
{
	uint8_t *pbyStrm = pDecArg->strmData;
	VLD_STREAM stStrm = { 0, pbyStrm, pDecArg->strmDataSize };

	if (vld_get_bits( &stStrm, 32 ) == 0x000001B6)
	{
		vld_flush_bits( &stStrm, 2 );								// vop_coding_type
		do
		{
			if ( vld_get_bits( &stStrm, 1 ) == 0 )	break;
		} while( stStrm.dwUsedBits < ((unsigned long)pDecArg->strmDataSize<<3) );
		vld_flush_bits( &stStrm, 1+hDec->vopTimeBits+1 );			// marker_bits, vop_time_increment, marker_bits

		if ( vld_get_bits( &stStrm, 1 ) == 0 )						// vop_coded
			pDecArg->strmDataSize = 0;
	}
}

static int32_t DecoderJpegHeader(	NX_VID_DEC_HANDLE hDec, uint8_t           *pbyStream, int32_t iSize )
{
	uint32_t   uPreFourByte = 0;
	uint8_t    *pbyStrm = pbyStream;
	int32_t    i, iStartCode;

	if ( (pbyStrm == NULL) || (iSize <= 0) )
		return -1;
	if ( (pbyStrm[0] != 0xFF) || (pbyStrm[1] != 0xD8) )	// SOI(Start Of Image) check
		return -1;

	pbyStrm += 2;
	do
	{
		if ( pbyStrm >= (pbyStream + iSize) )		return VID_ERR_NOT_ENOUGH_STREAM;
		uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;
		iStartCode   = uPreFourByte & 0xFFFF;

		if ( iStartCode == 0xFFC0 )						// SOF(Start of Frame)- Baseline DCT
		{
			VLD_STREAM stStrm = { 0, pbyStrm, iSize };
			if ( DecoderJpegSOF( hDec, &stStrm ) < 0 )
				return -1;
			pbyStrm += (stStrm.dwUsedBits >> 3);
		}
		else if ( iStartCode == 0xFFC4 )				// DHT(Define Huffman Table)
		{
			VLD_STREAM stStrm = { 0, pbyStrm, iSize };
			DecoderJpegDHT( hDec, &stStrm );
			pbyStrm += (stStrm.dwUsedBits >> 3);
		}
		else if ( iStartCode == 0xFFDA )				// SOS(Start Of Scan)
		{
			VLD_STREAM stStrm = { 0, pbyStrm, iSize };
			DecoderJpegSOS( hDec, &stStrm );
			pbyStrm += (stStrm.dwUsedBits >> 3);
			break;
		}
		else if ( iStartCode == 0xFFDB )				// DQT(Define Quantization Table)
		{
			VLD_STREAM stStrm = { 0, pbyStrm, iSize };
			if ( DecoderJpegDQT( hDec, &stStrm ) < 0 )
				return -1;
			pbyStrm += (stStrm.dwUsedBits >> 3);
		}
		else if ( iStartCode == 0xFFDD )				// DRI(Define Restart Interval)
		{
			hDec->rstInterval = (pbyStrm[2] << 8) | (pbyStrm[3]);
			pbyStrm += 4;
		}
		else if ( iStartCode == 0xFFD8 )				// SOI
		{
			if ( hDec->thumbnailMode == 0 )
			{
				do
				{
					if ( pbyStrm >= (pbyStream + iSize) )		return VID_ERR_NOT_ENOUGH_STREAM;
					uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;
					iStartCode   = uPreFourByte & 0xFFFF;
				}
				while ( iStartCode != 0xFFD9 );
			}
		}
		else if ( iStartCode == 0xFFD9 )				// EOI(End Of Image)
		{
			break;
		}
	} while( 1 );

	hDec->headerSize = (uint32_t)((long)pbyStrm - (long)pbyStream + 3);

	{
		int ecsPtr;

		ecsPtr = 0;
		//ecsPtr = hDec->headerSize;

		hDec->pagePtr = ecsPtr / 256;
		hDec->wordPtr = (ecsPtr % 256) / 4;
		if ( hDec->pagePtr & 1 )
			hDec->wordPtr += 64;
		if ( hDec->wordPtr & 1 )
			hDec->wordPtr -= 1;

		hDec->bitPtr = (ecsPtr % 4) * 8;
		if (((ecsPtr % 256) / 4) & 1 )
			hDec->bitPtr += 32;
	}

	// Generate Huffman table information
	for( i=0; i<4; i++ )
		GenerateJpegHuffmanTable( hDec, i );

	hDec->qIdx = (hDec->infoTable[0][3] << 2) | (hDec->infoTable[1][3] << 1) | (hDec->infoTable[2][3]);
	hDec->huffDcIdx = (hDec->infoTable[0][4] << 2) | (hDec->infoTable[1][4] << 1) | (hDec->infoTable[2][4]);
	hDec->huffAcIdx = (hDec->infoTable[0][5] << 2) | (hDec->infoTable[1][5] << 1) | (hDec->infoTable[2][5]);

	return 0;
}

static int32_t DecoderJpegSOF( NX_VID_DEC_HANDLE hDec, VLD_STREAM *pstStrm )
{
	int32_t iWidth, iHeight;
	int32_t iCompNum;
	int32_t iSampleFactor;
	int32_t i;

	vld_flush_bits( pstStrm, 16 );				// frame header length
	if ( vld_get_bits( pstStrm, 8 ) != 8 )		// sample precision
		return -1;

	iHeight = vld_get_bits( pstStrm, 16 );
	iWidth  = vld_get_bits( pstStrm, 16 );

	iCompNum = vld_get_bits( pstStrm, 8 ); 		// number of image components in frame
	if ( iCompNum > 3 )
		return -1;

	for (i=0 ; i<iCompNum ; i++)
	{
	    hDec->infoTable[i][0] = vld_get_bits( pstStrm, 8 );	// CompId
	    hDec->infoTable[i][1] = vld_get_bits( pstStrm, 4 );	// HSampligFactor
	    hDec->infoTable[i][2] = vld_get_bits( pstStrm, 4 );	// VSampligFactor
	    hDec->infoTable[i][3] = vld_get_bits( pstStrm, 8 );	// QTableDestSelector
	}

	if ( iCompNum == 1 )
	{
		hDec->imgFourCC = FOURCC_GRAY;
		hDec->width  = ( iWidth  + 7 ) & (~7);
		hDec->height = ( iHeight + 7 ) & (~7);
		hDec->busReqNum = 4;
		hDec->mcuBlockNum = 1;
		hDec->compNum = 1;
		hDec->compInfo[0] = 5;
		hDec->compInfo[1] = 0;
		hDec->compInfo[2] = 0;
		hDec->mcuWidth  = 8;
		hDec->mcuHeight = 8;
	}
	else if ( iCompNum == 3 )
	{
		iSampleFactor = ( (hDec->infoTable[0][1]&3) << 4 ) | ( hDec->infoTable[0][2]&3 );
		switch ( iSampleFactor )
		{
			case 0x11 :
				hDec->imgFourCC = FOURCC_MVS4;
				hDec->width  = ( iWidth  + 7 ) & (~7);
				hDec->height = ( iHeight + 7 ) & (~7);
				hDec->busReqNum = 4;
				hDec->mcuBlockNum = 3;
				hDec->compNum = 3;
				hDec->compInfo[0] = 5;
				hDec->compInfo[1] = 5;
				hDec->compInfo[2] = 5;
				hDec->mcuWidth  = 8;
				hDec->mcuHeight = 8;
				break;

			case 0x12 :
				hDec->imgFourCC = FOURCC_V422;
				hDec->width  = ( iWidth  + 7  ) & ( ~7);
				hDec->height = ( iHeight + 15 ) & (~15);
				hDec->busReqNum = 3;
				hDec->mcuBlockNum = 4;
				hDec->compNum = 3;
				hDec->compInfo[0] = 6;
				hDec->compInfo[1] = 5;
				hDec->compInfo[2] = 5;
				hDec->mcuWidth  = 8;
				hDec->mcuHeight = 16;
				break;

			case 0x21 :
				hDec->imgFourCC = FOURCC_H422;
				hDec->width  = ( iWidth  + 15 ) & (~15);
				hDec->height = ( iHeight +  7 ) & ( ~7);
				hDec->busReqNum = 3;
				hDec->mcuBlockNum = 4;
				hDec->compNum = 3;
				hDec->compInfo[0] = 9;
				hDec->compInfo[1] = 5;
				hDec->compInfo[2] = 5;
				hDec->mcuWidth  = 16;
				hDec->mcuHeight = 8;
				break;

			case 0x22 :
				hDec->imgFourCC = FOURCC_MVS0;
				hDec->width  = ( iWidth  + 15 ) & (~15);
				hDec->height = ( iHeight + 15 ) & (~15);
				hDec->busReqNum = 2;
				hDec->mcuBlockNum = 6;
				hDec->compNum = 3;
				hDec->compInfo[0] = 10;
				hDec->compInfo[1] = 5;
				hDec->compInfo[2] = 5;
				hDec->mcuWidth  = 16;
				hDec->mcuHeight = 16;
				break;
			default :
				return -1;
		}
	}

	return 0;
}

static void DecoderJpegDHT( NX_VID_DEC_HANDLE hDec, VLD_STREAM *pstStrm )
{
#if 1
	const uint8_t DefHuffmanBits[4][16] =	{
		{ 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },	// DC index 0 (Luminance DC)
		{ 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D },	// AC index 0 (Luminance AC)
		{ 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },	// DC index 1 (Chrominance DC)
		{ 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77 }	// AC index 1 (Chrominance AC)
	};

	const uint8_t DefHuffmanValue[4][162] =
	{
		{	// DC index 0 (Luminance DC)
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
		},
		{	// AC index 0 (Luminance AC)
			0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
			0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
			0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
			0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
			0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
			0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
			0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
			0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
			0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
			0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
			0xF9, 0xFA
		},
		{	// DC index 1 (Chrominance DC)
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
		},
		{	// AC index 1 (Chrominance AC)
			0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
			0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
			0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
			0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
			0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
			0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
			0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
			0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
			0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
			0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
			0xF9, 0xFA
		}
	};
#endif

	int32_t i;
	int32_t iLength;
	int32_t iTC, iTH, iTcTh;
	int32_t iBitCnt;

	iLength = vld_get_bits( pstStrm, 16 ) - 2;
	do
	{
		iTC = vld_get_bits( pstStrm, 4 );			// table class
		iTH = vld_get_bits( pstStrm, 4 );			// table destination identifier
		iTcTh = ( (iTH&1) << 1 ) | ( iTC&1 );

		// Get Huff Bits list
		iBitCnt = 0;
		for (i=0 ; i<16 ; i++)
		{
			hDec->huffBits[iTcTh][i] = vld_get_bits( pstStrm, 8 );
			iBitCnt += hDec->huffBits[iTcTh][i];
			if (DefHuffmanBits[iTcTh][i] != hDec->huffBits[iTcTh][i])
				hDec->userHuffTable = 1;
		}

		// Get Huff Val list
		for (i=0 ; i<iBitCnt ; i++)
		{
			hDec->huffValue[iTcTh][i] = vld_get_bits( pstStrm, 8 );
			if (DefHuffmanValue[iTcTh][i] != hDec->huffValue[iTcTh][i])
				hDec->userHuffTable = 1;
		}
	} while ( iLength > (pstStrm->dwUsedBits >> 3) );
}

static int32_t DecoderJpegDQT( NX_VID_DEC_HANDLE hDec, VLD_STREAM *pstStrm )
{
	int32_t i;
	int32_t iLength;
	int32_t iTQ;
	uint8_t *pbyQTable;

	iLength = vld_get_bits( pstStrm, 16 ) - 2;
	do
	{
		if ( vld_get_bits( pstStrm, 4 ) >= 1 )		// Pq
			return -1;

		if ( (iTQ = vld_get_bits( pstStrm, 4 )) > 3 )
			return -1;

		pbyQTable = hDec->quantTable[iTQ];

		for (i=0 ; i<64 ; i++)
		{
			if ( (pbyQTable[i] = vld_get_bits( pstStrm, 8 )) == 0 )
				return -1;
		}
	}
	while( iLength > (pstStrm->dwUsedBits >> 3) );

	return 0;
}

static void	DecoderJpegSOS( NX_VID_DEC_HANDLE hDec, VLD_STREAM *pstStrm )
{
	int32_t i, j;
	int32_t iCompNum, iCompId;
	int32_t iDcHuffTabIdx[3];
	int32_t iAcHuffTabIdx[3];

	vld_flush_bits( pstStrm, 16 );
	iCompNum = vld_get_bits( pstStrm, 8 );
	for( i=0 ; i<iCompNum ; i++ )
	{
		iCompId = vld_get_bits( pstStrm, 8 );
		iDcHuffTabIdx[i] = vld_get_bits( pstStrm, 4 );
		iAcHuffTabIdx[i] = vld_get_bits( pstStrm, 4 );

		for ( j=0 ; j<iCompNum ; j++ )
		{
			if ( iCompId == hDec->infoTable[j][0] )
			{
				hDec->infoTable[i][4] = iDcHuffTabIdx[i];
				hDec->infoTable[i][5] = iAcHuffTabIdx[i];
			}
		}
	}
}

static void GenerateJpegHuffmanTable( NX_VID_DEC_HANDLE hDec, int32_t iTabNum )
{
	int32_t iPtrCnt = 0;
	int32_t iHuffCode = 0;
	int32_t iZeroFlag = 0;
	int32_t iDataFlag = 0;
	int32_t i;

	uint8_t   *pbyHuffBits = hDec->huffBits[iTabNum];
	uint8_t   *pbyHuffPtr  = hDec->huffPtr[iTabNum];
	uint32_t  *uHuffMax    = hDec->huffMax[iTabNum];
	uint32_t  *uHuffMin    = hDec->huffMin[iTabNum];

	for (i=0; i<16; i++)
	{
		if ( pbyHuffBits[i] ) // if there is bit cnt value
		{
			pbyHuffPtr[i] = iPtrCnt;
			iPtrCnt += pbyHuffBits[i];
			uHuffMin[i] = iHuffCode;
			uHuffMax[i] = iHuffCode + (pbyHuffBits[i] - 1);
			iDataFlag = 1;
			iZeroFlag = 0;
		}
		else
		{
			pbyHuffPtr[i] = 0xFF;
			uHuffMin[i] = 0xFFFF;
			uHuffMax[i] = 0xFFFF;
			iZeroFlag = 1;
		}

		if (iDataFlag == 1)
			iHuffCode = ( iZeroFlag == 1 ) ? ( iHuffCode << 1 ) : ( (uHuffMax[i] + 1) << 1 );
	}
}

#ifdef HEVC_DEC
//////////////////////////////////////////////////////////////////////////////
//	HEVC Decoder Functions
//////////////////////////////////////////////////////////////////////////////
static int32_t NX_HevcDecOpen ( NX_VID_DEC_HANDLE hDec, int32_t iOptions )
{
	IV_API_CALL_STATUS_T e_dec_status;

	uint32_t u4_ip_buf_len;
	uint8_t *pu1_bs_buf;

	int32_t iLevel;

	if( iOptions && DEC_OPT_CHROMA_INTERLEAVE )
	{
		NX_ErrMsg( ("Chroma interleave format is not supported in HEVC!!!\n") );
		return -1;
	}

	hDec->codecStd = CODEC_STD_HEVC;
	hDec->instIndex = -1;			// TBD

    {
		iv_num_mem_rec_ip_t s_no_of_mem_rec_query_ip;
		iv_num_mem_rec_op_t s_no_of_mem_rec_query_op;

		s_no_of_mem_rec_query_ip.u4_size = sizeof(s_no_of_mem_rec_query_ip);
		s_no_of_mem_rec_query_op.u4_size = sizeof(s_no_of_mem_rec_query_op);
		s_no_of_mem_rec_query_ip.e_cmd = IV_CMD_GET_NUM_MEM_REC;

        /*****************************************************************************/
        /*   API Call: Get Number of Mem Records                                     */
        /*****************************************************************************/
        if ( IV_SUCCESS != ihevcd_cxa_api_function( NULL, (void *)&s_no_of_mem_rec_query_ip, (void *)&s_no_of_mem_rec_query_op) )
        {
            NX_ErrMsg( ("Error in get mem records\n") );
            return -1;
        }

        hDec->u4_num_mem_recs = s_no_of_mem_rec_query_op.u4_num_mem_rec;
		hDec->pv_mem_rec_location = memalign(128, hDec->u4_num_mem_recs * sizeof(iv_mem_rec_t));
	    if( hDec->pv_mem_rec_location == NULL )
	    {
	        NX_ErrMsg( ("Allocation failure for mem_rec_location\n") );
	        return -1;
	    }
	    memset(hDec->pv_mem_rec_location, 0, hDec->u4_num_mem_recs * sizeof(iv_mem_rec_t));
    }

	return 0;
}

static VID_ERROR_E NX_HevcDecParseVideoCfg( NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn, NX_VID_SEQ_OUT *pstSeqOut )
{
#if 0
	uint8_t *pbyStrm = pstSeqIn->seqInfo;
	uint32_t uPreFourByte = (uint32_t)-1;
	uint32_t iLevel;

	if ( pbyStrm == NULL )
		return VID_ERR_PARAM;

	do
	{
		if ( pbyStrm >= (pstSeqIn->seqInfo + pstSeqIn->seqSize) )		return VID_ERR_NOT_ENOUGH_STREAM;
		uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;

		if ( uPreFourByte == 0x00000001 || uPreFourByte<<8 == 0x00000100 )
		{
			int32_t iNaluType = (pbyStrm[0] >> 1) & 0x3F;

			// SPS
			if ( iNaluType == 33 )
			{
				VLD_STREAM stStrm = { 16, pbyStrm, pstSeqIn->seqSize };
				int32_t iRead, i;
				int32_t iProfileFlg[7], iLevelFlg[7];

				vld_flush_bits( &stStrm, 4 );							// sps_video_parameter_set_id
				iRead = vld_get_bits( &stStrm, 3 );						// sps_max_sub_layers_minus1
				vld_flush_bits( &stStrm, 1 );							// sps_temporal_id_nesting_flag

				vld_flush_bits( &stStrm, 2 + 1 + 5 );					// profile_space, tier_flag, profile_idc
				vld_flush_bits( &stStrm, 32 );							// profile_compatibility_flag
				vld_flush_bits( &stStrm, 1 + 1 + 1 + 1 + 44 );			// progressive_flag, interlaced_flag, non_packed_constraint_flag, frame_only_constraint_flag, reserved_zero_44bits
				iLevel = vld_get_bits( &stStrm, 8 );					// level_idc

				for(i=0 ; i<iRead ; i++)
				{
					iProfileFlg[i] = vld_get_bits( &stStrm, 1 );		// sub_layer_profile_present_flag[i]
					iLevelFlg[i] = vld_get_bits( &stStrm, 1 );			// sub_layer_level_present_flag[i]
				}

				if ( iRead > 0 )
					for ( i=iRead ; i<8 ; i++ )
						vld_flush_bits( &stStrm, 2 );					// reserved_zero_2bits[i]

				for( i=0 ; i<iRead ; i++ )
				{
					if( iProfileFlg[i] )
					{

						vld_flush_bits( &stStrm, 2 + 1 + 5 );			// sub_layer_profile_space, sub_layer_tier_flag, sub_layer_profile_idc
						vld_flush_bits( &stStrm, 32 );					// sub_layer_profile_compatibility_flag
						vld_flush_bits( &stStrm, 1 + 1 + 1 + 1 + 44 );	// sub_layer_progressive_source_flag, sub_layer_interlaced_source_flag, sub_layer_non_packed_constraint_flag, sub_layer_frame_only_constraint_flag, sub_layer_reserved_zero_44bits
					}
					if( iLevelFlg[i] )
						vld_flush_bits( &stStrm, 8 );					// sub_layer_level_idc
				}

				NX_ErrMsg(("Read Offset = %d, %2x %2x %2x %2x \n", stStrm.dwUsedBits, vld_get_bits(&stStrm, 8), vld_get_bits(&stStrm, 8), vld_get_bits(&stStrm, 8), vld_get_bits(&stStrm, 8)    ));


				iRead = vld_get_uev( &stStrm );									// sps_seq_parameter_set_id

				NX_ErrMsg(("SPS_ID = %d\n", iRead));

				iRead = vld_get_uev( &stStrm );							// chroma_format_idc

				NX_ErrMsg(("chroma_format_idc = %d\n", iRead));

				if( iRead == 3 )
					vld_flush_bits( &stStrm, 1 );						// separate_colour_plane_flag

				pstSeqOut->width = vld_get_uev( &stStrm );				// pic_width_in_luma_samples
				pstSeqOut->height = vld_get_uev( &stStrm );				// pic_height_in_luma_samples


				NX_ErrMsg(("w = %d, h = %d\n", pstSeqOut->width, pstSeqOut->height));

				return VID_ERR_NONE;
			}
		}
	} while(1);

	return VID_ERR_FAIL;
#else
	int32_t iWidth  = ( pstSeqIn->width  ) ? ( pstSeqIn->width  ) : ( HEVC_MAX_FRAME_WIDTH  );
	int32_t iHeight = ( pstSeqIn->height ) ? ( pstSeqIn->height ) : ( HEVC_MAX_FRAME_HEIGHT );
	int32_t iLevel;
    uint32_t i;

	do
	{
		if ( (iWidth > HEVC_MAX_FRAME_WIDTH) || (iHeight > HEVC_MAX_FRAME_HEIGHT) )
		{
			NX_ErrMsg( ("Resolution is not supported!! (%d x %d)\n", iWidth, iHeight) );
			return VID_ERR_NOT_SUPPORT;
		}

		{
			int32_t iImgSz = iWidth * iHeight;
		    if (iImgSz > (1920 * 1088))		iLevel = 50;
		    else if (iImgSz > (1280 * 720))	iLevel = 40;
		    else if (iImgSz > (960 * 540))	iLevel = 31;
		    else if (iImgSz > (640 * 360))	iLevel = 30;
		    else if (iImgSz > (352 * 288))	iLevel = 21;
		    else							iLevel = 20;
		}

	    /*****************************************************************************/
	    /*   Fill Mem Records                                     		 	         */
	    /*****************************************************************************/
	    {
	        ihevcd_cxa_fill_mem_rec_ip_t s_fill_mem_rec_ip;
	        ihevcd_cxa_fill_mem_rec_op_t s_fill_mem_rec_op;
	        iv_mem_rec_t *ps_mem_rec;

	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.u4_size = sizeof(ihevcd_cxa_fill_mem_rec_ip_t);
	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.e_cmd = IV_CMD_FILL_NUM_MEM_REC;
	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.pv_mem_rec_location = ps_mem_rec = hDec->pv_mem_rec_location;
	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.u4_max_frm_wd = iWidth;
	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.u4_max_frm_ht = iHeight;
			s_fill_mem_rec_ip.i4_level = iLevel;
	        s_fill_mem_rec_ip.u4_num_reorder_frames = HEVC_MAX_REORDER_FRAMES;
	        s_fill_mem_rec_ip.u4_num_ref_frames = HEVC_MAX_REF_FRAMES;
	        s_fill_mem_rec_ip.u4_share_disp_buf = 0;		// TBD.
	        s_fill_mem_rec_ip.u4_num_extra_disp_buf = 0;	// TBD.
	        s_fill_mem_rec_ip.e_output_format = IV_YUV_420P;
	        s_fill_mem_rec_op.s_ivd_fill_mem_rec_op_t.u4_size = sizeof(ihevcd_cxa_fill_mem_rec_op_t);

	        for(i = 0; i < hDec->u4_num_mem_recs; i++)
	            ps_mem_rec[i].u4_size = sizeof(iv_mem_rec_t);

	        if( IV_SUCCESS != ihevcd_cxa_api_function(NULL, (void *)&s_fill_mem_rec_ip, (void *)&s_fill_mem_rec_op) )
	        {
	            NX_ErrMsg( ("Error in fill mem records: %x\n", s_fill_mem_rec_op.s_ivd_fill_mem_rec_op_t.u4_error_code) );
	            return VID_ERR_FAIL;
	        }
	        hDec->u4_num_mem_recs = s_fill_mem_rec_op.s_ivd_fill_mem_rec_op_t.u4_num_mem_rec_filled;

	        for(i = 0; i < hDec->u4_num_mem_recs; i++)
	        {
	            ps_mem_rec->pv_base = memalign(ps_mem_rec->u4_mem_alignment, ps_mem_rec->u4_mem_size);
	            if(ps_mem_rec->pv_base == NULL)
	            {
	                NX_ErrMsg( ("\nAllocation failure for mem record id %d size %d\n", i, ps_mem_rec->u4_mem_size) );
	                return VID_ERR_NOT_ALLOC_BUFF;
	            }
	            ps_mem_rec++;
	        }
	    }

	    /*****************************************************************************/
	    /*   Initialize the Decoder                                                  */
	    /*****************************************************************************/
	    {
	        ihevcd_cxa_init_ip_t s_init_ip;
	        ihevcd_cxa_init_op_t s_init_op;
	        void *fxns = &ihevcd_cxa_api_function;

	        s_init_ip.s_ivd_init_ip_t.u4_size = sizeof(ihevcd_cxa_init_ip_t);
	        s_init_ip.s_ivd_init_ip_t.e_cmd = (IVD_API_COMMAND_TYPE_T)IV_CMD_INIT;
	        s_init_ip.s_ivd_init_ip_t.pv_mem_rec_location = hDec->pv_mem_rec_location;
	        s_init_ip.s_ivd_init_ip_t.u4_frm_max_wd = iWidth;
	        s_init_ip.s_ivd_init_ip_t.u4_frm_max_ht = iHeight;
	        s_init_ip.i4_level = iLevel;
	        s_init_ip.u4_num_reorder_frames = HEVC_MAX_REORDER_FRAMES;
	        s_init_ip.u4_num_ref_frames = HEVC_MAX_REF_FRAMES;
	        s_init_ip.u4_share_disp_buf = 0;
	        s_init_ip.u4_num_extra_disp_buf = 0;
	        s_init_ip.s_ivd_init_ip_t.u4_num_mem_rec = hDec->u4_num_mem_recs;
	        s_init_ip.s_ivd_init_ip_t.e_output_format = IV_YUV_420P;
	        s_init_op.s_ivd_init_op_t.u4_size = sizeof(ihevcd_cxa_init_op_t);

	        hDec->codec_obj = (iv_obj_t *)hDec->pv_mem_rec_location[0].pv_base;
	        hDec->codec_obj->pv_fxns = fxns;
	        hDec->codec_obj->u4_size = sizeof(iv_obj_t);

	        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_init_ip, (void *)&s_init_op) )
	        {
	            NX_ErrMsg( ("Error in Init %8x\n", s_init_op.s_ivd_init_op_t.u4_error_code) );
	            return VID_ERR_INIT;
	        }
	    }

	    /*****************************************************************************/
	    /*   Set Parameters                                                          */
	    /*****************************************************************************/
		{
	        ivd_ctl_set_config_ip_t s_ctl_ip;
	        ivd_ctl_set_config_op_t s_ctl_op;

	        s_ctl_ip.u4_disp_wd = iWidth;
	        s_ctl_ip.e_frm_skip_mode = IVD_SKIP_NONE;
	        s_ctl_ip.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
	        s_ctl_ip.e_vid_dec_mode = IVD_DECODE_HEADER;
	        s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
	        s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
	        s_ctl_ip.u4_size = sizeof(ivd_ctl_set_config_ip_t);
	        s_ctl_op.u4_size = sizeof(ivd_ctl_set_config_op_t);

	        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_ctl_ip, (void *)&s_ctl_op) )
	        {
	            NX_ErrMsg( ("Error in setting the codec in header decode mode : 0x%x\n", s_ctl_op.u4_error_code) );
				return VID_ERR_PARAM;
	        }
		}

	    /*****************************************************************************/
	    /*   Header Decode                                                           */
	    /*****************************************************************************/
		{
		    ivd_video_decode_ip_t s_video_decode_ip;
		    ivd_video_decode_op_t s_video_decode_op;

	        s_video_decode_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
	        s_video_decode_ip.u4_ts = 0;
	        s_video_decode_ip.pv_stream_buffer = pstSeqIn->seqInfo;
	        s_video_decode_ip.u4_num_Bytes = pstSeqIn->seqSize;
	        s_video_decode_ip.u4_size = sizeof(ivd_video_decode_ip_t);
	        s_video_decode_op.u4_size = sizeof(ivd_video_decode_op_t);

	        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_video_decode_ip, (void *)&s_video_decode_op) )
	        {
	            NX_ErrMsg( ("Error in header decode %x\n", s_video_decode_op.u4_error_code) );
				return VID_ERR_WRONG_SEQ;
	        }

			if ( ( iWidth == s_video_decode_op.u4_pic_wd ) && ( iHeight == s_video_decode_op.u4_pic_ht ) )
				break;

			iWidth = s_video_decode_op.u4_pic_wd;
			iHeight = s_video_decode_op.u4_pic_ht;
	    }

	    /*****************************************************************************/
	    /*   Free                                                                    */
	    /*****************************************************************************/
	    {
	        iv_mem_rec_t *ps_mem_rec = hDec->pv_mem_rec_location;

			for(i = 0; i < hDec->u4_num_mem_recs; i++)
	        {
				free( ps_mem_rec->pv_base );
	            ps_mem_rec++;
	        }
	    }
	}while ( 1 );

    pstSeqOut->width = iWidth;
    pstSeqOut->height = iHeight;
	pstSeqOut->minBuffers = 2;

	return VID_ERR_NONE;
#endif
}

static VID_ERROR_E NX_HevcDecInit (NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn)
{
    uint32_t i;

#if 0
	int32_t iWidth  = ( pstSeqIn->width  ) ? ( pstSeqIn->width  ) : ( HEVC_DEFAULT_WIDTH  );
	int32_t iHeight = ( pstSeqIn->height ) ? ( pstSeqIn->height ) : ( HEVC_DEFAULT_HEIGHT );
	int32_t iLevel;

	do
	{
		if ( (iWidth > HEVC_MAX_FRAME_WIDTH) || (iHeight > HEVC_MAX_FRAME_HEIGHT) )
		{
			NX_ErrMsg( ("Resolution is not supported!! (%d x %d)\n", iWidth, iHeight) );
			return VID_ERR_NOT_SUPPORT;
		}

		{
			int32_t iImgSz = iWidth * iHeight;
		    if (iImgSz > (1920 * 1088))		iLevel = 50;
		    else if (iImgSz > (1280 * 720))	iLevel = 40;
		    else if (iImgSz > (960 * 540))	iLevel = 31;
		    else if (iImgSz > (640 * 360))	iLevel = 30;
		    else if (iImgSz > (352 * 288))	iLevel = 21;
		    else							iLevel = 20;
		}

	    /*****************************************************************************/
	    /*   Fill Mem Records                                     		 	         */
	    /*****************************************************************************/
	    {
	        ihevcd_cxa_fill_mem_rec_ip_t s_fill_mem_rec_ip;
	        ihevcd_cxa_fill_mem_rec_op_t s_fill_mem_rec_op;
	        iv_mem_rec_t *ps_mem_rec;

	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.u4_size = sizeof(ihevcd_cxa_fill_mem_rec_ip_t);
	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.e_cmd = IV_CMD_FILL_NUM_MEM_REC;
	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.pv_mem_rec_location = ps_mem_rec = hDec->pv_mem_rec_location;
	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.u4_max_frm_wd = iWidth;
	        s_fill_mem_rec_ip.s_ivd_fill_mem_rec_ip_t.u4_max_frm_ht = iHeight;
			s_fill_mem_rec_ip.i4_level = iLevel;
	        s_fill_mem_rec_ip.u4_num_reorder_frames = HEVC_MAX_REORDER_FRAMES;
	        s_fill_mem_rec_ip.u4_num_ref_frames = HEVC_MAX_REF_FRAMES;
	        s_fill_mem_rec_ip.u4_share_disp_buf = 0;		// TBD.
	        s_fill_mem_rec_ip.u4_num_extra_disp_buf = 0;	// TBD.
	        s_fill_mem_rec_ip.e_output_format = IV_YUV_420P;
	        s_fill_mem_rec_op.s_ivd_fill_mem_rec_op_t.u4_size = sizeof(ihevcd_cxa_fill_mem_rec_op_t);

	        for(i = 0; i < hDec->u4_num_mem_recs; i++)
	            ps_mem_rec[i].u4_size = sizeof(iv_mem_rec_t);

	        if( IV_SUCCESS != ihevcd_cxa_api_function(NULL, (void *)&s_fill_mem_rec_ip, (void *)&s_fill_mem_rec_op) )
	        {
	            NX_ErrMsg( ("Error in fill mem records: %x\n", s_fill_mem_rec_op.s_ivd_fill_mem_rec_op_t.u4_error_code) );
	            return VID_ERR_FAIL;
	        }
	        hDec->u4_num_mem_recs = s_fill_mem_rec_op.s_ivd_fill_mem_rec_op_t.u4_num_mem_rec_filled;

	        for(i = 0; i < hDec->u4_num_mem_recs; i++)
	        {
	            ps_mem_rec->pv_base = memalign(ps_mem_rec->u4_mem_alignment, ps_mem_rec->u4_mem_size);
	            if(ps_mem_rec->pv_base == NULL)
	            {
	                NX_ErrMsg( ("\nAllocation failure for mem record id %d size %d\n", i, ps_mem_rec->u4_mem_size) );
	                return VID_ERR_NOT_ALLOC_BUFF;
	            }
	            ps_mem_rec++;
	        }
	    }

	    /*****************************************************************************/
	    /*   Initialize the Decoder                                                  */
	    /*****************************************************************************/
	    {
	        ihevcd_cxa_init_ip_t s_init_ip;
	        ihevcd_cxa_init_op_t s_init_op;
	        void *fxns = &ihevcd_cxa_api_function;

	        s_init_ip.s_ivd_init_ip_t.u4_size = sizeof(ihevcd_cxa_init_ip_t);
	        s_init_ip.s_ivd_init_ip_t.e_cmd = (IVD_API_COMMAND_TYPE_T)IV_CMD_INIT;
	        s_init_ip.s_ivd_init_ip_t.pv_mem_rec_location = hDec->pv_mem_rec_location;
	        s_init_ip.s_ivd_init_ip_t.u4_frm_max_wd = iWidth;
	        s_init_ip.s_ivd_init_ip_t.u4_frm_max_ht = iHeight;
	        s_init_ip.i4_level = iLevel;
	        s_init_ip.u4_num_reorder_frames = HEVC_MAX_REORDER_FRAMES;
	        s_init_ip.u4_num_ref_frames = HEVC_MAX_REF_FRAMES;
	        s_init_ip.u4_share_disp_buf = 0;
	        s_init_ip.u4_num_extra_disp_buf = 0;
	        s_init_ip.s_ivd_init_ip_t.u4_num_mem_rec = hDec->u4_num_mem_recs;
	        s_init_ip.s_ivd_init_ip_t.e_output_format = IV_YUV_420P;
	        s_init_op.s_ivd_init_op_t.u4_size = sizeof(ihevcd_cxa_init_op_t);

	        hDec->codec_obj = (iv_obj_t *)hDec->pv_mem_rec_location[0].pv_base;
	        hDec->codec_obj->pv_fxns = fxns;
	        hDec->codec_obj->u4_size = sizeof(iv_obj_t);

	        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_init_ip, (void *)&s_init_op) )
	        {
	            NX_ErrMsg( ("Error in Init %8x\n", s_init_op.s_ivd_init_op_t.u4_error_code) );
	            return VID_ERR_INIT;
	        }
	    }

	    /*****************************************************************************/
	    /*   Set Parameters                                                          */
	    /*****************************************************************************/
		{
	        ivd_ctl_set_config_ip_t s_ctl_ip;
	        ivd_ctl_set_config_op_t s_ctl_op;

	        s_ctl_ip.u4_disp_wd = iWidth;
	        s_ctl_ip.e_frm_skip_mode = IVD_SKIP_NONE;
	        s_ctl_ip.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
	        s_ctl_ip.e_vid_dec_mode = IVD_DECODE_HEADER;
	        s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
	        s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
	        s_ctl_ip.u4_size = sizeof(ivd_ctl_set_config_ip_t);
	        s_ctl_op.u4_size = sizeof(ivd_ctl_set_config_op_t);

	        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_ctl_ip, (void *)&s_ctl_op) )
	        {
	            NX_ErrMsg( ("Error in setting the codec in header decode mode : 0x%x\n", s_ctl_op.u4_error_code) );
				return VID_ERR_PARAM;
	        }
		}

	    /*****************************************************************************/
	    /*   Header Decode                                                           */
	    /*****************************************************************************/
		{
		    ivd_video_decode_ip_t s_video_decode_ip;
		    ivd_video_decode_op_t s_video_decode_op;

	        s_video_decode_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
	        s_video_decode_ip.u4_ts = 0;
	        s_video_decode_ip.pv_stream_buffer = pstSeqIn->seqInfo;
	        s_video_decode_ip.u4_num_Bytes = pstSeqIn->seqSize;
	        s_video_decode_ip.u4_size = sizeof(ivd_video_decode_ip_t);
	        s_video_decode_op.u4_size = sizeof(ivd_video_decode_op_t);

	        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_video_decode_ip, (void *)&s_video_decode_op) )
	        {
	            NX_ErrMsg( ("Error in header decode %x\n", s_video_decode_op.u4_error_code) );
				return VID_ERR_WRONG_SEQ;
	        }

			if ( ( iWidth == s_video_decode_op.u4_pic_wd ) && ( iHeight == s_video_decode_op.u4_pic_ht ) )
				break;

			NX_ErrMsg( ("Error in Parser (%d x %d) -> (%d x %d) \n", iWidth, iHeight, s_video_decode_op.u4_pic_wd, s_video_decode_op.u4_pic_ht ) );

			iWidth = s_video_decode_op.u4_pic_wd;
			iHeight = s_video_decode_op.u4_pic_ht;
	    }

	    /*****************************************************************************/
	    /*   Free                                                                    */
	    /*****************************************************************************/
	    {
	        iv_mem_rec_t *ps_mem_rec = hDec->pv_mem_rec_location;

			for(i = 0; i < hDec->u4_num_mem_recs; i++)
	        {
				free( ps_mem_rec->pv_base );
	            ps_mem_rec++;
	        }
	    }
	}while ( 1 );

    //pstSeqOut->width = iWidth;
    //pstSeqOut->height = iHeight;
#endif

	int32_t iWidth  = pstSeqIn->width;
	int32_t iHeight = pstSeqIn->height;

    /*************************************************************************/
    /* set num of cores                                                      */
    /*************************************************************************/
    {
        ihevcd_cxa_ctl_set_num_cores_ip_t s_ctl_set_cores_ip;
        ihevcd_cxa_ctl_set_num_cores_op_t s_ctl_set_cores_op;

        s_ctl_set_cores_ip.e_cmd = IVD_CMD_VIDEO_CTL;
        s_ctl_set_cores_ip.e_sub_cmd = (IVD_CONTROL_API_COMMAND_TYPE_T)IHEVCD_CXA_CMD_CTL_SET_NUM_CORES;
        s_ctl_set_cores_ip.u4_num_cores = HEVC_MAX_NUM_CORES;
        s_ctl_set_cores_ip.u4_size = sizeof(ihevcd_cxa_ctl_set_num_cores_ip_t);
        s_ctl_set_cores_op.u4_size = sizeof(ihevcd_cxa_ctl_set_num_cores_op_t);

        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_ctl_set_cores_ip, (void *)&s_ctl_set_cores_op) )
        {
			NX_ErrMsg( ("Error in setting number of cores : %d \n", s_ctl_set_cores_ip.u4_num_cores) );
			return VID_ERR_FAIL;
        }
    }

    /*************************************************************************/
    /* set processsor                                                        */
    /*************************************************************************/
    {
        ihevcd_cxa_ctl_set_processor_ip_t s_ctl_set_num_processor_ip;
        ihevcd_cxa_ctl_set_processor_op_t s_ctl_set_num_processor_op;

        s_ctl_set_num_processor_ip.e_cmd = IVD_CMD_VIDEO_CTL;
        s_ctl_set_num_processor_ip.e_sub_cmd = (IVD_CONTROL_API_COMMAND_TYPE_T)IHEVCD_CXA_CMD_CTL_SET_PROCESSOR;
        s_ctl_set_num_processor_ip.u4_arch = ARCH_ARM_A9Q;	// TBD
        s_ctl_set_num_processor_ip.u4_soc = SOC_GENERIC;	// TBD
        s_ctl_set_num_processor_ip.u4_size = sizeof(ihevcd_cxa_ctl_set_processor_ip_t);
        s_ctl_set_num_processor_op.u4_size = sizeof(ihevcd_cxa_ctl_set_processor_op_t);

        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_ctl_set_num_processor_ip, (void *)&s_ctl_set_num_processor_op) )
        {
			NX_ErrMsg( ("Error in setting Processor type\n") );
			return VID_ERR_FAIL;
        }
    }

#if 0
    /*****************************************************************************/
    /*   Get Input and output buffer allocation                                  */
    /*****************************************************************************/
    {
        ivd_ctl_getbufinfo_ip_t s_ctl_ip;
        ivd_ctl_getbufinfo_op_t s_ctl_op;
		uint32_t frameSz = 0;

        s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
        s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_GETBUFINFO;
        s_ctl_ip.u4_size = sizeof(ivd_ctl_getbufinfo_ip_t);
        s_ctl_op.u4_size = sizeof(ivd_ctl_getbufinfo_op_t);
        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_ctl_ip, (void *)&s_ctl_op ) )
        {
            NX_ErrMsg( ("Error in Get Buf Info %x\n", s_ctl_op.u4_error_code) );
            return VID_ERR_FAIL;
        }

		hDec->numFrameBuffers = s_ctl_op.u4_num_disp_bufs + pstSeqIn->addNumBuffers;

		//pstSeqOut->minBuffers = s_ctl_op.u4_num_disp_bufs;
		//pstSeqOut->numBuffers = hDec->numFrameBuffers;

	    hDec->ps_out_buf = (ivd_out_bufdesc_t *)malloc(sizeof(ivd_out_bufdesc_t));
		for (i=0 ; i<s_ctl_op.u4_min_num_out_bufs ; i++)
		{
            hDec->ps_out_buf->u4_min_out_buf_size[i] = s_ctl_op.u4_min_out_buf_size[i];
            frameSz += s_ctl_op.u4_min_out_buf_size[i];
		}
		hDec->ps_out_buf->u4_num_bufs = s_ctl_op.u4_min_num_out_bufs;

	    // Allocate internal picture buffer
		hDec->ps_out_buf->pu1_bufs[0] = (uint8_t *)memalign(128, frameSz);
		if( hDec->ps_out_buf->pu1_bufs[0] == NULL )
		{
		    NX_ErrMsg( ("Allocation failure for output buffer of size %d\n", frameSz) );
		    return VID_ERR_NOT_ALLOC_BUFF;
		}

		if(s_ctl_op.u4_min_num_out_bufs > 1)
		    hDec->ps_out_buf->pu1_bufs[1] = hDec->ps_out_buf->pu1_bufs[0] + s_ctl_op.u4_min_out_buf_size[0];

		if(s_ctl_op.u4_min_num_out_bufs > 2)
		    hDec->ps_out_buf->pu1_bufs[2] = hDec->ps_out_buf->pu1_bufs[1] + s_ctl_op.u4_min_out_buf_size[1];

		// hDec->s_disp_buffers[i] = hDec->hFrameBuffer[i]
		printf("Frame Buffer Number = %d \n", hDec->numFrameBuffers );

	    for(i = 0; i < hDec->numFrameBuffers ; i++)
	    {
            hDec->s_disp_buffers[i].u4_min_out_buf_size[0] = s_ctl_op.u4_min_out_buf_size[0];
            hDec->s_disp_buffers[i].u4_min_out_buf_size[1] = s_ctl_op.u4_min_out_buf_size[1];
            hDec->s_disp_buffers[i].u4_min_out_buf_size[2] = s_ctl_op.u4_min_out_buf_size[2];

			frameSz = s_ctl_op.u4_min_out_buf_size[0];
            if(s_ctl_op.u4_min_num_out_bufs > 1)
                frameSz += s_ctl_op.u4_min_out_buf_size[1];

            if(s_ctl_op.u4_min_num_out_bufs > 2)
                frameSz += s_ctl_op.u4_min_out_buf_size[2];

            hDec->s_disp_buffers[i].pu1_bufs[0] = (uint8_t *)malloc(frameSz);
            if( hDec->s_disp_buffers[i].pu1_bufs[0] == NULL)
            {
                NX_ErrMsg( ("Allocation failure for output buffer of size %d", frameSz) );
                return VID_ERR_NOT_ALLOC_BUFF;
            }

            if(s_ctl_op.u4_min_num_out_bufs > 1)
                hDec->s_disp_buffers[i].pu1_bufs[1] = hDec->s_disp_buffers[i].pu1_bufs[0] + hDec->s_disp_buffers[i].u4_min_out_buf_size[0];

            if(s_ctl_op.u4_min_num_out_bufs > 2)
                hDec->s_disp_buffers[i].pu1_bufs[2] = hDec->s_disp_buffers[i].pu1_bufs[1] + hDec->s_disp_buffers[i].u4_min_out_buf_size[1];

            hDec->s_disp_buffers[i].u4_num_bufs = s_ctl_op.u4_min_num_out_bufs;
        }
    }

    /*****************************************************************************/
    /*   Send the allocated display buffers to codec                             */
    /*****************************************************************************/
    {
        ivd_set_display_frame_ip_t s_set_display_frame_ip;
        ivd_set_display_frame_op_t s_set_display_frame_op;

        s_set_display_frame_ip.e_cmd = IVD_CMD_SET_DISPLAY_FRAME;
        s_set_display_frame_ip.num_disp_bufs = hDec->numFrameBuffers;
        s_set_display_frame_ip.u4_size = sizeof(ivd_set_display_frame_ip_t);
        s_set_display_frame_op.u4_size = sizeof(ivd_set_display_frame_op_t);

        memcpy(&(s_set_display_frame_ip.s_disp_buffer), &(hDec->s_disp_buffers), hDec->numFrameBuffers * sizeof(ivd_out_bufdesc_t));

        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_set_display_frame_ip, (void *)&s_set_display_frame_op ) )
        {
            NX_ErrMsg( ("Error in Set display frame\n") );
            return VID_ERR_FAIL;
        }
    }
#else
    /*****************************************************************************/
    /*   Frame buffer allocation                                  */
    /*****************************************************************************/
    {
		if( pstSeqIn->numBuffers > 0 )
		{
			hDec->useExternalFrameBuffer = 1;
			hDec->numFrameBuffers = pstSeqIn->numBuffers;

			for( i=0 ; i< hDec->numFrameBuffers ; i++ )
				hDec->hFrameBuffer[i] = pstSeqIn->pMemHandle[i];
		}
		else
		{
			hDec->numFrameBuffers = 2 + pstSeqIn->addNumBuffers;
			for( i=0 ; i< hDec->numFrameBuffers ; i++ )
			{
#if 1
				hDec->hFrameBuffer[i] = NX_VideoAllocateMemory( 4096, iWidth, iHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );	//	Planar Lu/Cb/Cr
				if( 0 == hDec->hFrameBuffer[i] ){
#else
				uint8_t *pbyImg = (uint8_t *)memalign(128, iWidth * iHeight * 3 / 2);

				hDec->hFrameBuffer[i] = (NX_VID_MEMORY_HANDLE)calloc(sizeof(NX_VID_MEMORY_INFO), 1);
				if( NULL == hDec->hFrameBuffer[i] )
				{
					NX_ErrMsg(("Memory info allocation failed\n"));
					return VID_ERR_NOT_ALLOC_BUFF;
				}

				hDec->hFrameBuffer[i]->luVirAddr = (unsigned int)pbyImg;		pbyImg += (iWidth * iHeight);
				hDec->hFrameBuffer[i]->cbVirAddr = (unsigned int)pbyImg;		pbyImg += (iWidth * iHeight) / 4;
				hDec->hFrameBuffer[i]->crVirAddr = (unsigned int)pbyImg;
				hDec->hFrameBuffer[i]->imgHeight = iHeight;
			    hDec->hFrameBuffer[i]->luStride  = iWidth;
			    hDec->hFrameBuffer[i]->cbStride  = iWidth / 2;
			    hDec->hFrameBuffer[i]->crStride  = iWidth / 2;
				if ( 0 == pbyImg ) {
#endif
					NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(i=%d)\n", iWidth, iHeight, i));
					return VID_ERR_NOT_ALLOC_BUFF;
				}
			}
		}

		hDec->curFrmIdx = -1;
    }
#endif

    /*****************************************************************************/
    /*   Set Parameters  (Decoder A Frame Mode )                                 */
    /*****************************************************************************/
	{
        ivd_ctl_set_config_ip_t s_ctl_ip;
        ivd_ctl_set_config_op_t s_ctl_op;

        s_ctl_ip.u4_disp_wd = iWidth;
        s_ctl_ip.e_frm_skip_mode = IVD_SKIP_NONE;
        s_ctl_ip.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
        s_ctl_ip.e_vid_dec_mode = IVD_DECODE_FRAME;
        s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
        s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
        s_ctl_ip.u4_size = sizeof(ivd_ctl_set_config_ip_t);
        s_ctl_op.u4_size = sizeof(ivd_ctl_set_config_op_t);

        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_ctl_ip, (void *)&s_ctl_op) )
        {
            NX_ErrMsg( ("Error in setting the codec in header decode mode : 0x%x\n", s_ctl_op.u4_error_code) );
			return VID_ERR_PARAM;
        }
	}

	return VID_ERR_NONE;
}

static VID_ERROR_E NX_HevcDecDecodeFrame( NX_VID_DEC_HANDLE hDec, NX_VID_DEC_IN *pstDecIn, NX_VID_DEC_OUT *pstDecOut )
{
    ivd_video_decode_ip_t s_video_decode_ip;
    ivd_video_decode_op_t s_video_decode_op;
	int32_t iIdx=0, i;

	if ( pstDecIn->eos == 1 )
	{
        ivd_ctl_flush_ip_t s_ctl_ip;
        ivd_ctl_flush_op_t s_ctl_op;

		int ret;

        s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
        s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_FLUSH;
        s_ctl_ip.u4_size = sizeof(ivd_ctl_flush_ip_t);
        s_ctl_op.u4_size = sizeof(ivd_ctl_flush_op_t);

        if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_ctl_ip, (void *)&s_ctl_op) )
        {
            NX_ErrMsg( ("Error in Setting the decoder in flush mode, error mode = %x \n", s_ctl_op.u4_error_code ) );
			pstDecOut->outImgIdx = -1;
			pstDecOut->outDecIdx = -1;
			return VID_ERR_NO_DEC_FRAME;
        }
	}

	for ( i=1 ; i<hDec->numFrameBuffers ; i++ )
	{
		iIdx = hDec->curFrmIdx + i;
		if ( iIdx >= hDec->numFrameBuffers ) iIdx -= hDec->numFrameBuffers;
		if ( hDec->validFrame[iIdx] == 0 ) break;
	}
	if ( i == hDec->numFrameBuffers )
	{
		NX_ErrMsg( ("There is no Frame for Decoder\n") );
		return VID_ERR_FAIL;
	}

    s_video_decode_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
    s_video_decode_ip.u4_ts = pstDecIn->timeStamp;
    s_video_decode_ip.pv_stream_buffer = pstDecIn->strmBuf;
    s_video_decode_ip.u4_num_Bytes = pstDecIn->strmSize;
    s_video_decode_ip.u4_size = sizeof(ivd_video_decode_ip_t);

    s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[0] = hDec->hFrameBuffer[iIdx]->luStride * hDec->hFrameBuffer[iIdx]->imgHeight;
    s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[1] = hDec->hFrameBuffer[iIdx]->cbStride * hDec->hFrameBuffer[iIdx]->imgHeight/2;
    s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[2] = hDec->hFrameBuffer[iIdx]->crStride * hDec->hFrameBuffer[iIdx]->imgHeight/2;
    s_video_decode_ip.s_out_buffer.pu1_bufs[0] = (uint8_t *)hDec->hFrameBuffer[iIdx]->luVirAddr;
    s_video_decode_ip.s_out_buffer.pu1_bufs[1] = (uint8_t *)hDec->hFrameBuffer[iIdx]->cbVirAddr;
    s_video_decode_ip.s_out_buffer.pu1_bufs[2] = (uint8_t *)hDec->hFrameBuffer[iIdx]->crVirAddr;
    s_video_decode_ip.s_out_buffer.u4_num_bufs = 3;

    s_video_decode_op.u4_size = sizeof(ivd_video_decode_op_t);

    if ( IV_SUCCESS != ihevcd_cxa_api_function( hDec->codec_obj, (void *)&s_video_decode_ip, (void *)&s_video_decode_op) )
    {
        NX_ErrMsg( ("Error in video Frame decode : Error %x\n", s_video_decode_op.u4_error_code) );
    }

	switch( s_video_decode_op.e_pic_type )
	{
		case IV_I_FRAME : pstDecOut->picType[DECODED_FRAME] = PIC_TYPE_I; break;
		case IV_P_FRAME : pstDecOut->picType[DECODED_FRAME] = PIC_TYPE_P; break;
		case IV_B_FRAME : pstDecOut->picType[DECODED_FRAME] = PIC_TYPE_B; break;
		case IV_IDR_FRAME : pstDecOut->picType[DECODED_FRAME] = PIC_TYPE_IDR; break;
		default : pstDecOut->picType[DECODED_FRAME] = PIC_TYPE_UNKNOWN;
	}

	pstDecOut->width = s_video_decode_op.u4_pic_wd;
	pstDecOut->height = s_video_decode_op.u4_pic_ht;
	pstDecOut->strmReadPos  = s_video_decode_op.u4_num_bytes_consumed;
	pstDecOut->strmWritePos = pstDecIn->strmSize;
	pstDecOut->isInterlace = !s_video_decode_op.u4_progressive_frame_flag;
	//pstDecOut->topFieldFirst = s_video_decode_op.e4_fld_type;
	pstDecOut->timeStamp[DECODED_FRAME] = (uint64_t)s_video_decode_op.u4_ts;
	//pstDecOut->outFrmReliable_0_100[]

	if ( s_video_decode_op.u4_output_present )
	{
		hDec->curFrmIdx = iIdx;
		hDec->validFrame[iIdx] = 1;

		pstDecOut->outImgIdx = iIdx;//s_video_decode_op.u4_disp_buf_id;
#if 1
		pstDecOut->outImg = *hDec->hFrameBuffer[ iIdx ];
#else
		pstDecOut->outImg.imgWidth  = s_video_decode_op.s_disp_frm_buf.u4_y_wd;
		pstDecOut->outImg.imgHeight = s_video_decode_op.s_disp_frm_buf.u4_y_ht;
		pstDecOut->outImg.luPhyAddr = 0;
		pstDecOut->outImg.luVirAddr = (unsigned int)s_video_decode_op.s_disp_frm_buf.pv_y_buf;
		pstDecOut->outImg.luStride  = s_video_decode_op.s_disp_frm_buf.u4_y_strd;
		pstDecOut->outImg.cbPhyAddr = 0;
		pstDecOut->outImg.cbVirAddr = (unsigned int)s_video_decode_op.s_disp_frm_buf.pv_u_buf;
		pstDecOut->outImg.cbStride  = s_video_decode_op.s_disp_frm_buf.u4_u_strd;
		pstDecOut->outImg.crPhyAddr = 0;
		pstDecOut->outImg.crVirAddr = (unsigned int)s_video_decode_op.s_disp_frm_buf.pv_v_buf;
		pstDecOut->outImg.crStride  = s_video_decode_op.s_disp_frm_buf.u4_v_strd;
#endif
	}

	//pstDecOut->outImg.fourCC = s_video_decode_op.e_output_format;

	//pstDecOut->outImgIdx;
	//pstDecOut->outDecIdx;
	//pstDecOut->timeStamp[NONE_FIELD];
	//pstDecOut->outFrmReliable_0_100[DISPLAY_FRAME];

	if ( s_video_decode_op.u4_frame_decoded_flag == 0 )
	{
		pstDecOut->outDecIdx = -1;			// Decode Index
		pstDecOut->outFrmReliable_0_100[DECODED_FRAME] = 0;
	}
	else
	{
		pstDecOut->outFrmReliable_0_100[DECODED_FRAME] = 100;
	}

	return VID_ERR_NONE;
}
#endif

//
//	End of Static Functions
//
//////////////////////////////////////////////////////////////////////////////
