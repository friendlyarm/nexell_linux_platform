//
//  Nexel Video En/Decoder API
//


#ifndef __NX_VIDEO_API_H__
#define __NX_VIDEO_API_H__

#include <nx_alloc_mem.h>
#include <vpu_types.h>
#include <stdint.h>

#define MAX_DEC_FRAME_BUFFERS       30
#define ENC_BITSTREAM_BUFFER        (4*1024*1024)

typedef struct  NX_VIDEO_ENC_INFO   *NX_VID_ENC_HANDLE;
typedef struct  NX_VIDEO_DEC_INFO   *NX_VID_DEC_HANDLE;


// Video Codec Type ( API Level )
typedef enum {
	//  Decoders
	NX_AVC_DEC      = 0x00,         // H.264( AVC )
	NX_VC1_DEC      = 0x01,         // WMV9
	NX_MP2_DEC      = 0x02,         // Mpeg2 Video
	NX_MP4_DEC      = 0x03,         // Mpeg4 Video
	NX_H263_DEC     = 0x04,         // H.263
	NX_DIV3_DEC     = 0x05,         // Divx 3.11(MS Mpeg4 V3)
	NX_RV_DEC       = 0x06,         // Real Video
	NX_THEORA_DEC   = 0x07,         // Theora
	NX_VP8_DEC      = 0x08,         // VP8
	NX_JPEG_DEC     = 0x09,         // JPEG
	NX_HEVC_DEC		= 0x0A,			// H.265( HEVC )

	//  Encoders
	NX_AVC_ENC      = 0x10,
	NX_MP4_ENC      = 0x12,
	NX_H263_ENC     = 0x13,
	NX_JPEG_ENC     = 0x20,         // JPEG Encoder
} VID_TYPE_E;

typedef enum{
	VID_ERR_NOT_ALLOC_BUFF          = -28,
	VID_ERR_NOT_ENOUGH_STREAM       = -27,
	VID_ERR_NO_DEC_FRAME            = -26,
	VID_ERR_NEED_MORE_BUF           = -25,
	VID_ERR_STRM_FULL               = -24,  // Bitstream Full
	VID_ERR_SRAM                    = -23,  // VPU SRAM Configruation Failed
	VID_ERR_INST                    = -22,  // VPU Have No Instance Space
	VID_ERR_BUSY                    = -21,  // VPU H/W Busy
	VID_ERR_TIMEOUT                 = -20,  // VPU Wait Timeout
	VID_ERR_MEM_ACCESS              = -19,  // Memory Access Violation
	VID_ERR_NOT_SUPPORT             = -7,
	VID_ERR_CHG_PARAM               = -6,   // VPU Not changed
	VID_ERR_WRONG_SEQ               = -5,   // Wrong Sequence
	VID_ERR_PARAM                   = -4,   // VPU Invalid Parameter
	VID_ERR_RUN                     = -3,
	VID_ERR_INIT                    = -2,   // VPU Not Initialized
	VID_ERR_FAIL                    = -1,   // General operation failed
	VID_ERR_NONE                    = 0,
	VID_NEED_STREAM                 = 1,    // Need More Stream
} VID_ERROR_E;

enum{
	PIC_TYPE_I                      = 0,    // Include  IDR in h264
	PIC_TYPE_P                      = 1,
	PIC_TYPE_B                      = 2,
	PIC_TYPE_VC1_BI                 = 2,
	PIC_TYPE_VC1_B                  = 3,
	PIC_TYPE_D                      = 3,    // D picture in mpeg2, and is only composed of DC codfficients
	PIC_TYPE_S                      = 3,    // S picture in mpeg4, and is an acronym of Sprite. and used for GMC
	PIC_TYPE_VC1_P_SKIP             = 4,
	PIC_TYPE_MP4_P_SKIP_NOT_CODED   = 4,    // Not Coded P Picture at mpeg4 packed mode
	PIC_TYPE_SKIP                   = 5,
	PIC_TYPE_IDR                    = 6,
	PIC_TYPE_UNKNOWN                = 0xff,
};

enum {
	DEC_OPT_CHROMA_INTERLEAVE       = 0x00000001,
};

typedef enum {
	VID_CHG_GOP                     = 1,            // GOP
	VID_CHG_BITRATE                 = (1 << 2),     // Bit Rate
	VID_CHG_FRAMERATE               = (1 << 3),     // Frame Rate
	VID_CHG_INTRARF                 = (1 << 4),     // Intra Refresh
	VID_CHG_MAX_QP                  = (1 << 7),     // Maximum quantization parameter
	VID_CHG_DISABLE_SKIP            = (1 << 8),     // Disable skip frame mode
	VID_CHG_VBV                     = (1 << 9),     // Reference decoder buffer size in byte
} VID_ENC_CHG_PARAM_E;

enum {
	NONE_FIELD                      = 0,
	FIRST_FIELD                     = 0,
	SECOND_FIELD                    = 1
};

enum {
	DECODED_FRAME                   = 0,
	DISPLAY_FRAME                   = 1
};


//
//  Encoder Specific APIs
//
typedef struct tNX_VID_ENC_IN {
	NX_VID_MEMORY_HANDLE pImage;            // Original captured frame's pointer
	uint64_t timeStamp;                     // Time stamp
	int32_t forcedIFrame;                   // Flag of forced intra frame
	int32_t forcedSkipFrame;                // Flag of forced skip frame
	int32_t quantParam;                     // User quantization parameter (It is valid only when VBR.)
}NX_VID_ENC_IN;

typedef struct tNX_VID_ENC_OUT{
	uint8_t *outBuf;                        // Output buffer's pointer
	int32_t bufSize;                        // OutBuf's size(input) and filled size(output)
	int32_t frameType;                      // Frame type
	int32_t width;                          // Encoded image width
	int32_t height;                         // Encoded image height
	NX_VID_MEMORY_INFO ReconImg;            // Reconstructed image's pointer
}NX_VID_ENC_OUT;

typedef struct tNX_VID_ENC_JPEG_PARAM{
	int32_t configFlag;
	uint8_t huffVal[4][162];
	uint8_t huffBits[4][256];
	uint8_t qMatTab[4][64];
	uint8_t cInfoTab[4][6];
}NX_VID_ENC_JPEG_PARAM;

typedef struct tNX_VID_ENC_INIT_PARAM{
	int32_t width;                          // Width of image
	int32_t height;                         // Height of image
	int32_t gopSize;                        // Size of key frame interval
	int32_t fpsNum;                         // Frame per second
	int32_t fpsDen;

	// Rate Control Parameters (They are valid only when enableRC is TRUE[CBR])
	int32_t enableRC;                       // En/Disable rate control, 0(Variable Bit Rate mode) or 1(Constant Bit Rate mode)
	int32_t RCAlgorithm;                    // 0 : Chips & Media Rate Control Algorithm, 1 : Nexell Rate Control Algorithm
	uint32_t bitrate;                       // Target bitrate in bits per second
	int32_t maximumQp;                      // Maximum quantization parameter
	int32_t disableSkip;                    // Disable skip frame mode
	int32_t RCDelay;                        // Valid value is 0 ~ 0x7FFF
	                                        // 0 does not check reference decoder buffer delay constraint.
	uint32_t rcVbvSize;                     // Reference decoder buffer size in byte
	                                        // Default value is 2sec if RCAlgorithm is 1.
	                                        // This valid is ignored if RCAlgorithm is 0 & RCDelay is 0.
	int32_t gammaFactor;                    // User gamma factor
	                                        // It is valid only when RCAlgorithm is 0.
	int32_t RcMode;

	int32_t initialQp;                     	// Initial quantization parameter
	                                        // It is computed if enableRC is 1 & RCAlgorithm is 1 and the value is 0.

	int32_t numIntraRefreshMbs;             // Intra MB refresh number.(Cyclic Intra Refresh)
	int32_t searchRange;                    // search range of motion estimaiton (0 : 128 x 64, 1 : 64 x 32, 2 : 32 x 16, 3 : 16 x 16)

	//  Input Buffer Format
	int32_t chromaInterleave;               // 0 : disable, 1 : enable

	int32_t rotAngle;
	int32_t mirDirection;                   // 0 : not mir, 1 : horizontal mir, 2 : vertical mir, 3 : horizontal & vertical mir

	//  for AVC Encoder
	int32_t enableAUDelimiter;              // Insert Access Unit Delimiter before NAL unit.

	//  for JPEG Specific Parameter
	int32_t jpgQuality;                     // 1~100
}NX_VID_ENC_INIT_PARAM;

typedef struct tNX_VID_ENC_CHG_PARAM{
    int32_t chgFlg;
	int32_t gopSize;                        // Size of key frame interval
	int32_t bitrate;                        // Target bitrate in bits/second
	int32_t fpsNum;                         // Frame per second
	int32_t fpsDen;
	int32_t maximumQp;                      // Maximum quantization parameter
	int32_t disableSkip;                    // Disable skip frame mode
	uint32_t rcVbvSize;                     // Reference decoder buffer size in byte.
	                                        // The value shall be set when bitrate is changed in Nexell RC Algorithm.
	                                        // Default value is 2sec if RCAlgorithm is 1. (The value is ignored when RCAlgorithm is 0 or enableRC is 0)

	int32_t numIntraRefreshMbs;             // Intra MB refresh number.(Cyclic Intra Refresh)
}NX_VID_ENC_CHG_PARAM;


//
//  Decoder Specific APIs
//
typedef struct tNX_VID_DEC_IN{
	uint8_t *strmBuf;                       // A compressed stream's pointer
	int32_t strmSize;                       // A compressed stream's size
	uint64_t timeStamp;                     // Time stamp
	int32_t eos;

	// for JPEG Decoder
	int32_t downScaleWidth;                 // 0 : No scaling, 1 : 1/2 down scaling, 2 : 1/4 down scaling, 3 : 1/8 down scaling
	int32_t downScaleHeight;                // 0 : No scaling, 1 : 1/2 down scaling, 2 : 1/4 down scaling, 3 : 1/8 down scaling
}NX_VID_DEC_IN;

typedef struct tNX_VID_DEC_OUT{
	NX_VID_MEMORY_INFO outImg;              // Decoded frame's pointer
	int32_t outImgIdx;                      // Display Index
	int32_t outDecIdx;                      // Decode Index
	int32_t width;
	int32_t height;
	int32_t picType[2];                     // Picture Type

	uint64_t timeStamp[2];                  // Time stamp
	uint32_t strmReadPos;                   // Remained bitstream buffer size
	uint32_t strmWritePos;                  // Remained bitstream buffer size

	int32_t isInterlace;                    // 0 : progressive, 1 : interlace
	int32_t topFieldFirst;                  // 0 : top field first, 1 : bottom field first

	int32_t outFrmReliable_0_100[2];        // Percentage of MB's are reliable ranging from 0[all damage] to 100 [all clear]

	// for VC1 Decoder
	int32_t multiResolution;                // 0 : non multi-resulution, 1 : horizontal scale is half, 2 : vertical scale is half, 3 : horizontal & vertical scale is half

	// for VP Decoder
	int32_t upSampledWidth;                 // 0 : No upscaling, other : upscaling width
	int32_t upSampledHeight;                // 0 : No upscaling, other : upscaling height
}NX_VID_DEC_OUT;

typedef struct tNX_VID_SEQ_IN{
	uint8_t *seqInfo;                       // Sequence header's pointer
	int32_t seqSize;                        // Sequence header's size
	int32_t width;
	int32_t height;

	// for External Buffer ( Optional )
	NX_VID_MEMORY_HANDLE *pMemHandle;       // Frame buffer for external buffer mode
	int32_t numBuffers;                     // Number of external frame buffer

	int32_t addNumBuffers;                  // Add to minimum frame buffer number (total frame buffer number = minimum number + addNumBuffers)
	                                        // This value is valid when External Buffer is not used.
	int32_t disableOutReorder;              // 1 : Decoding Order, 0 : Display Order

	// for MPEG2, MPEG4, Divx3.11 Decoder
	int32_t enablePostFilter;               // 1 : Deblock filter, 2 : Deringing filter, 3 : Deblock & Deringing filter, 0 : Disable post filter

	// for MPEG2 Decoder
	int32_t enableUserData;

	// for JPEG Decoder
	int32_t thumbnailMode;                  // 0 : jpeg mode, 1 : thumbnail mode
}NX_VID_SEQ_IN;

typedef struct tNX_VID_SEQ_OUT{
	int32_t minBuffers;                     // Needed minimum number of decoded frames
	int32_t numBuffers;
	int32_t width;
	int32_t height;
	int32_t frameBufDelay;
	int32_t isInterlace;                    // 0 : Progressive YUV, 1 : interlaced YUV

	int32_t frameRateNum;                   // Frame Rate Numerator
	int32_t frameRateDen;                   // Frame Rate Denominator (-1 : no information)

	//  for User Data( MPEG2 Decoder Only )
	int32_t userDataNum;
	int32_t userDataSize;
	int32_t userDataBufFull;

	// for VP8 Decoder
	int32_t vp8ScaleWidth;
	int32_t vp8ScaleHeight;

	int32_t unsupportedFeature;             // Flag to inform the feature is unsupported in NX Codec

	int32_t imgFourCC;                      // FourCC according to decoded image type (ASCII hexadecimal representation of four characters)
	int32_t thumbnailWidth;                 // Width of thumbnail image
	int32_t thumbnailHeight;                // Height of thumbnail image
}NX_VID_SEQ_OUT;

typedef struct tNX_VID_VERSION{
    int32_t iMajor;
    int32_t iMinor;
    int32_t iPatch;
	int32_t iReserved;
} NX_VID_VERSION;



#ifdef __cplusplus
extern "C" {
#endif

//
//  Encoder
//
NX_VID_ENC_HANDLE NX_VidEncOpen( VID_TYPE_E eCodecType, int32_t *piInstanceIdx );
VID_ERROR_E NX_VidEncClose( NX_VID_ENC_HANDLE hEnc );
VID_ERROR_E NX_VidEncInit( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_INIT_PARAM *pstParam );
VID_ERROR_E NX_VidEncGetSeqInfo( NX_VID_ENC_HANDLE hEnc, uint8_t *pbySeqBuf, int32_t *piSeqBufSize );
VID_ERROR_E NX_VidEncEncodeFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_IN *pstEncIn, NX_VID_ENC_OUT *pstEncOut );
VID_ERROR_E NX_VidEncChangeParameter( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_CHG_PARAM *pstChgParam );

//
//  Decoder
//
NX_VID_DEC_HANDLE NX_VidDecOpen( VID_TYPE_E eCodecType, uint32_t uMp4Class, int32_t iOptions, int32_t *piInstanceIdx  );
VID_ERROR_E NX_VidDecClose( NX_VID_DEC_HANDLE hDec );
VID_ERROR_E NX_VidDecParseVideoCfg(NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn, NX_VID_SEQ_OUT *pstSeqOut);
VID_ERROR_E NX_VidDecInit(NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn);
VID_ERROR_E NX_VidDecDecodeFrame( NX_VID_DEC_HANDLE hDec, NX_VID_DEC_IN *pstDecIn, NX_VID_DEC_OUT *pstDecOut );
VID_ERROR_E NX_VidDecClrDspFlag( NX_VID_DEC_HANDLE hDec, NX_VID_MEMORY_HANDLE hFrameBuf, int32_t iFrameIdx );
VID_ERROR_E NX_VidDecFlush( NX_VID_DEC_HANDLE hDec );
VID_ERROR_E NX_VidDecGetFrameType( VID_TYPE_E eCodecType, NX_VID_DEC_IN *pstDecIn, int32_t *piFrameType );

//
//  Jpeg Encoder APIs
//  Usage : NX_VidEncOpen() --> NX_VidEncInit() NX_VidEncJpegRunFrame() --> NX_VidEncClose()
//
VID_ERROR_E NX_VidEncJpegGetHeader( NX_VID_ENC_HANDLE hEnc, uint8_t *pbyJpgHeader, int32_t *piHeaderSize );
VID_ERROR_E NX_VidEncJpegRunFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_MEMORY_HANDLE hInImage, NX_VID_ENC_OUT *pstEncOut );

VID_ERROR_E NX_VidGetVersion( NX_VID_VERSION *pstVersion );

#ifdef __cplusplus
}
#endif

#endif  //  __NX_VPU_API_H__
