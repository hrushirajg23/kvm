#ifndef RK_COMPATIBILITY_H
#define RK_COMPATIBILITY_H

#include <rockchip/rk_type.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_err.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/mpp_task.h>
#include <rockchip/mpp_meta.h>

// Return codes
#ifndef RK_SUCCESS
#define RK_SUCCESS MPP_OK
#endif
#ifndef RK_FAILURE
#define RK_FAILURE MPP_NOK
#endif
// Logging Shim
#include <stdio.h>
#ifndef RK_LOGE
#define RK_LOGE(fmt, ...) fprintf(stderr, "[RK_LOGE] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef RK_LOGW
#define RK_LOGW(fmt, ...) fprintf(stderr, "[RK_LOGW] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef RK_LOGI
#define RK_LOGI(fmt, ...) fprintf(stdout, "[RK_LOGI] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef RK_LOGD
#define RK_LOGD(fmt, ...) fprintf(stdout, "[RK_LOGD] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef RK_LOGV
#define RK_LOGV(fmt, ...) fprintf(stdout, "[RK_LOGV] " fmt "\n", ##__VA_ARGS__)
#endif

// Types
#define MB_BLK MppBuffer
#define MB_POOL MppBufferGroup
#define MB_INVALID_POOLID NULL
#define RK_NULL NULL
#ifndef RK_TRUE
#define RK_TRUE 1
#endif
#ifndef RK_FALSE
#define RK_FALSE 0
#endif
#ifndef RK_U32
#define RK_U32 mpp_used_u32
typedef unsigned int mpp_used_u32;
#endif
#ifndef RK_U64
#define RK_U64 mpp_used_u64
typedef unsigned long long mpp_used_u64;
#endif
typedef int RK_S32;
typedef int RK_BOOL; // Added RK_BOOL typedef

// Enums
#define RK_VIDEO_ID_AVC MPP_VIDEO_CodingAVC
#define RK_FMT_YUV422_YUYV MPP_FMT_YUV422_YUYV
#define COMPRESS_MODE_NONE 0 // No direct equivalent in MPP_FMT, handled by frame config usually
#define MIRROR_NONE 0 

// Encoder Attributes Configuration Structures (Legacy shim)
typedef enum {
    VENC_RC_MODE_H264CBR = 1,
    VENC_RC_MODE_H264VBR,
    VENC_RC_MODE_H264AVBR,
    VENC_RC_MODE_MJPEGCBR,
    VENC_RC_MODE_MJPEGVBR,
    VENC_RC_MODE_H265CBR,
    VENC_RC_MODE_H265VBR,
    VENC_RC_MODE_H265AVBR,
} VENC_RC_MODE_E;

typedef struct {
    RK_U32 u32Gop;
    RK_U32 u32StatTime;
    RK_U32 u32SrcFrameRateNum;
    RK_U32 u32SrcFrameRateDen;
    RK_U32 u32DstFrameRateNum;
    RK_U32 u32DstFrameRateDen;
    RK_U32 u32BitRate;
    RK_U32 u32MaxBitRate; // Added for VBR
} VENC_H264_CBR_S;

typedef VENC_H264_CBR_S VENC_H264_VBR_S; // Shim

typedef struct {
    VENC_RC_MODE_E enRcMode;
    union {
        VENC_H264_CBR_S stH264Cbr;
        VENC_H264_VBR_S stH264Vbr;
    };
} VENC_RC_ATTR_S;

typedef struct {
    MppCodingType enType;
    MppFrameFormat enPixelFormat;
    RK_U32 u32Profile;
    RK_U32 u32PicWidth;
    RK_U32 u32PicHeight;
    RK_U32 u32VirWidth;
    RK_U32 u32VirHeight;
    RK_U32 u32StreamBufCnt;
    RK_U32 u32BufSize;
    RK_U32 enMirror;
} VENC_ATTR_S;

typedef struct {
    VENC_ATTR_S stVencAttr;
    VENC_RC_ATTR_S stRcAttr;
} VENC_CHN_ATTR_S;

typedef struct {
    RK_S32 s32RecvPicNum;
} VENC_RECV_PIC_PARAM_S;

// Stream structures
typedef struct {
    RK_U64 u64PTS;
    RK_U32 u32Len;
    MB_BLK pMbBlk; // Keep for compatibility, but might be NULL or fake
    RK_U8 *pu8Addr;
    RK_BOOL bStreamEnd;
    RK_U64 u64DTS;
    void *internal_packet; // Added for MPP shim
} VENC_PACK_S;

typedef struct {
    VENC_PACK_S *pstPack;
    RK_U32 u32PackCount;
    RK_U32 u32Seq;
} VENC_STREAM_S;

// Frame info
typedef struct {
    RK_U32 u32Width;
    RK_U32 u32Height;
    RK_U32 u32VirWidth;
    RK_U32 u32VirHeight;
    MppFrameFormat enPixelFormat;
    RK_U32 u32TimeRef;
    RK_U64 u64PTS;
    RK_U32 u32FrameFlag;
    RK_U32 enCompressMode;
    MB_BLK pMbBlk;
} VIDEO_FRAME_INFO_S_INNER;

typedef struct {
    VIDEO_FRAME_INFO_S_INNER stVFrame;
} VIDEO_FRAME_INFO_S;

// Buffer Pool Config
typedef enum {
    MB_ALLOC_TYPE_DMA,
    MB_ALLOC_TYPE_NORMAL
} MB_ALLOC_TYPE_E;

typedef struct {
    RK_U64 u64MBSize;
    RK_U32 u32MBCnt;
    MB_ALLOC_TYPE_E enAllocType;
    RK_BOOL bPreAlloc;
} MB_POOL_CONFIG_S;

// H264 Profile Shim
#define H264E_PROFILE_HIGH 100

// Function Declarations (Shims to implemented in video.c)
RK_S32 RK_MPI_SYS_Init();
MB_POOL RK_MPI_MB_CreatePool(MB_POOL_CONFIG_S *pstMbPoolCfg);
RK_S32 RK_MPI_MB_DestroyPool(MB_POOL pool);
MB_BLK RK_MPI_MB_GetMB(MB_POOL pool, RK_U64 u64Size, RK_BOOL bBlock);
RK_S32 RK_MPI_MB_ReleaseMB(MB_BLK blk);
RK_S32 RK_MPI_MB_Handle2Fd(MB_BLK blk);
void *RK_MPI_MB_Handle2VirAddr(MB_BLK blk);
MB_BLK RK_MPI_MMZ_Fd2Handle(RK_S32 fd);

RK_S32 RK_MPI_VENC_CreateChn(RK_S32 VencChn, const VENC_CHN_ATTR_S *pstAttr);
RK_S32 RK_MPI_VENC_DestroyChn(RK_S32 VencChn);
RK_S32 RK_MPI_VENC_StartRecvFrame(RK_S32 VencChn, const VENC_RECV_PIC_PARAM_S *pstRecvParam);
RK_S32 RK_MPI_VENC_StopRecvFrame(RK_S32 VencChn);
RK_S32 RK_MPI_VENC_SendFrame(RK_S32 VencChn, const VIDEO_FRAME_INFO_S *pstFrame, RK_S32 s32MilliSec);
RK_S32 RK_MPI_VENC_GetStream(RK_S32 VencChn, VENC_STREAM_S *pstStream, RK_S32 s32MilliSec);
RK_S32 RK_MPI_VENC_ReleaseStream(RK_S32 VencChn, VENC_STREAM_S *pstStream);

#endif
