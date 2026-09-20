//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "StickerFilter.h"

#include "Log.h"
#include "Utils/GLUtils.h"

namespace av {

static const char* kStickerFilterTag = "StickerFilter";

StickerFilter::StickerFilter() {
    m_description.type = VideoFilterType::kSticker;
    m_description.vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec2 a_position;
        layout(location = 1) in vec2 a_texCoord;

        out vec2 v_texCoord;
        uniform mat4 u_model;
        uniform bool u_isSticker;
        uniform bool u_isKeyPoint;

        void main() {
            if (u_isKeyPoint) {
                // 如果是关键点渲染，则只考虑位置，不需要纹理坐标
                gl_Position = u_model * vec4(a_position, 0.0, 1.0);
            } else {
                // 普通纹理渲染
                if (u_isSticker) {
                    gl_Position = u_model * vec4(a_position, 0.0, 1.0);
                    v_texCoord = vec2(a_texCoord.x, 1.0 - a_texCoord.y);
                } else {
                    gl_Position = vec4(a_position, 0.0, 1.0);
                    v_texCoord = a_texCoord;
                }
            }
        }
    )";

    m_description.fragmentShaderSource = R"(
        #version 330 core
        in vec2 v_texCoord;
        out vec4 FragColor;

        uniform sampler2D u_texture;
        uniform sampler2D u_sticker;
        uniform bool u_isSticker;
        uniform bool u_isKeyPoint;
        uniform vec4 u_color;  // 用于控制关键点的颜色

        void main() {
            if (u_isKeyPoint) {
                // 如果是关键点渲染，直接输出 u_color，默认为绿色
                FragColor = u_color;
            } else {
                // 普通的纹理渲染
                vec4 color = texture(u_isSticker ? u_sticker : u_texture, v_texCoord);
                if (u_isSticker && color.a < 0.1)
                    discard;
                FragColor = color;
            }
        }
    )";
}

StickerFilter::~StickerFilter() {
    if (m_stickerTexture) {
        glDeleteTextures(1, &m_stickerTexture);
        m_stickerTexture = 0;
    }

    HFReleaseInspireFaceSession(m_session);
}

void StickerFilter::SetString(const std::string& name, const std::string& value) {
    VideoFilter::SetString(name, value);
    if (name == "StickerPath") m_stickerPath = value;
    if (name == "ModelPath") m_modelPath = value;
}

void StickerFilter::Initialize() {
    if (m_initialized) return;
    VideoFilter::Initialize();
    if (m_stickerPath.empty() || m_modelPath.empty()) {
        m_initialized = false;
        return;
    }

    HFSetLogLevel(HF_LOG_NONE);
    if (!LoadFaceDetectionModel(m_modelPath)) {
        m_initialized = false;
        return;
    }

    m_stickerTexture = GLUtils::LoadImageFileToTexture(m_stickerPath, m_stickerTextureWidth, m_stickerTextureHeight);
    m_uStickerTextureLocation = glGetUniformLocation(m_shaderProgram, "u_sticker");
    m_uModelMatrixLocation = glGetUniformLocation(m_shaderProgram, "u_model");
    m_uIsStickerLocation = glGetUniformLocation(m_shaderProgram, "u_isSticker");
    m_uIsKeyPointLocation = glGetUniformLocation(m_shaderProgram, "u_isKeyPoint");
}

bool StickerFilter::MainRender(std::shared_ptr<IVideoFrame> frame, unsigned int outputTexture) {
    if (!DetectFaces(frame)) return false;

    // 1. 渲染原始视频帧
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, frame->textureId);
    glUniform1i(m_uTextureLocation, 0);

    glUniform1i(m_uIsStickerLocation, GL_FALSE);   // 渲染原始视频帧
    glUniform1i(m_uIsKeyPointLocation, GL_FALSE);  // 设置为关键点模式
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);  // 使用全屏渲染原始帧
    glBindVertexArray(0);

    // 2. 渲染贴纸
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_stickerTexture);
    glUniform1i(m_uStickerTextureLocation, 1);

    // 渲染每个检测到的人脸的贴纸
    for (const auto& face : m_detectedFaces) {
        // 渲染贴纸
        glm::mat4 model = CalculateStickerModelMatrix(face.leftEye, face.rightEye, face.roll, face.yaw, face.pitch);
        glUniformMatrix4fv(m_uModelMatrixLocation, 1, GL_FALSE, glm::value_ptr(model));

        glUniform1i(m_uIsStickerLocation, GL_TRUE);    // 开始渲染贴纸
        glUniform1i(m_uIsKeyPointLocation, GL_FALSE);  // 设置为关键点模式
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);  // 缩放并定位贴纸
        glBindVertexArray(0);

#if 0
        // 3. 渲染人脸关键点（使用绿色）
        glUniform4f(glGetUniformLocation(m_shaderProgram, "u_color"), 0.0f, 1.0f, 0.0f, 1.0f);

        // 渲染关键点
        for (const auto& point : face.keyPoints) {
            glm::vec2 pos = point;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos, 0.0f));
            model = glm::scale(model, glm::vec3(0.005f, 0.003f, 1.0f));  // 适当缩放关键点大小

            glUniformMatrix4fv(m_uModelMatrixLocation, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(m_uIsKeyPointLocation, GL_TRUE);  // 设置为关键点模式
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);  // 绘制关键点
            glBindVertexArray(0);
        }
#endif
    }

    return true;
}

glm::mat4 StickerFilter::CalculateStickerModelMatrix(const glm::vec2& leftEye, const glm::vec2& rightEye, float roll,
                                                     float yaw, float pitch) {
    // 1. 计算左右眼之间的中心点作为脸部中心
    glm::vec2 faceCenter2D = (leftEye + rightEye) * 0.5f;

    // 2. 估计眼睛中心到额头的相对位置
    float eyeDistance = glm::distance(leftEye, rightEye);
    glm::vec3 relativeForeheadPos(0.0f, eyeDistance * 2.5f, 0.0f);  // 额头位于眼睛中心上方

    // 3. 将相对额头位置应用旋转矩阵，得到旋转后的三维位置
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));    // 绕Y轴
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));  // 绕X轴
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(-roll), glm::vec3(0.0f, 0.0f, 1.0f));  // 绕Z轴

    // 4. 计算额头的绝对位置
    glm::vec3 foreheadPos3D = glm::vec3(rotationMatrix * glm::vec4(relativeForeheadPos, 1.0f));
    glm::vec3 faceCenter3D = glm::vec3(faceCenter2D, 0.0f);
    glm::vec3 finalForeheadPos = faceCenter3D + foreheadPos3D;

    // 5. 初始化模型矩阵
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // 6. 将贴纸平移到额头位置
    modelMatrix = glm::translate(modelMatrix, finalForeheadPos);

    // 7. 应用旋转（已经通过旋转矩阵包含在内）
    modelMatrix *= rotationMatrix;

    // 8. 缩放，确保贴纸的大小符合脸部比例
    float scaleFactor = eyeDistance * 2.5f;  // 缩放比例，具体可调
    modelMatrix = glm::scale(modelMatrix, glm::vec3(scaleFactor, scaleFactor, scaleFactor));

    return modelMatrix;
}

bool StickerFilter::LoadFaceDetectionModel(const std::string& modelPath) {
    if (modelPath.empty()) return false;

    if (HFLaunchInspireFace(modelPath.c_str()) != HSUCCEED) {
        LOGE(kStickerFilterTag, "Load Resource error");
        return false;
    }

    HOption option = HF_ENABLE_QUALITY | HF_ENABLE_MASK_DETECT | HF_ENABLE_INTERACTION;
    HFDetectMode detMode = HF_DETECT_MODE_TRACK_BY_DETECTION;
    HInt32 maxDetectNum = 2;
    HInt32 detectPixelLevel = 320;
    HInt32 trackByDetectFps = 20;
    if (HFCreateInspireFaceSessionOptional(option, detMode, maxDetectNum, detectPixelLevel, trackByDetectFps,
                                           &m_session) != HSUCCEED) {
        LOGE(kStickerFilterTag, "Create FaceContext error");
        return false;
    }

    HFSessionSetTrackPreviewSize(m_session, detectPixelLevel);
    HFSessionSetFilterMinimumFacePixelSize(m_session, 0);

    return true;
}

bool StickerFilter::DetectFaces(std::shared_ptr<IVideoFrame> frame) {
    if (!frame || !frame->data) return false;

    HFImageData imageParam = {0};
    imageParam.data = frame->data.get();
    imageParam.width = frame->width;
    imageParam.height = frame->height;
    imageParam.rotation = HF_CAMERA_ROTATION_0;
    imageParam.format = HF_STREAM_RGBA;

    HFImageStream imageHandle = {0};
    if (HFCreateImageStream(&imageParam, &imageHandle) != HSUCCEED) {
        LOGE(kStickerFilterTag, "Create ImageStream error");
        return false;
    }

    HFMultipleFaceData multipleFaceData = {0};
    if (HFExecuteFaceTrack(m_session, imageHandle, &multipleFaceData) != HSUCCEED) {
        LOGE(kStickerFilterTag, "Execute HFExecuteFaceTrack error");
        HFReleaseImageStream(imageHandle);
        return false;
    }

    m_detectedFaces.clear();

    for (int index = 0; index < multipleFaceData.detectedNum; ++index) {
        HInt32 numOfLmk;
        HFGetNumOfFaceDenseLandmark(&numOfLmk);
        HPoint2f denseLandmarkPoints[numOfLmk];
        if (HFGetFaceDenseLandmarkFromFaceToken(multipleFaceData.tokens[index], denseLandmarkPoints, numOfLmk) !=
            HSUCCEED) {
            LOGE(kStickerFilterTag, "HFGetFaceDenseLandmarkFromFaceToken error!!");
            HFReleaseImageStream(imageHandle);
            return false;
        }

        std::vector<glm::vec2> points;
        for (int i = 0; i < numOfLmk; ++i) {
            // 坐标转换，映射到[-1.0, 1.0]的OpenGL坐标系
            points.emplace_back((denseLandmarkPoints[i].x / frame->width) * 2.0f - 1.0f,
                                1.0f - (denseLandmarkPoints[i].y / frame->height) * 2.0f);
        }

        // 转换左眼和右眼坐标
        glm::vec2 leftEye = glm::vec2((denseLandmarkPoints[105].x / frame->width) * 2.0f - 1.0f,
                                      1.0f - (denseLandmarkPoints[105].y / frame->height) * 2.0f);
        glm::vec2 rightEye = glm::vec2((denseLandmarkPoints[55].x / frame->width) * 2.0f - 1.0f,
                                       1.0f - (denseLandmarkPoints[55].y / frame->height) * 2.0f);

        m_detectedFaces.push_back(DetectedFaceInfo(leftEye, rightEye, multipleFaceData.angles.roll[index],
                                                   multipleFaceData.angles.yaw[index],
                                                   multipleFaceData.angles.pitch[index], points));
    }

    if (HFReleaseImageStream(imageHandle) != HSUCCEED) {
        LOGE(kStickerFilterTag, "Release ImageStream error");
    }
    return multipleFaceData.detectedNum > 0;
}

}  // namespace av