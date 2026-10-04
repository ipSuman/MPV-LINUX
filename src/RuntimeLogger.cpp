#include "RuntimeLogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>

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
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
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
    return true;
}

void RuntimeLogger::append(const QString& message) {
    if (m_path.isEmpty()) return;

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) return;

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
        << " | " << message << "\n";
    out.flush();
    file.flush();
}

QString RuntimeLogger::path() const {
    return m_path;
}

QString RuntimeLogger::contents() const {
    if (m_path.isEmpty()) return {};
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}
