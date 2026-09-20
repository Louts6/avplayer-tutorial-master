//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "FlipVerticalFilter.h"

namespace av {

FlipVerticalFilter::FlipVerticalFilter() {
    m_description.type = VideoFilterType::kFlipVertical;
#ifdef ANDROID
    m_description.vertexShaderSource = R"(#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = vec2(a_texCoord.x, 1.0 - a_texCoord.y);
}
)";
    m_description.fragmentShaderSource = R"(#version 300 es
precision mediump float;
precision mediump sampler2D;
in vec2 v_texCoord;
out vec4 FragColor;
uniform sampler2D u_texture;
void main() {
    FragColor = texture(u_texture, v_texCoord);
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
                v_texCoord = vec2(a_texCoord.x, 1.0 - a_texCoord.y);
            }
        )";
    m_description.fragmentShaderSource = R"(
            #version 330 core
            in vec2 v_texCoord;
            out vec4 FragColor;
            uniform sampler2D u_texture;
            void main() {
                FragColor = texture(u_texture, v_texCoord);
            }
        )";
#endif
}

}  // namespace av