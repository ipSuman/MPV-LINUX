#pragma once

#include <QObject>

class RuntimeLogger;
class QProcess;

class PlaybackInhibitor final : public QObject {
public:
    explicit PlaybackInhibitor(RuntimeLogger* logger, QObject* parent = nullptr);
    ~PlaybackInhibitor() override;

    void setActive(bool active);
    bool isActive() const { return m_active; }

private:
    void log(const QString& message) const;

    RuntimeLogger* m_logger = nullptr;
    QProcess* m_process = nullptr;
    bool m_active = false;
};
