//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "InvertFilter.h"

namespace av {

InvertFilter::InvertFilter() {
    m_description.type = VideoFilterType::kInvert;
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
    FragColor = vec4(1.0 - color.r, 1.0 - color.g, 1.0 - color.b, color.a);
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
            FragColor = vec4(1.0 - color.r, 1.0 - color.g, 1.0 - color.b, color.a);
        }
    )";
#endif
}

}  // namespace av