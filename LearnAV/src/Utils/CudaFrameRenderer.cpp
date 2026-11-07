//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "CudaFrameRenderer.h"

#include "Log.h"

#if defined(_WIN32) && defined(LEARNAV_HAS_CUDA_INTEROP)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cuda.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/pixfmt.h>
}

#include "GLUtils.h"

namespace av {

static const char* kTag = "CudaFrameRenderer";

namespace {

// CUDA Driver API 通过 nvcuda.dll 动态加载，避免工程对 CUDA 导入库产生链接依赖
using Fn_cuInit = CUresult (*)(unsigned int);
using Fn_cuDeviceGetCount = CUresult (*)(int*);
using Fn_cuCtxPushCurrent = CUresult (*)(CUcontext);
using Fn_cuCtxPopCurrent = CUresult (*)(CUcontext*);
using Fn_cuGraphicsGLRegisterImage = CUresult (*)(CUgraphicsResource*, unsigned int, unsigned int, unsigned int);
using Fn_cuGraphicsUnregisterResource = CUresult (*)(CUgraphicsResource);
using Fn_cuGraphicsMapResources = CUresult (*)(unsigned int, CUgraphicsResource*, CUstream);
using Fn_cuGraphicsUnmapResources = CUresult (*)(unsigned int, CUgraphicsResource*, CUstream);
using Fn_cuGraphicsSubResourceGetMappedArray = CUresult (*)(CUarray*, CUgraphicsResource, unsigned int, unsigned int);
using Fn_cuMemcpy2D = CUresult (*)(const CUDA_MEMCPY2D*);
using Fn_cuGetErrorString = CUresult (*)(CUresult, const char**);

const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
)";

// NV12 → RGB，支持 BT.709 / BT.601 与 limited / full range
const char* kFragmentShader = R"(#version 330 core
in vec2 v_texCoord;
out vec4 FragColor;
uniform sampler2D u_yTexture;
uniform sampler2D u_uvTexture;
uniform int u_bt709;
uniform int u_fullRange;
void main() {
    float y = texture(u_yTexture, v_texCoord).r;
    vec2 uv = texture(u_uvTexture, v_texCoord).rg - vec2(0.5);
    if (u_fullRange == 0) y = (y * 255.0 - 16.0) / 219.0;

    vec3 rgb;
    if (u_bt709 == 1) {
        rgb.r = y + 1.5748 * uv.y;
        rgb.g = y - 0.1873 * uv.x - 0.4681 * uv.y;
        rgb.b = y + 1.8556 * uv.x;
    } else {
        rgb.r = y + 1.4020 * uv.y;
        rgb.g = y - 0.3441 * uv.x - 0.7141 * uv.y;
        rgb.b = y + 1.7720 * uv.x;
    }
    FragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)";

template <typename T>
bool LoadProc(HMODULE module, const char* name, T& fn) {
    fn = reinterpret_cast<T>(GetProcAddress(module, name));
    return fn != nullptr;
}

// cuCtxPush/PopCurrent 在 CUDA 12 中导出名为 *_v2，做一次兼容回退
template <typename T>
bool LoadProcCompat(HMODULE module, const char* name, T& fn) {
    char versioned[128];
    snprintf(versioned, sizeof(versioned), "%s_v2", name);
    if (LoadProc(module, versioned, fn)) return true;
    return LoadProc(module, name, fn);
}

}  // namespace

struct CudaFrameRenderer::Impl {
    HMODULE module{nullptr};
    bool loaded{false};

    Fn_cuInit cuInit{nullptr};
    Fn_cuDeviceGetCount cuDeviceGetCount{nullptr};
    Fn_cuCtxPushCurrent cuCtxPushCurrent{nullptr};
    Fn_cuCtxPopCurrent cuCtxPopCurrent{nullptr};
    Fn_cuGraphicsGLRegisterImage cuGraphicsGLRegisterImage{nullptr};
    Fn_cuGraphicsUnregisterResource cuGraphicsUnregisterResource{nullptr};
    Fn_cuGraphicsMapResources cuGraphicsMapResources{nullptr};
    Fn_cuGraphicsUnmapResources cuGraphicsUnmapResources{nullptr};
    Fn_cuGraphicsSubResourceGetMappedArray cuGraphicsSubResourceGetMappedArray{nullptr};
    Fn_cuMemcpy2D cuMemcpy2DFn{nullptr};
    Fn_cuGetErrorString cuGetErrorString{nullptr};

    // 与解码帧同尺寸的 Y / UV 纹理，由 CUDA 直接写入
    unsigned int yTexture{0};
    unsigned int uvTexture{0};
    CUgraphicsResource yResource{nullptr};
    CUgraphicsResource uvResource{nullptr};
    // 注册纹理时使用的 CUDA 上下文，注销时必须重新将其置为当前上下文
    CUcontext registeredContext{nullptr};
    int width{0};
    int height{0};

    unsigned int shaderProgram{0};
    unsigned int vao{0};
    unsigned int vbo{0};
    int uYTextureLocation{-1};
    int uUvTextureLocation{-1};
    int uBt709Location{-1};
    int uFullRangeLocation{-1};

    bool Load();
    bool EnsureTextures(int w, int h, CUcontext cudaContext);
    void ReleaseTextures();
    bool EnsureShader();
    bool UploadPlanes(AVFrame* frame, CUcontext cudaContext);
    bool RenderYuvToRgb(unsigned int outputTexture, AVFrame* frame);
};

bool CudaFrameRenderer::Impl::Load() {
    if (loaded) return true;
    loaded = true;

    module = LoadLibraryA("nvcuda.dll");
    if (!module) {
        LOGW(kTag, "nvcuda.dll not found, CUDA-GL interop disabled");
        return false;
    }

    bool ok = LoadProc(module, "cuInit", cuInit) && LoadProc(module, "cuDeviceGetCount", cuDeviceGetCount) &&
              LoadProcCompat(module, "cuCtxPushCurrent", cuCtxPushCurrent) &&
              LoadProcCompat(module, "cuCtxPopCurrent", cuCtxPopCurrent) &&
              LoadProc(module, "cuGraphicsGLRegisterImage", cuGraphicsGLRegisterImage) &&
              LoadProc(module, "cuGraphicsUnregisterResource", cuGraphicsUnregisterResource) &&
              LoadProc(module, "cuGraphicsMapResources", cuGraphicsMapResources) &&
              LoadProc(module, "cuGraphicsUnmapResources", cuGraphicsUnmapResources) &&
              LoadProc(module, "cuGraphicsSubResourceGetMappedArray", cuGraphicsSubResourceGetMappedArray) &&
              LoadProcCompat(module, "cuMemcpy2D", cuMemcpy2DFn);
    LoadProc(module, "cuGetErrorString", cuGetErrorString);

    if (!ok) {
        LOGE(kTag, "Failed to resolve CUDA driver API, CUDA-GL interop disabled");
        return false;
    }
    if (cuInit(0) != CUDA_SUCCESS) {
        LOGE(kTag, "cuInit failed, CUDA-GL interop disabled");
        return false;
    }
    return true;
}

void CudaFrameRenderer::Impl::ReleaseTextures() {
    // 注销 CUDA 资源要求注册时的 CUDA 上下文处于当前状态，否则注销会失败并泄漏
    CUcontext previous = nullptr;
    bool pushed = registeredContext && cuCtxPushCurrent && cuCtxPushCurrent(registeredContext) == CUDA_SUCCESS;

    if (cuGraphicsUnregisterResource) {
        if (yResource) cuGraphicsUnregisterResource(yResource);
        if (uvResource) cuGraphicsUnregisterResource(uvResource);
    }
    yResource = nullptr;
    uvResource = nullptr;

    if (pushed) cuCtxPopCurrent(&previous);
    registeredContext = nullptr;

    unsigned int textures[2] = {yTexture, uvTexture};
    if (textures[0] || textures[1]) glDeleteTextures(2, textures);
    yTexture = 0;
    uvTexture = 0;
    width = 0;
    height = 0;
}

bool CudaFrameRenderer::Impl::EnsureTextures(int w, int h, CUcontext cudaContext) {
    if (yTexture && width == w && height == h) return true;
    ReleaseTextures();

    int chromaWidth = (w + 1) / 2;
    int chromaHeight = (h + 1) / 2;

    unsigned int textures[2] = {0, 0};
    glGenTextures(2, textures);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, chromaWidth, chromaHeight, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    yTexture = textures[0];
    uvTexture = textures[1];

    // cuGraphicsGLRegisterImage 需要当前线程上有 CUDA 上下文（与解码线程共用同一个）
    CUcontext previous = nullptr;
    if (cuCtxPushCurrent(cudaContext) != CUDA_SUCCESS) {
        LOGE(kTag, "cuCtxPushCurrent failed while registering GL textures");
        ReleaseTextures();
        return false;
    }
    registeredContext = cudaContext;

    CUresult yResult =
        cuGraphicsGLRegisterImage(&yResource, textures[0], GL_TEXTURE_2D, CU_GRAPHICS_REGISTER_FLAGS_NONE);
    CUresult uvResult =
        cuGraphicsGLRegisterImage(&uvResource, textures[1], GL_TEXTURE_2D, CU_GRAPHICS_REGISTER_FLAGS_NONE);

    cuCtxPopCurrent(&previous);

    if (yResult != CUDA_SUCCESS || uvResult != CUDA_SUCCESS) {
        const char* errorText = nullptr;
        if (cuGetErrorString &&
            cuGetErrorString(yResult != CUDA_SUCCESS ? yResult : uvResult, &errorText) == CUDA_SUCCESS) {
            LOGE(kTag, "cuGraphicsGLRegisterImage failed: %s", errorText ? errorText : "unknown");
        } else {
            LOGE(kTag, "cuGraphicsGLRegisterImage failed");
        }
        ReleaseTextures();
        return false;
    }

    width = w;
    height = h;
    LOGI(kTag, "CUDA-GL interop ready: %dx%d (NV12 -> RGB on GPU)", w, h);
    return true;
}

bool CudaFrameRenderer::Impl::EnsureShader() {
    if (shaderProgram) return true;

    shaderProgram = GLUtils::CompileAndLinkProgram(kVertexShader, kFragmentShader);
    if (!shaderProgram) return false;

    const float vertices[] = {
        // 位置          纹理坐标
        -1.0f, 1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f,  -1.0f, 1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f};

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glUseProgram(shaderProgram);
    uYTextureLocation = glGetUniformLocation(shaderProgram, "u_yTexture");
    uUvTextureLocation = glGetUniformLocation(shaderProgram, "u_uvTexture");
    uBt709Location = glGetUniformLocation(shaderProgram, "u_bt709");
    uFullRangeLocation = glGetUniformLocation(shaderProgram, "u_fullRange");
    glUseProgram(0);
    return true;
}

bool CudaFrameRenderer::Impl::UploadPlanes(AVFrame* frame, CUcontext cudaContext) {
    CUcontext previous = nullptr;
    CUresult pushResult = cuCtxPushCurrent(cudaContext);
    if (pushResult != CUDA_SUCCESS) {
        LOGE(kTag, "cuCtxPushCurrent failed: %d", static_cast<int>(pushResult));
        return false;
    }

    bool ok = false;
    CUgraphicsResource resources[2] = {yResource, uvResource};
    if (cuGraphicsMapResources(2, resources, nullptr) == CUDA_SUCCESS) {
        CUarray yArray = nullptr;
        CUarray uvArray = nullptr;
        if (cuGraphicsSubResourceGetMappedArray(&yArray, yResource, 0, 0) == CUDA_SUCCESS &&
            cuGraphicsSubResourceGetMappedArray(&uvArray, uvResource, 0, 0) == CUDA_SUCCESS) {
            int chromaWidth = (frame->width + 1) / 2;
            int chromaHeight = (frame->height + 1) / 2;

            CUDA_MEMCPY2D copy{};
            copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
            copy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
            copy.srcXInBytes = 0;
            copy.srcY = 0;
            copy.dstXInBytes = 0;
            copy.dstY = 0;
            copy.dstPitch = 0;  // 目标为数组时忽略

            copy.srcDevice = (CUdeviceptr)frame->data[0];
            copy.srcPitch = (size_t)frame->linesize[0];
            copy.dstArray = yArray;
            copy.WidthInBytes = (size_t)frame->width;
            copy.Height = (size_t)frame->height;
            CUresult yResult = cuMemcpy2DFn(&copy);

            copy.srcDevice = (CUdeviceptr)frame->data[1];
            copy.srcPitch = (size_t)frame->linesize[1];
            copy.dstArray = uvArray;
            // RG8 纹理每个色度采样占 2 字节（U、V 交错），按字节数拷贝整行
            copy.WidthInBytes = (size_t)chromaWidth * 2;
            copy.Height = (size_t)chromaHeight;
            CUresult uvResult = cuMemcpy2DFn(&copy);

            ok = (yResult == CUDA_SUCCESS && uvResult == CUDA_SUCCESS);
            if (!ok) {
                const char* errorText = nullptr;
                if (cuGetErrorString && cuGetErrorString(yResult == CUDA_SUCCESS ? uvResult : yResult, &errorText) ==
                                             CUDA_SUCCESS) {
                    LOGE(kTag, "cuMemcpy2D failed: %s", errorText ? errorText : "unknown");
                }
            }
        } else {
            LOGE(kTag, "cuGraphicsSubResourceGetMappedArray failed");
        }
        cuGraphicsUnmapResources(2, resources, nullptr);
    } else {
        LOGE(kTag, "cuGraphicsMapResources failed");
    }

    cuCtxPopCurrent(&previous);
    return ok;
}

bool CudaFrameRenderer::Impl::RenderYuvToRgb(unsigned int outputTexture, AVFrame* frame) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGE(kTag, "Framebuffer not complete for hardware frame");
        return false;
    }

    glViewport(0, 0, width, height);
    glUseProgram(shaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, yTexture);
    glUniform1i(uYTextureLocation, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, uvTexture);
    glUniform1i(uUvTextureLocation, 1);

    // 优先使用码流中的色彩信息，未标注时按分辨率做常见推断
    int isBt709 = (frame->colorspace == AVCOL_SPC_BT709 || frame->colorspace == AVCOL_SPC_BT2020_NCL ||
                   (frame->colorspace == AVCOL_SPC_UNSPECIFIED && frame->height >= 720))
                      ? 1
                      : 0;
    glUniform1i(uBt709Location, isBt709);
    glUniform1i(uFullRangeLocation, frame->color_range == AVCOL_RANGE_JPEG ? 1 : 0);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    return true;
}

CudaFrameRenderer::CudaFrameRenderer() : m_impl(std::make_unique<Impl>()) {}

CudaFrameRenderer::~CudaFrameRenderer() { Release(); }

bool CudaFrameRenderer::IsAvailable() {
    static const bool available = [] {
        HMODULE module = LoadLibraryA("nvcuda.dll");
        if (!module) return false;

        Fn_cuInit init = reinterpret_cast<Fn_cuInit>(GetProcAddress(module, "cuInit"));
        Fn_cuDeviceGetCount getCount =
            reinterpret_cast<Fn_cuDeviceGetCount>(GetProcAddress(module, "cuDeviceGetCount"));
        if (!init || !getCount) return false;
        if (init(0) != CUDA_SUCCESS) return false;

        int count = 0;
        return getCount(&count) == CUDA_SUCCESS && count > 0;
    }();
    return available;
}

bool CudaFrameRenderer::RenderHardwareFrame(void* avFrame, unsigned int outputTexture, int width, int height) {
    AVFrame* frame = static_cast<AVFrame*>(avFrame);
    if (!frame || !frame->hw_frames_ctx) return false;

    auto* framesContext = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
    if (framesContext->format != AV_PIX_FMT_CUDA || framesContext->sw_format != AV_PIX_FMT_NV12) {
        LOGW(kTag, "Unsupported hardware frame format, fallback to CPU path");
        return false;
    }

    if (!m_impl->Load()) return false;

    // FFmpeg 8/9 中 device_ctx 直接指向 AVHWDeviceContext
    if (!framesContext->device_ctx) return false;
    auto* deviceContext = reinterpret_cast<AVCUDADeviceContext*>(framesContext->device_ctx->hwctx);
    if (!deviceContext || !deviceContext->cuda_ctx) return false;

    if (!m_impl->EnsureTextures(width, height, deviceContext->cuda_ctx) || !m_impl->EnsureShader()) return false;

    // 与解码线程共用同一个 CUDA 上下文，按 CUDA 上下文栈的方式在其上执行互操作
    if (!m_impl->UploadPlanes(frame, deviceContext->cuda_ctx)) return false;

    return m_impl->RenderYuvToRgb(outputTexture, frame);
}

void CudaFrameRenderer::Release() {
    if (!m_impl) return;
    m_impl->ReleaseTextures();
    if (m_impl->shaderProgram) {
        glDeleteProgram(m_impl->shaderProgram);
        m_impl->shaderProgram = 0;
    }
    if (m_impl->vbo) {
        glDeleteBuffers(1, &m_impl->vbo);
        m_impl->vbo = 0;
    }
    if (m_impl->vao) {
        glDeleteVertexArrays(1, &m_impl->vao);
        m_impl->vao = 0;
    }
}

}  // namespace av

#else  // 非 Windows 或未检测到 CUDA Toolkit：提供空实现，由调用方回退 CPU 路径

namespace av {

struct CudaFrameRenderer::Impl {};

CudaFrameRenderer::CudaFrameRenderer() : m_impl(std::make_unique<Impl>()) {}

CudaFrameRenderer::~CudaFrameRenderer() = default;

bool CudaFrameRenderer::IsAvailable() { return false; }

bool CudaFrameRenderer::RenderHardwareFrame(void*, unsigned int, int, int) { return false; }

void CudaFrameRenderer::Release() {}

}  // namespace av

#endif
