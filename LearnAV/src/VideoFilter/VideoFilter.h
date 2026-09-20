//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_VIDEOFILTER_H
#define LEARNAV_VIDEOFILTER_H

#include <string>
#include <unordered_map>

#include "Define/IVideoFrame.h"
#include "IVideoFilter.h"

namespace av {

class VideoFilter : public IVideoFilter {
public:
    VideoFilter() = default;
    ~VideoFilter();

    VideoFilterType GetType() const override { return m_description.type; }

    void SetFloat(const std::string& name, float value) override;
    float GetFloat(const std::string& name) override;

    void SetInt(const std::string& name, int value) override;
    int GetInt(const std::string& name) override;

    void SetString(const std::string& name, const std::string& value) override;
    std::string GetString(const std::string& name) override;

    bool Render(std::shared_ptr<IVideoFrame> frame, unsigned int outputTexture);

    static VideoFilter* Create(VideoFilterType type);

protected:
    virtual void Initialize();
    virtual bool MainRender(std::shared_ptr<IVideoFrame> frame, unsigned int outputTexture);
    bool PreRender(std::shared_ptr<IVideoFrame> frame, unsigned int outputTexture);
    bool PostRender();
    int GetUniformLocation(const std::string& name);

protected:
    struct VideoFilterDescription {
        VideoFilterType type{VideoFilterType::kNone};
        const char* vertexShaderSource{nullptr};
        const char* fragmentShaderSource{nullptr};
    };

protected:
    VideoFilterDescription m_description;
    bool m_initialized{false};
    unsigned int m_shaderProgram{0};
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    int m_uTextureLocation;
    std::unordered_map<std::string, int> m_uniformLocationCache;
    std::unordered_map<std::string, float> m_floatValues;
    std::unordered_map<std::string, int> m_intValues;
    std::unordered_map<std::string, std::string> m_stringValues;
};

}  // namespace av

#endif  // LEARNAV_VIDEOFILTER_H
