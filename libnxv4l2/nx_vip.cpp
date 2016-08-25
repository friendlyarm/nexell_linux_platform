//------------------------------------------------------------------------------
//
//  Copyright (C) 2013 Nexell Co. All Rights Reserved
//  Nexell Co. Proprietary & Confidential
//
//  NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//  Module      :
//  File        :
//  Description :
//  Author      : 
//  Export      :
//  History     :
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include <nx_alloc_mem.h>
#include <nxp-v4l2-media.h>

#include "nx_vip.h"

#define	VIP_MAX_BUF_SIZE				32		//	currently set minimum size for encoding & display

typedef struct VIP_HANDLE_INFO	VIP_HANDLE_INFO;

struct VIP_HANDLE_INFO {
	V4L2_PRIVATE_HANDLE	hPrivate;		//  private handle
	VIP_INFO			vipInfo;		//	Input Information
	int32_t				mode;			//	same as Video Input Type' mode
	
	int32_t				sensorId;		//	sensor id
	int32_t				cliperId;		//	clipper id
	int32_t				decimatorId;	//	decimator id

	int32_t				numPlane;		//	Input Image's Plane Number

	NX_VID_MEMORY_INFO *pMgmtMem1[VIP_MAX_BUF_SIZE];	// video memory slot1
	NX_VID_MEMORY_INFO *pMgmtMem2[VIP_MAX_BUF_SIZE];	// video memory slot2

	int32_t				curQueuedSize;	//	Number of video memory

	int32_t				streamOnFlag;	//	on/off flag
	pthread_mutex_t		hMutex;
};

//#define DISPLAY_FPS

#ifdef DISPLAY_FPS
uint64_t GetSystemTime( void )
{
	struct timeval tv;
	gettimeofday( &tv, NULL );
	return ((uint64_t)tv.tv_sec)*1000 + tv.tv_usec / 1000;
}
#endif

// CLIP_STATUS(a) :: clipper only / clipper with memory + decimator with memory
// DECI_STATUS(a) :: decimator only / clipper + decimator with memory / clipper with memory + decimator with memory
#define CLIP_STATUS(a) 	((a == VIP_MODE_CLIPPER) || (a == VIP_MODE_CLIP_DEC2)) ? true : false
#define DECI_STATUS(a) 	((a == VIP_MODE_DECIMATOR) || (a == VIP_MODE_CLIP_DEC) || (a == VIP_MODE_CLIP_DEC2)) ? true : false

#define	ALIGN(X,N)		((X+N-1) & (~(N-1)))

VIP_HANDLE NX_VipInit( VIP_INFO *pVipInfo )
{
	V4L2_PRIVATE_HANDLE hPrivate = NULL;
	VIP_HANDLE hVip = NULL;

	int32_t cliperId, decimatorId, sensorId;
	int32_t format;

	struct V4l2UsageScheme s;
	memset( &s, 0x00, sizeof(s) );
	
	// 1. Check vip port and Set using memory.
	if( pVipInfo->port == VIP_PORT_0 ) {
		s.useClipper0 	= CLIP_STATUS( pVipInfo->mode );
		s.useDecimator0 = DECI_STATUS( pVipInfo->mode );

		sensorId 	= nxp_v4l2_sensor0;
		cliperId 	= nxp_v4l2_clipper0;
		decimatorId	= nxp_v4l2_decimator0;
	}
	else if( pVipInfo->port == VIP_PORT_1 ) {
		s.useClipper1 	= CLIP_STATUS( pVipInfo->mode );
		s.useDecimator1 = DECI_STATUS( pVipInfo->mode );

		sensorId 	= nxp_v4l2_sensor1;
		cliperId 	= nxp_v4l2_clipper1;
		decimatorId	= nxp_v4l2_decimator1;
	}
	else if( pVipInfo->port == VIP_PORT_2 ) {
		s.useClipper1	= CLIP_STATUS( pVipInfo->mode );
		s.useDecimator1	= DECI_STATUS( pVipInfo->mode );

		sensorId	= nxp_v4l2_sensor0;
		cliperId	= nxp_v4l2_clipper0;
		decimatorId	= nxp_v4l2_decimator0;
	}
	else if( pVipInfo->port == VIP_PORT_MIPI ) {
		s.useClipper1	= CLIP_STATUS( pVipInfo->mode );
		s.useDecimator1 = DECI_STATUS( pVipInfo->mode );

		sensorId	= nxp_v4l2_sensor1;
		cliperId	= nxp_v4l2_clipper1;
		decimatorId	= nxp_v4l2_decimator1;
	}
	else {
		return NULL;
	}

	// 2. Get private handle.
	if( !(hPrivate = v4l2_init(&s)) ) {
		printf("%s(): v4l2_init() failed!\n", __func__);
		return NULL;
	}

	// 3. Set sensor format. (Normal case / MIPI case)
	if( pVipInfo->port != VIP_PORT_MIPI ) {
		v4l2_set_format( hPrivate, sensorId, pVipInfo->width, pVipInfo->height, PIXCODE_YUV422_PACKED );
	}
	else {
		v4l2_set_format( hPrivate, sensorId, pVipInfo->width, pVipInfo->height, PIXCODE_YUV422_PACKED );
		v4l2_set_format( hPrivate, nxp_v4l2_mipicsi, pVipInfo->width, pVipInfo->height, PIXCODE_YUV422_PACKED );
	}

	// 4. Memory Align -> width is 32-aligned
	pVipInfo->cropWidth		= ALIGN( pVipInfo->cropWidth, 32 );
	pVipInfo->outWidth		= ALIGN( pVipInfo->outWidth, 32 );

	// 5. Config Clipper & Decimator
	// 5-a. clipper only / clipper with memory + decimator with memory
	if( CLIP_STATUS(pVipInfo->mode) ) {
		if( pVipInfo->numPlane == 3 )
			v4l2_set_format( hPrivate, cliperId, pVipInfo->width, pVipInfo->height, PIXFORMAT_YUV420_PLANAR );
		else 
			v4l2_set_format( hPrivate, cliperId, pVipInfo->width, pVipInfo->height, PIXFORMAT_YUV420_YV12 );

		v4l2_set_crop_with_pad( hPrivate, cliperId, 0, pVipInfo->cropX, pVipInfo->cropY, pVipInfo->cropWidth, pVipInfo->cropHeight );
		v4l2_reqbuf( hPrivate, cliperId, VIP_MAX_BUF_SIZE );
	}

	// 5-b. decimator only / clipper + decimator with memory / clipper with memory + decimator with memory
	if( DECI_STATUS(pVipInfo->mode) ) {
		if( pVipInfo->numPlane == 3 )
			v4l2_set_format( hPrivate, decimatorId, pVipInfo->width, pVipInfo->height, PIXFORMAT_YUV420_PLANAR );
		else
			v4l2_set_format( hPrivate, decimatorId, pVipInfo->width, pVipInfo->height, PIXFORMAT_YUV420_YV12 );

		if( pVipInfo->mode == VIP_MODE_CLIP_DEC ) {
			v4l2_set_crop_with_pad( hPrivate, decimatorId, 2, pVipInfo->cropX, pVipInfo->cropY, pVipInfo->cropWidth, pVipInfo->cropHeight );
		}
		
		v4l2_set_crop_with_pad( hPrivate, decimatorId, 0, 0, 0, pVipInfo->outWidth, pVipInfo->outHeight );
		v4l2_reqbuf( hPrivate, decimatorId, VIP_MAX_BUF_SIZE );
	}

	// 6. Create Handle
	hVip = (VIP_HANDLE)malloc( sizeof(VIP_HANDLE_INFO) );
	memset( hVip, 0, sizeof(VIP_HANDLE_INFO) );

	hVip->hPrivate		= hPrivate;
	hVip->vipInfo		= *pVipInfo;
	hVip->mode			= pVipInfo->mode;
	hVip->numPlane  	= pVipInfo->numPlane;
	hVip->sensorId		= sensorId;
	hVip->cliperId		= cliperId;
	hVip->decimatorId	= decimatorId;

	pthread_mutex_init( &hVip->hMutex, NULL );
	return hVip;
}

void NX_VipClose( VIP_HANDLE hVip )
{
	if( hVip )
	{
		pthread_mutex_destroy( &hVip->hMutex );
		if( hVip->streamOnFlag )
		{
			if( CLIP_STATUS(hVip->vipInfo.mode) ) v4l2_streamoff( hVip->hPrivate, hVip->cliperId );
			if( DECI_STATUS(hVip->vipInfo.mode) ) v4l2_streamoff( hVip->hPrivate, hVip->decimatorId );

			hVip->streamOnFlag = false;
		}
		
		if( hVip->hPrivate ) v4l2_exit(hVip->hPrivate);
		if( hVip ) free( hVip );
	}
}

int32_t NX_VipStreamControl( VIP_HANDLE hVip, int32_t bEnable )
{
	printf("%s(): Not implemetation.\n", __func__);
	return 0;
	
	if( hVip )
	{
		if( bEnable )
		{
			if( !hVip->streamOnFlag )
			{
				hVip->streamOnFlag = true;
				if( DECI_STATUS(hVip->vipInfo.mode) ) v4l2_streamon( hVip->hPrivate, hVip->decimatorId );
				if( CLIP_STATUS(hVip->vipInfo.mode) ) v4l2_streamon( hVip->hPrivate, hVip->cliperId );
			}
		}
		else
		{
			if( hVip->streamOnFlag )
			{
				hVip->streamOnFlag = false;
				if( DECI_STATUS(hVip->vipInfo.mode) ) v4l2_streamoff( hVip->hPrivate, hVip->decimatorId );
				if( CLIP_STATUS(hVip->vipInfo.mode) ) v4l2_streamoff( hVip->hPrivate, hVip->cliperId );
			}
		}

	} 
	else
	{
		return -1;
	}

	return 0;
}

int32_t NX_VipQueueBuffer( VIP_HANDLE hVip, NX_VID_MEMORY_INFO *pMem )
{
	int32_t slotIndex, i;

	if( hVip->mode == VIP_MODE_CLIP_DEC2 ) {
		printf("%s(): Not support api. (mode = %d)\n", __func__, hVip->mode);
		return -1;
	}

	pthread_mutex_lock( &hVip->hMutex );
	if( hVip->curQueuedSize >= VIP_MAX_BUF_SIZE ) {
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}

	//	Find Empty Slot & index
	for( i = 0; i < VIP_MAX_BUF_SIZE; i++ ) {
		if( hVip->pMgmtMem1[i] == NULL ) {
			slotIndex = i;
			hVip->pMgmtMem1[i] = pMem;
			break;
		}
	}

	if( i == VIP_MAX_BUF_SIZE ) {
		printf("%s(): Have no empty slot.\n", __func__);
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}
	hVip->curQueuedSize ++;

	pthread_mutex_unlock( &hVip->hMutex );
	{
		struct nxp_vid_buffer vipBuffer;
		NX_MEMORY_INFO *vidMem;
		for( i = 0; i<hVip->numPlane; i++ ) {
			vidMem				= (NX_MEMORY_INFO *)pMem->privateDesc[i];
			vipBuffer.fds[i]	= (int)vidMem->privateDesc;
			vipBuffer.virt[i]	= (char*)vidMem->virAddr;
			vipBuffer.phys[i]	= vidMem->phyAddr;
			vipBuffer.sizes[i]	= vidMem->size;
		}
		if( DECI_STATUS(hVip->mode) ) v4l2_qbuf( hVip->hPrivate, hVip->decimatorId , hVip->numPlane, slotIndex, &vipBuffer, -1, NULL);
		if( CLIP_STATUS(hVip->mode) ) v4l2_qbuf( hVip->hPrivate, hVip->cliperId , hVip->numPlane, slotIndex, &vipBuffer, -1, NULL);
	}

	if( !hVip->streamOnFlag )
	{
		hVip->streamOnFlag = true;
		if( DECI_STATUS(hVip->mode) ) v4l2_streamon( hVip->hPrivate, hVip->decimatorId );
		if( CLIP_STATUS(hVip->mode) ) v4l2_streamon( hVip->hPrivate, hVip->cliperId );
	}

	return 0;
}

int32_t NX_VipDequeueBuffer( VIP_HANDLE hVip, NX_VID_MEMORY_INFO **ppMem, int64_t *pTimeStamp )
{
	int32_t ret = 0;
	int32_t caputreIdx;

	if( hVip->mode == VIP_MODE_CLIP_DEC2 ) {
		printf("%s(): Not support api. (mode = %d)\n", __func__, hVip->mode);
		return -1;
	}

	pthread_mutex_lock( &hVip->hMutex );
	if( hVip->curQueuedSize < 2 )
	{
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}
	pthread_mutex_unlock( &hVip->hMutex );

	if( CLIP_STATUS(hVip->mode) ) {
		v4l2_dqbuf(hVip->hPrivate, hVip->cliperId, hVip->numPlane, &caputreIdx, NULL);
		v4l2_get_timestamp(hVip->hPrivate, hVip->cliperId, pTimeStamp);
	}
	else if( DECI_STATUS(hVip->mode) ) {
		v4l2_dqbuf(hVip->hPrivate, hVip->decimatorId, hVip->numPlane, &caputreIdx, NULL);
		v4l2_get_timestamp(hVip->hPrivate, hVip->decimatorId, pTimeStamp);
	}

	*ppMem = hVip->pMgmtMem1[caputreIdx];
	hVip->pMgmtMem1[caputreIdx] = NULL;
	if( *ppMem == NULL )
	{
		printf("%s(): Buffering error!\n", __func__);
		ret = -1;
	}

	pthread_mutex_lock( &hVip->hMutex );
	hVip->curQueuedSize--;
	pthread_mutex_unlock( &hVip->hMutex );

#ifdef DISPLAY_FPS
	{
		static uint64_t startTime = 0, endTime = 0;
		static uint32_t captureCnt = 0;

		endTime = GetSystemTime();
		captureCnt++;
		if( (endTime - startTime) >= 1000) {
			printf("instance [ %p ] frame [ %03.1f fps ]\n", hVip, (double)(captureCnt / ((endTime - startTime) / 1000.) ));
			startTime = endTime;
			captureCnt = 0;
		}
	}
#endif

	return ret;
}

int32_t NX_VipQueueBuffer2( VIP_HANDLE hVip, NX_VID_MEMORY_INFO *pClipMem, NX_VID_MEMORY_INFO *pDeciMem )
{
	int32_t slotIndex1, slotIndex2, i;

	if( hVip->mode != VIP_MODE_CLIP_DEC2 ) {
		printf("%s(): Not support api. (mode = %d)\n", __func__, hVip->mode);
		return -1;
	}

	pthread_mutex_lock( &hVip->hMutex );
	if( hVip->curQueuedSize >= VIP_MAX_BUF_SIZE ) {
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}

	//	Find Empty Slot & index
	for( i = 0; i < VIP_MAX_BUF_SIZE; i++ ) {
		if( hVip->pMgmtMem1[i] == NULL ) {
			slotIndex1 = i;
			hVip->pMgmtMem1[i] = pClipMem;
			break;
		}
	}
	if( i == VIP_MAX_BUF_SIZE ) {
		printf("%s(): Have no empty slot.\n", __func__);
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}

	for( i = 0; i < VIP_MAX_BUF_SIZE; i++ ) {
		if( hVip->pMgmtMem2[i] == NULL ) {
			slotIndex2 = i;
			hVip->pMgmtMem2[i] = pDeciMem;
			break;
		}
	}
	if( i == VIP_MAX_BUF_SIZE ) {
		printf("%s(): Have no empty slot.\n", __func__);
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}

	hVip->curQueuedSize ++;

	pthread_mutex_unlock( &hVip->hMutex );
	{
		struct nxp_vid_buffer vipBuffer1, vipBuffer2;
		NX_MEMORY_INFO *vidMem1, *vidMem2;

		for( i = 0; i < hVip->numPlane; i++ ) {
			vidMem1				= (NX_MEMORY_INFO *)pClipMem->privateDesc[i];
			vipBuffer1.fds[i]	= (int)vidMem1->privateDesc;
			vipBuffer1.virt[i]	= (char*)vidMem1->virAddr;
			vipBuffer1.phys[i]	= vidMem1->phyAddr;
			vipBuffer1.sizes[i]	= vidMem1->size;

			vidMem2				= (NX_MEMORY_INFO *)pDeciMem->privateDesc[i];
			vipBuffer2.fds[i]	= (int)vidMem2->privateDesc;
			vipBuffer2.virt[i]	= (char*)vidMem2->virAddr;
			vipBuffer2.phys[i]	= vidMem2->phyAddr;
			vipBuffer2.sizes[i]	= vidMem2->size;
		}
		
		v4l2_qbuf( hVip->hPrivate, hVip->cliperId, hVip->numPlane, slotIndex1, &vipBuffer1, -1, NULL);
		v4l2_qbuf( hVip->hPrivate, hVip->decimatorId, hVip->numPlane, slotIndex2, &vipBuffer2, -1, NULL);
	}

	if( !hVip->streamOnFlag ) {
		hVip->streamOnFlag = true;
		v4l2_streamon( hVip->hPrivate, hVip->decimatorId );
		v4l2_streamon( hVip->hPrivate, hVip->cliperId );
	}

	return 0;
}

int32_t NX_VipDequeueBuffer2( VIP_HANDLE hVip, NX_VID_MEMORY_INFO **ppClipMem, NX_VID_MEMORY_INFO **ppDeciMem, int64_t *pClipTimeStamp, int64_t *pDeciTimeStamp )
{
	int32_t ret = 0;
	int32_t captureIdx1, captureIdx2;

	if( hVip->mode != VIP_MODE_CLIP_DEC2 ) {
		printf("%s(): Not support api. (mode = %d)\n", __func__, hVip->mode);
		return -1;
	}

	pthread_mutex_lock( &hVip->hMutex );
	if( hVip->curQueuedSize < 2 )
	{
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}
	pthread_mutex_unlock( &hVip->hMutex );

	v4l2_dqbuf( hVip->hPrivate, hVip->cliperId, hVip->numPlane, &captureIdx1, NULL );
	v4l2_dqbuf( hVip->hPrivate, hVip->decimatorId, hVip->numPlane, &captureIdx2, NULL );
	
	v4l2_get_timestamp( hVip->hPrivate, hVip->cliperId, pClipTimeStamp );
	v4l2_get_timestamp( hVip->hPrivate, hVip->decimatorId, pDeciTimeStamp );

	*ppClipMem = hVip->pMgmtMem1[captureIdx1];
	*ppDeciMem = hVip->pMgmtMem2[captureIdx2];
	
	hVip->pMgmtMem1[captureIdx1] = NULL;
	hVip->pMgmtMem2[captureIdx2] = NULL;

	if( *ppClipMem == NULL ) {
		printf("%s(): Cliiper memory buffering error!\n", __func__);
		ret = -1;
	}	

	if( *ppDeciMem == NULL ) {
		printf("%s(): Decimator memory buffering error!\n", __func__);
		ret = -1;
	}

	pthread_mutex_lock( &hVip->hMutex );
	hVip->curQueuedSize--;
	pthread_mutex_unlock( &hVip->hMutex );

	return ret;
}

int32_t NX_VipChangeConfig( VIP_HANDLE hVip, VIP_INFO *pVipInfo )
{
	int32_t i;
	pthread_mutex_lock( &hVip->hMutex );
	
	// a. stream off
	if( hVip->streamOnFlag ) {
		if( CLIP_STATUS(hVip->vipInfo.mode) ) v4l2_streamoff( hVip->hPrivate, hVip->cliperId );
		if( DECI_STATUS(hVip->vipInfo.mode) ) v4l2_streamoff( hVip->hPrivate, hVip->decimatorId );

		hVip->streamOnFlag = false;
	}

	// b. reconfiguration
	if( pVipInfo->port != VIP_PORT_MIPI ) {
		v4l2_set_format( hVip->hPrivate, hVip->sensorId, pVipInfo->width, pVipInfo->height, PIXCODE_YUV422_PACKED );
	}
	else {
		v4l2_set_format( hVip->hPrivate, hVip->sensorId, pVipInfo->width, pVipInfo->height, PIXCODE_YUV422_PACKED );
		v4l2_set_format( hVip->hPrivate, nxp_v4l2_mipicsi, pVipInfo->width, pVipInfo->height, PIXCODE_YUV422_PACKED );
	}

	pVipInfo->cropWidth		= ALIGN( pVipInfo->cropWidth, 32 );
	pVipInfo->outWidth		= ALIGN( pVipInfo->outWidth, 32 );

	if( CLIP_STATUS(pVipInfo->mode) ) {
		if( pVipInfo->numPlane == 3 )
			v4l2_set_format( hVip->hPrivate, hVip->cliperId, pVipInfo->width, pVipInfo->height, PIXFORMAT_YUV420_PLANAR );
		else 
			v4l2_set_format( hVip->hPrivate, hVip->cliperId, pVipInfo->width, pVipInfo->height, PIXFORMAT_YUV420_YV12 );

		v4l2_set_crop_with_pad( hVip->hPrivate, hVip->cliperId, 0, pVipInfo->cropX, pVipInfo->cropY, pVipInfo->cropWidth, pVipInfo->cropHeight );
		v4l2_reqbuf( hVip->hPrivate, hVip->cliperId, VIP_MAX_BUF_SIZE );
	}

	if( DECI_STATUS(pVipInfo->mode) ) {
		if( pVipInfo->numPlane == 3 )
			v4l2_set_format( hVip->hPrivate, hVip->decimatorId, pVipInfo->width, pVipInfo->height, PIXFORMAT_YUV420_PLANAR );
		else
			v4l2_set_format( hVip->hPrivate, hVip->decimatorId, pVipInfo->width, pVipInfo->height, PIXFORMAT_YUV420_YV12 );

		if( pVipInfo->mode == VIP_MODE_CLIP_DEC ) {
			v4l2_set_crop_with_pad( hVip->hPrivate, hVip->decimatorId, 2, pVipInfo->cropX, pVipInfo->cropY, pVipInfo->cropWidth, pVipInfo->cropHeight );
		}
		
		v4l2_set_crop_with_pad( hVip->hPrivate, hVip->decimatorId, 0, 0, 0, pVipInfo->outWidth, pVipInfo->outHeight );
		v4l2_reqbuf( hVip->hPrivate, hVip->decimatorId, VIP_MAX_BUF_SIZE );
	}

	// c. variable initilaize
	hVip->vipInfo.width			= pVipInfo->width;
	hVip->vipInfo.height		= pVipInfo->height;
	hVip->vipInfo.cropX			= pVipInfo->cropX;
	hVip->vipInfo.cropY			= pVipInfo->cropY;
	hVip->vipInfo.cropWidth		= pVipInfo->cropWidth;
	hVip->vipInfo.cropHeight	= pVipInfo->cropHeight;
	hVip->vipInfo.outWidth		= pVipInfo->outWidth;
	hVip->vipInfo.outHeight		= pVipInfo->outHeight;
	
	hVip->curQueuedSize 		= 0;

	for( i = 0; i < VIP_MAX_BUF_SIZE; i++ ) {
		hVip->pMgmtMem1[i] = NULL;
		hVip->pMgmtMem2[i] = NULL;
	}
	pthread_mutex_unlock( &hVip->hMutex );
	return 0;
}

int32_t NX_VipGetCurrentBufCount( VIP_HANDLE hVip, int32_t *maxSize )
{
	int32_t ret;
	pthread_mutex_lock( &hVip->hMutex );
	*maxSize = VIP_MAX_BUF_SIZE;
	ret = hVip->curQueuedSize;
	pthread_mutex_unlock( &hVip->hMutex );
	return ret;
}

