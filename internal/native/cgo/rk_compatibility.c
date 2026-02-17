#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rk_compatibility.h"
// #include <rockchip/mpp_env.h> // Internal?
// #include <rockchip/mpp_mem.h> // Internal?
// #include <rockchip/mpp_log.h> // Internal?
// #include <rockchip/mpp_common.h>

// Globals to manage "channels"
#define MAX_CHANNELS 4
static MppCtx g_ctxs[MAX_CHANNELS] = {NULL};
static MppApi *g_mpis[MAX_CHANNELS] = {NULL};
// static MppEncCfg g_cfgs[MAX_CHANNELS] = {NULL}; // Need config objects?

// --- System Init ---
RK_S32 RK_MPI_SYS_Init() {
    // Standard MPP doesn't require global init usually, but we can set log levels
    // mpp_env_set_u32("mpp_debug", -1);
    return RK_SUCCESS;
}

// --- Memory Block (MB) Wrappers ---
// Mapping MB_POOL to MppBufferGroup
MB_POOL RK_MPI_MB_CreatePool(MB_POOL_CONFIG_S *pstMbPoolCfg) {
    if (!pstMbPoolCfg) return NULL;
    MppBufferGroup group = NULL;
    // Mode depends on enAllocType. MPP_BUFFER_TYPE_DRM is common for DMA.
    // Legacy MB_ALLOC_TYPE_DMA -> MPP_BUFFER_TYPE_DRM ?
    // Or just let MPP decide usually:
    MPP_RET ret = mpp_buffer_group_get(&group, MPP_BUFFER_TYPE_DRM, MPP_BUFFER_INTERNAL, NULL, __FUNCTION__);
    if (ret != MPP_OK) {
        printf("Failed to get buffer group: %d\n", ret);
        return NULL;
    }
    // Pre-alloc not directly supported in same way, but group is created.
    // If bPreAlloc is true, we might want to verify limits, but MPP handles valid.
    return (MB_POOL)group;
}

RK_S32 RK_MPI_MB_DestroyPool(MB_POOL pool) {
    if (!pool) return RK_FAILURE;
    mpp_buffer_group_put((MppBufferGroup)pool);
    return RK_SUCCESS;
}

MB_BLK RK_MPI_MB_GetMB(MB_POOL pool, RK_U64 u64Size, RK_BOOL bBlock) {
    if (!pool) return NULL;
    MppBuffer buffer = NULL;
    // bBlock ignored? MPP doesn't have blocking get usually?
    MPP_RET ret = mpp_buffer_get((MppBufferGroup)pool, &buffer, u64Size);
    if (ret != MPP_OK) {
        printf("Failed to get buffer: %d\n", ret);
        return NULL;
    }
    return (MB_BLK)buffer;
}

RK_S32 RK_MPI_MB_ReleaseMB(MB_BLK blk) {
    if (!blk) return RK_FAILURE;
    mpp_buffer_put((MppBuffer)blk);
    return RK_SUCCESS;
}

RK_S32 RK_MPI_MB_Handle2Fd(MB_BLK blk) {
    if (!blk) return -1;
    return mpp_buffer_get_fd((MppBuffer)blk);
}

void *RK_MPI_MB_Handle2VirAddr(MB_BLK blk) {
    if (!blk) return NULL;
    return mpp_buffer_get_ptr((MppBuffer)blk);
}

MB_BLK RK_MPI_MMZ_Fd2Handle(RK_S32 fd) {
    // This is tricky. Importing an FD into an MppBuffer without a group?
    // MppBufferInfo info;
    // info.type = MPP_BUFFER_TYPE_DRM;
    // info.fd = fd;
    // ...
    // mpp_buffer_import needed.
    MppBuffer buffer = NULL;
    MppBufferInfo info;
    memset(&info, 0, sizeof(info));
    info.type = MPP_BUFFER_TYPE_EXT_DMA; // External FD
    info.fd = fd;
    info.size = 1920*1080*3; // FIXME: Size is unknown! This is dangerous.
    // video.c usually calls this on captured frames.
    
    MPP_RET ret = mpp_buffer_import(&buffer, &info);
    if (ret != MPP_OK) {
        printf("Failed to import fd %d: %d\n", fd, ret);
        return NULL;
    }
    return (MB_BLK)buffer;
}

// --- VENC Wrappers ---

RK_S32 RK_MPI_VENC_CreateChn(RK_S32 VencChn, const VENC_CHN_ATTR_S *pstAttr) {
    if (VencChn < 0 || VencChn >= MAX_CHANNELS) return RK_FAILURE;
    if (g_ctxs[VencChn] != NULL) return RK_FAILURE; // Already created

    MppCtx ctx = NULL;
    MppApi *mpi = NULL;
    MPP_RET ret = mpp_create(&ctx, &mpi);
    if (ret != MPP_OK) return RK_FAILURE;

    ret = mpp_init(ctx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC); // Assuming AVC/H.264 as per video.c
    if (ret != MPP_OK) {
        mpp_destroy(ctx);
        return RK_FAILURE;
    }

    // Configure Encoder
    MppEncCfg cfg = NULL;
    mpp_enc_cfg_init(&cfg);

    // Set format
    mpp_enc_cfg_set_s32(cfg, "prep:width", pstAttr->stVencAttr.u32PicWidth);
    mpp_enc_cfg_set_s32(cfg, "prep:height", pstAttr->stVencAttr.u32PicHeight);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", pstAttr->stVencAttr.u32VirWidth);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", pstAttr->stVencAttr.u32VirHeight);
    mpp_enc_cfg_set_s32(cfg, "prep:format", MPP_FMT_YUV422_YUYV); // Fixed assumption from video.c?

    // Set Rate Control
    mpp_enc_cfg_set_s32(cfg, "rc:mode", MPP_ENC_RC_MODE_VBR);
    mpp_enc_cfg_set_s32(cfg, "rc:bps_target", pstAttr->stRcAttr.stH264Vbr.u32BitRate * 1000); // kbps vs bps? video.c seems to pass raw value
    mpp_enc_cfg_set_s32(cfg, "rc:bps_max", pstAttr->stRcAttr.stH264Vbr.u32MaxBitRate * 1000);
    // mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", 0);
    // mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", pstAttr->stRcAttr.stH264Vbr.u32SrcFrameRateNum);

    // Apply config
    mpi->control(ctx, MPP_ENC_SET_CFG, cfg);
    mpp_enc_cfg_deinit(cfg);

    g_ctxs[VencChn] = ctx;
    g_mpis[VencChn] = mpi;

    return RK_SUCCESS;
}

RK_S32 RK_MPI_VENC_DestroyChn(RK_S32 VencChn) {
    if (VencChn < 0 || VencChn >= MAX_CHANNELS) return RK_FAILURE;
    if (g_ctxs[VencChn]) {
        mpp_destroy(g_ctxs[VencChn]);
        g_ctxs[VencChn] = NULL;
        g_mpis[VencChn] = NULL;
    }
    return RK_SUCCESS;
}

RK_S32 RK_MPI_VENC_StartRecvFrame(RK_S32 VencChn, const VENC_RECV_PIC_PARAM_S *pstRecvParam) {
    // MPP doesn't have explicit "StartRecv". Creation is enough?
    return RK_SUCCESS;
}

RK_S32 RK_MPI_VENC_StopRecvFrame(RK_S32 VencChn) {
    // Flush?
    // mpi->reset(ctx);
    return RK_SUCCESS;
}

RK_S32 RK_MPI_VENC_SendFrame(RK_S32 VencChn, const VIDEO_FRAME_INFO_S *pstFrame, RK_S32 s32MilliSec) {
    if (VencChn < 0 || VencChn >= MAX_CHANNELS) return RK_FAILURE;
    MppCtx ctx = g_ctxs[VencChn];
    MppApi *mpi = g_mpis[VencChn];
    
    // Convert VIDEO_FRAME_INFO_S to MppFrame
    // Actually, MPP expects us to put the frame into the context?
    // Standard flow: mpp_frame_init, set buffer, set info, mpi->encode_put_frame
    
    MppFrame frame = NULL;
    mpp_frame_init(&frame);
    mpp_frame_set_width(frame, pstFrame->stVFrame.u32Width);
    mpp_frame_set_height(frame, pstFrame->stVFrame.u32Height);
    mpp_frame_set_hor_stride(frame, pstFrame->stVFrame.u32VirWidth);
    mpp_frame_set_ver_stride(frame, pstFrame->stVFrame.u32VirHeight);
    mpp_frame_set_fmt(frame, pstFrame->stVFrame.enPixelFormat);
    
    // Set buffer
    mpp_frame_set_buffer(frame, (MppBuffer)pstFrame->stVFrame.pMbBlk);
    
    mpp_frame_set_pts(frame, pstFrame->stVFrame.u64PTS);
    
    MPP_RET ret = mpi->encode_put_frame(ctx, frame);
    mpp_frame_deinit(&frame);
    
    return (ret == MPP_OK) ? RK_SUCCESS : RK_FAILURE;
}

RK_S32 RK_MPI_VENC_GetStream(RK_S32 VencChn, VENC_STREAM_S *pstStream, RK_S32 s32MilliSec) {
    if (VencChn < 0 || VencChn >= MAX_CHANNELS) return RK_FAILURE;
    MppCtx ctx = g_ctxs[VencChn];
    MppApi *mpi = g_mpis[VencChn];
    
    if (!ctx || !mpi) return RK_FAILURE;

    MppPacket packet = NULL;
    MPP_RET ret = mpi->encode_get_packet(ctx, &packet);
    if (ret != MPP_OK || packet == NULL) {
        return RK_ERR_VENC_BUF_EMPTY;
    }

    void *ptr = mpp_packet_get_pos(packet);
    size_t len = mpp_packet_get_length(packet);
    RK_U64 pts = mpp_packet_get_pts(packet);
    
    if (pstStream->pstPack) {
        pstStream->pstPack->pu8Addr = ptr;
        pstStream->pstPack->u32Len = len;
        pstStream->pstPack->u64PTS = pts;
        pstStream->pstPack->pMbBlk = NULL; // We don't have a buffer block handle, just raw data
        pstStream->pstPack->internal_packet = packet; // Save for release
    } else {
        mpp_packet_deinit(&packet);
        return RK_FAILURE;
    }
    
    return RK_SUCCESS;
}

RK_S32 RK_MPI_VENC_ReleaseStream(RK_S32 VencChn, VENC_STREAM_S *pstStream) {
    if (!pstStream || !pstStream->pstPack) return RK_FAILURE;
    MppPacket packet = (MppPacket)pstStream->pstPack->internal_packet;
    if (packet) {
        mpp_packet_deinit(&packet);
        pstStream->pstPack->internal_packet = NULL;
    }
    return RK_SUCCESS;
}
