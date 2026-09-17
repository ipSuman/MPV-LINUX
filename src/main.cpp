#include "MainWindow.h"
#include <clocale>

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QKeyEvent>
#include <QLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QStringConverter>
#include <QTextStream>
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

void setupPlaylistFileButtons(MainWindow& window) {
    const auto playlistLists = window.findChildren<QListWidget*>();
    if (playlistLists.isEmpty()) return;
    QListWidget* playlist = playlistLists.constFirst();

    QPushButton* addFilesButton = nullptr;
    const auto buttons = playlist->parentWidget()->findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button->text() == QStringLiteral("+ Files")) {
            addFilesButton = button;
            break;
        }
    }
    if (!addFilesButton) return;

    auto* buttonRow = qobject_cast<QBoxLayout*>(addFilesButton->parentWidget()->layout());
    if (!buttonRow) return;

    auto* saveButton = new QPushButton(QStringLiteral("💾"), addFilesButton->parentWidget());
    saveButton->setFixedWidth(42);
    saveButton->setToolTip(QStringLiteral("Save playlist"));
    QObject::connect(saveButton, &QPushButton::clicked, &window, [&window, playlist] {
        if (playlist->count() == 0) {
            QMessageBox::information(&window, QStringLiteral("Save playlist"),
                                     QStringLiteral("The playlist is empty."));
            return;
        }

        const QString path = QFileDialog::getSaveFileName(
            &window, QStringLiteral("Save playlist"), QString(),
            QStringLiteral("M3U8 Playlist (*.m3u8);;M3U Playlist (*.m3u);;All files (*.*)"));
        if (path.isEmpty()) return;

        QString savePath = path;
        if (QFileInfo(savePath).suffix().isEmpty()) savePath += QStringLiteral(".m3u8");

        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(&window, QStringLiteral("Save playlist"),
                                 QStringLiteral("Could not save the playlist:\n%1").arg(file.errorString()));
            return;
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << QStringLiteral("#EXTM3U\n");
        for (int i = 0; i < playlist->count(); ++i) {
            const QString mediaPath = playlist->item(i)->data(Qt::UserRole).toString();
            if (!mediaPath.isEmpty()) stream << mediaPath << QLatin1Char('\n');
        }
        file.close();
    });

    auto* openButton = new QPushButton(QStringLiteral("📖"), addFilesButton->parentWidget());
    openButton->setFixedWidth(42);
    openButton->setToolTip(QStringLiteral("Open playlist"));
    QObject::connect(openButton, &QPushButton::clicked, &window, [&window, playlist] {
        const QString path = QFileDialog::getOpenFileName(
            &window, QStringLiteral("Open playlist"), QString(),
            QStringLiteral("Playlist files (*.m3u8 *.m3u);;All files (*.*)"));
        if (path.isEmpty()) return;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(&window, QStringLiteral("Open playlist"),
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

        if (paths.isEmpty()) {
            playlist->clear();
            return;
        }

        playlist->clear();
        int missingCount = 0;
        for (const QString& mediaPath : paths) {
            const QFileInfo info(mediaPath);
            if (!info.exists() || !info.isFile()) {
                ++missingCount;
                continue;
            }
            auto* item = new QListWidgetItem(info.fileName(), playlist);
            item->setData(Qt::UserRole, info.absoluteFilePath());
        }

        if (playlist->count() > 0) {
            playlist->setCurrentRow(0);
            QMetaObject::invokeMethod(&window, "playlistActivated", Qt::DirectConnection);
        }

        if (missingCount > 0) {
            QMessageBox::information(
                &window, QStringLiteral("Open playlist"),
                QStringLiteral("%1 playlist item(s) could not be found and were skipped.").arg(missingCount));
        }
    });

    const int addIndex = buttonRow->indexOf(addFilesButton);
    buttonRow->insertWidget(addIndex >= 0 ? addIndex : 0, saveButton);
    buttonRow->insertWidget(addIndex >= 0 ? addIndex + 1 : 1, openButton);
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

    setupPlaylistFileButtons(window);

    window.show();
    return app.exec();
}
