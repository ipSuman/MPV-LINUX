#pragma once

#include <QObject>
#include <QString>

class RuntimeLogger;
class QMenu;
class QWidget;
struct mpv_handle;

class TrackController final : public QObject {
public:
    TrackController(mpv_handle* mpv, RuntimeLogger* logger, QObject* parent = nullptr);

    void showMenu(QWidget* anchor = nullptr);
    void cycleSubtitles();

private:
    void log(const QString& message) const;

    mpv_handle* m_mpv = nullptr;
    RuntimeLogger* m_logger = nullptr;
};
