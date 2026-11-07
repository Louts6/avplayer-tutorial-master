//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_CUDAFRAMERENDERER_H
#define LEARNAV_CUDAFRAMERENDERER_H

#include <memory>

namespace av {

/**
 * 把 FFmpeg 的 CUDA 硬件解码帧（NV12）直接转换到 OpenGL 纹理，全程留在 GPU：
 *
 *   CUDA 显存(Y/UV) --cuMemcpy2D(device->array)--> GL 纹理 --YUV→RGB shader--> RGBA 纹理
 *
 * 不经过 CPU，因此不存在 av_hwframe_transfer_data(GPU→CPU) 与 glTexImage2D(CPU→GPU)
 * 的往返拷贝。仅 Windows + NVIDIA + 已安装 CUDA Toolkit 时可用；其余情况 IsAvailable()
 * 返回 false，由调用方回退到原有 CPU 路径。
 *
 * 所有方法必须在持有当前 OpenGL 上下文的线程（VideoPipeline 线程）调用。
 */
class CudaFrameRenderer {
public:
    CudaFrameRenderer();
    ~CudaFrameRenderer();

    /** 当前环境是否支持 CUDA-GL 互操作（nvcuda.dll 可用且存在 CUDA 设备）。 */
    static bool IsAvailable();

    /**
     * 将 CUDA 硬件帧（AV_PIX_FMT_CUDA / NV12）转换并渲染到 outputTexture。
     * @param avFrame       指向 AVFrame 的指针
     * @param outputTexture 已创建的 RGBA 纹理，尺寸需与帧一致
     * @return true 成功；false 不支持或失败，调用方应回退 CPU 路径
     */
    bool RenderHardwareFrame(void* avFrame, unsigned int outputTexture, int width, int height);

    /** 释放 GL/CUDA 资源（需在 GL 线程调用）。 */
    void Release();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace av

#endif  // LEARNAV_CUDAFRAMERENDERER_H
