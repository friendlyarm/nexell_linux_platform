#include <stdlib.h>		//	malloc & free

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ion.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <nx_alloc_mem.h>
#include <nx_fourcc.h>

#ifdef ANDROID
#include <ion-private.h>
#include <gralloc_priv.h>
#endif

//#define	EN_MULTIPLE_PLANE

#define	DTAG			"[DEBUG|ALLOC] "
#define	ETAG			"[ERROR|ALLOC] "

#define	DbgMsg(xxx...)	do{					\
							printf(DTAG);	\
							printf(xxx);	\
						}while(0)
#define	ErrMsg(xxx...)	do{					\
							printf(ETAG);	\
							printf("%s (line%d)\n", __FILE__, __LINE__);	\
							printf(xxx);	\
						}while(0)

#ifndef ALIGN
#define	ALIGN(X,N)	( (X+N-1) & (~(N-1)) )
#endif

#define	ALIGNED16(X)	ALIGN(X,16)

//	Nexell Private Memory Allocator for ION
NX_MEMORY_HANDLE NX_AllocateMemory( int size, int align )
{
	int ret;
	int ionFd=-1, ionAllocFdVar=-1;
	unsigned long phyAddr;
	unsigned long virAddr;
	NX_MEMORY_HANDLE handle=NULL;

	handle = malloc(sizeof(NX_MEMORY_INFO));
	if( NULL == handle )
	{
		ErrMsg("Memory info allocation failed\n");
		goto Error_Exit;
	}

	ionFd = ion_open();
	if( ionFd < 0 )
	{
		ErrMsg("ion_open failed\n");
		goto Error_Exit;
	}

    // psw0523 for alloc from system
	ret = ion_alloc_fd( ionFd, size, align, ION_HEAP_NXP_CONTIG_MASK, 0, &ionAllocFdVar );
	/* ret = ion_alloc_fd( ionFd, size, align, ION_HEAP_SYSTEM_MASK, 0, &ionAllocFdVar ); */
	if( ret<0 )
	{
		ErrMsg("ion_alloc_fd failed.(ret=%d)\n", ret);
		goto Error_Exit;
	}

	ret = ion_get_phys( ionFd, ionAllocFdVar, &phyAddr );
	if( ret<0 )
	{
		ErrMsg("ion_get_phys failed.(ret=%d)\n", ret);
		goto Error_Exit;
	}

	virAddr = (unsigned long)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, ionAllocFdVar, 0);

	if( MAP_FAILED == (void*)virAddr )
	{
		ErrMsg("mmap failed.(size=%d, align=%d)\n", size, align);
		goto Error_Exit;
	}

	memset( (unsigned char*)virAddr, 0x80, size );

	handle->privateDesc = (void*)ionAllocFdVar;
	handle->align = align;
	handle->size = size;
	handle->virAddr = virAddr;
	handle->phyAddr = phyAddr;
	close( ionFd );
	return handle;

Error_Exit:
	if( handle )
	{
		if( ionFd > 0 )
		{
			close( ionFd );
		}
		if( ionAllocFdVar > 0 )
		{
			close( ionFd );
		}
		free(handle);
	}
	return NULL;
}

void NX_FreeMemory( NX_MEMORY_HANDLE handle )
{
	if( handle )
	{
		if( handle->privateDesc )
		{
			munmap( (void*)handle->virAddr, handle->size );
			close( (int)handle->privateDesc );
		}
		free( handle );
	}
}

//	Nexell Private Video Specific Allocator for ION
NX_VID_MEMORY_HANDLE NX_VideoAllocateMemory( int align, int width, int height, int memMap, int fourCC )
{
	int lWidth, lHeight;
	int cWidth, cHeight;
	unsigned int lSize;	//	Luminance plane size
	unsigned int cSize;	//	Chrominance plane size
	NX_VID_MEMORY_HANDLE handle = NULL;
	NX_MEMORY_HANDLE luHandle=NULL, cbHandle=NULL, crHandle=NULL;

	handle = (NX_VID_MEMORY_HANDLE)calloc(sizeof(NX_VID_MEMORY_INFO), 1);
	if( NULL == handle )
	{
		ErrMsg("Memory info allocation failed\n");
		goto Error_Exit;
	}

	lWidth  = ALIGN(width,  32);
	lHeight = ALIGN(height, 16);
	lSize = lWidth * lHeight;
	switch( fourCC )
	{
		case FOURCC_MVS0:
			cWidth  = ALIGN(width/2,  16);
			cHeight = ALIGN(height/2, 16);
			// cWidth  = lWidth/2;
			// cHeight = lHeight/2;
			break;
		case FOURCC_MVS2:
		case FOURCC_H422:
			cWidth  = ALIGN(width/2, 16);
			cHeight = lHeight;
			break;
		case FOURCC_V422:
			cWidth  = lWidth;
			cHeight = ALIGN(height/2, 16);
			break;
		case FOURCC_MVS4:
			cWidth  = lWidth;
			cHeight = lHeight;
			break;
		case FOURCC_NV12:
		case FOURCC_NV21:
			cWidth  = lWidth;
			cHeight = lHeight/2;
			break;
		case FOURCC_GRAY:
			cWidth  = 0;
			cHeight = 0;
			break;
		default:
			ErrMsg("Unknown fourCC type.\n");
			goto Error_Exit;
	}
	cSize = cWidth * cHeight;

#ifdef EN_MULTIPLE_PLANE
	luHandle = NX_AllocateMemory(lSize, align);
	if( NULL == luHandle )
	{
		ErrMsg("NX_AllocateMemory failed!!\n");
		goto Error_Exit;
	}

	if( cWidth > 0 )
	{
		switch(fourCC)
		{
			case FOURCC_NV12:
			case FOURCC_NV21:
				cbHandle = NX_AllocateMemory(cSize, align);
				if( NULL==cbHandle )
				{
					ErrMsg("NX_AllocateMemory failed!!\n");
					goto Error_Exit;
				}
				break;
			default:
				cbHandle = NX_AllocateMemory(cSize, align);
				crHandle = NX_AllocateMemory(cSize, align);
				if( NULL==cbHandle || NULL==crHandle )
				{
					ErrMsg("NX_AllocateMemory failed!!\n");
					goto Error_Exit;
				}
				break;
		}
	}
	handle->privateDesc[0] 	= luHandle;
	handle->privateDesc[1] 	= cbHandle;
	handle->privateDesc[2] 	= crHandle;
	handle->align 			= align;
	handle->memoryMap		= memMap;
	handle->fourCC			= fourCC;
	handle->imgWidth		= width;
	handle->imgHeight		= height;
	handle->luPhyAddr		= luHandle->phyAddr;
	handle->luVirAddr		= luHandle->virAddr;
	handle->luStride		= lWidth;
	if( cbHandle )
	{
		handle->cbPhyAddr		= cbHandle->phyAddr;
		handle->cbVirAddr		= cbHandle->virAddr;
		handle->cbStride		= cWidth;
	}
	if( crHandle )
	{
		handle->crPhyAddr		= crHandle->phyAddr;
		handle->crVirAddr		= crHandle->virAddr;
		handle->crStride		= cWidth;
	}
#else
	luHandle = NX_AllocateMemory(lSize+cSize*2, align);
	handle->privateDesc[0] 	= luHandle;
	handle->privateDesc[1] 	= 0;
	handle->privateDesc[2] 	= 0;
	handle->align 			= align;	//	Start Address Align
	handle->memoryMap		= memMap;
	handle->fourCC			= fourCC;
	handle->imgWidth		= width;
	handle->imgHeight		= height;
	handle->luPhyAddr		= luHandle->phyAddr;
	handle->luVirAddr		= luHandle->virAddr;
	handle->luStride		= lWidth;

	handle->cbPhyAddr		= luHandle->phyAddr + lSize;
	handle->cbVirAddr		= luHandle->virAddr + lSize;
	handle->cbStride		= cWidth;

	if( fourCC == FOURCC_NV12 || fourCC == FOURCC_NV21 )
	{
		//	Write cb Address & Value
		handle->crPhyAddr		= handle->cbPhyAddr;
		handle->crVirAddr		= handle->cbVirAddr;
		handle->crStride		= cWidth;
	}
	else
	{
		handle->crPhyAddr		= handle->cbPhyAddr + cSize;
		handle->crVirAddr		= handle->cbVirAddr + cSize;
		handle->crStride		= cWidth;
	}
#endif
	return handle;

Error_Exit:
	if( handle )
	{
		if( NULL != luHandle )	NX_FreeMemory( luHandle );
		if( NULL != cbHandle )	NX_FreeMemory( cbHandle );
		if( NULL != crHandle )	NX_FreeMemory( crHandle );
		free(handle);
	}
	return handle;
}

//
//	For Interlace Camera
//
NX_VID_MEMORY_HANDLE NX_VideoAllocateMemory2( int align, int width, int height, int memMap, int fourCC )
{
	int lWidth, lHeight;
	int cWidth, cHeight;
	NX_VID_MEMORY_HANDLE handle = NULL;
	NX_MEMORY_HANDLE luHandle=NULL, cbHandle=NULL, crHandle=NULL;

	handle = (NX_VID_MEMORY_HANDLE)malloc(sizeof(NX_VID_MEMORY_INFO));
	if( NULL == handle )
	{
		ErrMsg("Memory info allocation failed\n");
		goto Error_Exit;
	}

	lWidth = ALIGN(width, 64);
	lHeight = ALIGN(height, 16);

	switch( fourCC )
	{
		case FOURCC_MVS0:
			cWidth = ALIGN(lWidth/2, 64);
			cHeight = lHeight/2;
			break;
		case FOURCC_MVS2:
			cWidth = lWidth/2;
			cHeight = ALIGN(height, 16);
			break;
		case FOURCC_MVS4:
			cWidth = ALIGN(width, 16);
			cHeight = ALIGN(height, 16);
			break;
		default:
			ErrMsg("Unknown fourCC type.\n");
			goto Error_Exit;

	}

	luHandle = NX_AllocateMemory(lWidth*lHeight, align);
	if( NULL == luHandle )
	{
		ErrMsg("NX_AllocateMemory failed!!\n");
		goto Error_Exit;
	}

	if( cWidth > 0 )
	{
		cbHandle = NX_AllocateMemory(cWidth*cHeight, align);
		crHandle = NX_AllocateMemory(cWidth*cHeight, align);
		if( NULL==cbHandle || NULL==crHandle )
		{
			ErrMsg("NX_AllocateMemory failed!!\n");
			goto Error_Exit;
		}
	}

	handle->privateDesc[0] 	= luHandle;
	handle->privateDesc[1] 	= cbHandle;
	handle->privateDesc[2] 	= crHandle;
	handle->align 			= align;
	handle->memoryMap		= memMap;
	handle->fourCC			= fourCC;
	handle->imgWidth		= width;
	handle->imgHeight		= height;
	handle->luPhyAddr		= luHandle->phyAddr;
	handle->luVirAddr		= luHandle->virAddr;
	handle->luStride		= lWidth;
	handle->cbPhyAddr		= cbHandle->phyAddr;
	handle->cbVirAddr		= cbHandle->virAddr;
	handle->cbStride		= cWidth;
	handle->crPhyAddr		= crHandle->phyAddr;
	handle->crVirAddr		= crHandle->virAddr;
	handle->crStride		= cWidth;

	return handle;

Error_Exit:
	if( handle )
	{
		if( NULL != luHandle )	NX_FreeMemory( luHandle );
		if( NULL != cbHandle )	NX_FreeMemory( cbHandle );
		if( NULL != crHandle )	NX_FreeMemory( crHandle );
		free(handle);
	}
	return handle;
}

void NX_FreeVideoMemory( NX_VID_MEMORY_HANDLE handle )
{
	if( handle )
	{
		if( NULL != handle->privateDesc[0] )	NX_FreeMemory( handle->privateDesc[0] );
		if( NULL != handle->privateDesc[1] )	NX_FreeMemory( handle->privateDesc[1] );
		if( NULL != handle->privateDesc[2] )	NX_FreeMemory( handle->privateDesc[2] );
		free(handle);
	}
}

#ifdef ANDROID

int NX_PrivateHandleToVideoMemory( struct private_handle_t const *handle, NX_VID_MEMORY_INFO *memInfo )
{
	int ion_fd, ret, vstride;

	vstride = ALIGN(handle->height, 16);

	ion_fd = ion_open();
    ret = ion_get_phys(ion_fd, handle->share_fd, (long unsigned int *)&memInfo->luPhyAddr);

	if( ret <0 || ion_fd < 0 )
	{
		return -1;
	}

	memset(memInfo, 0, sizeof(NX_VID_MEMORY_INFO));
	memInfo->fourCC    = FOURCC_MVS0;
	memInfo->imgWidth  = handle->width;
	memInfo->imgHeight = handle->height;
	memInfo->cbPhyAddr = memInfo->luPhyAddr + handle->stride * vstride;
	memInfo->crPhyAddr = memInfo->cbPhyAddr + ALIGN((handle->stride>>1),16) * ALIGN((vstride>>1),16);
	memInfo->luStride  = handle->stride;
	memInfo->cbStride  =
	memInfo->crStride  = handle->stride >> 1;;
	close( ion_fd );
	return 0;
}

#endif
