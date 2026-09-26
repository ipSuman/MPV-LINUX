#pragma once

#include <QObject>
#include <QProcess>

class AudioExporter final : public QObject {
    Q_OBJECT
public:
    explicit AudioExporter(QObject* parent = nullptr);

    bool isRunning() const;
    bool start(const QString& ffmpegPath,
               const QString& sourcePath,
               const QString& mapSpecifier,
               const QString& outputPath,
               QString* errorMessage = nullptr);

signals:
    void logMessage(const QString& message);
    void finished(bool success,
                  const QString& outputPath,
                  const QString& errorMessage,
                  const QString& stderrText,
                  const QString& stdoutText,
                  int exitCode,
                  QProcess::ExitStatus exitStatus);

private:
    QProcess* m_process = nullptr;
    QString m_outputPath;
};
