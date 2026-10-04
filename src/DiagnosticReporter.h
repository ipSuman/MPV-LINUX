#pragma once

#include <QObject>
#include <QString>
#include <functional>

class RuntimeLogger;
class QWidget;
struct mpv_handle;
class PlaylistController;

class DiagnosticReporter final : public QObject {
public:
    DiagnosticReporter(mpv_handle* mpv, RuntimeLogger* logger,
                       PlaylistController* playlistController, QWidget* parent = nullptr);

    void saveReport();
    void setControlStateProvider(std::function<QString()> provider);

private:
    void log(const QString& message) const;

    mpv_handle* m_mpv = nullptr;
    RuntimeLogger* m_logger = nullptr;
    PlaylistController* m_playlistController = nullptr;
    QWidget* m_parentWidget = nullptr;
    std::function<QString()> m_controlStateProvider;
};
