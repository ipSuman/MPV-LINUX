#include "MainWindow.h"
#include <clocale>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QKeyEvent>
#include <QLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {
void toggleFullscreenFromButton(MainWindow& window) {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::NoModifier);
    QApplication::sendEvent(&window, &event);

    // Windows can leave the native top-level window one frame short of the
    // screen edge when entering Qt fullscreen. Keep both states asserted so
    // the fullscreen window is always maximized and no thin edge is exposed.
    if (window.isFullScreen()) {
        window.setWindowState(window.windowState() | Qt::WindowFullScreen | Qt::WindowMaximized);
        window.show();
    }
}
}

int main(int argc, char* argv[]) {
    std::setlocale(LC_NUMERIC, "C");
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("REX Player"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/rex-player.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("REX Player desktop media player"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Video or audio file to open"));
    parser.process(app);

    const QString mediaPath = parser.positionalArguments().value(0);
    MainWindow window(mediaPath);

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
                        auto* fullscreenButton = new QPushButton(QStringLiteral("[  ]"), controls);
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
