#pragma once

#include <QMainWindow>
#include <atomic>
#include <QPointF>
#include <QKeySequence>
#include <QTimer>
#include <QDateTime>
#include <QFont>

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
class QShortcut;
class QProcess;
class AudioExporter;
class RuntimeLogger;
class PlaybackPositionManager;
class PlaylistController;
class VideoTransformer;
class TrackController;
class PlaybackInhibitor;
class DiagnosticReporter;
class DisplayController;
class ScreenshotController;
class ControlSettings;

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
    void keyReleaseEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void pumpMpvEvents();
    void openFile();
    void addFiles();
    void addFolder();
    void savePlaylist();
    void openPlaylist();
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
    void saveSelectedAudioTrack();
    void showSpeedMenu();
    void increasePlaybackSpeed();
    void decreasePlaybackSpeed();
    void cutAbSelection();
    void rotateVideo90();
    void toggleFlipHorizontal();
    void toggleFlipVertical();
    void increaseSubtitleSize();
    void decreaseSubtitleSize();
    void cycleSubtitles();
    void increaseSubtitlePosition();
    void decreaseSubtitlePosition();

signals:
    void mpvWakeup();

private:
    bool initializeMpv();
    static void mpvWakeupCallback(void* context);
    void buildUi();
    void loadFile(const QString& path);
    void addToPlaylist(const QString& path);
    void removePlaylistItem(int index);
    void playPlaylistIndex(int index, bool promptResume = true);
    void syncPlaylistSelection();
    void updatePlaylistCurrentRowStyle();
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
    void showToast(const QString& message);
    QString formatTime(double seconds) const;
    void setControlsVisible(bool visible);
    void toggleFullscreen();
    void resizeWindowForVideoAspect();
    void showDisplayDialog();
    void captureScreenshot();
    void saveLogReport();
    void loadControlSettings();
    void applyInterfaceFont(const QString& fontPath, const QString& fontFamily, int pointSize);
    void updatePlaybackInhibit(bool active);
    bool keyMatches(QKeyEvent* event, const QKeySequence& sequence) const;
    void saveCurrentPlaybackPosition();
    QString mpvEventName(int eventId) const;
    QString diagnosticControlState() const;

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
    QLabel* m_toastLabel = nullptr;
    QPushButton* m_playButton = nullptr;
    QPushButton* m_hwButton = nullptr;
    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QPushButton* m_seekBackButton = nullptr;
    QPushButton* m_seekForwardButton = nullptr;
    QPushButton* m_cutAbButton = nullptr;
    QPushButton* m_saveAudioButton = nullptr;
    QPushButton* m_speedButton = nullptr;
    QPushButton* m_rotateButton = nullptr;
    QCheckBox* m_autoplayCheck = nullptr;
    QPushButton* m_loopPlaylistButton = nullptr;
    QProcess* m_cutProcess = nullptr;
    AudioExporter* m_audioExporter = nullptr;
    RuntimeLogger* m_runtimeLogger = nullptr;
    PlaybackPositionManager* m_playbackPositions = nullptr;
    PlaylistController* m_playlistController = nullptr;
    VideoTransformer* m_videoTransformer = nullptr;
    TrackController* m_trackController = nullptr;
    PlaybackInhibitor* m_playbackInhibitor = nullptr;
    DiagnosticReporter* m_diagnosticReporter = nullptr;
    DisplayController* m_displayController = nullptr;
    ScreenshotController* m_screenshotController = nullptr;
    ControlSettings* m_controlSettings = nullptr;
    QFont m_defaultApplicationFont;
    std::atomic_bool m_mpvWakeQueued{false};
    QString m_cutOutputPath;
    bool m_seeking = false;
    bool m_panningVideo = false;
    QPointF m_panStart;
    double m_videoPanX = 0.0;
    double m_videoPanY = 0.0;
    double m_abLoopStart = -1.0;
    double m_abLoopEnd = -1.0;
    bool m_showRemainingTime = false;
    bool m_timerBesideProgress = false;
    bool m_cutWithZoom = false;
    bool m_promptResumeNextLoad = false;
    QString m_pendingResumePath;
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
    QKeySequence m_speedUpKey = QKeySequence(Qt::AltModifier | Qt::Key_Up);
    QKeySequence m_speedDownKey = QKeySequence(Qt::AltModifier | Qt::Key_Down);
    QShortcut* m_speedUpShortcut = nullptr;
    QShortcut* m_speedDownShortcut = nullptr;
    double m_speedJump = 0.10;
    QKeySequence m_holdSpeedKey = QKeySequence(Qt::Key_2);
    bool m_holdSpeedActive = false;
    double m_holdSpeedPrevious = 1.0;
    int m_holdSpeedKeyCode = Qt::Key_unknown;
    int m_saturation = 0;
    int m_brightness = 0;
    int m_contrast = 0;
    QTimer m_fullscreenHideTimer;
    QTimer m_cursorHideTimer;
    QTimer m_toastTimer;
    bool m_playlistWasVisibleBeforeFullscreen = false;
};