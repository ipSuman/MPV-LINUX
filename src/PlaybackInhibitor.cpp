#include "PlaybackInhibitor.h"

#include <QProcess>
#include <QStandardPaths>
#include <QString>

#include "RuntimeLogger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

PlaybackInhibitor::PlaybackInhibitor(RuntimeLogger* logger, QObject* parent)
    : QObject(parent), m_logger(logger) {}

PlaybackInhibitor::~PlaybackInhibitor() {
    setActive(false);
}

void PlaybackInhibitor::log(const QString& message) const {
    if (m_logger) m_logger->append(message);
}

void PlaybackInhibitor::setActive(bool active) {
#ifdef Q_OS_WIN
    if (active == m_active) return;

    if (active) {
        const EXECUTION_STATE result =
            SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
        m_active = result != 0;
        log(QStringLiteral("POWER INHIBIT: enable Windows display execution state result=%1")
                .arg(result != 0 ? QStringLiteral("success") : QStringLiteral("FAILED")));
    } else {
        const EXECUTION_STATE result = SetThreadExecutionState(ES_CONTINUOUS);
        m_active = false;
        log(QStringLiteral("POWER INHIBIT: disable Windows execution state result=%1")
                .arg(result != 0 ? QStringLiteral("success") : QStringLiteral("FAILED")));
    }
#elif defined(Q_OS_LINUX)
    if (active) {
        if (!m_process || m_process->state() == QProcess::NotRunning) {
            if (m_process) {
                log(QStringLiteral("POWER INHIBIT: previous inhibitor process was not running; replacing it"));
                m_process->deleteLater();
                m_process = nullptr;
            }

            m_process = new QProcess(this);
            const QString gnomeInhibit =
                QStandardPaths::findExecutable(QStringLiteral("gnome-session-inhibit"));

            if (!gnomeInhibit.isEmpty()) {
                m_process->setProgram(gnomeInhibit);
                m_process->setArguments({
                    QStringLiteral("--app-id=rex-player"),
                    QStringLiteral("--reason=Video playback"),
                    QStringLiteral("--inhibit=idle"),
                    QStringLiteral("--inhibit-only")
                });
                log(QStringLiteral("POWER INHIBIT: starting gnome-session-inhibit"));
            } else {
                m_process->setProgram(QStringLiteral("systemd-inhibit"));
                m_process->setArguments({
                    QStringLiteral("--what=idle"),
                    QStringLiteral("--who=REX Player"),
                    QStringLiteral("--why=Video playback"),
                    QStringLiteral("--mode=block"),
                    QStringLiteral("sleep"),
                    QStringLiteral("infinity")
                });
                log(QStringLiteral("POWER INHIBIT: gnome-session-inhibit unavailable; using systemd-inhibit"));
            }

            m_process->setProcessChannelMode(QProcess::SeparateChannels);
            m_process->start();

            if (!m_process->waitForStarted(500)) {
                log(QStringLiteral("POWER INHIBIT: inhibitor process FAILED to start: %1")
                        .arg(m_process->errorString()));
            } else {
                log(QStringLiteral("POWER INHIBIT: inhibitor process started"));
            }
        }

        m_active = m_process && m_process->state() != QProcess::NotRunning;
        log(QStringLiteral("POWER INHIBIT: active=%1")
                .arg(m_active ? QStringLiteral("yes") : QStringLiteral("no")));
    } else {
        if (m_process) {
            log(QStringLiteral("POWER INHIBIT: stopping inhibitor process"));
            m_process->terminate();
            if (!m_process->waitForFinished(500)) {
                m_process->kill();
                m_process->waitForFinished(200);
            }
            m_process->deleteLater();
            m_process = nullptr;
        }
        m_active = false;
        log(QStringLiteral("POWER INHIBIT: disabled"));
    }
#else
    m_active = active;
    log(QStringLiteral("POWER INHIBIT: platform has no implementation; requested active=%1")
            .arg(active ? QStringLiteral("yes") : QStringLiteral("no")));
#endif
}
