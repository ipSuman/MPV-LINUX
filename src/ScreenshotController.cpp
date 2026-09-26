#include "ScreenshotController.h"

#include <QDir>
#include <QStandardPaths>
#include <QTimer>

#include <mpv/client.h>

ScreenshotController::ScreenshotController(mpv_handle* mpv, QObject* parent)
    : QObject(parent), m_mpv(mpv) {}

void ScreenshotController::capture(const QString& currentTitle, const QString& currentPath) {
    emit logMessage(QStringLiteral("SCREENSHOT: capture requested; path=%1").arg(currentPath));
    if (!m_mpv) {
        emit errorMessage(QStringLiteral("REX Player — Screenshot failed"));
        return;
    }

    emit logMessage(QStringLiteral("SCREENSHOT: resolving Pictures directory"));

    const QString picturesPath =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.isEmpty()) {
        emit errorMessage(QStringLiteral("REX Player — Could not locate the Pictures folder"));
        return;
    }

    const QString screenshotDir =
        QDir(picturesPath).filePath(QStringLiteral("REX Player"));
    emit logMessage(QStringLiteral("SCREENSHOT: target directory=%1").arg(screenshotDir));
    if (!QDir().mkpath(screenshotDir)) {
        emit errorMessage(QStringLiteral("REX Player — Could not create the screenshot folder"));
        return;
    }

    const QByteArray dirUtf8 = QDir::toNativeSeparators(screenshotDir).toUtf8();
    const char* setDirArgs[] = {"set", "screenshot-dir", dirUtf8.constData(), nullptr};
    emit logMessage(QStringLiteral("SCREENSHOT: configuring mpv screenshot-dir"));
    if (mpv_command(m_mpv, setDirArgs) < 0) {
        emit errorMessage(QStringLiteral("REX Player — Could not configure screenshot folder"));
        return;
    }

    const char* screenshotArgs[] = {"screenshot", nullptr};
    emit logMessage(QStringLiteral("SCREENSHOT: issuing mpv screenshot command"));
    if (mpv_command(m_mpv, screenshotArgs) < 0) {
        emit errorMessage(QStringLiteral("REX Player — Screenshot failed"));
        return;
    }

    emit logMessage(QStringLiteral("SCREENSHOT: mpv accepted screenshot command"));
    emit successMessage(QStringLiteral("REX Player — Screenshot captured"), currentPath);

    QTimer::singleShot(2000, this, [this, currentTitle, currentPath] {
        if (!m_mpv) return;
        emit logMessage(QStringLiteral("SCREENSHOT: restoring window title if media is unchanged"));
        emit restoreTitleRequested(currentTitle, currentPath);
    });
}
