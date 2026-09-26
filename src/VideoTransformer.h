#pragma once

#include <QString>

class RuntimeLogger;
struct mpv_handle;

class VideoTransformer {
public:
    VideoTransformer(mpv_handle* mpv, RuntimeLogger* logger);

    void setRotation(int rotation);
    int rotation() const { return m_rotation; }

    void toggleFlipHorizontal();
    void toggleFlipVertical();
    bool flipHorizontal() const { return m_flipHorizontal; }
    bool flipVertical() const { return m_flipVertical; }

    void reset();
    void apply();
    void clearHardwareOverride();

private:
    QString propertyString(const char* name) const;
    void command(const char** args);

    mpv_handle* m_mpv = nullptr;
    RuntimeLogger* m_logger = nullptr;
    int m_rotation = 0;
    bool m_flipHorizontal = false;
    bool m_flipVertical = false;
    bool m_flipHwdecOverride = false;
    QString m_flipPreHwdec;
};
