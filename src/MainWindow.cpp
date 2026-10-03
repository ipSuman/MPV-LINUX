#include "MainWindow.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCloseEvent>
#include <QCheckBox>
#include <QCoreApplication>
#include <QApplication>
#include <QCursor>
#include <QComboBox>
#include <QDir>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QStyle>
#include <QTextStream>
#include <QToolButton>
#include <QStyleOptionSlider>
#include <QUrl>
#include <QSettings>
#include <QScreen>
#include <QScrollArea>
#include <QSizePolicy>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include "MpvNodeUtils.h"
#include "AudioExporter.h"
#include "RuntimeLogger.h"
#include "PlaybackPositionManager.h"
#include "PlaylistController.h"
#include "VideoTransformer.h"
#include "TrackController.h"
#include "PlaybackInhibitor.h"
#include "DiagnosticReporter.h"
#include "DisplayController.h"
#include "ScreenshotController.h"
#include "ControlSettings.h"

#include <algorithm>
#include <cmath>
#include <clocale>
#include <cstdint>

#include <mpv/client.h>


namespace {
const QStringList kMediaExtensions = {
    QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("webm"),
    QStringLiteral("avi"), QStringLiteral("mov"), QStringLiteral("m4v"),
    QStringLiteral("ts"), QStringLiteral("m2ts"), QStringLiteral("flv"),
    QStringLiteral("wmv"), QStringLiteral("mpg"), QStringLiteral("mpeg"),
    QStringLiteral("3gp"), QStringLiteral("ogv"), QStringLiteral("mp3"),
    QStringLiteral("flac"), QStringLiteral("m4a"), QStringLiteral("aac"),
    QStringLiteral("opus"), QStringLiteral("wav")
};

bool isMediaFile(const QFileInfo& info) {
    return info.isFile() && kMediaExtensions.contains(info.suffix().toLower());
}

bool wheelModeMatches(const QString& mode, Qt::KeyboardModifiers modifiers) {
    if (mode == QStringLiteral("wheel")) return modifiers == Qt::NoModifier;
    if (mode == QStringLiteral("shift-wheel")) return modifiers == Qt::ShiftModifier;
    if (mode == QStringLiteral("alt-wheel")) return modifiers == Qt::AltModifier;
    if (mode == QStringLiteral("ctrl-wheel")) return modifiers == Qt::ControlModifier;
    return false;
}

void addWheelModes(QComboBox* combo) {
    combo->addItem(QStringLiteral("Mouse wheel"), QStringLiteral("wheel"));
    combo->addItem(QStringLiteral("Shift + wheel"), QStringLiteral("shift-wheel"));
    combo->addItem(QStringLiteral("Alt + wheel"), QStringLiteral("alt-wheel"));
    combo->addItem(QStringLiteral("Ctrl + wheel"), QStringLiteral("ctrl-wheel"));
    combo->addItem(QStringLiteral("Disabled"), QStringLiteral("off"));
}

void selectData(QComboBox* combo, const QVariant& value) {
    const int index = combo->findData(value);
    if (index >= 0) combo->setCurrentIndex(index);
}
}

MainWindow::MainWindow(const QString& mediaPath, QWidget* parent)
    : QMainWindow(parent) {
    m_defaultApplicationFont = QApplication::font();
    setWindowTitle(QStringLiteral("REX Player"));
    resize(1200, 760);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    m_controlSettings = new ControlSettings;
    loadControlSettings();
    m_playlistController = new PlaylistController();
    buildUi();

    m_toastTimer.setSingleShot(true);
    connect(&m_toastTimer, &QTimer::timeout, this, [this] {
        if (m_toastLabel) m_toastLabel->hide();
    });

    m_runtimeLogger = new RuntimeLogger(this);
    m_playbackPositions = new PlaybackPositionManager(this);
    m_audioExporter = new AudioExporter(this);
    connect(m_audioExporter, &AudioExporter::logMessage,
            m_runtimeLogger, &RuntimeLogger::append);
    connect(m_audioExporter, &AudioExporter::finished, this,
            [this](bool success, const QString& output, const QString& errorMessage,
                   const QString& stderrText, const QString&, int, QProcess::ExitStatus) {
        if (success) {
            showToast(QStringLiteral("Audio Saved"));
            showError(QStringLiteral("REX Player — Audio saved: %1")
                          .arg(QFileInfo(output).fileName()));
            QMessageBox::information(
                this, QStringLiteral("Save Audio"),
                QStringLiteral("Audio track saved successfully:\n%1").arg(output));
        } else {
            showError(QStringLiteral("REX Player — Audio save failed"));
            const QString details = !stderrText.isEmpty() ? stderrText : errorMessage;
            QMessageBox::warning(
                this, QStringLiteral("Save Audio"),
                QStringLiteral("Could not save the selected audio track.\n\n%1")
                    .arg(details.isEmpty()
                             ? QStringLiteral("FFmpeg returned an error.")
                             : details));
            if (!output.isEmpty()) QFile::remove(output);
        }
        if (m_saveAudioButton) m_saveAudioButton->setEnabled(true);

    });

    auto* enterFullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    enterFullscreenShortcut->setContext(Qt::WindowShortcut);
    connect(enterFullscreenShortcut, &QShortcut::activated, this, &MainWindow::toggleFullscreen);
    auto* keypadEnterFullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    keypadEnterFullscreenShortcut->setContext(Qt::WindowShortcut);
    connect(keypadEnterFullscreenShortcut, &QShortcut::activated, this, &MainWindow::toggleFullscreen);

    m_fullscreenHideTimer.setSingleShot(true);
    m_fullscreenHideTimer.setInterval(2500);
    connect(&m_fullscreenHideTimer, &QTimer::timeout, this, [this] {
        if (!isFullScreen() || !m_videoWidget) return;

        // Keep the fullscreen control panel visible while the mouse pointer
        // is still visible. It may hide only after the cursor is hidden.
        if (m_videoWidget->cursor().shape() == Qt::BlankCursor) {
            setControlsVisible(false);
        } else {
            m_fullscreenHideTimer.start(500);
        }
    });

    m_cursorHideTimer.setSingleShot(true);
    m_cursorHideTimer.setInterval(5000);
    connect(&m_cursorHideTimer, &QTimer::timeout, this, [this] {
        if (!m_videoWidget) return;
        const QPoint cursorPos = QCursor::pos();
        const QRect videoRect(m_videoWidget->mapToGlobal(QPoint(0, 0)), m_videoWidget->size());
        if (videoRect.contains(cursorPos)) m_videoWidget->setCursor(Qt::BlankCursor);
    });

    connect(this, &MainWindow::mpvWakeup, this, &MainWindow::pumpMpvEvents, Qt::QueuedConnection);

    if (!initializeMpv()) return;

    // m_mpv must exist before VideoTransformer captures the handle.
    m_videoTransformer = new VideoTransformer(m_mpv, m_runtimeLogger, this);

    m_trackController = new TrackController(m_mpv, m_runtimeLogger, this);

    m_playbackInhibitor = new PlaybackInhibitor(m_runtimeLogger, this);

    m_diagnosticReporter = new DiagnosticReporter(m_mpv, m_runtimeLogger, m_playlistController, this);
    m_diagnosticReporter->setControlStateProvider([this] { return diagnosticControlState(); });
    m_displayController = new DisplayController(
        this, &m_saturation, &m_brightness, &m_contrast,
        [this](const char* property, double value) { setPropertyDouble(property, value); }, this);
    connect(m_displayController, &DisplayController::logMessage,
            m_runtimeLogger, &RuntimeLogger::append);
    m_screenshotController = new ScreenshotController(m_mpv, this);
    connect(m_screenshotController, &ScreenshotController::errorMessage,
            this, &MainWindow::showError);
    connect(m_screenshotController, &ScreenshotController::successMessage,
            this, [this](const QString& message, const QString&) { showToast(message); });
    connect(m_screenshotController, &ScreenshotController::logMessage,
            m_runtimeLogger, &RuntimeLogger::append);

    m_uiTimer.setInterval(250);
    connect(&m_uiTimer, &QTimer::timeout, this, &MainWindow::updatePlaybackUi);
    m_uiTimer.start();

    if (!mediaPath.isEmpty()) loadFile(mediaPath);
}

MainWindow::~MainWindow() {
    m_uiTimer.stop();
    updatePlaybackInhibit(false);
    m_videoTransformer = nullptr;
    m_playbackInhibitor = nullptr;
    delete m_playlistController;
    m_playlistController = nullptr;
    delete m_controlSettings;
    m_controlSettings = nullptr;
    if (m_mpv) {
        // The wakeup callback may originate from an mpv worker thread. Unregister
        // it before destroying the client handle so no callback can target this
        // QObject after its lifetime ends.
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void MainWindow::buildUi() {
    auto* root = new QWidget(this);
    m_rootWidget = root;
    root->setObjectName(QStringLiteral("root"));
    root->setStyleSheet(QStringLiteral("QWidget#root{background:#000;}"));

    m_toastLabel = new QLabel(root);
    m_toastLabel->setObjectName(QStringLiteral("toastLabel"));
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->setStyleSheet(QStringLiteral(
        "QLabel#toastLabel{background:#222;color:#fff;border:1px solid #555;"
        "border-radius:6px;padding:7px 14px;font-weight:600;}"));
    m_toastLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_toastLabel->hide();
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_videoWidget = new QWidget(root);
    m_videoWidget->setAttribute(Qt::WA_NativeWindow);
    m_videoWidget->setFocusPolicy(Qt::StrongFocus);
    m_videoWidget->setMouseTracking(true);
    m_videoWidget->setAttribute(Qt::WA_AcceptTouchEvents);
    m_videoWidget->setStyleSheet(QStringLiteral("background:#000;"));
    m_videoWidget->setMinimumSize(320, 180);
    m_videoWidget->installEventFilter(this);
    layout->addWidget(m_videoWidget, 1);

    m_controls = new QWidget(root);
    m_controls->setObjectName(QStringLiteral("controls"));
    m_controls->setStyleSheet(QStringLiteral(
        "QWidget#controls{background:#171717;color:#eee;}"
        "QPushButton{background:transparent;color:#eee;border:0;padding:4px 6px;}"
        "QPushButton:hover{background:#303030;border-radius:5px;}"
        "QSlider::groove:horizontal{height:4px;background:#555;border-radius:2px;}"
        "QSlider::handle:horizontal{width:12px;margin:-4px 0;border-radius:6px;background:#ddd;}"
        "QLabel{color:#ddd;}"));
    auto* controlsLayout = new QVBoxLayout(m_controls);
    controlsLayout->setContentsMargins(8, 5, 8, 6);
    controlsLayout->setSpacing(3);

    auto* progressRow = new QHBoxLayout();
    progressRow->setContentsMargins(0, 0, 0, 0);
    progressRow->setSpacing(6);

    m_currentTimeLabel = new QLabel(QStringLiteral("00:00"), m_controls);
    m_currentTimeLabel->setFixedWidth(64);
    m_currentTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_currentTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    progressRow->addWidget(m_currentTimeLabel);

    m_seekSlider = new QSlider(Qt::Horizontal, m_controls);
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setTracking(false);
    m_seekSlider->installEventFilter(this);
    connect(m_seekSlider, &QSlider::sliderPressed, this, [this] { m_seeking = true; });
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this] {
        m_seeking = false;
        seekTo(m_seekSlider->value());
    });
    progressRow->addWidget(m_seekSlider, 1);

    m_progressTimeLabel = new QLabel(QStringLiteral("00:00"), m_controls);
    m_progressTimeLabel->setFixedWidth(64);
    m_progressTimeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_progressTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_progressTimeLabel->setToolTip(QStringLiteral("Click to switch between total and remaining time"));
    m_progressTimeLabel->setCursor(Qt::PointingHandCursor);
    m_progressTimeLabel->installEventFilter(this);
    progressRow->addWidget(m_progressTimeLabel);
    progressRow->setAlignment(Qt::AlignVCenter);
    controlsLayout->addLayout(progressRow);

    // Two control rows matching the reference layout exactly.
    // Row 1: fullscreen/open/decoder/A-B, playback cluster, volume/tracks/playlist/menu.
    // Row 2: settings/capture/transforms on the left and About on the right.
    auto* row1 = new QHBoxLayout();
    row1->setContentsMargins(0, 0, 0, 0);
    row1->setSpacing(10);

    auto* fullscreenButton = new QPushButton(QStringLiteral("⛶"), m_controls);
    fullscreenButton->setToolTip(QStringLiteral("Toggle fullscreen"));
    fullscreenButton->setFixedWidth(32);
    connect(fullscreenButton, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    row1->addWidget(fullscreenButton);

    auto* open = new QPushButton(QStringLiteral("Open"), m_controls);
    connect(open, &QPushButton::clicked, this, &MainWindow::openFile);
    row1->addWidget(open);

    m_hwButton = new QPushButton(QStringLiteral("SW"), m_controls);
    m_hwButton->setToolTip(QStringLiteral("Software decoding. Click to enable hardware decoding when supported."));
    connect(m_hwButton, &QPushButton::clicked, this, &MainWindow::toggleHardwareDecoding);
    row1->addWidget(m_hwButton);

    m_abLoopLabel = new QLabel(QStringLiteral("AB Off"), m_controls);
    m_abLoopLabel->setToolTip(QStringLiteral("A: set loop start, B: set loop end, L: clear loop"));
    row1->addWidget(m_abLoopLabel);

    m_cutAbButton = new QPushButton(QStringLiteral("AB Cut"), m_controls);
    m_cutAbButton->setToolTip(QStringLiteral("Cut the current A-B selection with FFmpeg without re-encoding"));
    connect(m_cutAbButton, &QPushButton::clicked, this, &MainWindow::cutAbSelection);
    row1->addWidget(m_cutAbButton);

    m_saveAudioButton = new QPushButton(QStringLiteral("Save Audio"), m_controls);
    m_saveAudioButton->setToolTip(QStringLiteral("Save the selected audio track"));
    connect(m_saveAudioButton, &QPushButton::clicked, this, &MainWindow::saveSelectedAudioTrack);
    row1->addWidget(m_saveAudioButton);
    
    m_speedButton = new QPushButton(QStringLiteral("Speed"), m_controls);
    m_speedButton->setToolTip(QStringLiteral("Change playback speed"));
    connect(m_speedButton, &QPushButton::clicked, this, &MainWindow::showSpeedMenu);
    row1->addWidget(m_speedButton);

    m_timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"), m_controls);
    m_timeLabel->setToolTip(QStringLiteral("Click to switch between elapsed / total and elapsed / remaining time"));
    m_timeLabel->setCursor(Qt::PointingHandCursor);
    m_timeLabel->installEventFilter(this);
    row1->addWidget(m_timeLabel);

    row1->addStretch(1);

    m_previousButton = new QPushButton(QStringLiteral("|«  "), m_controls);
    m_previousButton->setToolTip(QStringLiteral("Previous item"));
    m_previousButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(m_previousButton, &QPushButton::clicked, this, &MainWindow::playPrevious);
    row1->addWidget(m_previousButton);

    m_seekBackButton = new QPushButton(QStringLiteral("−10s"), m_controls);
    m_seekBackButton->setToolTip(QStringLiteral("Seek backward 10 seconds"));
    connect(m_seekBackButton, &QPushButton::clicked, this, [this] {
        const char* args[] = {"seek", "-10", "relative", "exact", nullptr};
        command(args);
    });
    row1->addWidget(m_seekBackButton);

    m_playButton = new QPushButton(QStringLiteral("  ▶  "), m_controls);
    m_playButton->setToolTip(QStringLiteral("Play / pause"));
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::togglePause);
    row1->addWidget(m_playButton);

    m_seekForwardButton = new QPushButton(QStringLiteral("+10s"), m_controls);
    m_seekForwardButton->setToolTip(QStringLiteral("Seek forward 10 seconds"));
    connect(m_seekForwardButton, &QPushButton::clicked, this, [this] {
        const char* args[] = {"seek", "10", "relative", "exact", nullptr};
        command(args);
    });
    row1->addWidget(m_seekForwardButton);

    m_nextButton = new QPushButton(QStringLiteral("  »|"), m_controls);
    m_nextButton->setToolTip(QStringLiteral("Next item"));
    m_nextButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(m_nextButton, &QPushButton::clicked, this, &MainWindow::playNext);
    row1->addWidget(m_nextButton);

    row1->addStretch(1);

    row1->addWidget(new QLabel(QStringLiteral("Volume"), m_controls));
    m_volumeSlider = new QSlider(Qt::Horizontal, m_controls);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(60);
    m_volumeSlider->setFixedWidth(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::setVolume);
    row1->addWidget(m_volumeSlider);

    auto* tracks = new QPushButton(QStringLiteral("Tracks"), m_controls);
    tracks->setToolTip(QStringLiteral("Select audio and subtitle tracks"));
    connect(tracks, &QPushButton::clicked, this, &MainWindow::showTracksMenu);
    row1->addWidget(tracks);

    auto* playlistButton = new QPushButton(QStringLiteral("Playlist"), m_controls);
    playlistButton->setToolTip(QStringLiteral("Show or hide playlist"));
    connect(playlistButton, &QPushButton::clicked, this, &MainWindow::togglePlaylist);
    row1->addWidget(playlistButton);

    auto* menu = new QPushButton(QStringLiteral("☰"), m_controls);
    menu->setToolTip(QStringLiteral("Show video information"));
    connect(menu, &QPushButton::clicked, this, &MainWindow::toggleControls);
    row1->addWidget(menu);

    auto* row2 = new QHBoxLayout();
    row2->setContentsMargins(42, 0, 0, 0);
    row2->setSpacing(8);

    auto* controlsButton = new QPushButton(QStringLiteral("Controls"), m_controls);
    controlsButton->setToolTip(QStringLiteral("Customize mouse and keyboard controls"));
    connect(controlsButton, &QPushButton::clicked, this, &MainWindow::showControlsDialog);
    row2->addWidget(controlsButton);

    auto* displayButton = new QPushButton(QStringLiteral("Display"), m_controls);
    displayButton->setToolTip(QStringLiteral("Adjust brightness, contrast and saturation"));
    connect(displayButton, &QPushButton::clicked, this, &MainWindow::showDisplayDialog);
    row2->addWidget(displayButton);

    auto* captureButton = new QPushButton(QStringLiteral("Capture"), m_controls);
    captureButton->setToolTip(QStringLiteral("Save the current video frame as a screenshot"));
    connect(captureButton, &QPushButton::clicked, this, &MainWindow::captureScreenshot);
    row2->addWidget(captureButton);

    m_rotateButton = new QPushButton(QStringLiteral("Rotate"), m_controls);
    m_rotateButton->setToolTip(QStringLiteral("Rotate the video 90° clockwise"));
    connect(m_rotateButton, &QPushButton::clicked, this, &MainWindow::rotateVideo90);
    row2->addWidget(m_rotateButton);

    auto* flipHorizontalButton = new QPushButton(QStringLiteral("Flip H"), m_controls);
    flipHorizontalButton->setToolTip(QStringLiteral("Flip the video horizontally"));
    connect(flipHorizontalButton, &QPushButton::clicked, this, &MainWindow::toggleFlipHorizontal);
    row2->addWidget(flipHorizontalButton);

    auto* flipVerticalButton = new QPushButton(QStringLiteral("Flip V"), m_controls);
    flipVerticalButton->setToolTip(QStringLiteral("Flip the video vertically"));
    connect(flipVerticalButton, &QPushButton::clicked, this, &MainWindow::toggleFlipVertical);
    row2->addWidget(flipVerticalButton);

    row2->addStretch(1);

    auto* aboutButton = new QPushButton(QStringLiteral("About"), m_controls);
    aboutButton->setToolTip(QStringLiteral("About REX Player"));
    connect(aboutButton, &QPushButton::clicked, this, [this] {
        QMessageBox::about(
            this,
            QStringLiteral("About REX Player"),
            QStringLiteral("REX Player 3.5.0\n\nA libmpv-based video player."));
    });
    row2->addWidget(aboutButton);

    controlsLayout->addLayout(row1);
    controlsLayout->addLayout(row2);

    // Keep the control bar in the root vertical layout so it stays at the
    // bottom of the window in normal mode.
    layout->addWidget(m_controls);

    setCentralWidget(root);

    // Playlist panel. Keep it as a real QDockWidget so the Playlist button
    // always has a concrete widget to show/hide.
    m_playlistDock = new QDockWidget(QStringLiteral("Playlist"), this);
    m_playlistDock->setObjectName(QStringLiteral("playlistDock"));
    m_playlistDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_playlistDock->setFeatures(QDockWidget::DockWidgetClosable |
                                QDockWidget::DockWidgetMovable);

    auto* playlistPanel = new QWidget(m_playlistDock);
    playlistPanel->setStyleSheet(QStringLiteral(
        "QWidget{background:#171717;color:#eee;}"
        "QListWidget{background:#111;color:#eee;border:0;}"
        "QPushButton{background:#252525;color:#eee;border:0;padding:6px 9px;}"
        "QPushButton:hover{background:#353535;border-radius:4px;}"));
    auto* playlistLayout = new QVBoxLayout(playlistPanel);
    playlistLayout->setContentsMargins(6, 6, 6, 6);
    playlistLayout->setSpacing(6);

    m_playlist = new QListWidget(playlistPanel);
    m_playlist->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlist->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_playlist->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_playlist, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem*) { playlistActivated(); });
    playlistLayout->addWidget(m_playlist, 1);

    auto* playlistButtons = new QHBoxLayout();
    playlistButtons->setContentsMargins(0, 0, 0, 0);
    playlistButtons->setSpacing(4);

    auto* openFolderButton = new QPushButton(QStringLiteral("Open Folder"), playlistPanel);
    openFolderButton->setToolTip(QStringLiteral("Add all supported media files from a folder to the playlist"));
    connect(openFolderButton, &QPushButton::clicked, this, &MainWindow::addFolder);
    playlistButtons->addWidget(openFolderButton);

    m_autoplayCheck = new QCheckBox(QStringLiteral("Autoplay"), playlistPanel);
    m_autoplayCheck->setChecked(m_playlistController && m_playlistController->autoplay());
    m_autoplayCheck->setToolTip(
        QStringLiteral("Automatically play the next playlist item when the current item reaches the end."));
    connect(m_autoplayCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_playlistController) m_playlistController->setAutoplay(enabled);
    });
    playlistButtons->addWidget(m_autoplayCheck);

    auto* savePlaylistButton = new QPushButton(QStringLiteral("Save Playlist"), playlistPanel);
    connect(savePlaylistButton, &QPushButton::clicked, this, &MainWindow::savePlaylist);
    playlistButtons->addWidget(savePlaylistButton);

    auto* openPlaylistButton = new QPushButton(QStringLiteral("Open Playlist"), playlistPanel);
    connect(openPlaylistButton, &QPushButton::clicked, this, &MainWindow::openPlaylist);
    playlistButtons->addWidget(openPlaylistButton);

    auto* clearPlaylistButton = new QPushButton(QStringLiteral("Clear"), playlistPanel);
    connect(clearPlaylistButton, &QPushButton::clicked, this, &MainWindow::clearPlaylist);
    playlistButtons->addWidget(clearPlaylistButton);

    playlistLayout->addLayout(playlistButtons);

    m_playlistDock->setWidget(playlistPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_playlistDock);
    m_playlistDock->hide();

    updateSeekButtonLabels();
}

void MainWindow::loadControlSettings() {
    if (!m_controlSettings) return;
    m_controlSettings->load();

    m_seekWheelMode = m_controlSettings->seekWheelMode;
    m_zoomWheelMode = m_controlSettings->zoomWheelMode;
    m_volumeWheelMode = m_controlSettings->volumeWheelMode;
    m_timerBesideProgress = m_controlSettings->timerBesideProgress;
    m_panButton = m_controlSettings->panButton;
    m_doubleClickButton = m_controlSettings->doubleClickButton;
    m_seekDurationSeconds = m_controlSettings->seekDurationSeconds;
    applyInterfaceFont(m_controlSettings->fontPath, m_controlSettings->fontFamily, m_controlSettings->fontSize);
    m_volumeUpKey = m_controlSettings->volumeUpKey;
    m_volumeDownKey = m_controlSettings->volumeDownKey;
    m_muteKey = m_controlSettings->muteKey;
    m_seekBackwardKey = m_controlSettings->seekBackwardKey;
    m_seekForwardKey = m_controlSettings->seekForwardKey;
    m_loopAKey = m_controlSettings->loopAKey;
    m_loopBKey = m_controlSettings->loopBKey;
    m_loopClearKey = m_controlSettings->loopClearKey;
    m_zoomInKey = m_controlSettings->zoomInKey;
    m_zoomOutKey = m_controlSettings->zoomOutKey;
    m_zoomResetKey = m_controlSettings->zoomResetKey;
    m_frameBackKey = m_controlSettings->frameBackKey;
    m_frameForwardKey = m_controlSettings->frameForwardKey;
    m_switchSubtitlesKey = m_controlSettings->switchSubtitlesKey;
    m_subtitlePosUpKey = m_controlSettings->subtitlePosUpKey;
    m_subtitlePosDownKey = m_controlSettings->subtitlePosDownKey;
    m_subtitleSizeUpKey = m_controlSettings->subtitleSizeUpKey;
    m_subtitleSizeDownKey = m_controlSettings->subtitleSizeDownKey;
    m_captureScreenshotKey = m_controlSettings->captureScreenshotKey;
    m_rotateVideoKey = m_controlSettings->rotateVideoKey;
    m_speedUpKey = m_controlSettings->speedUpKey;
    m_speedDownKey = m_controlSettings->speedDownKey;
    m_speedJump = std::clamp(m_controlSettings->speedJump, 0.10, 1.00);
    m_holdSpeedKey = m_controlSettings->holdSpeedKey;
    m_cutWithZoom = m_controlSettings->cutWithZoom;

    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    m_saturation = std::clamp(settings.value(QStringLiteral("display/saturation"), m_saturation).toInt(), -100, 100);
    m_brightness = std::clamp(settings.value(QStringLiteral("display/brightness"), m_brightness).toInt(), -100, 100);
    m_contrast = std::clamp(settings.value(QStringLiteral("display/contrast"), m_contrast).toInt(), -100, 100);
}

void MainWindow::applyInterfaceFont(const QString& fontPath, const QString& fontFamily, int pointSize) {
    QFont font = m_defaultApplicationFont;

    QString family = fontFamily.trimmed();
    if (!fontPath.trimmed().isEmpty() && QFileInfo::exists(fontPath)) {
        const int fontId = QFontDatabase::addApplicationFont(fontPath);
        if (fontId >= 0) {
            const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
            if (!families.isEmpty()) family = families.first();
            if (m_runtimeLogger) m_runtimeLogger->append(
                QStringLiteral("FONT: loaded custom font file=%1 family=%2")
                    .arg(fontPath, family));
        } else {
            if (m_runtimeLogger) m_runtimeLogger->append(
                QStringLiteral("FONT: failed to load custom font file=%1")
                    .arg(fontPath));
        }
    }

    if (!family.isEmpty()) font.setFamily(family);
    if (pointSize > 0) font.setPointSize(pointSize);

    QApplication::setFont(font);

    // QApplication::setFont() updates the application default, but existing
    // widgets may retain an explicit/style-derived font. Reapply the selected
    // font to all current widgets so buttons, labels and dialogs update too.
    const auto widgets = QApplication::allWidgets();
    for (QWidget* widget : widgets) {
        if (widget) widget->setFont(font);
    }

    if (m_runtimeLogger) m_runtimeLogger->append(
        QStringLiteral("FONT: applied family=%1 size=%2")
            .arg(font.family())
            .arg(font.pointSizeF()));
}

void MainWindow::showControlsDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Controls"));
    dialog.setModal(true);
    dialog.resize(460, 560);
    dialog.setMinimumSize(380, 420);
    dialog.setSizeGripEnabled(true);

    auto* mainLayout = new QVBoxLayout(&dialog);
    auto* scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    QString selectedFontPath = m_controlSettings ? m_controlSettings->fontPath : QString();
    QString selectedFontFamily = m_controlSettings ? m_controlSettings->fontFamily : QString();
    if (selectedFontFamily.isEmpty()) selectedFontFamily = QApplication::font().family();

    auto* chooseFontButton = new QPushButton(
        selectedFontFamily.isEmpty() ? QStringLiteral("System default") : selectedFontFamily, &dialog);
    chooseFontButton->setToolTip(QStringLiteral("Load a downloaded TrueType, OpenType or TrueType Collection font file."));
    connect(chooseFontButton, &QPushButton::clicked, &dialog, [&, chooseFontButton] {
        const QString path = QFileDialog::getOpenFileName(
            &dialog, QStringLiteral("Choose font file"), QDir::homePath(),
            QStringLiteral("Font files (*.ttf *.otf *.ttc);;All files (*)"));
        if (path.isEmpty()) return;

        const int fontId = QFontDatabase::addApplicationFont(path);
        if (fontId < 0) {
            QMessageBox::warning(
                &dialog, QStringLiteral("Choose font"),
                QStringLiteral("Could not load this font file."));
            return;
        }

        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (families.isEmpty()) {
            QMessageBox::warning(
                &dialog, QStringLiteral("Choose font"),
                QStringLiteral("The font file did not expose a usable font family."));
            return;
        }

        selectedFontPath = path;
        selectedFontFamily = families.first();
        chooseFontButton->setText(selectedFontFamily);
        if (m_runtimeLogger) m_runtimeLogger->append(
            QStringLiteral("FONT: selected custom font file=%1 family=%2")
                .arg(selectedFontPath, selectedFontFamily));
    });
    form->addRow(QStringLiteral("Choose font"), chooseFontButton);

    auto* fontSize = new QSpinBox(&dialog);
    fontSize->setRange(8, 32);
    const int currentFontSize = QApplication::font().pointSize() > 0
        ? QApplication::font().pointSize()
        : 10;
    fontSize->setValue(m_controlSettings && m_controlSettings->fontSize > 0
        ? m_controlSettings->fontSize
        : currentFontSize);
    fontSize->setSuffix(QStringLiteral(" pt"));
    form->addRow(QStringLiteral("Font size"), fontSize);

    auto* seekDuration = new QComboBox(&dialog);
    for (int seconds : {5, 10, 30})
        seekDuration->addItem(QStringLiteral("%1 seconds").arg(seconds), seconds);
    for (int minutes = 1; minutes <= 120; ++minutes)
        seekDuration->addItem(QStringLiteral("%1 min").arg(minutes), minutes * 60);
    selectData(seekDuration, m_seekDurationSeconds);
    form->addRow(QStringLiteral("Seek duration"), seekDuration);

    auto* seekWheel = new QComboBox(&dialog);
    addWheelModes(seekWheel);
    selectData(seekWheel, m_seekWheelMode);
    form->addRow(QStringLiteral("Touchpad / wheel → Seek"), seekWheel);

    auto* zoomWheel = new QComboBox(&dialog);
    addWheelModes(zoomWheel);
    selectData(zoomWheel, m_zoomWheelMode);
    form->addRow(QStringLiteral("Wheel → Zoom"), zoomWheel);

    auto* volumeWheel = new QComboBox(&dialog);
    addWheelModes(volumeWheel);
    selectData(volumeWheel, m_volumeWheelMode);
    form->addRow(QStringLiteral("Wheel → Volume"), volumeWheel);

    auto* panButton = new QComboBox(&dialog);
    panButton->addItem(QStringLiteral("Left button"), static_cast<int>(Qt::LeftButton));
    panButton->addItem(QStringLiteral("Middle button"), static_cast<int>(Qt::MiddleButton));
    panButton->addItem(QStringLiteral("Right button"), static_cast<int>(Qt::RightButton));
    panButton->addItem(QStringLiteral("Disabled"), static_cast<int>(Qt::NoButton));
    selectData(panButton, static_cast<int>(m_panButton));
    form->addRow(QStringLiteral("Alt + Ctrl + drag → Pan"), panButton);

    auto* timerPositionButton = new QPushButton(
        m_timerBesideProgress ? QStringLiteral("Timer: Progress bar") : QStringLiteral("Timer: Controls"),
        &dialog);
    timerPositionButton->setCheckable(true);
    timerPositionButton->setChecked(m_timerBesideProgress);
    timerPositionButton->setToolTip(QStringLiteral("Switch the timer between the playback controls and the progress bar."));
    connect(timerPositionButton, &QPushButton::toggled, &dialog, [timerPositionButton](bool checked) {
        timerPositionButton->setText(checked ? QStringLiteral("Timer: Progress bar") : QStringLiteral("Timer: Controls"));
    });
    form->addRow(QStringLiteral("Timer position"), timerPositionButton);

    auto* cutWithZoomButton = new QPushButton(
        m_cutWithZoom ? QStringLiteral("Cut with zoom: On") : QStringLiteral("Cut with zoom: Off"),
        &dialog);
    cutWithZoomButton->setCheckable(true);
    cutWithZoomButton->setChecked(m_cutWithZoom);
    cutWithZoomButton->setToolTip(QStringLiteral("When enabled, Cut AB bakes the current video zoom and pan into the exported video. This requires video re-encoding; audio and subtitles are copied when possible."));
    connect(cutWithZoomButton, &QPushButton::toggled, &dialog, [cutWithZoomButton](bool checked) {
        cutWithZoomButton->setText(checked ? QStringLiteral("Cut with zoom: On") : QStringLiteral("Cut with zoom: Off"));
    });
    form->addRow(QStringLiteral("A-B cutting"), cutWithZoomButton);

    auto* resetResumeChoiceButton = new QPushButton(&dialog);
    QSettings resumeSettings(
        QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    const int rememberedResumeChoice =
        resumeSettings.value(QStringLiteral("playback/resume-choice"), -1).toInt();
    if (rememberedResumeChoice == 1) {
        resetResumeChoiceButton->setText(QStringLiteral("Remembered choice: Resume"));
    } else if (rememberedResumeChoice == 0) {
        resetResumeChoiceButton->setText(QStringLiteral("Remembered choice: Start from beginning"));
    } else {
        resetResumeChoiceButton->setText(QStringLiteral("No remembered resume choice"));
    }
    resetResumeChoiceButton->setToolTip(
        QStringLiteral("Clear the saved Resume playback choice so the prompt appears again."));
    connect(resetResumeChoiceButton, &QPushButton::clicked, &dialog,
            [resetResumeChoiceButton, this] {
                QSettings settings(
                    QStringLiteral("REX Player"), QStringLiteral("REX Player"));
                settings.remove(QStringLiteral("playback/resume-choice"));
                settings.sync();
                resetResumeChoiceButton->setText(
                    QStringLiteral("No remembered resume choice"));
                if (m_runtimeLogger) {
                    m_runtimeLogger->append(
                        QStringLiteral("RESUME: remembered choice reset"));
                }
            });
    form->addRow(QStringLiteral("Resume playback"), resetResumeChoiceButton);

    auto* doubleClickButton = new QComboBox(&dialog);
    doubleClickButton->addItem(QStringLiteral("Left button"), static_cast<int>(Qt::LeftButton));
    doubleClickButton->addItem(QStringLiteral("Middle button"), static_cast<int>(Qt::MiddleButton));
    doubleClickButton->addItem(QStringLiteral("Right button"), static_cast<int>(Qt::RightButton));
    doubleClickButton->addItem(QStringLiteral("Disabled"), static_cast<int>(Qt::NoButton));
    selectData(doubleClickButton, static_cast<int>(m_doubleClickButton));
    form->addRow(QStringLiteral("Double-click zones"), doubleClickButton);

    // Place the Mouse / touchpad heading directly after the Font size row.
    auto* mouseTouchpadHeading = new QLabel(QStringLiteral("Mouse / touchpad"), content);
    mouseTouchpadHeading->setStyleSheet(QStringLiteral("font-weight:600;"));
    form->insertRow(2, mouseTouchpadHeading);

    contentLayout->addLayout(form);
    contentLayout->addWidget(new QLabel(QStringLiteral("Keyboard shortcuts"), content));

    auto* keyForm = new QFormLayout();
    auto* volumeUp = new QKeySequenceEdit(m_volumeUpKey, &dialog);
    auto* volumeDown = new QKeySequenceEdit(m_volumeDownKey, &dialog);
    auto* mute = new QKeySequenceEdit(m_muteKey, &dialog);
    auto* seekBack = new QKeySequenceEdit(m_seekBackwardKey, &dialog);
    auto* seekForward = new QKeySequenceEdit(m_seekForwardKey, &dialog);
    auto* loopA = new QKeySequenceEdit(m_loopAKey, &dialog);
    auto* loopB = new QKeySequenceEdit(m_loopBKey, &dialog);
    auto* loopClear = new QKeySequenceEdit(m_loopClearKey, &dialog);
    auto* zoomIn = new QKeySequenceEdit(m_zoomInKey, &dialog);
    auto* zoomOut = new QKeySequenceEdit(m_zoomOutKey, &dialog);
    auto* zoomReset = new QKeySequenceEdit(m_zoomResetKey, &dialog);
    auto* frameBack = new QKeySequenceEdit(m_frameBackKey, &dialog);
    auto* frameForward = new QKeySequenceEdit(m_frameForwardKey, &dialog);
    auto* switchSubtitles = new QKeySequenceEdit(m_switchSubtitlesKey, &dialog);
    auto* subtitlePosUp = new QKeySequenceEdit(m_subtitlePosUpKey, &dialog);
    auto* subtitlePosDown = new QKeySequenceEdit(m_subtitlePosDownKey, &dialog);
    auto* subtitleSizeUp = new QKeySequenceEdit(m_subtitleSizeUpKey, &dialog);
    auto* subtitleSizeDown = new QKeySequenceEdit(m_subtitleSizeDownKey, &dialog);
    auto* captureScreenshot = new QKeySequenceEdit(m_captureScreenshotKey, &dialog);
    auto* rotateVideo = new QKeySequenceEdit(m_rotateVideoKey, &dialog);
    auto* speedUp = new QKeySequenceEdit(m_speedUpKey, &dialog);
    auto* speedDown = new QKeySequenceEdit(m_speedDownKey, &dialog);
    auto* speedJump = new QComboBox(&dialog);
    for (int i = 2; i <= 20; ++i) {
        const double jump = i * 0.05;
        speedJump->addItem(QStringLiteral("%1x").arg(jump, 0, 'f', 2), jump);
    }
    selectData(speedJump, m_speedJump);
    auto* holdSpeed = new QKeySequenceEdit(m_holdSpeedKey, &dialog);
    const QList<QKeySequenceEdit*> edits = {volumeUp, volumeDown, mute, seekBack, seekForward, loopA, loopB, loopClear, zoomIn, zoomOut, zoomReset, frameBack, frameForward, switchSubtitles, subtitlePosUp, subtitlePosDown, subtitleSizeUp, subtitleSizeDown, captureScreenshot, rotateVideo, speedUp, speedDown, holdSpeed};
    for (auto* edit : edits) edit->setClearButtonEnabled(false);

    auto addShortcut = [&keyForm, &dialog](const QString& label, QKeySequenceEdit* edit) {
        auto* row = new QWidget(&dialog);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(edit, 1);
        auto* clearButton = new QToolButton(row);
        clearButton->setText(QStringLiteral("×"));
        clearButton->setToolTip(QStringLiteral("Clear shortcut"));
        clearButton->setFixedSize(28, 28);
        clearButton->setAutoRaise(true);
        clearButton->setStyleSheet(QStringLiteral("QToolButton{font-weight:600;color:#ddd;border:0;}QToolButton:hover{background:#3a3a3a;border-radius:4px;}"));
        QObject::connect(clearButton, &QToolButton::clicked, edit, &QKeySequenceEdit::clear);
        rowLayout->addWidget(clearButton);
        keyForm->addRow(label, row);
    };

    addShortcut(QStringLiteral("Shift + V → Volume +"), volumeUp);
    addShortcut(QStringLiteral("V → Volume −"), volumeDown);
    addShortcut(QStringLiteral("M → Mute / unmute"), mute);
    addShortcut(QStringLiteral("Left Arrow → Seek backward"), seekBack);
    addShortcut(QStringLiteral("Right Arrow → Seek forward"), seekForward);
    addShortcut(QStringLiteral("A → Loop start"), loopA);
    addShortcut(QStringLiteral("B → Loop end"), loopB);
    addShortcut(QStringLiteral("L → Clear loop"), loopClear);
    addShortcut(QStringLiteral("+ → Zoom in"), zoomIn);
    addShortcut(QStringLiteral("− → Zoom out"), zoomOut);
    addShortcut(QStringLiteral("Z → Reset zoom / pan"), zoomReset);
    addShortcut(QStringLiteral(", → Previous frame"), frameBack);
    addShortcut(QStringLiteral(". → Next frame"), frameForward);
    addShortcut(QStringLiteral("S → Switch subtitles"), switchSubtitles);
    addShortcut(QStringLiteral("Ctrl + Up → Lift subtitles upward"), subtitlePosUp);
    addShortcut(QStringLiteral("Ctrl + Down → Lift subtitles downward"), subtitlePosDown);
    addShortcut(QStringLiteral("Shift + I → Increase subtitle text size"), subtitleSizeUp);
    addShortcut(QStringLiteral("I → Decrease subtitle text size"), subtitleSizeDown);
    addShortcut(QStringLiteral("C → Capture screenshot"), captureScreenshot);
    addShortcut(QStringLiteral("R → Rotate video 90° clockwise"), rotateVideo);
    addShortcut(QStringLiteral("Alt + Up → Increase playback speed"), speedUp);
    addShortcut(QStringLiteral("Alt + Down → Decrease playback speed"), speedDown);
    keyForm->addRow(QStringLiteral("Alt + Up / Down → Speed jump"), speedJump);
    addShortcut(QStringLiteral("2 (hold) → Temporary 2x playback speed"), holdSpeed);
    contentLayout->addLayout(keyForm);

    auto* note = new QLabel(QStringLiteral("Seek duration applies to the arrow keys, wheel seek and double-click seek zones. Choose 5, 10 or 30 seconds, or a value from 1 to 120 minutes. The −10s and +10s buttons always seek exactly 10 seconds. Speed jump controls how much Alt + Up / Down changes playback speed; choose 0.10x to 1.00x in 0.05x steps. Changes are saved for the next launch. Clear a shortcut to disable it. Cut with zoom bakes positive video zoom/pan and the current 90°-step rotation into the A-B output and therefore re-encodes the video."), &dialog);
    note->setWordWrap(true);
    contentLayout->addWidget(note);
    content->setLayout(contentLayout);
    scrollArea->setWidget(content);
    mainLayout->addWidget(scrollArea, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* saveLog = buttons->addButton(QStringLiteral("Save Log"), QDialogButtonBox::ActionRole);
    saveLog->setToolTip(QStringLiteral("Save a diagnostic log report"));
    connect(saveLog, &QPushButton::clicked, &dialog, [this] { saveLogReport(); });
    auto* reset = buttons->addButton(QStringLiteral("Reset defaults"), QDialogButtonBox::ResetRole);
    mainLayout->addWidget(buttons);

    connect(reset, &QPushButton::clicked, &dialog, [&] {
        selectedFontPath.clear();
        selectedFontFamily = m_defaultApplicationFont.family();
        chooseFontButton->setText(QStringLiteral("System default"));
        fontSize->setValue(m_defaultApplicationFont.pointSize() > 0 ? m_defaultApplicationFont.pointSize() : 10);
        selectData(seekDuration, 60);
        selectData(seekWheel, QStringLiteral("wheel"));
        selectData(zoomWheel, QStringLiteral("alt-wheel"));
        selectData(volumeWheel, QStringLiteral("ctrl-wheel"));
        timerPositionButton->setChecked(true);
        selectData(panButton, static_cast<int>(Qt::MiddleButton));
        selectData(doubleClickButton, static_cast<int>(Qt::LeftButton));
        volumeUp->setKeySequence(QKeySequence(Qt::SHIFT | Qt::Key_V));
        volumeDown->setKeySequence(QKeySequence(Qt::Key_V));
        mute->setKeySequence(QKeySequence(Qt::Key_M));
        seekBack->setKeySequence(QKeySequence(Qt::Key_Left));
        seekForward->setKeySequence(QKeySequence(Qt::Key_Right));
        loopA->setKeySequence(QKeySequence(Qt::Key_A));
        loopB->setKeySequence(QKeySequence(Qt::Key_B));
        loopClear->setKeySequence(QKeySequence(Qt::Key_L));
        zoomIn->setKeySequence(QKeySequence(Qt::SHIFT | Qt::Key_Equal));
        zoomOut->setKeySequence(QKeySequence(Qt::Key_Minus));
        zoomReset->setKeySequence(QKeySequence(Qt::Key_Z));
        frameBack->setKeySequence(QKeySequence(Qt::Key_Comma));
        frameForward->setKeySequence(QKeySequence(Qt::Key_Period));
        switchSubtitles->setKeySequence(QKeySequence(Qt::Key_S));
        subtitlePosUp->setKeySequence(QKeySequence(Qt::ControlModifier | Qt::Key_Up));
        subtitlePosDown->setKeySequence(QKeySequence(Qt::ControlModifier | Qt::Key_Down));
        subtitleSizeUp->setKeySequence(QKeySequence(Qt::SHIFT | Qt::Key_I));
        subtitleSizeDown->setKeySequence(QKeySequence(Qt::Key_I));
        captureScreenshot->setKeySequence(QKeySequence(Qt::Key_C));
        rotateVideo->setKeySequence(QKeySequence(Qt::Key_R));
        speedUp->setKeySequence(QKeySequence(Qt::AltModifier | Qt::Key_Up));
        speedDown->setKeySequence(QKeySequence(Qt::AltModifier | Qt::Key_Down));
        speedJump->setCurrentIndex(speedJump->findData(0.10));
        holdSpeed->setKeySequence(QKeySequence(Qt::Key_2));
        cutWithZoomButton->setChecked(false);
    });

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const QStringList wheelModes = {seekWheel->currentData().toString(), zoomWheel->currentData().toString(), volumeWheel->currentData().toString()};
        for (int i = 0; i < wheelModes.size(); ++i) {
            if (wheelModes[i] == QStringLiteral("off")) continue;
            for (int j = i + 1; j < wheelModes.size(); ++j) {
                if (wheelModes[i] == wheelModes[j]) {
                    QMessageBox::warning(&dialog, QStringLiteral("Controls"), QStringLiteral("The same wheel gesture is assigned to more than one action. Please choose different gestures."));
                    return;
                }
            }
        }
        selectedFontFamily = selectedFontFamily.trimmed();
        const int selectedFontSize = fontSize->value();
        m_seekDurationSeconds = seekDuration->currentData().toInt();
        m_seekWheelMode = seekWheel->currentData().toString();
        m_zoomWheelMode = zoomWheel->currentData().toString();
        m_volumeWheelMode = volumeWheel->currentData().toString();
        m_timerBesideProgress = timerPositionButton->isChecked();
        m_panButton = static_cast<Qt::MouseButton>(panButton->currentData().toInt());
        m_doubleClickButton = static_cast<Qt::MouseButton>(doubleClickButton->currentData().toInt());
        m_volumeUpKey = volumeUp->keySequence();
        m_volumeDownKey = volumeDown->keySequence();
        m_muteKey = mute->keySequence();
        m_seekBackwardKey = seekBack->keySequence();
        m_seekForwardKey = seekForward->keySequence();
        m_loopAKey = loopA->keySequence();
        m_loopBKey = loopB->keySequence();
        m_loopClearKey = loopClear->keySequence();
        m_zoomInKey = zoomIn->keySequence();
        m_zoomOutKey = zoomOut->keySequence();
        m_zoomResetKey = zoomReset->keySequence();
        m_frameBackKey = frameBack->keySequence();
        m_frameForwardKey = frameForward->keySequence();
        m_switchSubtitlesKey = switchSubtitles->keySequence();
        m_subtitlePosUpKey = subtitlePosUp->keySequence();
        m_subtitlePosDownKey = subtitlePosDown->keySequence();
        m_subtitleSizeUpKey = subtitleSizeUp->keySequence();
        m_subtitleSizeDownKey = subtitleSizeDown->keySequence();
        m_captureScreenshotKey = captureScreenshot->keySequence();
        m_rotateVideoKey = rotateVideo->keySequence();
        m_speedUpKey = speedUp->keySequence();
        m_speedDownKey = speedDown->keySequence();
        m_speedJump = std::clamp(speedJump->currentData().toDouble(), 0.10, 1.00);
        m_holdSpeedKey = holdSpeed->keySequence();
        m_cutWithZoom = cutWithZoomButton->isChecked();

        if (m_controlSettings) {
            m_controlSettings->seekDurationSeconds = m_seekDurationSeconds;
            m_controlSettings->fontPath = selectedFontPath;
            m_controlSettings->fontFamily = selectedFontFamily == m_defaultApplicationFont.family() && selectedFontPath.isEmpty() ? QString() : selectedFontFamily;
            m_controlSettings->fontSize = selectedFontSize;
            m_controlSettings->seekWheelMode = m_seekWheelMode;
            m_controlSettings->zoomWheelMode = m_zoomWheelMode;
            m_controlSettings->volumeWheelMode = m_volumeWheelMode;
            m_controlSettings->timerBesideProgress = m_timerBesideProgress;
            m_controlSettings->panButton = m_panButton;
            m_controlSettings->doubleClickButton = m_doubleClickButton;
            m_controlSettings->volumeUpKey = m_volumeUpKey;
            m_controlSettings->volumeDownKey = m_volumeDownKey;
            m_controlSettings->muteKey = m_muteKey;
            m_controlSettings->seekBackwardKey = m_seekBackwardKey;
            m_controlSettings->seekForwardKey = m_seekForwardKey;
            m_controlSettings->loopAKey = m_loopAKey;
            m_controlSettings->loopBKey = m_loopBKey;
            m_controlSettings->loopClearKey = m_loopClearKey;
            m_controlSettings->zoomInKey = m_zoomInKey;
            m_controlSettings->zoomOutKey = m_zoomOutKey;
            m_controlSettings->zoomResetKey = m_zoomResetKey;
            m_controlSettings->frameBackKey = m_frameBackKey;
            m_controlSettings->frameForwardKey = m_frameForwardKey;
            m_controlSettings->switchSubtitlesKey = m_switchSubtitlesKey;
            m_controlSettings->subtitlePosUpKey = m_subtitlePosUpKey;
            m_controlSettings->subtitlePosDownKey = m_subtitlePosDownKey;
            m_controlSettings->subtitleSizeUpKey = m_subtitleSizeUpKey;
            m_controlSettings->subtitleSizeDownKey = m_subtitleSizeDownKey;
            m_controlSettings->captureScreenshotKey = m_captureScreenshotKey;
            m_controlSettings->rotateVideoKey = m_rotateVideoKey;
            m_controlSettings->speedUpKey = m_speedUpKey;
            m_controlSettings->speedDownKey = m_speedDownKey;
            m_controlSettings->speedJump = m_speedJump;
            m_controlSettings->holdSpeedKey = m_holdSpeedKey;
            m_controlSettings->cutWithZoom = m_cutWithZoom;
            m_controlSettings->save();
            applyInterfaceFont(m_controlSettings->fontPath, m_controlSettings->fontFamily, m_controlSettings->fontSize);
        }
        updateSeekButtonLabels();
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

void MainWindow::mpvWakeupCallback(void* context) {
    auto* window = static_cast<MainWindow*>(context);
    if (!window) return;

    // mpv may call this from arbitrary threads. The callback must only notify
    // the GUI thread; all libmpv API work remains in pumpMpvEvents().
    //
    // Coalesce wakeups so a burst of mpv events results in one Qt event rather
    // than an unbounded queue of identical wakeup notifications.
    if (!window->m_mpvWakeQueued.exchange(true)) {
        emit window->mpvWakeup();
    }
}

QString MainWindow::mpvEventName(int eventId) const {
    switch (eventId) {
    case MPV_EVENT_NONE: return QStringLiteral("NONE");
    case MPV_EVENT_SHUTDOWN: return QStringLiteral("SHUTDOWN");
    case MPV_EVENT_START_FILE: return QStringLiteral("START_FILE");
    case MPV_EVENT_END_FILE: return QStringLiteral("END_FILE");
    case MPV_EVENT_FILE_LOADED: return QStringLiteral("FILE_LOADED");
    case MPV_EVENT_IDLE: return QStringLiteral("IDLE");
    case MPV_EVENT_TICK: return QStringLiteral("TICK");
    case MPV_EVENT_CLIENT_MESSAGE: return QStringLiteral("CLIENT_MESSAGE");
    case MPV_EVENT_VIDEO_RECONFIG: return QStringLiteral("VIDEO_RECONFIG");
    case MPV_EVENT_AUDIO_RECONFIG: return QStringLiteral("AUDIO_RECONFIG");
    case MPV_EVENT_SEEK: return QStringLiteral("SEEK");
    case MPV_EVENT_PLAYBACK_RESTART: return QStringLiteral("PLAYBACK_RESTART");
    case MPV_EVENT_PROPERTY_CHANGE: return QStringLiteral("PROPERTY_CHANGE");
    case MPV_EVENT_QUEUE_OVERFLOW: return QStringLiteral("QUEUE_OVERFLOW");
    case MPV_EVENT_LOG_MESSAGE: return QStringLiteral("LOG_MESSAGE");
    case MPV_EVENT_GET_PROPERTY_REPLY: return QStringLiteral("GET_PROPERTY_REPLY");
    case MPV_EVENT_SET_PROPERTY_REPLY: return QStringLiteral("SET_PROPERTY_REPLY");
    case MPV_EVENT_COMMAND_REPLY: return QStringLiteral("COMMAND_REPLY");
    default: return QStringLiteral("EVENT_%1").arg(eventId);
    }
}

bool MainWindow::initializeMpv() {
    if (m_runtimeLogger) m_runtimeLogger->initialize();
    m_runtimeLogger->append(QStringLiteral("initializeMpv: creating libmpv instance"));
    std::setlocale(LC_NUMERIC, "C");
    m_mpv = mpv_create();
    if (!m_mpv) { showError(QStringLiteral("Could not create libmpv instance.")); return false; }

    // libmpv's wakeup callback integrates its event queue with Qt's event loop.
    // Unlike the old fixed 10 ms timer, the GUI thread sleeps normally and is
    // woken only when libmpv has client events to deliver.
    mpv_set_wakeup_callback(m_mpv, &MainWindow::mpvWakeupCallback, this);

    const QByteArray wid = QByteArray::number(static_cast<qulonglong>(m_videoWidget->winId()));
    if (mpv_set_option_string(m_mpv, "wid", wid.constData()) < 0 ||
        mpv_set_option_string(m_mpv, "terminal", "no") < 0 ||
        mpv_set_option_string(m_mpv, "osc", "no") < 0 ||
        mpv_set_option_string(m_mpv, "idle", "yes") < 0 ||
        mpv_set_option_string(m_mpv, "keep-open", "no") < 0 ||
        mpv_set_option_string(m_mpv, "hwdec", "auto") < 0 ||
        mpv_set_option_string(m_mpv, "input-vo-keyboard", "no") < 0 ||
        mpv_set_option_string(m_mpv, "input-cursor-passthrough", "yes") < 0 ||
        mpv_set_option_string(m_mpv, "stop-screensaver", "yes") < 0 ||
        // Keep the video edge-to-edge in the available viewport while
        // preserving its native/container aspect ratio. Panscan=1 would
        // deliberately crop the image to fill the viewport.
        mpv_set_option_string(m_mpv, "panscan", "0.0") < 0 ||
        mpv_set_option_string(m_mpv, "keepaspect", "yes") < 0 ||
        mpv_set_option_string(m_mpv, "keepaspect-window", "yes") < 0) {
        showError(QStringLiteral("Could not configure libmpv.")); return false;
    }
    if (mpv_initialize(m_mpv) < 0) {
        m_runtimeLogger->append(QStringLiteral("initializeMpv: mpv_initialize FAILED"));
        showError(QStringLiteral("Could not initialize libmpv.")); return false;
    }
    m_runtimeLogger->append(QStringLiteral("initializeMpv: mpv_initialize succeeded"));
    const int logResult = mpv_request_log_messages(m_mpv, "info");
    m_runtimeLogger->append(QStringLiteral("initializeMpv: requested mpv info logs, result=%1").arg(logResult));
    setPropertyDouble("saturation", m_saturation);
    setPropertyDouble("brightness", m_brightness);
    setPropertyDouble("contrast", m_contrast);
    setPropertyDouble("volume", 60.0);
    return true;
}

void MainWindow::loadFile(const QString& path) {
    if (!m_mpv || path.isEmpty()) return;
    clearAbLoop();
    const QString absolute = QFileInfo(path).absoluteFilePath();
    addToPlaylist(absolute);
    const int index = m_playlistController ? m_playlistController->indexOf(absolute) : -1;
    if (index >= 0) playPlaylistIndex(index);
    else {
        const QByteArray encoded = absolute.toUtf8();
        const char* args[] = {"loadfile", encoded.constData(), "replace", nullptr};
        mpv_command_async(m_mpv, 0, args);
    }
}

void MainWindow::addToPlaylist(const QString& path) {
    if (!m_playlistController || !m_playlist || path.isEmpty()) return;
    const QString absolute = QFileInfo(path).absoluteFilePath();
    const int existingIndex = m_playlistController->indexOf(absolute);
    if (existingIndex >= 0) {
        m_playlistController->setCurrentIndex(existingIndex);
        m_playlist->setCurrentRow(existingIndex);
        return;
    }
    const bool added = m_playlistController->addPath(absolute);
    if (!added) return;
    auto* item = new QListWidgetItem(m_playlist);
    item->setToolTip(absolute);
    item->setData(Qt::UserRole, absolute);

    auto* rowWidget = new QWidget(m_playlist);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(6, 2, 4, 2);
    rowLayout->setSpacing(4);

    auto* fileLabel = new QLabel(QFileInfo(absolute).fileName(), rowWidget);
    fileLabel->setToolTip(absolute);
    fileLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    fileLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    fileLabel->setProperty("playlist-path", absolute);
    fileLabel->installEventFilter(this);
    rowLayout->addWidget(fileLabel, 1);

    auto* removeButton = new QPushButton(QStringLiteral("x"), rowWidget);
    removeButton->setFixedSize(24, 24);
    removeButton->setToolTip(QStringLiteral("Remove this file from the playlist"));
    removeButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2b2b2b;color:#ddd;border:0;border-radius:4px;font-weight:700;}"
        "QPushButton:hover{background:#8b2f2f;color:#fff;}"));
    connect(removeButton, &QPushButton::clicked, this, [this, item] {
        if (m_playlist) removePlaylistItem(m_playlist->row(item));
    });
    rowLayout->addWidget(removeButton);

    m_playlist->setItemWidget(item, rowWidget);
    item->setSizeHint(rowWidget->sizeHint());

    if (m_playlistController->count() == 1) {
        m_playlistController->setCurrentIndex(0);
        m_playlist->setCurrentRow(0);
    }
    updatePlaylistCurrentRowStyle();
}

void MainWindow::playPlaylistIndex(int index, bool promptResume) {
    if (!m_playlistController || !m_playlist || index < 0 || index >= m_playlistController->count()) return;
    saveCurrentPlaybackPosition();

    // A newly selected file starts with normal hardware decoding and
    // no manual rotation or flip transforms.
    if (m_videoTransformer) m_videoTransformer->reset();

    const QString path = m_playlistController->pathAt(index);
    if (path.isEmpty() || !QFileInfo::exists(path)) return;
    m_playlistController->setCurrentIndex(index);
    m_playlist->setCurrentRow(index);
    updatePlaylistCurrentRowStyle();
    const QByteArray encoded = path.toUtf8();
    const char* args[] = {"loadfile", encoded.constData(), "replace", nullptr};
    if (m_mpv) mpv_command_async(m_mpv, 0, args);
    m_promptResumeNextLoad = promptResume;
    m_pendingResumePath = path;
    setWindowTitle(QStringLiteral("%1 - Rex Player").arg(QFileInfo(path).fileName()));
}

void MainWindow::updatePlaylistCurrentRowStyle() {
    if (!m_playlist) return;

    for (int i = 0; i < m_playlist->count(); ++i) {
        auto* rowWidget = m_playlist->itemWidget(m_playlist->item(i));
        if (!rowWidget) continue;

        const bool current = i == m_playlist->currentRow();
        rowWidget->setStyleSheet(current
            ? QStringLiteral(
                  "QWidget{background:#18352b;color:#e8f5ee;border-radius:4px;}"
                  "QLabel{background:transparent;color:#e8f5ee;}"
                  "QPushButton{background:#24463a;color:#dff3e8;border:0;border-radius:4px;font-weight:700;}"
                  "QPushButton:hover{background:#35634f;color:#fff;}")
            : QStringLiteral(
                  "QWidget{background:transparent;color:#eee;}"
                  "QLabel{background:transparent;color:#eee;}"
                  "QPushButton{background:#2b2b2b;color:#ddd;border:0;border-radius:4px;font-weight:700;}"
                  "QPushButton:hover{background:#8b2f2f;color:#fff;}"));
    }
}

void MainWindow::syncPlaylistSelection() {
    if (m_playlistController && m_playlist &&
        m_playlistController->currentIndex() >= 0 &&
        m_playlistController->currentIndex() < m_playlist->count()) {
        m_playlist->setCurrentRow(m_playlistController->currentIndex());
    }
    updatePlaylistCurrentRowStyle();
}

void MainWindow::command(const char** args) {
    if (!m_mpv || !args) return;
    QStringList parts;
    for (int i = 0; args[i] != nullptr; ++i) parts << QString::fromUtf8(args[i]);
    m_runtimeLogger->append(QStringLiteral("COMMAND async: %1").arg(parts.join(QStringLiteral(" | "))));
    const int result = mpv_command_async(m_mpv, 0, args);
    if (result < 0) {
        m_runtimeLogger->append(QStringLiteral("COMMAND submit FAILED: %1").arg(result));
    }
}
void MainWindow::togglePause() { const char* args[] = {"cycle", "pause", nullptr}; command(args); }
void MainWindow::seekBackward() { const QByteArray seconds = QByteArray::number(-m_seekDurationSeconds); const char* args[] = {"seek", seconds.constData(), "relative", "exact", nullptr}; command(args); }
void MainWindow::seekForward() { const QByteArray seconds = QByteArray::number(m_seekDurationSeconds); const char* args[] = {"seek", seconds.constData(), "relative", "exact", nullptr}; command(args); }
void MainWindow::seekTo(int value) { const double duration = getPropertyDouble("duration"); if (duration > 0) setPropertyDouble("time-pos", duration * value / 1000.0); }
void MainWindow::setVolume(int value) { setPropertyDouble("volume", value); }
void MainWindow::volumeUp() { m_volumeSlider->setValue(std::clamp(m_volumeSlider->value() + 5, 0, 100)); }
void MainWindow::volumeDown() { m_volumeSlider->setValue(std::clamp(m_volumeSlider->value() - 5, 0, 100)); }
void MainWindow::toggleMute() { const char* args[] = {"cycle", "mute", nullptr}; command(args); }
double MainWindow::getPropertyDouble(const char* name) const { if (!m_mpv) return 0.0; double value = 0.0; return mpv_get_property(m_mpv, name, MPV_FORMAT_DOUBLE, &value) >= 0 ? value : 0.0; }
QString MainWindow::getPropertyString(const char* name) const { if (!m_mpv) return {}; char* value = nullptr; if (mpv_get_property(m_mpv, name, MPV_FORMAT_STRING, &value) < 0 || !value) return {}; const QString result = QString::fromUtf8(value); mpv_free(value); return result; }
void MainWindow::setPropertyDouble(const char* name, double value) {
    if (!m_mpv) return;
    m_runtimeLogger->append(QStringLiteral("SET_PROPERTY async: %1=%2").arg(QString::fromUtf8(name)).arg(QString::number(value, 'g', 12)));
    const int result = mpv_set_property_async(m_mpv, 0, name, MPV_FORMAT_DOUBLE, &value);
    if (result < 0) m_runtimeLogger->append(QStringLiteral("SET_PROPERTY submit FAILED: %1 result=%2").arg(QString::fromUtf8(name)).arg(result));
}
void MainWindow::updateSeekButtonLabels() { if (!m_seekBackButton || !m_seekForwardButton) return; m_seekBackButton->setText(QStringLiteral("−10s")); m_seekForwardButton->setText(QStringLiteral("+10s")); }
void MainWindow::adjustVideoZoom(double amount) { setPropertyDouble("video-zoom", std::clamp(getPropertyDouble("video-zoom") + amount, -2.0, 3.0)); }
void MainWindow::resizeWindowForVideoAspect() {
    if (!m_mpv || isFullScreen() || isMaximized() || isMinimized() || !m_videoWidget) return;

    int64_t displayWidth = 0;
    int64_t displayHeight = 0;
    const bool haveDisplayWidth =
        mpv_get_property(m_mpv, "video-out-params/dw", MPV_FORMAT_INT64, &displayWidth) >= 0;
    const bool haveDisplayHeight =
        mpv_get_property(m_mpv, "video-out-params/dh", MPV_FORMAT_INT64, &displayHeight) >= 0;

    if (!haveDisplayWidth || !haveDisplayHeight || displayWidth <= 0 || displayHeight <= 0) {
        mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &displayWidth);
        mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &displayHeight);
    }

    if (displayWidth <= 0 || displayHeight <= 0) return;

    const double aspect = static_cast<double>(displayWidth) /
                          static_cast<double>(displayHeight);
    if (!std::isfinite(aspect) || aspect <= 0.0) return;

    QScreen* currentScreen = screen();
    if (!currentScreen) currentScreen = QGuiApplication::primaryScreen();
    if (!currentScreen) return;

    const QRect available = currentScreen->availableGeometry();
    const int screenMargin = 32;
    const int controlsHeight = m_controls ? m_controls->sizeHint().height() : 0;
    const int maxVideoWidth = std::max(320, available.width() - screenMargin * 2);
    const int maxVideoHeight = std::max(180, available.height() - controlsHeight - screenMargin * 2);

    const int preferredWidth = std::clamp(m_videoWidget->width(), 640, 1400);
    int videoWidth = std::min(preferredWidth, maxVideoWidth);
    int videoHeight = static_cast<int>(std::lround(videoWidth / aspect));

    if (videoHeight > maxVideoHeight) {
        videoHeight = maxVideoHeight;
        videoWidth = static_cast<int>(std::lround(videoHeight * aspect));
    }

    videoWidth = std::clamp(videoWidth, 320, maxVideoWidth);
    videoHeight = std::clamp(
        static_cast<int>(std::lround(videoWidth / aspect)),
        180,
        maxVideoHeight);

    videoWidth = std::min(
        videoWidth,
        static_cast<int>(std::lround(videoHeight * aspect)));
    videoHeight = static_cast<int>(std::lround(videoWidth / aspect));

    const int targetHeight = videoHeight + controlsHeight;
    if (targetHeight <= 0 || videoWidth <= 0) return;

    resize(videoWidth, targetHeight);
}

void MainWindow::captureScreenshot() {
    if (!m_screenshotController || !m_mpv || getPropertyString("filename").isEmpty()) {
        showError(QStringLiteral("REX Player — No video is currently loaded"));
        return;
    }
    m_screenshotController->capture(windowTitle(), getPropertyString("path"));
}

void MainWindow::showDisplayDialog() {
    if (m_displayController) m_displayController->showDialog();
}

void MainWindow::saveLogReport() {
    if (m_diagnosticReporter) m_diagnosticReporter->saveReport();
}

void MainWindow::rotateVideo90() {
    if (!m_videoTransformer) return;
    const int nextRotation = (m_videoTransformer->rotation() + 90) % 360;
    m_videoTransformer->setRotation(nextRotation);
    m_runtimeLogger->append(QStringLiteral("USER ACTION: Rotate clicked; new rotation=%1").arg(nextRotation));
    m_videoTransformer->apply();
    QTimer::singleShot(100, this, &MainWindow::resizeWindowForVideoAspect);
}

void MainWindow::toggleFlipHorizontal() {
    if (!m_videoTransformer) return;
    m_videoTransformer->toggleFlipHorizontal();
    m_runtimeLogger->append(QStringLiteral("USER ACTION: Flip H clicked; new state=%1")
        .arg(m_videoTransformer->flipHorizontal() ? QStringLiteral("on") : QStringLiteral("off")));
    m_videoTransformer->apply();
    QTimer::singleShot(100, this, &MainWindow::resizeWindowForVideoAspect);
}

void MainWindow::toggleFlipVertical() {
    if (!m_videoTransformer) return;
    m_videoTransformer->toggleFlipVertical();
    m_runtimeLogger->append(QStringLiteral("USER ACTION: Flip V clicked; new state=%1")
        .arg(m_videoTransformer->flipVertical() ? QStringLiteral("on") : QStringLiteral("off")));
    m_videoTransformer->apply();
    QTimer::singleShot(100, this, &MainWindow::resizeWindowForVideoAspect);
}

void MainWindow::resetVideoTransform() { setPropertyDouble("video-zoom", 0.0); setPropertyDouble("video-pan-x", 0.0); setPropertyDouble("video-pan-y", 0.0); m_videoPanX = 0.0; m_videoPanY = 0.0; }
void MainWindow::setAbLoopStart() { if (getPropertyDouble("duration") <= 0.0) return; const double position = getPropertyDouble("time-pos"); clearAbLoop(); m_abLoopStart = position; setPropertyDouble("ab-loop-a", position); updateAbLoopLabel(); }
void MainWindow::setAbLoopEnd() { const double position = getPropertyDouble("time-pos"); if (m_abLoopStart < 0.0 || position <= m_abLoopStart) return; m_abLoopEnd = position; setPropertyDouble("ab-loop-a", m_abLoopStart); setPropertyDouble("ab-loop-b", m_abLoopEnd); updateAbLoopLabel(); }
void MainWindow::clearAbLoop() { static char noLoop[] = "no"; char* value = noLoop; if (m_mpv) { mpv_set_property_async(m_mpv, 0, "ab-loop-a", MPV_FORMAT_STRING, &value); mpv_set_property_async(m_mpv, 0, "ab-loop-b", MPV_FORMAT_STRING, &value); } m_abLoopStart = -1.0; m_abLoopEnd = -1.0; updateAbLoopLabel(); }
void MainWindow::updateAbLoopLabel() { if (!m_abLoopLabel) return; if (m_abLoopStart < 0.0) m_abLoopLabel->setText(QStringLiteral("A-B: Off")); else if (m_abLoopEnd < 0.0) m_abLoopLabel->setText(QStringLiteral("A-B: %1 — …").arg(formatTime(m_abLoopStart))); else m_abLoopLabel->setText(QStringLiteral("A-B: %1 — %2").arg(formatTime(m_abLoopStart), formatTime(m_abLoopEnd))); }
void MainWindow::stepFrame(bool forward) { const char* args[] = {forward ? "frame-step" : "frame-back-step", nullptr}; command(args); }
void MainWindow::toggleHardwareDecoding() { if (!m_mpv) return; const char* args[] = {"cycle-values", "hwdec", "auto", "no", nullptr}; command(args); }

void MainWindow::cutAbSelection() {
    if (!m_mpv) return;
    if (m_abLoopStart < 0.0 || m_abLoopEnd <= m_abLoopStart) {
        QMessageBox::information(this, QStringLiteral("Cut A-B"), QStringLiteral("Set both A and B points first."));
        return;
    }
    if (m_cutProcess && m_cutProcess->state() != QProcess::NotRunning) {
        QMessageBox::information(this, QStringLiteral("Cut A-B"), QStringLiteral("An A-B cut is already in progress."));
        return;
    }

    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("FFmpeg not found"),
                             QStringLiteral("FFmpeg is required for A-B cutting. Install the ffmpeg package and try again."));
        return;
    }

    QString inputPath = getPropertyString("path").trimmed();
    const QUrl inputUrl(inputPath);
    if (inputUrl.isLocalFile()) inputPath = inputUrl.toLocalFile();
    const QFileInfo inputInfo(inputPath);
    if (!inputInfo.isFile()) {
        QMessageBox::warning(this, QStringLiteral("Cut A-B"), QStringLiteral("The current media is not a local file."));
        return;
    }

    const double duration = m_abLoopEnd - m_abLoopStart;
    const QString start = QString::number(m_abLoopStart, 'f', 6);
    const QString length = QString::number(duration, 'f', 6);
    const QString suffix = inputInfo.suffix();
    const QString defaultName = inputInfo.dir().filePath(
        inputInfo.completeBaseName() + QStringLiteral("_AB_cut") +
        (suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix));
    const QString filter = suffix.isEmpty()
        ? QStringLiteral("All files (*)")
        : QStringLiteral("%1 (*.%1);;All files (*)").arg(suffix);
    const QString outputPath = QFileDialog::getSaveFileName(this, QStringLiteral("Save A-B cut"), defaultName, filter);
    if (outputPath.isEmpty()) return;

    const QFileInfo outputInfo(outputPath);
    if (outputInfo.absoluteFilePath() == inputInfo.absoluteFilePath()) {
        QMessageBox::warning(this, QStringLiteral("Cut A-B"), QStringLiteral("The output file must be different from the input file."));
        return;
    }
    if (outputInfo.exists()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Overwrite file?"),
            QStringLiteral("%1 already exists. Replace it?").arg(outputInfo.fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }

    m_cutOutputPath = outputPath;
    m_cutProcess = new QProcess(this);
    m_cutProcess->setProcessChannelMode(QProcess::SeparateChannels);
    m_cutAbButton->setEnabled(false);

    connect(m_cutProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const QString error = QString::fromLocal8Bit(m_cutProcess->readAllStandardError()).trimmed();
        const QString output = m_cutOutputPath;
        const bool success = exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(output);
        if (success) {
            showToast(QStringLiteral("A-B Cut Saved"));
            QMessageBox::information(
                this, QStringLiteral("A-B cut complete"),
                QStringLiteral("Saved:\n%1\n\n%2").arg(output, m_cutWithZoom && (getPropertyDouble("video-zoom") > 0.0001 || std::abs(getPropertyDouble("video-pan-x")) > 0.0001 || std::abs(getPropertyDouble("video-pan-y")) > 0.0001 || std::abs(getPropertyDouble("video-rotate")) > 0.0001 || (m_videoTransformer && m_videoTransformer->hasTransforms())) ? QStringLiteral("The current zoom/pan/rotation/flip was baked into the video, so the video was re-encoded; audio/subtitles were copied when supported.") : QStringLiteral("Streams were copied without re-encoding. Because this is stream-copy cutting, the start may align to a nearby keyframe.")));
        } else {
            if (QFileInfo::exists(output)) QFile::remove(output);
            const QString detail = error.isEmpty() ? QStringLiteral("FFmpeg exited with code %1.").arg(exitCode) : error;
            QMessageBox::warning(this, QStringLiteral("A-B cut failed"), detail);
        }
        m_cutAbButton->setEnabled(true);
        m_cutProcess->deleteLater();
        m_cutProcess = nullptr;
        m_cutOutputPath.clear();
    });

    QStringList args = {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-ss"), start,
        QStringLiteral("-i"), inputPath,
        QStringLiteral("-t"), length,
        QStringLiteral("-map"), QStringLiteral("0"),
    };

    const double zoom = getPropertyDouble("video-zoom");
    const double panX = getPropertyDouble("video-pan-x");
    const double panY = getPropertyDouble("video-pan-y");
    int64_t sourceWidth64 = 0;
    int64_t sourceHeight64 = 0;
    int64_t rotation64 = 0;
    const bool haveSourceWidth = mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &sourceWidth64) >= 0;
    const bool haveSourceHeight = mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &sourceHeight64) >= 0;
    mpv_get_property(m_mpv, "video-params/rotate", MPV_FORMAT_INT64, &rotation64);
    const int sourceWidth = haveSourceWidth ? std::max(0, static_cast<int>(sourceWidth64)) : 0;
    const int sourceHeight = haveSourceHeight ? std::max(0, static_cast<int>(sourceHeight64)) : 0;
    const int rotation = static_cast<int>(rotation64);
    const int normalizedRotation = ((rotation % 360) + 360) % 360;
    const double zoomFactor = std::pow(2.0, std::max(0.0, zoom));
    const bool bakeZoom = m_cutWithZoom && sourceWidth > 0 && sourceHeight > 0 && zoom > 0.0001;
    // FFmpeg already applies the file's native rotation metadata during
    // transcoding. Only the user's additional manual rotation must be
    // explicitly filtered here.
    const bool bakeRotation = m_cutWithZoom && sourceWidth > 0 && sourceHeight > 0 &&
                               m_videoTransformer && m_videoTransformer->rotation() != 0;
    const bool bakeFlipHorizontal = m_cutWithZoom && sourceWidth > 0 && sourceHeight > 0 &&
                                     m_videoTransformer && m_videoTransformer->flipHorizontal();
    const bool bakeFlipVertical = m_cutWithZoom && sourceWidth > 0 && sourceHeight > 0 &&
                                   m_videoTransformer && m_videoTransformer->flipVertical();
    const bool bakeTransform = bakeZoom || bakeRotation || bakeFlipHorizontal || bakeFlipVertical;

    if (bakeTransform) {
        // mpv rotates first, then applies zoom/pan to the displayed video.
        // Mirror that order in FFmpeg: rotate into the current display
        // orientation, then crop the visible zoom/pan region.
        const int rotatedWidth = (normalizedRotation == 90 || normalizedRotation == 270)
            ? sourceHeight : sourceWidth;
        const int rotatedHeight = (normalizedRotation == 90 || normalizedRotation == 270)
            ? sourceWidth : sourceHeight;

        QStringList filters;
        if (m_videoTransformer && m_videoTransformer->rotation() == 90) {
            filters << QStringLiteral("transpose=clock");
        } else if (m_videoTransformer && m_videoTransformer->rotation() == 180) {
            filters << QStringLiteral("hflip") << QStringLiteral("vflip");
        } else if (m_videoTransformer && m_videoTransformer->rotation() == 270) {
            filters << QStringLiteral("transpose=cclock");
        }
        if (bakeFlipHorizontal) {
            filters << QStringLiteral("hflip");
        }
        if (bakeFlipVertical) {
            filters << QStringLiteral("vflip");
        }

        if (bakeZoom) {
            const int cropWidth = std::max(2, std::min(rotatedWidth,
                static_cast<int>(std::floor(rotatedWidth / zoomFactor / 2.0) * 2.0)));
            const int cropHeight = std::max(2, std::min(rotatedHeight,
                static_cast<int>(std::floor(rotatedHeight / zoomFactor / 2.0) * 2.0)));
            const int maxX = rotatedWidth - cropWidth;
            const int maxY = rotatedHeight - cropHeight;
            // mpv's positive pan moves the displayed video rectangle right/down,
            // so the visible source window moves left/up. After rotation, pan
            // is still expressed in the rotated/displayed video dimensions.
            const int centerX = static_cast<int>(std::lround(
                (rotatedWidth - cropWidth) / 2.0 - panX * rotatedWidth));
            const int centerY = static_cast<int>(std::lround(
                (rotatedHeight - cropHeight) / 2.0 - panY * rotatedHeight));
            const int cropX = std::clamp(centerX, 0, maxX);
            const int cropY = std::clamp(centerY, 0, maxY);
            filters << QStringLiteral("crop=%1:%2:%3:%4")
                .arg(cropWidth).arg(cropHeight).arg(cropX).arg(cropY);
        }

        args << QStringLiteral("-vf") << filters.join(QLatin1Char(','))
             << QStringLiteral("-c:v") << (suffix.compare(QStringLiteral("webm"), Qt::CaseInsensitive) == 0
                                               ? QStringLiteral("libvpx-vp9")
                                               : QStringLiteral("libx264"));
        if (suffix.compare(QStringLiteral("webm"), Qt::CaseInsensitive) == 0) {
            args << QStringLiteral("-crf") << QStringLiteral("30")
                 << QStringLiteral("-b:v") << QStringLiteral("0");
        } else {
            args << QStringLiteral("-preset") << QStringLiteral("medium")
                 << QStringLiteral("-crf") << QStringLiteral("18");
        }
        args << QStringLiteral("-c:a") << QStringLiteral("copy")
             << QStringLiteral("-c:s") << QStringLiteral("copy")
             << QStringLiteral("-c:d") << QStringLiteral("copy");
    } else {
        // No visual transform needs to be baked. Keep the original fast
        // stream-copy path.
        args << QStringLiteral("-c") << QStringLiteral("copy");
    }

    args << QStringLiteral("-avoid_negative_ts") << QStringLiteral("make_zero")
         << QStringLiteral("-y") << outputPath;

    m_cutProcess->start(ffmpeg, args);
}

QString MainWindow::diagnosticControlState() const {
    QString state;
    QTextStream out(&state);
    out << "A-B start: " << m_abLoopStart << "\n";
    out << "A-B end: " << m_abLoopEnd << "\n";
    out << "Interface font family: " << QApplication::font().family() << "\n";
    out << "Interface font size: " << QApplication::font().pointSizeF() << "\n";
    out << "Custom font file: " << (m_controlSettings ? m_controlSettings->fontPath : QString()) << "\n";
    out << "Seek duration (seconds): " << m_seekDurationSeconds << "\n";
    out << "Seek wheel: " << m_seekWheelMode << "\n";
    out << "Zoom wheel: " << m_zoomWheelMode << "\n";
    out << "Volume wheel: " << m_volumeWheelMode << "\n";
    out << "Pan gesture: Alt + Ctrl + configured pan-button drag\n";
    out << "Double-click button: " << static_cast<int>(m_doubleClickButton) << "\n";
    out << "Double-click zones: " << (m_doubleClickZones ? "enabled" : "disabled") << "\n";
    out << "Volume up shortcut: " << m_volumeUpKey.toString() << "\n";
    out << "Volume down shortcut: " << m_volumeDownKey.toString() << "\n";
    out << "Mute shortcut: " << m_muteKey.toString() << "\n";
    out << "Seek backward shortcut: " << m_seekBackwardKey.toString() << "\n";
    out << "Seek forward shortcut: " << m_seekForwardKey.toString() << "\n";
    out << "Loop A shortcut: " << m_loopAKey.toString() << "\n";
    out << "Loop B shortcut: " << m_loopBKey.toString() << "\n";
    out << "Loop clear shortcut: " << m_loopClearKey.toString() << "\n";
    out << "Zoom in shortcut: " << m_zoomInKey.toString() << "\n";
    out << "Zoom out shortcut: " << m_zoomOutKey.toString() << "\n";
    out << "Zoom reset shortcut: " << m_zoomResetKey.toString() << "\n";
    out << "Frame back shortcut: " << m_frameBackKey.toString() << "\n";
    out << "Frame forward shortcut: " << m_frameForwardKey.toString() << "\n";
    out << "Switch subtitles shortcut: " << m_switchSubtitlesKey.toString() << "\n";
    out << "Autoplay next item: "
        << ((m_playlistController && m_playlistController->autoplay()) ? "enabled" : "disabled") << "\n";
    out << "Loop playlist: "
        << ((m_playlistController && m_playlistController->loop()) ? "enabled" : "disabled") << "\n";

    out << "\nPlaylist\n--------\n";
    if (m_playlist) {
        out << "Count: " << m_playlist->count() << "\n";
        out << "Current index: "
            << (m_playlistController ? m_playlistController->currentIndex() : -1) << "\n";
        for (int i = 0; i < m_playlist->count(); ++i) {
            const auto* item = m_playlist->item(i);
            if (item) out << (i + 1) << ": " << item->data(Qt::UserRole).toString() << "\n";
        }
    }
    return state;
}



void MainWindow::updatePlaybackInhibit(bool active) {
    if (m_playbackInhibitor) m_playbackInhibitor->setActive(active);
}

void MainWindow::updateHardwareButton() {
    if (!m_hwButton || !m_mpv) return;
    const QString current = getPropertyString("hwdec-current").trimmed().toLower();
    const bool hardwareActive = !current.isEmpty() && current != QStringLiteral("no");
    m_hwButton->setText(hardwareActive ? QStringLiteral("HW") : QStringLiteral("SW"));
    m_hwButton->setToolTip(hardwareActive
        ? QStringLiteral("Hardware decoding active (%1). Click to switch to software decoding.").arg(current)
        : QStringLiteral("Software decoding active. Click to enable hardware decoding when supported."));
}

void MainWindow::showTracksMenu() {
    if (m_trackController) m_trackController->showMenu(qobject_cast<QWidget*>(sender()));
}

void MainWindow::increasePlaybackSpeed() {
    const double current = getPropertyDouble("speed");
    const double base = std::isfinite(current) ? current : 1.0;
    const double next = std::clamp(std::round((base + m_speedJump) * 20.0) / 20.0, 0.25, 3.0);
    setPropertyDouble("speed", next);
    if (m_runtimeLogger) {
        m_runtimeLogger->append(
            QStringLiteral("PLAYBACK SPEED: %1x").arg(next, 0, 'f', 2));
    }
}

void MainWindow::decreasePlaybackSpeed() {
    const double current = getPropertyDouble("speed");
    const double base = std::isfinite(current) ? current : 1.0;
    const double next = std::clamp(std::round((base - m_speedJump) * 20.0) / 20.0, 0.25, 3.0);
    setPropertyDouble("speed", next);
    if (m_runtimeLogger) {
        m_runtimeLogger->append(
            QStringLiteral("PLAYBACK SPEED: %1x").arg(next, 0, 'f', 2));
    }
}

void MainWindow::showSpeedMenu() {
    if (!m_speedButton) return;

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu{max-height:320px;}"
        "QMenu::item{padding:5px 18px 5px 10px;}"
        "QMenu::item:checked{font-weight:600;}"
    ));
    const double currentSpeed = std::clamp(getPropertyDouble("speed"), 0.25, 3.0);

    for (int i = 5; i <= 60; ++i) {
        const double speed = i * 0.05;
        const QString label = QStringLiteral("%1x").arg(speed, 0, 'f', 2);
        auto* action = menu.addAction(label);
        action->setCheckable(true);
        action->setChecked(std::abs(currentSpeed - speed) < 0.001);
        connect(action, &QAction::triggered, this, [this, speed] {
            setPropertyDouble("speed", speed);
            if (m_speedButton) m_speedButton->setText(QStringLiteral("Speed"));
            if (m_runtimeLogger) {
                m_runtimeLogger->append(
                    QStringLiteral("PLAYBACK SPEED: %1x").arg(speed, 0, 'f', 2));
            }
        });
    }

    menu.exec(m_speedButton->mapToGlobal(QPoint(0, m_speedButton->height())));
}

void MainWindow::saveSelectedAudioTrack() {
    if (!m_mpv || !m_audioExporter) return;

    if (m_audioExporter->isRunning()) {
        QMessageBox::information(this, QStringLiteral("Save Audio"),
                                 QStringLiteral("An audio export is already in progress."));
        return;
    }

    mpv_node tracks{};
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &tracks) < 0 ||
        tracks.format != MPV_FORMAT_NODE_ARRAY || !tracks.u.list) {
        mpv_free_node_contents(&tracks);
        m_runtimeLogger->append(QStringLiteral("AUDIO SAVE: could not read track-list"));
        QMessageBox::warning(this, QStringLiteral("Save Audio"),
                             QStringLiteral("Could not read the current audio track."));
        return;
    }

    int selectedId = -1;
    int ffIndex = -1;
    int selectedAudioIndex = -1;
    int audioIndex = 0;
    QString trackTitle;
    QString codec;
    QString externalFilename;

    for (int i = 0; i < tracks.u.list->num; ++i) {
        const mpv_node& track = tracks.u.list->values[i];
        if (track.format != MPV_FORMAT_NODE_MAP || !track.u.list) continue;
        if (MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "type")) != QStringLiteral("audio")) {
            continue;
        }

        const bool isSelected = MpvNodeUtils::nodeFlag(
            MpvNodeUtils::mapValue(track.u.list, "selected"));
        if (isSelected) selectedAudioIndex = audioIndex;

        if (isSelected) {
            selectedId = MpvNodeUtils::nodeInt(MpvNodeUtils::mapValue(track.u.list, "id"));
            ffIndex = MpvNodeUtils::nodeInt(MpvNodeUtils::mapValue(track.u.list, "ff-index"));
            trackTitle = MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "title"));
            codec = MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "codec"))
                        .trimmed().toLower();
            externalFilename =
                MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "external-filename"));
        }

        ++audioIndex;
        if (isSelected) break;
    }
    mpv_free_node_contents(&tracks);

    if (selectedId < 0) {
        m_runtimeLogger->append(QStringLiteral("AUDIO SAVE: no selected audio track"));
        QMessageBox::information(this, QStringLiteral("Save Audio"),
                                 QStringLiteral("No audio track is currently selected."));
        return;
    }

    QString inputPath = externalFilename.trimmed();
    if (inputPath.isEmpty()) inputPath = getPropertyString("path").trimmed();
    const QUrl inputUrl(inputPath);
    if (inputUrl.isLocalFile()) inputPath = inputUrl.toLocalFile();

    if (inputPath.isEmpty()) {
        m_runtimeLogger->append(QStringLiteral("AUDIO SAVE: selected track %1 has no source path").arg(selectedId));
        QMessageBox::warning(this, QStringLiteral("Save Audio"),
                             QStringLiteral("The selected audio track has no accessible source."));
        return;
    }

    const QFileInfo inputInfo(inputPath);
    const QString sourcePath = inputInfo.isFile() ? inputInfo.absoluteFilePath() : inputPath;

    QString extension = QStringLiteral("mka");
    if (codec == QStringLiteral("mp3")) extension = QStringLiteral("mp3");
    else if (codec == QStringLiteral("aac") || codec == QStringLiteral("alac")) extension = QStringLiteral("m4a");
    else if (codec == QStringLiteral("flac")) extension = QStringLiteral("flac");
    else if (codec == QStringLiteral("opus")) extension = QStringLiteral("opus");
    else if (codec == QStringLiteral("vorbis")) extension = QStringLiteral("ogg");
    else if (codec == QStringLiteral("pcm_s16le") || codec == QStringLiteral("pcm_s24le") ||
             codec == QStringLiteral("pcm_s32le") || codec == QStringLiteral("pcm_f32le")) {
        extension = QStringLiteral("wav");
    } else if (codec == QStringLiteral("ac3")) extension = QStringLiteral("ac3");
    else if (codec == QStringLiteral("eac3")) extension = QStringLiteral("eac3");
    else if (codec == QStringLiteral("dts")) extension = QStringLiteral("dts");

    QString baseName = QFileInfo(getPropertyString("filename")).completeBaseName();
    if (baseName.isEmpty()) baseName = QStringLiteral("audio");
    if (!trackTitle.isEmpty()) {
        QString safeTitle = trackTitle;
        safeTitle.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")),
                          QStringLiteral("_"));
        safeTitle = safeTitle.trimmed();
        if (!safeTitle.isEmpty()) baseName += QStringLiteral("_") + safeTitle;
    }

    QString musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (musicDir.isEmpty()) musicDir = QDir::homePath();
    const QString defaultName =
        QDir(musicDir).filePath(baseName + QStringLiteral(".") + extension);
    const QString filter =
        QStringLiteral("Audio files (*.%1);;Matroska audio (*.mka);;All files (*)").arg(extension);

    const QString outputPath = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save selected audio track"), defaultName, filter);
    if (outputPath.isEmpty()) {
        m_runtimeLogger->append(QStringLiteral("AUDIO SAVE: user cancelled save dialog"));
        return;
    }

    const QFileInfo outputInfo(outputPath);
    if (inputInfo.isFile() && outputInfo.absoluteFilePath() == inputInfo.absoluteFilePath()) {
        QMessageBox::warning(this, QStringLiteral("Save Audio"),
                             QStringLiteral("The output file must be different from the source media."));
        return;
    }

    if (outputInfo.exists()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Overwrite file?"),
            QStringLiteral("%1 already exists. Replace it?").arg(outputInfo.fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }

    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        m_runtimeLogger->append(QStringLiteral("AUDIO SAVE: ffmpeg not found"));
        QMessageBox::warning(
            this, QStringLiteral("Save Audio"),
            QStringLiteral("FFmpeg is required to save the selected audio track. "
                           "Install ffmpeg and try again."));
        return;
    }

    const QString mapSpecifier = ffIndex >= 0
        ? QStringLiteral("0:%1").arg(ffIndex)
        : QStringLiteral("0:a:%1").arg(std::max(0, selectedAudioIndex));

    m_runtimeLogger->append(QStringLiteral(
        "AUDIO SAVE: selected id=%1 audio-index=%2 ff-index=%3 codec=%4 external=%5")
        .arg(selectedId)
        .arg(selectedAudioIndex)
        .arg(ffIndex)
        .arg(codec.isEmpty() ? QStringLiteral("<unknown>") : codec)
        .arg(externalFilename.isEmpty() ? QStringLiteral("no") : externalFilename));
    m_runtimeLogger->append(QStringLiteral("AUDIO SAVE: input=%1 output=%2 map=%3")
                         .arg(sourcePath, outputPath, mapSpecifier));

    m_saveAudioButton->setEnabled(false);

    QString startError;
    if (!m_audioExporter->start(ffmpeg, sourcePath, mapSpecifier, outputPath, &startError)) {
        m_saveAudioButton->setEnabled(true);
        QMessageBox::warning(
            this, QStringLiteral("Save Audio"),
            QStringLiteral("Could not start FFmpeg:\n%1").arg(startError));
    }
}

void MainWindow::saveCurrentPlaybackPosition() {
    if (!m_mpv || m_pendingResumePath.isEmpty()) return;
    const double pos = getPropertyDouble("time-pos");
    const double duration = getPropertyDouble("duration");
    if (!std::isfinite(pos) || pos <= 0.5 || duration <= 0.0) return;
    if (m_playbackPositions) {
        m_playbackPositions->save(m_pendingResumePath, pos);
    }
}

void MainWindow::pumpMpvEvents() {
    // This slot runs on the Qt GUI thread through the queued wakeup signal.
    m_mpvWakeQueued.store(false);
    if (!m_mpv) return;
    while (true) {
        mpv_event* event = mpv_wait_event(m_mpv, 0);
        if (!event || event->event_id == MPV_EVENT_NONE) break;

        m_runtimeLogger->append(QStringLiteral("MPV_EVENT: %1 (%2)").arg(mpvEventName(event->event_id)).arg(event->event_id));
        if (event->event_id == MPV_EVENT_LOG_MESSAGE && event->data) {
            const auto* log = static_cast<mpv_event_log_message*>(event->data);
            m_runtimeLogger->append(QStringLiteral("MPV_LOG [%1] %2: %3")
                .arg(QString::fromUtf8(log->level ? log->level : ""))
                .arg(QString::fromUtf8(log->prefix ? log->prefix : ""))
                .arg(QString::fromUtf8(log->text ? log->text : "").trimmed()));
        }
        if (event->event_id == MPV_EVENT_FILE_LOADED) {
            // Update the native window title from the file mpv actually loaded.
            // This is deliberately done on FILE_LOADED rather than only when
            // issuing loadfile, so the title stays correct for every loading
            // path (startup argument, Open, drag/drop, playlist and next/previous).
            const QString loadedFilename = getPropertyString("filename").trimmed();
            const QString loadedPath = getPropertyString("path").trimmed();
            const QString titleName = !loadedFilename.isEmpty()
                ? QFileInfo(loadedFilename).fileName()
                : QFileInfo(loadedPath).fileName();
            if (!titleName.isEmpty()) {
                setWindowTitle(QStringLiteral("%1 - Rex Player").arg(titleName));
                m_runtimeLogger->append(
                    QStringLiteral("WINDOW TITLE: %1 - Rex Player").arg(titleName));
            }

            // Transform state is reset by playPlaylistIndex() when a genuinely
            // new file is selected. Do not reset it here: video-reload is also
            // used to rebuild the decoder for transform mode, and that reload
            // Preserve the requested rotation and flip state.
            if (m_videoTransformer) m_videoTransformer->apply();
            // The video viewport is embedded in the Qt window, so mpv cannot
            // resize the parent window itself. Resize the normal window here
            // using mpv's actual display dimensions. This makes the windowed
            // video area match the video aspect ratio exactly, allowing the
            // edge-to-edge viewport to fill without cropping or distortion.
            if (!isFullScreen() && !isMaximized()) {
                QTimer::singleShot(0, this, &MainWindow::resizeWindowForVideoAspect);
            }
            if (m_promptResumeNextLoad && !m_pendingResumePath.isEmpty()) {
                const double saved =
                    m_playbackPositions ? m_playbackPositions->load(m_pendingResumePath) : 0.0;
                const double duration = getPropertyDouble("duration");
                m_promptResumeNextLoad = false;
                if (saved >= 5.0 && duration > 0.0 && saved < duration - 5.0) {
                    QSettings resumeSettings(
                        QStringLiteral("REX Player"), QStringLiteral("REX Player"));
                    constexpr int kNoRememberedResumeChoice = -1;
                    constexpr int kStartFromBeginning = 0;
                    constexpr int kResumeFromLastPosition = 1;
                    const int rememberedChoice = resumeSettings.value(
                        QStringLiteral("playback/resume-choice"),
                        kNoRememberedResumeChoice).toInt();

                    if (rememberedChoice == kResumeFromLastPosition) {
                        m_runtimeLogger->append(
                            QStringLiteral("RESUME: applying remembered choice = resume"));
                        setPropertyDouble("time-pos", saved);
                    } else if (rememberedChoice == kStartFromBeginning) {
                        m_runtimeLogger->append(
                            QStringLiteral("RESUME: applying remembered choice = start"));
                        setPropertyDouble("time-pos", 0.0);
                    } else {
                        QMessageBox box(
                            QMessageBox::Question,
                            QStringLiteral("Resume playback?"),
                            QStringLiteral(
                                "This video was previously played at %1.\n\n"
                                "Choose whether to start over or continue from the last played position.")
                                .arg(formatTime(saved)),
                            QMessageBox::NoButton,
                            this);

                        auto* rememberChoice = new QCheckBox(
                            QStringLiteral("Remember my choice"), &box);
                        rememberChoice->setToolTip(
                            QStringLiteral(
                                "Use this choice automatically for future resume prompts. "
                                "The setting can be reset from Controls."));
                        box.setCheckBox(rememberChoice);

                        auto* startOver = box.addButton(
                            QStringLiteral("Start from beginning"), QMessageBox::NoRole);
                        auto* resume = box.addButton(
                            QStringLiteral("Resume from last position"), QMessageBox::YesRole);
                        box.setDefaultButton(static_cast<QPushButton*>(resume));
                        box.exec();

                        const bool shouldResume = box.clickedButton() == resume;
                        const bool shouldStartOver = box.clickedButton() == startOver;

                        if (rememberChoice->isChecked() && (shouldResume || shouldStartOver)) {
                            resumeSettings.setValue(
                                QStringLiteral("playback/resume-choice"),
                                shouldResume ? kResumeFromLastPosition : kStartFromBeginning);
                            resumeSettings.sync();
                            m_runtimeLogger->append(
                                QStringLiteral("RESUME: remembered choice = %1")
                                    .arg(shouldResume ? QStringLiteral("resume")
                                                       : QStringLiteral("start")));
                        }

                        if (shouldResume) {
                            setPropertyDouble("time-pos", saved);
                        } else if (shouldStartOver) {
                            setPropertyDouble("time-pos", 0.0);
                        }
                    }
                }
            }
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            auto* end = static_cast<mpv_event_end_file*>(event->data);
            if (end && end->reason == MPV_END_FILE_REASON_EOF) {
                // Release the display inhibitor immediately at EOF.
                updatePlaybackInhibit(false);

                const bool hasNext = m_playlist &&
                                     m_playlistController &&
                                     m_playlistController->nextIndex() >= 0;
                const bool hasPlaylist = m_playlistController && m_playlistController->count() > 0;
                if (m_playlistController && m_playlistController->autoplay() && hasNext) {
                    playNext();
                } else if (m_playlistController && m_playlistController->autoplay() && m_playlistController->loop() && hasPlaylist) {
                    playPlaylistIndex(0, false);
                } else if (isFullScreen()) {
                    // Return to the normal window when playback really ends.
                    toggleFullscreen();
                }
            }
        } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
            close();
            break;
        }
    }
}



void MainWindow::updatePlaybackUi() {
    if (!m_mpv) return;
    const double pos = getPropertyDouble("time-pos");
    const double duration = getPropertyDouble("duration");
    int paused = 0;
    if (mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &paused) < 0) paused = 0;
    if (!m_seeking) m_seekSlider->setValue(duration > 0 ? static_cast<int>(std::clamp(pos / duration, 0.0, 1.0) * 1000.0) : 0);
    const double remaining = std::max(0.0, duration - pos);
    const QString rightTime = m_showRemainingTime ? QStringLiteral("- %1").arg(formatTime(remaining)) : formatTime(duration);
    const QString currentText = formatTime(pos);
    m_timeLabel->setText(QStringLiteral("%1 / %2").arg(currentText, rightTime));
    m_currentTimeLabel->setText(currentText);
    m_progressTimeLabel->setText(rightTime);
    m_timeLabel->setVisible(!m_timerBesideProgress);
    m_currentTimeLabel->setVisible(m_timerBesideProgress);
    m_progressTimeLabel->setVisible(m_timerBesideProgress);
    updatePlayButton(paused != 0);
    updateHardwareButton();
    updatePlaybackInhibit(paused == 0 && !getPropertyString("path").isEmpty());
    if (m_pendingResumePath == getPropertyString("path") && (QDateTime::currentMSecsSinceEpoch() - m_lastPositionSaveMs) >= 1000) {
        saveCurrentPlaybackPosition();
        m_lastPositionSaveMs = QDateTime::currentMSecsSinceEpoch();
    }
    syncPlaylistSelection();
}

void MainWindow::updatePlayButton(bool paused) { m_playButton->setText(paused ? QStringLiteral("▶") : QStringLiteral("Ⅱ")); }
void MainWindow::increaseSubtitlePosition() {
    const double current = std::clamp(getPropertyDouble("sub-pos"), 0.0, 150.0);
    setPropertyDouble("sub-pos", std::max(0.0, current - 1.0));
}

void MainWindow::decreaseSubtitlePosition() {
    const double current = std::clamp(getPropertyDouble("sub-pos"), 0.0, 150.0);
    setPropertyDouble("sub-pos", std::min(150.0, current + 1.0));
}

void MainWindow::cycleSubtitles() {
    if (m_trackController) m_trackController->cycleSubtitles();
}

void MainWindow::increaseSubtitleSize() { const double current = getPropertyDouble("sub-scale"); setPropertyDouble("sub-scale", std::clamp((current > 0.0 ? current : 1.0) + 0.1, 0.1, 100.0)); }
void MainWindow::decreaseSubtitleSize() { const double current = getPropertyDouble("sub-scale"); setPropertyDouble("sub-scale", std::clamp((current > 0.0 ? current : 1.0) - 0.1, 0.1, 100.0)); }
QString MainWindow::formatTime(double seconds) const { if (!std::isfinite(seconds) || seconds < 0) seconds = 0; const int total = static_cast<int>(seconds); const int h = total / 3600, m = (total % 3600) / 60, s = total % 60; return h > 0 ? QStringLiteral("%1:%2:%3").arg(h).arg(m,2,10,QLatin1Char('0')).arg(s,2,10,QLatin1Char('0')) : QStringLiteral("%1:%2").arg(m).arg(s,2,10,QLatin1Char('0')); }
void MainWindow::openFile() { const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open video")); if (!path.isEmpty()) loadFile(path); }
void MainWindow::addFiles() {
    const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Add media files"));
    if (paths.isEmpty() || !m_playlistController || !m_playlist) return;
    const int currentIndex = m_playlistController->currentIndex();
    const bool wasEmpty = m_playlistController->count() == 0;
    for (const QString& path : paths) addToPlaylist(path);
    if (wasEmpty && m_playlistController->count() > 0) {
        m_playlistController->setCurrentIndex(0);
        m_playlist->setCurrentRow(0);
        playlistActivated();
    } else if (currentIndex >= 0 && currentIndex < m_playlist->count()) {
        m_playlistController->setCurrentIndex(currentIndex);
        m_playlist->setCurrentRow(currentIndex);
    }
}
void MainWindow::addFolder() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Add media folder"));
    if (path.isEmpty() || !m_playlistController || !m_playlist) return;
    const int currentIndex = m_playlistController->currentIndex();
    const bool wasEmpty = m_playlistController->count() == 0;
    QDir dir(path);
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& info : files) if (isMediaFile(info)) addToPlaylist(info.absoluteFilePath());
    if (wasEmpty && m_playlistController->count() > 0) {
        m_playlistController->setCurrentIndex(0);
        m_playlist->setCurrentRow(0);
        playlistActivated();
    } else if (currentIndex >= 0 && currentIndex < m_playlist->count()) {
        m_playlistController->setCurrentIndex(currentIndex);
        m_playlist->setCurrentRow(currentIndex);
    }
}
void MainWindow::savePlaylist() {
    if (!m_playlist || m_playlist->count() == 0) {
        QMessageBox::information(this, QStringLiteral("Save playlist"),
                                 QStringLiteral("The playlist is empty."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save playlist"), QString(),
        QStringLiteral("M3U8 Playlist (*.m3u8);;M3U Playlist (*.m3u);;All files (*.*)"));
    if (path.isEmpty()) return;

    QString savePath = path;
    if (QFileInfo(savePath).suffix().isEmpty()) savePath += QStringLiteral(".m3u8");

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_runtimeLogger->append(QStringLiteral("PLAYLIST SAVE: failed: %1").arg(file.errorString()));
        QMessageBox::warning(this, QStringLiteral("Save playlist"),
                             QStringLiteral("Could not save the playlist:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << QStringLiteral("#EXTM3U\n");
    for (int i = 0; i < m_playlist->count(); ++i) {
        const QString mediaPath = m_playlist->item(i)->data(Qt::UserRole).toString();
        if (!mediaPath.isEmpty()) stream << mediaPath << QLatin1Char('\n');
    }
    file.close();
    m_runtimeLogger->append(QStringLiteral("PLAYLIST SAVE: saved %1 item(s) to %2")
                             .arg(m_playlist->count()).arg(savePath));
}

void MainWindow::openPlaylist() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open playlist"), QString(),
        QStringLiteral("Playlist files (*.m3u8 *.m3u);;All files (*.*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_runtimeLogger->append(QStringLiteral("PLAYLIST OPEN: failed: %1").arg(file.errorString()));
        QMessageBox::warning(this, QStringLiteral("Open playlist"),
                             QStringLiteral("Could not open the playlist:\n%1").arg(file.errorString()));
        return;
    }

    QStringList paths;
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        paths.append(line);
    }
    file.close();

    m_playlist->clear();
    m_playlistController->clear();
    int missingCount = 0;
    for (const QString& mediaPath : paths) {
        const QFileInfo info(mediaPath);
        QFileInfo resolvedInfo = info;
        if (resolvedInfo.isRelative()) resolvedInfo = QFileInfo(QFileInfo(path).dir(), resolvedInfo.filePath());
        if (!resolvedInfo.exists() || !resolvedInfo.isFile()) {
            ++missingCount;
            continue;
        }
        addToPlaylist(resolvedInfo.absoluteFilePath());
    }

    if (m_playlistController->count() > 0) {
        m_playlistController->setCurrentIndex(0);
        m_playlist->setCurrentRow(0);
        playlistActivated();
    }

    m_runtimeLogger->append(QStringLiteral(
        "PLAYLIST OPEN: loaded %1 item(s) from %2; missing=%3")
        .arg(m_playlistController->count()).arg(path).arg(missingCount));

    if (missingCount > 0) {
        QMessageBox::information(
            this, QStringLiteral("Open playlist"),
            QStringLiteral("%1 playlist item(s) could not be found and were skipped.").arg(missingCount));
    }
}

void MainWindow::removePlaylistItem(int index) {
    if (!m_playlistController || !m_playlist || index < 0 || index >= m_playlist->count()) return;

    const QString removedPath = m_playlistController->pathAt(index);
    const bool wasCurrent = index == m_playlistController->currentIndex();
    QListWidgetItem* item = m_playlist->takeItem(index);
    delete item;
    m_playlistController->removeAt(index);

    if (m_playlist->count() == 0) {
        m_playlistController->clear();
    } else if (wasCurrent) {
        const int nextSelection = std::clamp(index, 0, m_playlist->count() - 1);
        m_playlistController->setCurrentIndex(nextSelection);
        m_playlist->setCurrentRow(nextSelection);
    } else {
        syncPlaylistSelection();
    }

    m_runtimeLogger->append(
        QStringLiteral("PLAYLIST REMOVE: %1").arg(removedPath));
}

void MainWindow::clearPlaylist() {
    if (m_playlist) m_playlist->clear();
    if (m_playlistController) m_playlistController->clear();
}
void MainWindow::playlistActivated() {
    if (m_playlistController && m_playlist && m_playlist->currentItem()) {
        m_playlistController->setCurrentIndex(m_playlist->currentRow());
        playPlaylistIndex(m_playlist->currentRow());
    }
}
void MainWindow::playPrevious() {
    if (!m_playlistController || m_playlistController->count() == 0) return;
    const int index = m_playlistController->previousIndex();
    if (index >= 0) playPlaylistIndex(index);
}
void MainWindow::playNext() {
    if (!m_playlistController || m_playlistController->count() == 0) return;
    const int index = m_playlistController->nextIndex();
    if (index >= 0) playPlaylistIndex(index, false);
}
void MainWindow::dragEnterEvent(QDragEnterEvent* event) { if (event->mimeData()->hasUrls()) event->acceptProposedAction(); }
void MainWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    const int currentIndex = m_playlistController ? m_playlistController->currentIndex() : -1;
    const bool wasEmpty = m_playlistController && m_playlistController->count() == 0;
    for (const auto& url : urls) if (url.isLocalFile()) addToPlaylist(url.toLocalFile());
    if (m_playlist) {
        if (wasEmpty && m_playlistController->count() > 0) {
            m_playlistController->setCurrentIndex(0);
            m_playlist->setCurrentRow(0);
            playlistActivated();
        } else if (currentIndex >= 0 && currentIndex < m_playlist->count()) {
            m_playlistController->setCurrentIndex(currentIndex);
            m_playlist->setCurrentRow(currentIndex);
        }
    }
    if (!urls.isEmpty()) event->acceptProposedAction();
}

bool MainWindow::keyMatches(QKeyEvent* event, const QKeySequence& sequence) const {
    if (sequence.isEmpty()) return false;
    const int key = event->key();
    if (key == Qt::Key_unknown) return false;
    const int combined = key | static_cast<int>(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    return QKeySequence(combined).matches(sequence) == QKeySequence::ExactMatch;
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastEscapePressMs > 0 && now - m_lastEscapePressMs <= 1000) {
            m_lastEscapePressMs = 0;
            close();
        } else {
            m_lastEscapePressMs = now;
            if (isFullScreen()) toggleFullscreen();
        }
        event->accept();
        return;
    }
    if (keyMatches(event, m_subtitleSizeUpKey)) { increaseSubtitleSize(); event->accept(); return; }
    if (keyMatches(event, m_subtitleSizeDownKey)) { decreaseSubtitleSize(); event->accept(); return; }
    if (keyMatches(event, m_captureScreenshotKey)) { captureScreenshot(); event->accept(); return; }
    if (keyMatches(event, m_rotateVideoKey)) { rotateVideo90(); event->accept(); return; }
    if (keyMatches(event, m_holdSpeedKey)) {
        if (!m_holdSpeedActive) {
            m_holdSpeedPrevious = getPropertyDouble("speed");
            if (!std::isfinite(m_holdSpeedPrevious) || m_holdSpeedPrevious <= 0.0) m_holdSpeedPrevious = 1.0;
            m_holdSpeedActive = true;
            m_holdSpeedKeyCode = event->key();
            setPropertyDouble("speed", 2.0);
            if (m_runtimeLogger) m_runtimeLogger->append(QStringLiteral("PLAYBACK SPEED HOLD: 2.00x"));
        }
        event->accept();
        return;
    }
    if (keyMatches(event, m_speedUpKey)) { increasePlaybackSpeed(); event->accept(); return; }
    if (keyMatches(event, m_speedDownKey)) { decreasePlaybackSpeed(); event->accept(); return; }
    if (keyMatches(event, m_volumeUpKey)) { volumeUp(); event->accept(); return; }
    if (keyMatches(event, m_volumeDownKey)) { volumeDown(); event->accept(); return; }
    if (keyMatches(event, m_muteKey)) { toggleMute(); event->accept(); return; }
    if (keyMatches(event, m_seekBackwardKey)) { seekBackward(); event->accept(); return; }
    if (keyMatches(event, m_seekForwardKey)) { seekForward(); event->accept(); return; }
    if (keyMatches(event, m_loopAKey)) { setAbLoopStart(); event->accept(); return; }
    if (keyMatches(event, m_loopBKey)) { setAbLoopEnd(); event->accept(); return; }
    if (keyMatches(event, m_loopClearKey)) { clearAbLoop(); event->accept(); return; }
    if (keyMatches(event, m_zoomInKey)) { adjustVideoZoom(0.1); event->accept(); return; }
    if (keyMatches(event, m_zoomOutKey)) { adjustVideoZoom(-0.1); event->accept(); return; }
    if (keyMatches(event, m_zoomResetKey)) { resetVideoTransform(); event->accept(); return; }
    if (keyMatches(event, m_frameBackKey)) { stepFrame(false); event->accept(); return; }
    if (keyMatches(event, m_frameForwardKey)) { stepFrame(true); event->accept(); return; }
    if (keyMatches(event, m_subtitlePosUpKey)) { increaseSubtitlePosition(); event->accept(); return; }
    if (keyMatches(event, m_subtitlePosDownKey)) { decreaseSubtitlePosition(); event->accept(); return; }
    if (keyMatches(event, m_switchSubtitlesKey)) { cycleSubtitles(); event->accept(); return; }

    switch (event->key()) {
    case Qt::Key_Space: togglePause(); break;
    case Qt::Key_Up: playPrevious(); break;
    case Qt::Key_Down: playNext(); break;
    case Qt::Key_F11: toggleFullscreen(); break;
    case Qt::Key_Escape: if (isFullScreen()) toggleFullscreen(); break;
    case Qt::Key_Return:
    case Qt::Key_Enter: toggleFullscreen(); break;
    default: QMainWindow::keyPressEvent(event); break;
    }
}


void MainWindow::keyReleaseEvent(QKeyEvent* event) {
    if (m_holdSpeedActive && event->key() == m_holdSpeedKeyCode) {
        const double restoreSpeed = (std::isfinite(m_holdSpeedPrevious) && m_holdSpeedPrevious > 0.0)
            ? m_holdSpeedPrevious : 1.0;
        m_holdSpeedActive = false;
        m_holdSpeedKeyCode = Qt::Key_unknown;
        setPropertyDouble("speed", restoreSpeed);
        if (m_runtimeLogger) {
            m_runtimeLogger->append(QStringLiteral("PLAYBACK SPEED HOLD: restored %1x").arg(restoreSpeed, 0, 'f', 2));
        }
        event->accept();
        return;
    }
    QMainWindow::keyReleaseEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (m_playlist && watched && watched->property("playlist-path").isValid()) {
        const QString path = watched->property("playlist-path").toString();
        const int index = m_playlistController ? m_playlistController->indexOf(path) : -1;
        if (index >= 0 && index < m_playlist->count()) {
            if (event->type() == QEvent::MouseButtonPress) {
                const auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    m_playlist->setCurrentRow(index);
                    m_playlistController->setCurrentIndex(index);
                    updatePlaylistCurrentRowStyle();
                }
            } else if (event->type() == QEvent::MouseButtonDblClick) {
                const auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    m_playlist->setCurrentRow(index);
                    m_playlistController->setCurrentIndex(index);
                    playlistActivated();
                    return true;
                }
            }
        }
    }

    if ((watched == m_timeLabel || watched == m_progressTimeLabel) &&
        event->type() == QEvent::MouseButtonPress) {
        m_showRemainingTime = !m_showRemainingTime;
        updatePlaybackUi();
        return true;
    }

    if (watched == m_videoWidget) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
            m_videoWidget->setCursor(Qt::ArrowCursor);
            m_cursorHideTimer.start();
        } else if (event->type() == QEvent::Leave) {
            m_cursorHideTimer.stop();
            m_videoWidget->setCursor(Qt::ArrowCursor);
        }
        if (event->type() == QEvent::MouseMove && isFullScreen()) {
            setControlsVisible(true);
            m_fullscreenHideTimer.start();
        }
    }

    if (watched == m_seekSlider && event->type() == QEvent::MouseButtonPress) {
        const auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton) {
            QStyleOptionSlider opt;
            opt.initFrom(m_seekSlider);
            opt.orientation = Qt::Horizontal;
            opt.minimum = m_seekSlider->minimum();
            opt.maximum = m_seekSlider->maximum();
            opt.sliderPosition = m_seekSlider->sliderPosition();
            const QRect handle = m_seekSlider->style()->subControlRect(
                QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, m_seekSlider);
            if (!handle.contains(e->position().toPoint())) {
                const QRect groove = m_seekSlider->style()->subControlRect(
                    QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, m_seekSlider);
                const int span = std::max(1, groove.width());
                const int x = static_cast<int>(e->position().x());
                const int position = std::clamp(x - groove.left(), 0, span);
                const int value = QStyle::sliderValueFromPosition(
                    opt.minimum, opt.maximum, position, span, opt.upsideDown);
                m_seekSlider->setValue(value);
                m_seeking = false;
                seekTo(value);
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

    if (watched != m_videoWidget) return QMainWindow::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonDblClick) {
        const auto* e = static_cast<QMouseEvent*>(event);
        if (m_doubleClickZones && m_doubleClickButton != Qt::NoButton && e->button() == m_doubleClickButton) {
            const qreal x = e->position().x();
            const qreal width = m_videoWidget->width();
            if (x < width / 3.0) seekBackward();
            else if (x > width * 2.0 / 3.0) seekForward();
            else togglePause();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        const auto* e = static_cast<QMouseEvent*>(event);
        const Qt::KeyboardModifiers modifiers = e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        if (m_panButton != Qt::NoButton && e->button() == m_panButton &&
            modifiers == (Qt::AltModifier | Qt::ControlModifier)) {
            m_panningVideo = true;
            m_panStart = e->position();
            m_videoPanX = getPropertyDouble("video-pan-x");
            m_videoPanY = getPropertyDouble("video-pan-y");
            return true;
        }
    }

    if (event->type() == QEvent::MouseMove && m_panningVideo) {
        const auto* e = static_cast<QMouseEvent*>(event);
        const QPointF delta = e->position() - m_panStart;
        const double width = std::max(1, m_videoWidget->width());
        const double height = std::max(1, m_videoWidget->height());
        m_videoPanX = std::clamp(m_videoPanX + delta.x() / width, -1.0, 1.0);
        m_videoPanY = std::clamp(m_videoPanY + delta.y() / height, -1.0, 1.0);
        setPropertyDouble("video-pan-x", m_videoPanX);
        setPropertyDouble("video-pan-y", m_videoPanY);
        m_panStart = e->position();
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        const auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == m_panButton && m_panningVideo) {
            m_panningVideo = false;
            return true;
        }
    }

    if (event->type() == QEvent::Wheel) {
        const auto* e = static_cast<QWheelEvent*>(event);
        const int delta = !e->angleDelta().isNull() ? e->angleDelta().y() : e->pixelDelta().y();
        if (delta == 0) return false;
        const Qt::KeyboardModifiers modifiers = e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        if (wheelModeMatches(m_zoomWheelMode, modifiers)) {
            adjustVideoZoom(delta > 0 ? 0.1 : -0.1);
            return true;
        }
        if (wheelModeMatches(m_volumeWheelMode, modifiers)) {
            m_volumeSlider->setValue(std::clamp(m_volumeSlider->value() + (delta > 0 ? 5 : -5), 0, 100));
            return true;
        }
        if (wheelModeMatches(m_seekWheelMode, modifiers)) {
            if (delta > 0) seekForward();
            else seekBackward();
            return true;
        }
        return false;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::toggleControls() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Video Information"));
    dialog.setModal(true);
    dialog.resize(720, 700);

    auto* mainLayout = new QVBoxLayout(&dialog);
    auto* scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    auto* content = new QWidget(scrollArea);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto valueOrDash = [this](const char* property) {
        const QString value = getPropertyString(property).trimmed();
        return value.isEmpty() ? QStringLiteral("—") : value;
    };
    auto numberOrDash = [this](const char* property, int decimals = 2) {
        const double value = getPropertyDouble(property);
        return value > 0.0 ? QString::number(value, 'f', decimals) : QStringLiteral("—");
    };
    auto addSection = [&layout](const QString& title) {
        auto* label = new QLabel(title, layout->parentWidget());
        label->setStyleSheet(QStringLiteral("font-weight:600; margin-top:6px;"));
        layout->addWidget(label);
    };
    auto addRow = [&layout](const QString& name, const QString& value) {
        auto* row = new QFormLayout();
        row->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto* label = new QLabel(value, layout->parentWidget());
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        row->addRow(name, label);
        layout->addLayout(row);
    };

    if (!m_mpv || getPropertyString("filename").isEmpty()) {
        layout->addWidget(new QLabel(QStringLiteral("No media is currently loaded."), content));
    } else {
        addSection(QStringLiteral("File"));
        addRow(QStringLiteral("File name"), valueOrDash("filename"));
        addRow(QStringLiteral("Title"), valueOrDash("media-title"));
        addRow(QStringLiteral("Path"), valueOrDash("path"));
        addRow(QStringLiteral("Container"), valueOrDash("file-format"));
        addRow(QStringLiteral("File size"), valueOrDash("file-size"));
        addRow(QStringLiteral("Duration"), formatTime(getPropertyDouble("duration")));
        addRow(QStringLiteral("Overall bitrate"), valueOrDash("bitrate"));

        addSection(QStringLiteral("Video"));
        addRow(QStringLiteral("Codec"), valueOrDash("video-codec"));
        addRow(QStringLiteral("Format"), valueOrDash("video-format"));
        addRow(QStringLiteral("Resolution"), QStringLiteral("%1 × %2").arg(numberOrDash("width", 0), numberOrDash("height", 0)));
        addRow(QStringLiteral("FPS"), valueOrDash("container-fps"));
        addRow(QStringLiteral("Bitrate"), valueOrDash("video-bitrate"));
        addRow(QStringLiteral("Pixel format"), valueOrDash("video-params/pixelformat"));
        addRow(QStringLiteral("Chroma location"), valueOrDash("video-params/chroma-location"));
        addRow(QStringLiteral("Color matrix"), valueOrDash("video-params/colormatrix"));
        addRow(QStringLiteral("Color primaries"), valueOrDash("video-params/primaries"));
        addRow(QStringLiteral("Transfer"), valueOrDash("video-params/transfer"));
        addRow(QStringLiteral("Rotation"), valueOrDash("video-params/rotate"));
        addRow(QStringLiteral("Aspect ratio"), valueOrDash("video-params/aspect"));
        addRow(QStringLiteral("HW decoder"), valueOrDash("hwdec-current"));

        addSection(QStringLiteral("Audio"));
        addRow(QStringLiteral("Codec"), valueOrDash("audio-codec-name"));
        addRow(QStringLiteral("Format"), valueOrDash("audio-format"));
        addRow(QStringLiteral("Sample rate"), valueOrDash("audio-params/samplerate"));
        addRow(QStringLiteral("Channels"), valueOrDash("audio-params/channel-count"));
        addRow(QStringLiteral("Channel layout"), valueOrDash("audio-params/channel-layout"));
        addRow(QStringLiteral("Bitrate"), valueOrDash("audio-bitrate"));

        addSection(QStringLiteral("Tracks"));
        mpv_node tracks{};
        bool haveTracks = false;
        if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &tracks) >= 0 &&
            tracks.format == MPV_FORMAT_NODE_ARRAY && tracks.u.list) {
            for (int i = 0; i < tracks.u.list->num; ++i) {
                const mpv_node& track = tracks.u.list->values[i];
                if (track.format != MPV_FORMAT_NODE_MAP || !track.u.list) continue;
                const QString type = MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "type"));
                const int id = MpvNodeUtils::nodeInt(MpvNodeUtils::mapValue(track.u.list, "id"));
                if (id < 0) continue;
                QString label = MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "title"));
                const QString lang = MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "lang"));
                const QString codec = MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "codec"));
                const QString external = MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "external-filename"));
                const bool selected = MpvNodeUtils::nodeFlag(MpvNodeUtils::mapValue(track.u.list, "selected"));
                if (label.isEmpty()) label = lang;
                if (label.isEmpty()) label = external.isEmpty() ? QStringLiteral("Track") : QFileInfo(external).fileName();
                if (label.isEmpty()) label = QStringLiteral("Track");
                QString details = QStringLiteral("#%1 — %2").arg(id).arg(label);
                if (!lang.isEmpty() && label != lang) details += QStringLiteral(" [%1]").arg(lang);
                if (!codec.isEmpty()) details += QStringLiteral(" • %1").arg(codec);
                if (!external.isEmpty()) details += QStringLiteral(" • %1").arg(QFileInfo(external).fileName());
                if (selected) details += QStringLiteral("  ✓ Active");
                const QString section = type == QStringLiteral("video") ? QStringLiteral("Video track")
                    : type == QStringLiteral("audio") ? QStringLiteral("Audio track")
                    : type == QStringLiteral("sub") ? QStringLiteral("Subtitle track")
                    : QStringLiteral("Other track");
                addRow(section, details);
                haveTracks = true;
            }
        }
        mpv_free_node_contents(&tracks);
        if (!haveTracks) addRow(QStringLiteral("Available"), QStringLiteral("No track information available."));

        addSection(QStringLiteral("Playback / Output"));
        addRow(QStringLiteral("Position"), QStringLiteral("%1 / %2").arg(formatTime(getPropertyDouble("time-pos")), formatTime(getPropertyDouble("duration"))));
        addRow(QStringLiteral("Speed"), numberOrDash("speed"));
        addRow(QStringLiteral("Pause"), valueOrDash("pause"));
        addRow(QStringLiteral("Video output"), valueOrDash("vo"));
        addRow(QStringLiteral("GPU API"), valueOrDash("gpu-api"));
        addRow(QStringLiteral("Hardware decoding"), valueOrDash("hwdec-current"));
    }

    scrollArea->setWidget(content);
    mainLayout->addWidget(scrollArea, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    mainLayout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::toggleFullscreen() {
    if (isFullScreen()) {
        m_fullscreenHideTimer.stop();
        if (m_rootWidget && m_controls) {
            setControlsVisible(true);
            m_controls->setParent(m_rootWidget);
            m_controls->show();
            auto* rootLayout = qobject_cast<QVBoxLayout*>(m_rootWidget->layout());
            if (rootLayout) {
                rootLayout->addWidget(m_controls);
            }
        }
        if (m_playlistDock) m_playlistDock->setVisible(m_playlistWasVisibleBeforeFullscreen);
        showNormal();
        return;
    }

    m_playlistWasVisibleBeforeFullscreen = m_playlistDock && m_playlistDock->isVisible();
    if (m_playlistDock) m_playlistDock->hide();

    // Keep the video viewport at the full-screen size. The controls are an
    // overlay rather than part of the vertical layout, so showing/hiding them
    // cannot change the video's scale.
    if (m_rootWidget && m_controls) {
        auto* rootLayout = qobject_cast<QVBoxLayout*>(m_rootWidget->layout());
        if (rootLayout) rootLayout->removeWidget(m_controls);
        m_controls->setParent(m_rootWidget);
        m_controls->raise();
        m_controls->setGeometry(0, std::max(0, m_rootWidget->height() - m_controls->sizeHint().height()),
                                 m_rootWidget->width(), m_controls->sizeHint().height());
    }
    setControlsVisible(false);
    showFullScreen();
    if (m_rootWidget && m_controls) {
        m_controls->setGeometry(0, std::max(0, m_rootWidget->height() - m_controls->sizeHint().height()),
                                 m_rootWidget->width(), m_controls->sizeHint().height());
        m_controls->raise();
    }
}

void MainWindow::setControlsVisible(bool visible) { if (m_controls) m_controls->setVisible(visible); }
void MainWindow::togglePlaylist() { if (m_playlistDock) m_playlistDock->setVisible(!m_playlistDock->isVisible()); }
void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (isFullScreen() && m_rootWidget && m_controls && m_controls->parentWidget() == m_rootWidget) {
        const int height = m_controls->sizeHint().height();
        m_controls->setGeometry(0, std::max(0, m_rootWidget->height() - height),
                                 m_rootWidget->width(), height);
        m_controls->raise();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) { saveCurrentPlaybackPosition(); if (m_mpv) { const char* args[] = {"quit", nullptr}; mpv_command(m_mpv, args); } QMainWindow::closeEvent(event); }
void MainWindow::showToast(const QString& message) {
    if (!m_toastLabel || !m_rootWidget) return;

    m_toastLabel->setText(message);
    m_toastLabel->adjustSize();

    const int controlsHeight =
        (m_controls && m_controls->isVisible()) ? m_controls->height() : 0;
    const int bottomMargin = controlsHeight + 16;
    const int x = std::max(0, (m_rootWidget->width() - m_toastLabel->width()) / 2);
    const int y = std::max(0, m_rootWidget->height() -
                              bottomMargin - m_toastLabel->height());

    m_toastLabel->move(x, y);
    m_toastLabel->raise();
    m_toastLabel->show();
    m_toastTimer.start(1800);
}

void MainWindow::showError(const QString& message) { setWindowTitle(QStringLiteral("REX Player — %1").arg(message)); }
