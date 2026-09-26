#pragma once

#include <QObject>
#include <QString>

struct mpv_handle;

class ScreenshotController final : public QObject {
    Q_OBJECT
public:
    ScreenshotController(mpv_handle* mpv, QObject* parent = nullptr);
    void capture(const QString& currentTitle, const QString& currentPath);

signals:
    void errorMessage(const QString& message);
    void successMessage(const QString& message, const QString& mediaPath);
    void restoreTitleRequested(const QString& title, const QString& mediaPath);
    void logMessage(const QString& message);

private:
    mpv_handle* m_mpv = nullptr;
};
