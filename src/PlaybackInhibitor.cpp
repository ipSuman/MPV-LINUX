#include "PlaybackInhibitor.h"

#include <QProcess>
#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#endif
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

void PlaybackInhibitor::startInhibitor(const QString& program, const QStringList& arguments, const QString& source) {
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_process = new QProcess(this);
    m_process->setProgram(program);
    m_process->setArguments(arguments);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::started, this, [this, source]() {
        m_active = true;
        log(QStringLiteral("POWER INHIBIT: %1 started").arg(source));
    });

    connect(m_process, &QProcess::errorOccurred, this,
            [this, source](QProcess::ProcessError) {
        m_active = false;
        log(QStringLiteral("POWER INHIBIT: %1 error: %2")
                .arg(source, m_process ? m_process->errorString() : QStringLiteral("unknown")));
    });

    connect(m_process, &QProcess::finished, this,
            [this, source](int exitCode, QProcess::ExitStatus exitStatus) {
        const QString stderrText = m_process
            ? QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed()
            : QString();
        log(QStringLiteral("POWER INHIBIT: %1 exited code=%2 status=%3%4")
                .arg(source)
                .arg(exitCode)
                .arg(exitStatus == QProcess::NormalExit ? QStringLiteral("normal")
                                                        : QStringLiteral("crash"))
                .arg(stderrText.isEmpty()
                         ? QString()
                         : QStringLiteral(" stderr=%1").arg(stderrText.left(300))));
        m_active = false;
    });

    log(QStringLiteral("POWER INHIBIT: starting %1").arg(source));
    m_process->start();
}

void PlaybackInhibitor::setActive(bool active) {
#ifdef Q_OS_WIN
    if (!active) {
        if (m_powerRequest) {
            const HANDLE request = reinterpret_cast<HANDLE>(m_powerRequest);
            const BOOL cleared = PowerClearRequest(request, PowerRequestDisplayRequired);
            const BOOL closed = CloseHandle(request);
            m_powerRequest = nullptr;
            m_active = false;
            log(QStringLiteral("POWER INHIBIT: Windows display/screensaver request released clear=%1 close=%2")
                    .arg(cleared ? QStringLiteral("success") : QStringLiteral("FAILED"))
                    .arg(closed ? QStringLiteral("success") : QStringLiteral("FAILED")));
        } else {
            m_active = false;
        }
        return;
    }

    if (m_active || m_powerRequest) return;

    REASON_CONTEXT context{};
    context.Version = POWER_REQUEST_CONTEXT_VERSION;
    context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    context.Reason.SimpleReasonString = const_cast<LPWSTR>(L"REX Player video playback");

    const HANDLE request = PowerCreateRequest(&context);
    if (request == INVALID_HANDLE_VALUE) {
        m_active = false;
        log(QStringLiteral("POWER INHIBIT: Windows display/screensaver request creation FAILED error=%1")
                .arg(static_cast<qulonglong>(GetLastError())));
        return;
    }

    if (!PowerSetRequest(request, PowerRequestDisplayRequired)) {
        const DWORD error = GetLastError();
        CloseHandle(request);
        m_active = false;
        log(QStringLiteral("POWER INHIBIT: Windows display/screensaver request FAILED error=%1")
                .arg(static_cast<qulonglong>(error)));
        return;
    }

    m_powerRequest = reinterpret_cast<void*>(request);
    m_active = true;
    log(QStringLiteral("POWER INHIBIT: Windows display/screensaver request acquired"));
#elif defined(Q_OS_LINUX)
    if (!active) {
        if (m_screenSaverInterface && m_screenSaverCookie != 0) {
            const QDBusMessage reply = m_screenSaverInterface->call(
                QStringLiteral("UnInhibit"), m_screenSaverCookie);
            log(QStringLiteral("POWER INHIBIT: D-Bus idle inhibition released result=%1")
                    .arg(reply.type() != QDBusMessage::ErrorMessage ? QStringLiteral("success") : reply.errorMessage()));
            m_screenSaverCookie = 0;
        }
        if (m_screenSaverInterface) {
            delete m_screenSaverInterface;
            m_screenSaverInterface = nullptr;
        }
        if (m_process) {
            log(QStringLiteral("POWER INHIBIT: stopping fallback inhibitor process"));
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
        return;
    }

    if (m_active || m_screenSaverCookie != 0 || (m_process && m_process->state() != QProcess::NotRunning)) return;

    m_screenSaverInterface = new QDBusInterface(
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("/org/freedesktop/ScreenSaver"),
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QDBusConnection::sessionBus(), this);
    if (m_screenSaverInterface->isValid()) {
        const QDBusReply<std::uint32_t> reply = m_screenSaverInterface->call(
            QStringLiteral("Inhibit"), QStringLiteral("REX Player"), QStringLiteral("Video playback"));
        if (reply.isValid() && reply.value() != 0) {
            m_screenSaverCookie = reply.value();
            m_active = true;
            log(QStringLiteral("POWER INHIBIT: D-Bus idle inhibition acquired cookie=%1")
                    .arg(m_screenSaverCookie));
            return;
        }
        log(QStringLiteral("POWER INHIBIT: D-Bus idle inhibition unavailable: %1")
                .arg(reply.isValid() ? QStringLiteral("invalid cookie") : reply.error().message()));
    } else {
        log(QStringLiteral("POWER INHIBIT: D-Bus idle inhibition service unavailable"));
    }
    delete m_screenSaverInterface;
    m_screenSaverInterface = nullptr;

    const QString systemdInhibit =
        QStandardPaths::findExecutable(QStringLiteral("systemd-inhibit"));
    if (!systemdInhibit.isEmpty()) {
        startInhibitor(systemdInhibit,
                       {
                           QStringLiteral("--what=idle"),
                           QStringLiteral("--who=REX Player"),
                           QStringLiteral("--why=Video playback"),
                           QStringLiteral("--mode=block"),
                           QStringLiteral("sleep"),
                           QStringLiteral("infinity")
                       },
                       QStringLiteral("systemd-inhibit"));
    } else {
        const QString gnomeInhibit =
            QStandardPaths::findExecutable(QStringLiteral("gnome-session-inhibit"));
        if (!gnomeInhibit.isEmpty()) {
            startInhibitor(gnomeInhibit,
                           {
                               QStringLiteral("--app-id=rex-player"),
                               QStringLiteral("--reason=Video playback"),
                               QStringLiteral("--inhibit=idle"),
                               QStringLiteral("--inhibit-only")
                           },
                           QStringLiteral("gnome-session-inhibit"));
        } else {
            m_active = true;
            log(QStringLiteral("POWER INHIBIT: no supported inhibitor executable found"));
        }
    }
#else
    m_active = active;
    log(QStringLiteral("POWER INHIBIT: platform has no implementation; requested active=%1")
            .arg(active ? QStringLiteral("yes") : QStringLiteral("no")));
#endif
}
