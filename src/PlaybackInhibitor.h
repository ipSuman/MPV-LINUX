#pragma once

#include <QObject>
#include <QStringList>
#include <cstdint>

class RuntimeLogger;
class QProcess;
class QDBusInterface;

class PlaybackInhibitor final : public QObject {
public:
    explicit PlaybackInhibitor(RuntimeLogger* logger, QObject* parent = nullptr);
    ~PlaybackInhibitor() override;

    void setActive(bool active);
    bool isActive() const { return m_active; }

private:
    void log(const QString& message) const;
    void startInhibitor(const QString& program, const QStringList& arguments, const QString& source);

    RuntimeLogger* m_logger = nullptr;
    QProcess* m_process = nullptr;
    QDBusInterface* m_screenSaverInterface = nullptr;
    std::uint32_t m_screenSaverCookie = 0;
    bool m_active = false;
};
