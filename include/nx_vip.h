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

#ifndef __NX_VIP_H__
#define	__NX_VIP_H__

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include <nx_alloc_mem.h>

enum {
	VIP_PORT_0		= 0x00,
	VIP_PORT_1		= 0x01,
	VIP_PORT_2		= 0x02,
	VIP_PORT_MIPI	= 0x03,
};

enum {
	VIP_MODE_CLIPPER   		= 0x00,
	VIP_MODE_DECIMATOR 		= 0x01,
	VIP_MODE_CLIP_DEC  		= 0x02,
	VIP_MODE_CLIP_DEC2		= 0x03,
};

typedef struct VIP_INFO			VIP_INFO;
typedef struct VIP_HANDLE_INFO	*VIP_HANDLE;

struct VIP_INFO {
	int32_t		port;		//	video input processor's port number
	int32_t		mode;		//	1 : clipper only, 2 : decimator only, 3 : clipper --> decimator

	int32_t		width;		//  Camera Input Width
	int32_t		height;		//  Camera Input Height

	int32_t		fpsNum;		//	Frame per seconds's Numerate value
	int32_t		fpsDen;		//	Frame per seconds's Denominate value

	int32_t		numPlane;	//	Input image's plane number

	int32_t		cropX;		//  Cliper x
	int32_t		cropY;		//  Cliper y
	int32_t		cropWidth;	//  Cliper width
	int32_t		cropHeight; //  Cliper height

	int32_t		outWidth;	//  Decimator width
	int32_t		outHeight;	//  Decimator height
};

#ifdef __cplusplus
extern "C"{
#endif

VIP_HANDLE		NX_VipInit					( VIP_INFO *pVipInfo );
void			NX_VipClose					( VIP_HANDLE hVip );
int32_t			NX_VipStreamControl			( VIP_HANDLE hVip, int32_t bEnable );

// clipper, decimator, clipper + decimator with memory
int32_t			NX_VipQueueBuffer			( VIP_HANDLE hVip, NX_VID_MEMORY_INFO *pMem );
int32_t			NX_VipDequeueBuffer			( VIP_HANDLE hVip, NX_VID_MEMORY_INFO **ppMem, int64_t *pTimeStamp );

// clipper with memory + decimator with memory
int32_t			NX_VipQueueBuffer2			( VIP_HANDLE hVip, NX_VID_MEMORY_INFO *pClipMem, NX_VID_MEMORY_INFO *pDeciMem );
int32_t			NX_VipDequeueBuffer2		( VIP_HANDLE hVip, NX_VID_MEMORY_INFO **ppClipMem, NX_VID_MEMORY_INFO **ppDeciMem, int64_t *pClipTimeStamp, int64_t *pDeciTimeStamp );

int32_t 		NX_VipChangeConfig			( VIP_HANDLE hVip, VIP_INFO *pVipInfo );

int32_t			NX_VipGetCurrentQueuedCount	( VIP_HANDLE hVip, int32_t *maxSize );

#ifdef __cplusplus
}
#endif

#endif	//	__NX_VIP_H__
