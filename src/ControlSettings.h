#pragma once

#include <QKeySequence>
#include <QString>
#include <Qt>

class ControlSettings final {
public:
    QString seekWheelMode = QStringLiteral("wheel");
    QString zoomWheelMode = QStringLiteral("alt-wheel");
    QString volumeWheelMode = QStringLiteral("ctrl-wheel");
    bool timerBesideProgress = true;
    bool mainPanelIcons = true;
    bool returnFocusToVideoAfterMouseAction = true;
    bool verboseLogging = false;
    Qt::MouseButton panButton = Qt::MiddleButton;
    Qt::MouseButton doubleClickButton = Qt::LeftButton;
    int seekDurationSeconds = 60;
    QString fontPath;
    QString fontFamily;
    int fontSize = -1;

    QKeySequence volumeUpKey = QKeySequence(Qt::SHIFT | Qt::Key_V);
    QKeySequence volumeDownKey = QKeySequence(Qt::Key_V);
    QKeySequence muteKey = QKeySequence(Qt::Key_M);
    QKeySequence seekBackwardKey = QKeySequence(Qt::Key_Left);
    QKeySequence seekForwardKey = QKeySequence(Qt::Key_Right);
    QKeySequence loopAKey = QKeySequence(Qt::Key_A);
    QKeySequence loopBKey = QKeySequence(Qt::Key_B);
    QKeySequence loopClearKey = QKeySequence(Qt::Key_L);
    QKeySequence zoomInKey = QKeySequence(Qt::SHIFT | Qt::Key_Equal);
    QKeySequence zoomOutKey = QKeySequence(Qt::Key_Minus);
    QKeySequence zoomResetKey = QKeySequence(Qt::Key_Z);
    QKeySequence frameBackKey = QKeySequence(Qt::Key_Comma);
    QKeySequence frameForwardKey = QKeySequence(Qt::Key_Period);
    QKeySequence switchSubtitlesKey = QKeySequence(Qt::Key_S);
    QKeySequence subtitlePosUpKey = QKeySequence(Qt::ControlModifier | Qt::Key_Up);
    QKeySequence subtitlePosDownKey = QKeySequence(Qt::ControlModifier | Qt::Key_Down);
    QKeySequence subtitleSizeUpKey = QKeySequence(Qt::SHIFT | Qt::Key_I);
    QKeySequence subtitleSizeDownKey = QKeySequence(Qt::Key_I);
    QKeySequence captureScreenshotKey = QKeySequence(Qt::Key_C);
    QKeySequence rotateVideoKey = QKeySequence(Qt::Key_R);
    QKeySequence speedUpKey = QKeySequence(Qt::AltModifier | Qt::Key_Up);
    QKeySequence speedDownKey = QKeySequence(Qt::AltModifier | Qt::Key_Down);
    double speedJump = 0.10;
    QKeySequence holdSpeedKey = QKeySequence(Qt::Key_2);
    bool cutWithZoom = false;

    void load();
    void save() const;
    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);
};
