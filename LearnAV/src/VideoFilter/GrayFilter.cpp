//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "GrayFilter.h"

namespace av {

GrayFilter::GrayFilter() {
    m_description.type = VideoFilterType::kGray;
#ifdef ANDROID
    m_description.vertexShaderSource = R"(#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
)";
    m_description.fragmentShaderSource = R"(#version 300 es
precision mediump float;
precision mediump sampler2D;
in vec2 v_texCoord;
out vec4 FragColor;
uniform sampler2D u_texture;
void main() {
    vec4 color = texture(u_texture, v_texCoord);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    FragColor = vec4(gray, gray, gray, color.a);
}
)";
#else
    m_description.vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec2 a_position;
        layout(location = 1) in vec2 a_texCoord;
        out vec2 v_texCoord;
        void main() {
            gl_Position = vec4(a_position, 0.0, 1.0);
            v_texCoord = a_texCoord;
        }
    )";
    m_description.fragmentShaderSource = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;
        uniform sampler2D u_texture;
        void main() {
            vec4 color = texture(u_texture, v_texCoord);
            float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
            FragColor = vec4(gray, gray, gray, color.a);
        }
    )";
#endif
}

}  // namespace av