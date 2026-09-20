//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "AndroidDisplayView.h"

#include "Core/SyncNotifier.h"
#include "Core/TaskPool.h"
#include "Log.h"
#include "Utils/GLUtils.h"

static const char* kAndroidDisplayViewTag = "AndroidDisplayView";

namespace av {

// Vertex Shader: #version must be first (no leading whitespace)
static const char* vertexShaderSource = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

// Fragment Shader
static const char* fragmentShaderSource = R"(#version 300 es
precision mediump float;
precision mediump sampler2D;
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D texture1;
void main() {
    FragColor = texture(texture1, TexCoord);
}
)";

IVideoDisplayView* IVideoDisplayView::Create() { return new AndroidDisplayView(); }

AndroidDisplayView::AndroidDisplayView() = default;

AndroidDisplayView::~AndroidDisplayView() { Clear(); }

void AndroidDisplayView::SetTaskPool(std::shared_ptr<TaskPool> taskPool) { m_taskPool = taskPool; }

void AndroidDisplayView::InitializeGL() {
    LOGI(kAndroidDisplayViewTag, "InitializeGL called");
    m_shaderProgram = GLUtils::CompileAndLinkProgram(vertexShaderSource, fragmentShaderSource);
    if (!m_shaderProgram) {
        LOGE(kAndroidDisplayViewTag, "Shader program failed to compile/link");
        return;
    }
    m_uTexture1Location = glGetUniformLocation(m_shaderProgram, "texture1");
    if (m_uTexture1Location < 0) {
        LOGE(kAndroidDisplayViewTag, "uniform texture1 not found");
    }
    LOGI(kAndroidDisplayViewTag, "Shader program: %u, texture1 location: %d", m_shaderProgram, m_uTexture1Location);

    float vertices[] = {// positions         // texture coords
                        1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
                        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f,  0.0f, 0.0f, 1.0f};

    unsigned int indices[] = {0, 1, 3, 1, 2, 3};

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOGE(kAndroidDisplayViewTag, "OpenGL error after InitializeGL: 0x%x", err);
    } else {
        LOGI(kAndroidDisplayViewTag, "InitializeGL completed successfully");
    }
}

void AndroidDisplayView::SetDisplaySize(int width, int height) {
    LOGI(kAndroidDisplayViewTag, "SetDisplaySize: %d x %d", width, height);
    glViewport(0, 0, width, height);
}

void AndroidDisplayView::Render(std::shared_ptr<IVideoFrame> videoFrame, EContentMode mode) {
    if (!videoFrame) {
        LOGI(kAndroidDisplayViewTag, "Render called with null videoFrame");
        return;
    }

    std::lock_guard<std::mutex> lock(m_videoFrameMutex);
    m_videoFrame = videoFrame;
    m_mode = mode;
}

void AndroidDisplayView::Render(int width, int height, float red, float green, float blue) {
    std::lock_guard<std::mutex> lock(m_videoFrameMutex);

    glClearColor(red, green, blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_videoFrame) {
        return;
    }
    if (!m_shaderProgram || m_uTexture1Location < 0) {
        return;
    }

    if (!m_videoFrame->textureId) {
        glGenTextures(1, &m_videoFrame->textureId);
        glBindTexture(GL_TEXTURE_2D, m_videoFrame->textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_videoFrame->width, m_videoFrame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     m_videoFrame->data.get());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindTexture(GL_TEXTURE_2D, m_videoFrame->textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glUseProgram(m_shaderProgram);
    glUniform1i(m_uTexture1Location, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_videoFrame->textureId);

    glBindVertexArray(m_VAO);

    switch (m_mode) {
        case EContentMode::kScaleToFill:
            glViewport(0, 0, width, height);
            break;
        case EContentMode::kScaleAspectFit: {
            float aspectRatio = static_cast<float>(m_videoFrame->width) / m_videoFrame->height;
            float screenAspectRatio = static_cast<float>(width) / height;
            if (aspectRatio > screenAspectRatio) {
                int newHeight = static_cast<int>(width / aspectRatio);
                glViewport(0, (height - newHeight) / 2, width, newHeight);
            } else {
                int newWidth = static_cast<int>(height * aspectRatio);
                glViewport((width - newWidth) / 2, 0, newWidth, height);
            }
            break;
        }
        case EContentMode::kScaleAspectFill: {
            float aspectRatio = static_cast<float>(m_videoFrame->width) / m_videoFrame->height;
            float screenAspectRatio = static_cast<float>(width) / height;
            if (aspectRatio > screenAspectRatio) {
                int newWidth = static_cast<int>(height * aspectRatio);
                glViewport((width - newWidth) / 2, 0, newWidth, height);
            } else {
                int newHeight = static_cast<int>(width / aspectRatio);
                glViewport(0, (height - newHeight) / 2, width, newHeight);
            }
            break;
        }
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void AndroidDisplayView::SetSurfaceSize(int width, int height) {
    m_surfaceWidth = width;
    m_surfaceHeight = height;
}

void AndroidDisplayView::OnDrawFrame() {
    // Use stored surface dimensions, default to reasonable values if not set
    int width = m_surfaceWidth > 0 ? m_surfaceWidth : 1080;
    int height = m_surfaceHeight > 0 ? m_surfaceHeight : 1920;
    Render(width, height, 0.0f, 0.0f, 0.0f);
}

void AndroidDisplayView::Clear() {
    SyncNotifier notifier;
    if (m_taskPool) {
        m_taskPool->SubmitTask([&]() {
            if (m_shaderProgram > 0) glDeleteProgram(m_shaderProgram);
            std::lock_guard<std::mutex> lock(m_videoFrameMutex);
            m_videoFrame = nullptr;
            notifier.Notify();
        });
    }
    notifier.Wait();
}

}  // namespace av
