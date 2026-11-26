#pragma once

#include <QObject>
#include <QString>
#include <functional>

class MprisController final : public QObject {
    Q_OBJECT
public:
    MprisController(std::function<void()> playPause,
                    std::function<void()> next,
                    std::function<void()> previous,
                    std::function<void()> stop,
                    std::function<QString()> playbackStatus,
                    QObject* parent = nullptr);
    ~MprisController() override;

private:
    bool m_registered = false;
    std::function<QString()> m_playbackStatus;
};
