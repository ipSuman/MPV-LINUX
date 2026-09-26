#include "AudioExporter.h"

#include <QFile>
#include <QFileInfo>

AudioExporter::AudioExporter(QObject* parent)
    : QObject(parent),
      m_process(new QProcess(this)) {
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const QString stderrText =
            QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
        const QString stdoutText =
            QString::fromLocal8Bit(m_process->readAllStandardOutput()).trimmed();
        const QFileInfo outputInfo(m_outputPath);
        const bool success =
            exitStatus == QProcess::NormalExit &&
            exitCode == 0 &&
            outputInfo.exists() &&
            outputInfo.size() > 0;

        emit logMessage(QStringLiteral(
            "AUDIO SAVE: ffmpeg finished exit=%1 status=%2 outputExists=%3 outputSize=%4")
            .arg(exitCode)
            .arg(exitStatus == QProcess::NormalExit
                     ? QStringLiteral("normal")
                     : QStringLiteral("crashed"))
            .arg(outputInfo.exists() ? QStringLiteral("yes") : QStringLiteral("no"))
            .arg(outputInfo.exists() ? QString::number(outputInfo.size())
                                     : QStringLiteral("0")));

        if (!stderrText.isEmpty()) {
            emit logMessage(QStringLiteral("AUDIO SAVE: ffmpeg stderr: %1").arg(stderrText));
        }
        if (!stdoutText.isEmpty()) {
            emit logMessage(QStringLiteral("AUDIO SAVE: ffmpeg stdout: %1").arg(stdoutText));
        }

        const QString output = m_outputPath;
        m_outputPath.clear();
        emit finished(success, output,
                      success ? QString() : QStringLiteral("FFmpeg returned an error."),
                      stderrText, stdoutText, exitCode, exitStatus);
    });
}

bool AudioExporter::isRunning() const {
    return m_process->state() != QProcess::NotRunning;
}

bool AudioExporter::start(const QString& ffmpegPath,
                          const QString& sourcePath,
                          const QString& mapSpecifier,
                          const QString& outputPath,
                          QString* errorMessage) {
    if (isRunning()) {
        if (errorMessage) *errorMessage = QStringLiteral("An audio export is already in progress.");
        return false;
    }

    m_outputPath = outputPath;

    QStringList args;
    args << QStringLiteral("-hide_banner")
         << QStringLiteral("-nostdin")
         << QStringLiteral("-i") << sourcePath
         << QStringLiteral("-map") << mapSpecifier
         << QStringLiteral("-vn")
         << QStringLiteral("-sn")
         << QStringLiteral("-dn")
         << QStringLiteral("-c:a") << QStringLiteral("copy")
         << QStringLiteral("-y") << outputPath;

    emit logMessage(QStringLiteral("AUDIO SAVE: starting ffmpeg"));
    m_process->start(ffmpegPath, args);

    if (m_process->waitForStarted(1000)) return true;

    const QString error = m_process->errorString();
    emit logMessage(QStringLiteral("AUDIO SAVE: failed to start ffmpeg: %1").arg(error));
    m_outputPath.clear();
    if (errorMessage) *errorMessage = error;
    return false;
}
