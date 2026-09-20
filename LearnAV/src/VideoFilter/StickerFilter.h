//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_STICKERFILTER_H
#define LEARNAV_STICKERFILTER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "VideoFilter.h"
#include "inspireface.h"

namespace av {

class StickerFilter : public VideoFilter {
public:
    StickerFilter();
    ~StickerFilter() override;
    void SetString(const std::string& name, const std::string& value) override;
    bool MainRender(std::shared_ptr<IVideoFrame> frame, unsigned int outputTexture) override;
    void Initialize() override;

private:
    bool LoadFaceDetectionModel(const std::string& modelPath);
    bool DetectFaces(std::shared_ptr<IVideoFrame> frame);
    glm::mat4 CalculateStickerModelMatrix(const glm::vec2& leftEye, const glm::vec2& rightEye, float roll, float yaw,
                                          float pitch);

private:
    struct DetectedFaceInfo {
        const glm::vec2 leftEye;
        const glm::vec2 rightEye;
        float roll;
        float yaw;
        float pitch;

        std::vector<glm::vec2> keyPoints;

        DetectedFaceInfo(const glm::vec2& leftEye, const glm::vec2& rightEye, float roll, float yaw, float pitch,
                         const std::vector<glm::vec2>& points)
            : leftEye(leftEye), rightEye(rightEye), roll(roll), yaw(yaw), pitch(pitch), keyPoints(points) {}
    };

    std::string m_stickerPath;
    std::string m_modelPath;
    unsigned m_stickerTexture{0};
    int m_stickerTextureWidth{0};
    int m_stickerTextureHeight{0};
    int m_uStickerTextureLocation{0};
    int m_uModelMatrixLocation{0};
    int m_uIsStickerLocation{0};
    int m_uIsKeyPointLocation{0};

    // 人脸检测相关
    HFSession m_session{0};
    std::vector<DetectedFaceInfo> m_detectedFaces;
};

}  // namespace av

#endif  // LEARNAV_STICKERFILTER_H
