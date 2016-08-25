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

#ifndef	__NX_DSP_H__
#define	__NX_DSP_H__

#include <stdint.h>
#include <pthread.h>

#include <nx_alloc_mem.h>

#define	DISPLAY_MAX_BUF_SIZE	2

//#define	DISABLE_PORT_CONFIG

enum {
	DISPLAY_PORT_LCD	= 0x00,
	DISPLAY_PORT_HDMI	= 0x01,
	DISPLAY_PORT_TVOUT	= 0x02,		// S5P6818 Only
};

enum {
	DISPLAY_MODULE_MLC0			= 0x00,
	DISPLAY_MODULE_MLC1			= 0x01,
	DISPLAY_MODULE_MLC0_RGB		= 0x02,
	DISPLAY_MODULE_MLC1_RGB		= 0x03,
};

typedef struct DSP_IMG_RECT {
	int32_t		left;
	int32_t		top;
	int32_t		right;
	int32_t		bottom;
} DSP_IMG_RECT;

typedef struct DISPLAY_INFO {
	int32_t			port;		// port ( DISPLAY_PORT_LCD or DISPLAY_PORT_HDMI )
	int32_t			module;		// module ( DISPLAY_MODULE_MLC0 or DISPLAY_MODULE_MLC1 )

	int32_t			width;		// source width
	int32_t			height;		// source height
	int32_t			stride;		// source image's strid
	int32_t			fourcc;		// source image's format fourcc
	int32_t			numPlane;	// source image's plane number

	DSP_IMG_RECT 	dspSrcRect;	// source image's crop region
	DSP_IMG_RECT	dspDstRect;	// target display rect
} DISPLAY_INFO;

typedef struct DISPLAY_HANDLE_INFO	*DISPLAY_HANDLE;

#ifdef __cplusplus
extern "C"{
#endif

DISPLAY_HANDLE	NX_DspInit					( DISPLAY_INFO *pDspInfo );
void			NX_DspClose					( DISPLAY_HANDLE hDisplay );
int32_t			NX_DspStreamControl			( DISPLAY_HANDLE hDisplay, int32_t bEnable );	

int32_t			NX_DspQueueBuffer			( DISPLAY_HANDLE hDisplay, NX_VID_MEMORY_INFO *pVidBuf );
int32_t			NX_DspRgbQueueBuffer		( DISPLAY_HANDLE hDisplay, NX_MEMORY_INFO *pMemInfo );
int32_t			NX_DspDequeueBuffer			( DISPLAY_HANDLE hDisplay );

int32_t			NX_DspVideoSetSourceFormat 	( DISPLAY_HANDLE hDisplay, int32_t width, int32_t height, int32_t stride, int32_t fourcc );
int32_t			NX_DspVideoSetSourceCrop	( DISPLAY_HANDLE hDisplay, DSP_IMG_RECT *pRect );
int32_t 		NX_DspVideoSetPosition		( DISPLAY_HANDLE hDisplay, DSP_IMG_RECT *pRect );

int32_t			NX_DspVideoSetPriority		( int32_t module, int32_t priority );
int32_t			NX_DspVideoGetPriority		( int32_t module, int32_t *priority );

int32_t			NX_DspSetColorKey			( int32_t module, int32_t colorkey );
int32_t			NX_DspGetColorKey			( int32_t module, int32_t *colorkey );
#ifdef __cplusplus
}
#endif

#endif	//	__NX_DSP_H__
