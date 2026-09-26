#pragma once

#include <QMainWindow>
#include <atomic>
#include <QPointF>
#include <QKeySequence>
#include <QTimer>
#include <QDateTime>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSlider;
class QWidget;
class QCloseEvent;
class QResizeEvent;
class QCheckBox;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QDockWidget;
class QKeyEvent;
class QProcess;

struct mpv_handle;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& mediaPath = {}, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void pumpMpvEvents();
    void openFile();
    void addFiles();
    void addFolder();
    void clearPlaylist();
    void playlistActivated();
    void togglePause();
    void seekBackward();
    void seekForward();
    void seekTo(int value);
    void setVolume(int value);
    void volumeUp();
    void volumeDown();
    void toggleMute();
    void updatePlaybackUi();
    void toggleControls();
    void togglePlaylist();
    void toggleHardwareDecoding();
    void showControlsDialog();
    void playPrevious();
    void playNext();
    void showTracksMenu();
    void cutAbSelection();
    void rotateVideo90();
    void toggleFlipHorizontal();
    void toggleFlipVertical();
    void applyVideoTransforms();
    void saveLogReport();
    void increaseSubtitleSize();
    void decreaseSubtitleSize();
    void increaseSubtitlePosition();
    void decreaseSubtitlePosition();
    void cycleSubtitles();

signals:
    void mpvWakeup();

private:
    bool initializeMpv();
    static void mpvWakeupCallback(void* context);
    void buildUi();
    void loadFile(const QString& path);
    void addToPlaylist(const QString& path);
    void playPlaylistIndex(int index, bool promptResume = true);
    void syncPlaylistSelection();
    void command(const char** args);
    double getPropertyDouble(const char* name) const;
    QString getPropertyString(const char* name) const;
    void setPropertyDouble(const char* name, double value);
    void updateHardwareButton();
    void updatePlayButton(bool paused);
    void updateSeekButtonLabels();
    void adjustVideoZoom(double amount);
    void resetVideoTransform();
    void setAbLoopStart();
    void setAbLoopEnd();
    void clearAbLoop();
    void updateAbLoopLabel();
    void stepFrame(bool forward);
    void showError(const QString& message);
    QString formatTime(double seconds) const;
    void setControlsVisible(bool visible);
    void toggleFullscreen();
    void resizeWindowForVideoAspect();
    void showDisplayDialog();
    void captureScreenshot();
    void loadControlSettings();
    void updatePlaybackInhibit(bool active);
    bool keyMatches(QKeyEvent* event, const QKeySequence& sequence) const;
    QString playbackPositionKey(const QString& path) const;
    void saveCurrentPlaybackPosition();
    void initializeRuntimeLog();
    void appendRuntimeLog(const QString& message);
    QString mpvEventName(int eventId) const;

    mpv_handle* m_mpv = nullptr;
    QTimer m_uiTimer;
    QWidget* m_rootWidget = nullptr;
    QWidget* m_videoWidget = nullptr;
    QWidget* m_controls = nullptr;
    QDockWidget* m_playlistDock = nullptr;
    QListWidget* m_playlist = nullptr;
    QSlider* m_seekSlider = nullptr;
    QSlider* m_volumeSlider = nullptr;
    QLabel* m_timeLabel = nullptr;
    QLabel* m_currentTimeLabel = nullptr;
    QLabel* m_progressTimeLabel = nullptr;
    QLabel* m_abLoopLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_playButton = nullptr;
    QPushButton* m_hwButton = nullptr;
    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QPushButton* m_seekBackButton = nullptr;
    QPushButton* m_seekForwardButton = nullptr;
    QPushButton* m_cutAbButton = nullptr;
    QPushButton* m_rotateButton = nullptr;
    QPushButton* m_logButton = nullptr;
    QCheckBox* m_autoplayCheck = nullptr;
    QPushButton* m_loopPlaylistButton = nullptr;
    QProcess* m_cutProcess = nullptr;
    QProcess* m_powerInhibitProcess = nullptr;
    bool m_playbackInhibited = false;
    std::atomic_bool m_mpvWakeQueued{false};
    QString m_cutOutputPath;
    bool m_seeking = false;
    bool m_panningVideo = false;
    QPointF m_panStart;
    double m_videoPanX = 0.0;
    double m_videoPanY = 0.0;
    double m_abLoopStart = -1.0;
    double m_abLoopEnd = -1.0;
    int m_currentPlaylistIndex = -1;
    bool m_autoplayPlaylist = true;
    bool m_loopPlaylist = false;
    bool m_showRemainingTime = false;
    bool m_timerBesideProgress = false;
    bool m_cutWithZoom = false;
    bool m_promptResumeNextLoad = false;
    QString m_pendingResumePath;
    QString m_runtimeLogPath;
    double m_pendingResumePosition = 0.0;
    qint64 m_lastPositionSaveMs = 0;
    qint64 m_lastEscapePressMs = 0;

    QString m_seekWheelMode = QStringLiteral("wheel");
    QString m_zoomWheelMode = QStringLiteral("alt-wheel");
    QString m_volumeWheelMode = QStringLiteral("ctrl-wheel");
    Qt::MouseButton m_panButton = Qt::MiddleButton;
    Qt::MouseButton m_doubleClickButton = Qt::LeftButton;
    bool m_doubleClickZones = true;
    int m_seekDurationSeconds = 60;
    QKeySequence m_volumeUpKey = QKeySequence(Qt::SHIFT | Qt::Key_V);
    QKeySequence m_volumeDownKey = QKeySequence(Qt::Key_V);
    QKeySequence m_muteKey = QKeySequence(Qt::Key_M);
    QKeySequence m_seekBackwardKey = QKeySequence(Qt::Key_Left);
    QKeySequence m_seekForwardKey = QKeySequence(Qt::Key_Right);
    QKeySequence m_loopAKey = QKeySequence(Qt::Key_A);
    QKeySequence m_loopBKey = QKeySequence(Qt::Key_B);
    QKeySequence m_loopClearKey = QKeySequence(Qt::Key_L);
    QKeySequence m_zoomInKey = QKeySequence(Qt::SHIFT | Qt::Key_Equal);
    QKeySequence m_zoomOutKey = QKeySequence(Qt::Key_Minus);
    QKeySequence m_zoomResetKey = QKeySequence(Qt::Key_Z);
    QKeySequence m_frameBackKey = QKeySequence(Qt::Key_Comma);
    QKeySequence m_frameForwardKey = QKeySequence(Qt::Key_Period);
    QKeySequence m_switchSubtitlesKey = QKeySequence(Qt::Key_S);
    QKeySequence m_subtitlePosUpKey = QKeySequence(Qt::ControlModifier | Qt::Key_Up);
    QKeySequence m_subtitlePosDownKey = QKeySequence(Qt::ControlModifier | Qt::Key_Down);
    QKeySequence m_subtitleSizeUpKey = QKeySequence(Qt::SHIFT | Qt::Key_I);
    QKeySequence m_subtitleSizeDownKey = QKeySequence(Qt::Key_I);
    QKeySequence m_captureScreenshotKey = QKeySequence(Qt::Key_C);
    QKeySequence m_rotateVideoKey = QKeySequence(Qt::Key_R);
    int m_videoRotation = 0;
    bool m_flipHorizontal = false;
    bool m_flipVertical = false;
    int m_saturation = 0;
    int m_brightness = 0;
    int m_contrast = 0;
    QTimer m_fullscreenHideTimer;
    QTimer m_cursorHideTimer;
    bool m_playlistWasVisibleBeforeFullscreen = false;
};
