#ifndef __API_OSAPI_H__
#define __DRV_OSAPI_H__

#ifdef __cplusplus
extern "C"{
#endif


//////////////////////////////////////////////////////////////////////////////
//
//							Debug Message
//

#define	EN_DEBUG_MSG	
#ifndef NX_DTAG
#define	NX_DTAG			""
#endif

#ifdef ANDROID
#define	LOG_TAG		NX_DTAG
#endif

#if defined(__linux) || defined(linux) || defined(ANDROID) || defined(__LINUX__)
	#if defined(__KERNEL__)
		#include <linux/kernel.h>
		#define	nx_print_msg		printk
		#define	nx_print_err		printk
	#elif defined(ANDROID)
		#include <cutils/log.h>
		#define nx_print_msg		ALOGD
		#define	nx_print_err		ALOGE
	#else
		#include <stdio.h>
		#define	nx_print_msg		printf
		#define	nx_print_err		printf
	#endif // __linux or linux
#else	
	void nx_print_msg( char fmt,... );
	void nx_print_err( char fmt,... );
#endif	//	__linux || linux || ANDROID


#ifdef EN_DEBUG_MSG
	#ifdef ANDROID
		#define	NX_DbgMsg( COND, MSG )	do{ if(COND){ nx_print_msg MSG; } }while(0)
	#else
		#define	NX_DbgMsg( COND, MSG )	do{ if(COND){ nx_print_msg(NX_DTAG); nx_print_msg MSG; } }while(0)
	#endif
#if 0
	#define	FUNC_IN()				do{ nx_print_msg("%s() In\n",  __func__); }while(0)
	#define	FUNC_OUT()				do{ nx_print_msg("%s() Out\n", __func__); }while(0)
#else
	#define	FUNC_IN()				do{}while(0)
	#define	FUNC_OUT()				do{}while(0)
#endif
#else
	#define	NX_DbgMsg( COND, MSG )	do{}while(0)
	#define	FUNC_IN()				
	#define	FUNC_OUT()				
#endif

#ifdef ANDROID
	#define	NX_RelMsg( COND, MSG )	do{ if(COND){ nx_print_msg MSG; } }while(0)
	#define NX_ErrMsg( MSG )		nx_print_err MSG
#else
	#define	NX_RelMsg( COND, MSG )	do{ if(COND){ nx_print_msg(NX_DTAG); nx_print_msg MSG; } }while(0)
	#define NX_ErrMsg( MSG )		do										\
									{										\
										nx_print_err("%s%s(%d) : ",			\
											NX_DTAG, __FILE__, __LINE__);	\
										nx_print_err MSG;					\
									}while(0)
#endif
//
//
//
//////////////////////////////////////////////////////////////////////////////



#ifdef __cplusplus
};
#endif

#endif	//	__DRV_OSAPI_H__

