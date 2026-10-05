#include "RuntimeLogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QStringConverter>
#include <utility>

RuntimeLogger::RuntimeLogger(QObject* parent)
    : QObject(parent) {
}

bool RuntimeLogger::initialize() {
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) return false;

    if (!QDir().mkpath(base)) return false;

    m_path = QDir(base).filePath(QStringLiteral("REX_Player_Runtime.log"));

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "\n===== REX Player session started "
        << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
        << " =====\n";
    out << "Application: " << QCoreApplication::applicationName()
        << " " << QCoreApplication::applicationVersion() << "\n";
    out << "OS: " << QSysInfo::prettyProductName()
        << " | Kernel: " << QSysInfo::kernelVersion()
        << " | CPU: " << QSysInfo::currentCpuArchitecture() << "\n";
    out.flush();

    m_flushTimer.setInterval(500);
    m_flushTimer.setSingleShot(false);
    QObject::connect(&m_flushTimer, &QTimer::timeout, this, &RuntimeLogger::flush);
    m_flushTimer.start();
    return true;
}

RuntimeLogger::~RuntimeLogger() {
    m_flushTimer.stop();
    flush();
}

void RuntimeLogger::append(const QString& message) {
    if (m_path.isEmpty()) return;

    m_pending.append(
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
        + QStringLiteral(" | ") + message + QLatin1Char('\n'));

    // Keep the hot path memory-only. Disk I/O is batched by the timer.
    if (m_pending.size() >= 128) flush();
}

void RuntimeLogger::flush() {
    if (m_path.isEmpty() || m_pending.isEmpty()) return;

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) return;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (const QString& line : std::as_const(m_pending)) out << line;
    out.flush();
    m_pending.clear();
}

QString RuntimeLogger::path() const {
    return m_path;
}

QString RuntimeLogger::contents() const {
    if (m_path.isEmpty()) return {};

    QFile file(m_path);
    QString result;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        result = QString::fromUtf8(file.readAll());

    // Include buffered entries in reports without forcing a disk flush on the
    // playback/UI thread.
    for (const QString& line : m_pending) result += line;
    return result;
}
