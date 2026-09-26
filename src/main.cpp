#include "MainWindow.h"
#include <clocale>

#include <QApplication>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif
#include <QCommandLineParser>
#include <QIcon>
#include <QKeyEvent>
#include <QLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>

namespace {
#ifdef Q_OS_WIN
void setWindowsFullscreenBorder(MainWindow& window, bool fullscreen) {
    const HWND hwnd = reinterpret_cast<HWND>(window.winId());
    if (!hwnd) return;

    // Windows 11 draws a thin DWM border around top-level windows. In
    // fullscreen mode it can remain visible in the user's accent colour.
    // DWMWA_COLOR_NONE explicitly suppresses that border.
    constexpr COLORREF kColorNone = static_cast<COLORREF>(0xFFFFFFFEu);
    constexpr COLORREF kColorDefault = static_cast<COLORREF>(0xFFFFFFFFu);
    const COLORREF color = fullscreen ? kColorNone : kColorDefault;
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(color));
}
#endif

void toggleFullscreenFromButton(MainWindow& window) {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::NoModifier);
    QApplication::sendEvent(&window, &event);

    // Qt fullscreen already requests a borderless, screen-sized window.
    // Do not combine WindowFullScreen with WindowMaximized here: on Windows 11
    // that can make DWM retain a one-pixel accent-colored non-client border.
    // showFullScreen() is the correct native fullscreen state for this mode.
}

}

int main(int argc, char* argv[]) {
    std::setlocale(LC_NUMERIC, "C");
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("REX Player"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.1.0"));
    // Tell GNOME and other freedesktop desktops exactly which .desktop entry
    // represents this window, so the running app uses the REX Player icon
    // instead of being grouped under a generic application icon.
    app.setDesktopFileName(QStringLiteral("rex-player"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/rex-player.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("REX Player desktop media player"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Video or audio file to open"));
    parser.process(app);

    const QString mediaPath = parser.positionalArguments().value(0);
    MainWindow window(mediaPath);

#ifdef Q_OS_WIN
    // Poll the top-level state because QWidget does not expose the
    // windowStateChanged signal used by QWindow. This also covers fullscreen
    // entered through F11/Enter as well as the fullscreen button.
    auto* fullscreenBorderTimer = new QTimer(&window);
    QObject::connect(fullscreenBorderTimer, &QTimer::timeout, &window, [&window] {
        setWindowsFullscreenBorder(window, window.isFullScreen());
    });
    fullscreenBorderTimer->start(100);
#endif

    // Add the fullscreen button immediately to the left of the existing Open
    // button without disturbing the existing MainWindow layout or controls.
    if (QWidget* controls = window.findChild<QWidget*>(QStringLiteral("controls"))) {
        if (QVBoxLayout* controlsLayout = qobject_cast<QVBoxLayout*>(controls->layout())) {
            QLayout* rowLayout = controlsLayout->itemAt(1)
                ? controlsLayout->itemAt(1)->layout() : nullptr;
            if (auto* buttonRow = qobject_cast<QBoxLayout*>(rowLayout)) {
                const auto buttons = controls->findChildren<QPushButton*>();
                for (QPushButton* button : buttons) {
                    if (button->text() == QStringLiteral("Open")) {
                        auto* fullscreenButton = new QPushButton(QStringLiteral("⛶"), controls);
                        fullscreenButton->setFixedWidth(48);
                        fullscreenButton->setToolTip(QStringLiteral("Fullscreen"));
                        QObject::connect(fullscreenButton, &QPushButton::clicked, &window,
                                         [&window] { toggleFullscreenFromButton(window); });
                        const int openIndex = buttonRow->indexOf(button);
                        buttonRow->insertWidget(openIndex >= 0 ? openIndex : 0, fullscreenButton);
                        break;
                    }
                }
            }
        }
    }


    window.show();
    return app.exec();
}
