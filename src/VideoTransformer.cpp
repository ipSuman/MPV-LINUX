#include "VideoTransformer.h"

#include "RuntimeLogger.h"

#include <mpv/client.h>
#include <QTimer>

VideoTransformer::VideoTransformer(mpv_handle* mpv, RuntimeLogger* logger)
    : m_mpv(mpv), m_logger(logger) {}

QString VideoTransformer::propertyString(const char* name) const {
    if (!m_mpv) return {};
    char* value = nullptr;
    if (mpv_get_property(m_mpv, name, MPV_FORMAT_STRING, &value) < 0 || !value) return {};
    const QString result = QString::fromUtf8(value);
    mpv_free(value);
    return result;
}

void VideoTransformer::command(const char** args) {
    if (!m_mpv || !args) return;
    const int result = mpv_command_async(m_mpv, 0, args);
    if (m_logger) {
        QStringList parts;
        for (int i = 0; args[i] != nullptr; ++i) parts << QString::fromUtf8(args[i]);
        m_logger->append(QStringLiteral("TRANSFORM COMMAND async: %1 result=%2")
            .arg(parts.join(QStringLiteral(" | "))).arg(result));
    }
}

void VideoTransformer::setRotation(int rotation) {
    if (!m_mpv) return;
    m_rotation = ((rotation % 360) + 360) % 360;
}

void VideoTransformer::toggleFlipHorizontal() {
    if (!m_mpv) return;
    m_flipHorizontal = !m_flipHorizontal;
}

void VideoTransformer::toggleFlipVertical() {
    if (!m_mpv) return;
    m_flipVertical = !m_flipVertical;
}

void VideoTransformer::reset() {
    m_rotation = 0;
    m_flipHorizontal = false;
    m_flipVertical = false;
    clearHardwareOverride();

    const char* removeH[] = {"vf", "remove", "@rex-flip-h", nullptr};
    const char* removeV[] = {"vf", "remove", "@rex-flip-v", nullptr};
    command(removeH);
    command(removeV);

    const char* args[] = {"set", "video-rotate", "0", nullptr};
    command(args);
    if (m_logger) m_logger->append(QStringLiteral("TRANSFORM: reset"));
}

void VideoTransformer::clearHardwareOverride() {
    if (!m_mpv || !m_flipHwdecOverride) return;

    const QString currentHwdec = propertyString("hwdec").trimmed();
    if (m_logger) {
        m_logger->append(QStringLiteral("FLIP HWDEC: clearing override; current=%1 saved=%2")
            .arg(currentHwdec, m_flipPreHwdec));
    }

    if (currentHwdec == QStringLiteral("auto-copy")) {
        const QByteArray saved = m_flipPreHwdec.toUtf8();
        const char* args[] = {"set", "hwdec", saved.constData(), nullptr};
        command(args);
        if (m_logger) m_logger->append(QStringLiteral("FLIP HWDEC: queued restore %1").arg(m_flipPreHwdec));
    }

    m_flipPreHwdec.clear();
    m_flipHwdecOverride = false;
}

void VideoTransformer::apply() {
    if (!m_mpv) return;

    const bool flipsActive = m_flipHorizontal || m_flipVertical;
    if (m_logger) {
        m_logger->append(QStringLiteral("TRANSFORM: apply rotation=%1 flipH=%2 flipV=%3 hwdec=%4 hwdec-current=%5")
            .arg(m_rotation)
            .arg(m_flipHorizontal ? QStringLiteral("on") : QStringLiteral("off"))
            .arg(m_flipVertical ? QStringLiteral("on") : QStringLiteral("off"))
            .arg(propertyString("hwdec"))
            .arg(propertyString("hwdec-current")));
    }

    if (flipsActive && !m_flipHwdecOverride) {
        const QString activeHwdec = propertyString("hwdec-current").trimmed();
        const QString configuredHwdec = propertyString("hwdec").trimmed();
        const bool hardwareActive =
            !activeHwdec.isEmpty() &&
            activeHwdec != QStringLiteral("no") &&
            activeHwdec != QStringLiteral("none");
        const bool alreadyCopyBack =
            activeHwdec == QStringLiteral("auto-copy") ||
            activeHwdec.endsWith(QStringLiteral("-copy"));

        if (hardwareActive && !alreadyCopyBack) {
            m_flipPreHwdec = configuredHwdec.isEmpty() ? QStringLiteral("auto") : configuredHwdec;
            const char* args[] = {"set", "hwdec", "auto-copy", nullptr};
            command(args);
            if (m_logger) m_logger->append(QStringLiteral("FLIP HWDEC: queued %1 -> auto-copy").arg(m_flipPreHwdec));
            m_flipHwdecOverride = true;
        }
    }

    if (!flipsActive) clearHardwareOverride();

    qint64 rotation = m_rotation;
    const int rotationResult = mpv_set_property(m_mpv, "video-rotate", MPV_FORMAT_INT64, &rotation);
    if (m_logger) {
        m_logger->append(QStringLiteral("TRANSFORM: video-rotate=%1 result=%2 (%3)")
            .arg(m_rotation).arg(rotationResult).arg(QString::fromUtf8(mpv_error_string(rotationResult))));
    }

    const bool waitForCopyBack = flipsActive && m_flipHwdecOverride;
    if (waitForCopyBack) {
        const QString activeHwdec = propertyString("hwdec-current").trimmed();
        const bool copyBackReady =
            activeHwdec.endsWith(QStringLiteral("-copy")) ||
            activeHwdec == QStringLiteral("no") ||
            activeHwdec == QStringLiteral("none");
        if (!copyBackReady) {
            if (m_logger) {
                m_logger->append(QStringLiteral("TRANSFORM: waiting for hwdec copy-back; current=%1")
                    .arg(activeHwdec.isEmpty() ? QStringLiteral("<empty>") : activeHwdec));
            }
            QTimer::singleShot(100, [this]() { apply(); });
            return;
        }
    }

    const char* removeH[] = {"vf", "remove", "@rex-flip-h", nullptr};
    const char* removeV[] = {"vf", "remove", "@rex-flip-v", nullptr};
    int result = mpv_command(m_mpv, removeH);
    if (m_logger) m_logger->append(QStringLiteral("TRANSFORM: remove @rex-flip-h result=%1 (%2)")
        .arg(result).arg(QString::fromUtf8(mpv_error_string(result))));
    result = mpv_command(m_mpv, removeV);
    if (m_logger) m_logger->append(QStringLiteral("TRANSFORM: remove @rex-flip-v result=%1 (%2)")
        .arg(result).arg(QString::fromUtf8(mpv_error_string(result))));

    if (m_flipHorizontal) {
        const char* addH[] = {"vf", "add", "@rex-flip-h:hflip", nullptr};
        result = mpv_command(m_mpv, addH);
        if (m_logger) m_logger->append(QStringLiteral("TRANSFORM: add horizontal flip result=%1 (%2)")
            .arg(result).arg(QString::fromUtf8(mpv_error_string(result))));
    }
    if (m_flipVertical) {
        const char* addV[] = {"vf", "add", "@rex-flip-v:vflip", nullptr};
        result = mpv_command(m_mpv, addV);
        if (m_logger) m_logger->append(QStringLiteral("TRANSFORM: add vertical flip result=%1 (%2)")
            .arg(result).arg(QString::fromUtf8(mpv_error_string(result))));
    }

    if (m_logger) {
        m_logger->append(QStringLiteral("TRANSFORM: apply complete hwdec=%1 hwdec-current=%2")
            .arg(propertyString("hwdec")).arg(propertyString("hwdec-current")));
    }
}
