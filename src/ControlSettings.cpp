#include "ControlSettings.h"

#include <QSettings>
#include <algorithm>

namespace {
QSettings makeSettings() {
    return QSettings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
}
}

void ControlSettings::load() {
    QSettings settings = makeSettings();
    seekWheelMode = settings.value(QStringLiteral("controls/seekWheel"), seekWheelMode).toString();
    zoomWheelMode = settings.value(QStringLiteral("controls/zoomWheel"), zoomWheelMode).toString();
    volumeWheelMode = settings.value(QStringLiteral("controls/volumeWheel"), volumeWheelMode).toString();
    timerBesideProgress = settings.value(QStringLiteral("controls/timerBesideProgress"), timerBesideProgress).toBool();
    panButton = static_cast<Qt::MouseButton>(
        settings.value(QStringLiteral("controls/panButton"), static_cast<int>(panButton)).toInt());
    doubleClickButton = static_cast<Qt::MouseButton>(
        settings.value(QStringLiteral("controls/doubleClickButton"), static_cast<int>(doubleClickButton)).toInt());

    const int legacyMinutes = std::clamp(
        settings.value(QStringLiteral("controls/seekDurationMinutes"), 1).toInt(), 1, 120);
    seekDurationSeconds = std::clamp(
        settings.value(QStringLiteral("controls/seekDurationSeconds"), legacyMinutes * 60).toInt(),
        5, 7200);
    fontPath = settings.value(QStringLiteral("controls/fontPath"), fontPath).toString();
    fontFamily = settings.value(QStringLiteral("controls/fontFamily"), fontFamily).toString();
    fontSize = settings.value(QStringLiteral("controls/fontSize"), fontSize).toInt();
    if (fontSize < 1) fontSize = -1;

#define LOAD_KEY(field, key)     field = QKeySequence(settings.value(QStringLiteral(key), field.toString()).toString())
    LOAD_KEY(volumeUpKey, "controls/volumeUp");
    LOAD_KEY(volumeDownKey, "controls/volumeDown");
    LOAD_KEY(muteKey, "controls/mute");
    LOAD_KEY(seekBackwardKey, "controls/seekBackward");
    LOAD_KEY(seekForwardKey, "controls/seekForward");
    LOAD_KEY(loopAKey, "controls/loopA");
    LOAD_KEY(loopBKey, "controls/loopB");
    LOAD_KEY(loopClearKey, "controls/loopClear");
    LOAD_KEY(zoomInKey, "controls/zoomIn");
    LOAD_KEY(zoomOutKey, "controls/zoomOut");
    LOAD_KEY(zoomResetKey, "controls/zoomReset");
    LOAD_KEY(frameBackKey, "controls/frameBack");
    LOAD_KEY(frameForwardKey, "controls/frameForward");
    LOAD_KEY(switchSubtitlesKey, "controls/switchSubtitles");
    LOAD_KEY(subtitlePosUpKey, "controls/subtitlePosUp");
    LOAD_KEY(subtitlePosDownKey, "controls/subtitlePosDown");
    LOAD_KEY(subtitleSizeUpKey, "controls/subtitleSizeUp");
    LOAD_KEY(subtitleSizeDownKey, "controls/subtitleSizeDown");
    LOAD_KEY(captureScreenshotKey, "controls/captureScreenshot");
    LOAD_KEY(rotateVideoKey, "controls/rotateVideo");
    LOAD_KEY(speedUpKey, "controls/speedUp");
    LOAD_KEY(speedDownKey, "controls/speedDown");
    speedJump = std::clamp(settings.value(QStringLiteral("controls/speedJump"), speedJump).toDouble(), 0.10, 1.00);
    speedJump = std::round(speedJump * 20.0) / 20.0;
    LOAD_KEY(holdSpeedKey, "controls/holdSpeed");
#undef LOAD_KEY

    cutWithZoom = settings.value(QStringLiteral("controls/cutWithZoom"), cutWithZoom).toBool();
}

void ControlSettings::save() const {
    QSettings settings = makeSettings();
    settings.setValue(QStringLiteral("controls/seekDurationSeconds"), seekDurationSeconds);
    settings.setValue(QStringLiteral("controls/fontPath"), fontPath);
    settings.setValue(QStringLiteral("controls/fontFamily"), fontFamily);
    settings.setValue(QStringLiteral("controls/fontSize"), fontSize);
    settings.setValue(QStringLiteral("controls/seekWheel"), seekWheelMode);
    settings.setValue(QStringLiteral("controls/zoomWheel"), zoomWheelMode);
    settings.setValue(QStringLiteral("controls/volumeWheel"), volumeWheelMode);
    settings.setValue(QStringLiteral("controls/timerBesideProgress"), timerBesideProgress);
    settings.setValue(QStringLiteral("controls/panButton"), static_cast<int>(panButton));
    settings.setValue(QStringLiteral("controls/doubleClickButton"), static_cast<int>(doubleClickButton));

#define SAVE_KEY(field, key) settings.setValue(QStringLiteral(key), field.toString())
    SAVE_KEY(volumeUpKey, "controls/volumeUp");
    SAVE_KEY(volumeDownKey, "controls/volumeDown");
    SAVE_KEY(muteKey, "controls/mute");
    SAVE_KEY(seekBackwardKey, "controls/seekBackward");
    SAVE_KEY(seekForwardKey, "controls/seekForward");
    SAVE_KEY(loopAKey, "controls/loopA");
    SAVE_KEY(loopBKey, "controls/loopB");
    SAVE_KEY(loopClearKey, "controls/loopClear");
    SAVE_KEY(zoomInKey, "controls/zoomIn");
    SAVE_KEY(zoomOutKey, "controls/zoomOut");
    SAVE_KEY(zoomResetKey, "controls/zoomReset");
    SAVE_KEY(frameBackKey, "controls/frameBack");
    SAVE_KEY(frameForwardKey, "controls/frameForward");
    SAVE_KEY(switchSubtitlesKey, "controls/switchSubtitles");
    SAVE_KEY(subtitlePosUpKey, "controls/subtitlePosUp");
    SAVE_KEY(subtitlePosDownKey, "controls/subtitlePosDown");
    SAVE_KEY(subtitleSizeUpKey, "controls/subtitleSizeUp");
    SAVE_KEY(subtitleSizeDownKey, "controls/subtitleSizeDown");
    SAVE_KEY(captureScreenshotKey, "controls/captureScreenshot");
    SAVE_KEY(rotateVideoKey, "controls/rotateVideo");
    SAVE_KEY(speedUpKey, "controls/speedUp");
    SAVE_KEY(speedDownKey, "controls/speedDown");
    settings.setValue(QStringLiteral("controls/speedJump"), speedJump);
    SAVE_KEY(holdSpeedKey, "controls/holdSpeed");
#undef SAVE_KEY

    settings.setValue(QStringLiteral("controls/cutWithZoom"), cutWithZoom);
    settings.sync();
}
