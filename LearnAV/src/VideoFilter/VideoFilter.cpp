//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "VideoFilter.h"

#include "FlipVerticalFilter.h"
#include "GrayFilter.h"
#include "InvertFilter.h"
#include "Log.h"
#ifndef ANDROID
#ifndef _WIN32
#include "StickerFilter.h"
#endif
#endif
#include "Utils/GLUtils.h"

namespace av {

static const char* kVideoFilterTag = "VideoFilter";

VideoFilter::~VideoFilter() {
    glDeleteProgram(m_shaderProgram);
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
}

void VideoFilter::SetFloat(const std::string& name, float value) { m_floatValues[name] = value; }

float VideoFilter::GetFloat(const std::string& name) { return m_floatValues[name]; }

void VideoFilter::SetInt(const std::string& name, int value) { m_intValues[name] = value; }

int VideoFilter::GetInt(const std::string& name) { return m_intValues[name]; }

void VideoFilter::SetString(const std::string& name, const std::string& value) { m_stringValues[name] = value; }

std::string VideoFilter::GetString(const std::string& name) { return m_stringValues[name]; }

bool VideoFilter::Render(std::shared_ptr<IVideoFrame> frame, unsigned int outputTexture) {
    if (!PreRender(frame, outputTexture)) return false;
    if (!MainRender(frame, outputTexture)) return false;
    return PostRender();
}

bool VideoFilter::PreRender(std::shared_ptr<IVideoFrame> frame, unsigned int outputTexture) {
    Initialize();
    if (!m_initialized) return false;

    if (outputTexture == 0) {
        LOGE(kVideoFilterTag, "ERROR::FRAMEBUFFER::outputTexture is 0, cannot attach.");
        return false;
    }

    // Bind FBO and attach output texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);

    // Check if FBO is complete
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE(kVideoFilterTag, "ERROR::FRAMEBUFFER::Framebuffer not complete, status=0x%x",
             static_cast<unsigned>(status));
        return false;
    }

    glViewport(0, 0, frame->width, frame->height);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_shaderProgram);
    return true;
}

bool VideoFilter::MainRender(std::shared_ptr<IVideoFrame> frame, unsigned int outputTexture) {
    // Bind input texture (glUseProgram already called in PreRender)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, frame->textureId);
    int loc = m_uTextureLocation;
    if (loc == -1) {
        loc = glGetUniformLocation(m_shaderProgram, "u_texture");
        if (loc != -1) m_uTextureLocation = loc;
    }
    if (loc == -1) {
        LOGE(kVideoFilterTag, "ERROR: uniform 'u_texture' not found or inactive!");
    } else {
        glUniform1i(loc, 0);
    }

    // Render quad
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    return true;
}

bool VideoFilter::PostRender() {
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void VideoFilter::Initialize() {
    if (m_initialized) return;
    m_initialized = true;

    // Compile and link shaders
    m_shaderProgram =
        GLUtils::CompileAndLinkProgram(m_description.vertexShaderSource, m_description.fragmentShaderSource);
    if (!m_shaderProgram) {
        LOGE(kVideoFilterTag, "Shader program failed to compile/link");
        m_initialized = false;
        return;
    }

    // Define a quad for rendering
    float vertices[] = {
        // Positions    // Texture Coords
        -1.0f, 1.0f,  0.0f, 1.0f,  // Top-left
        -1.0f, -1.0f, 0.0f, 0.0f,  // Bottom-left
        1.0f,  -1.0f, 1.0f, 0.0f,  // Bottom-right
        1.0f,  1.0f,  1.0f, 1.0f   // Top-right
    };

    // Setup VAO and VBO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    m_uTextureLocation = glGetUniformLocation(m_shaderProgram, "u_texture");
}

int VideoFilter::GetUniformLocation(const std::string& name) {
    auto [it, inserted] = m_uniformLocationCache.try_emplace(name, glGetUniformLocation(m_shaderProgram, name.c_str()));
    if (it->second == -1) LOGW(kVideoFilterTag, "Warning: uniform '%s' doesn't exist!", name.c_str());
    return it->second;
}

VideoFilter* VideoFilter::Create(VideoFilterType type) {
    switch (type) {
        case VideoFilterType::kFlipVertical:
            return new FlipVerticalFilter();
        case VideoFilterType::kGray:
            return new GrayFilter();
        case VideoFilterType::kInvert:
            return new InvertFilter();
#if !defined(ANDROID) && !defined(_WIN32)
        case VideoFilterType::kSticker:
            return new StickerFilter();
#endif
        default:
            break;
    }

    return nullptr;
}

}  // namespace av