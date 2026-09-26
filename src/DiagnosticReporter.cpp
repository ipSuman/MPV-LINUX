#include "DiagnosticReporter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QSysInfo>
#include <QTextStream>
#include <QWidget>

#include "MpvNodeUtils.h"
#include "PlaylistController.h"
#include "RuntimeLogger.h"

#include <mpv/client.h>

DiagnosticReporter::DiagnosticReporter(mpv_handle* mpv, RuntimeLogger* logger,
                                       PlaylistController* playlistController,
                                       QWidget* parent)
    : QObject(parent),
      m_mpv(mpv),
      m_logger(logger),
      m_playlistController(playlistController),
      m_parentWidget(parent) {}

void DiagnosticReporter::log(const QString& message) const {
    if (m_logger) m_logger->append(message);
}

void DiagnosticReporter::saveReport() {
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString defaultName =
        QDir::home().filePath(QStringLiteral("REX_Player_Log_%1.txt").arg(timestamp));

    log(QStringLiteral("DIAGNOSTIC REPORT: opening save dialog"));
    const QString path = QFileDialog::getSaveFileName(
        m_parentWidget, QStringLiteral("Save REX Player log report"), defaultName,
        QStringLiteral("Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        log(QStringLiteral("DIAGNOSTIC REPORT: user cancelled"));
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        log(QStringLiteral("DIAGNOSTIC REPORT: failed to open output: %1")
                .arg(file.errorString()));
        QMessageBox::warning(
            m_parentWidget, QStringLiteral("Save Log"),
            QStringLiteral("Could not write the log report:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << "REX Player - Diagnostic Log Report\n";
    out << "=================================\n\n";
    out << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "Application: " << QCoreApplication::applicationName() << " "
        << QCoreApplication::applicationVersion() << "\n";
    out << "Qt: " << QT_VERSION_STR << "\n";
    out << "OS: " << QSysInfo::prettyProductName() << "\n";
    out << "Kernel: " << QSysInfo::kernelType() << " "
        << QSysInfo::kernelVersion() << "\n";
    out << "CPU architecture: " << QSysInfo::currentCpuArchitecture() << "\n";
    out << "Build ABI: " << QSysInfo::buildAbi() << "\n";
    out << "Host name: " << QSysInfo::machineHostName() << "\n";
    out << "LC_NUMERIC: " << qgetenv("LC_NUMERIC") << "\n";
    out << "QT_QPA_PLATFORM: " << qgetenv("QT_QPA_PLATFORM") << "\n";
    out << "WAYLAND_DISPLAY: " << qgetenv("WAYLAND_DISPLAY") << "\n";
    out << "DISPLAY: " << qgetenv("DISPLAY") << "\n\n";

    auto writeString = [&out, this](const char* key, const char* label) {
        if (!m_mpv) return;
        const QString value = [&] {
            char* raw = nullptr;
            if (mpv_get_property(m_mpv, key, MPV_FORMAT_STRING, &raw) < 0 || !raw)
                return QString();
            const QString result = QString::fromUtf8(raw);
            mpv_free(raw);
            return result;
        }();
        if (!value.isEmpty()) out << label << ": " << value << "\n";
    };

    auto writeDouble = [&out, this](const char* key, const char* label) {
        if (!m_mpv) return;
        double value = 0.0;
        if (mpv_get_property(m_mpv, key, MPV_FORMAT_DOUBLE, &value) >= 0 &&
            std::isfinite(value)) {
            out << label << ": " << QString::number(value, 'g', 12) << "\n";
        }
    };

    out << "Playback / Media\n----------------\n";
    writeString("path", "Path");
    writeString("filename", "Filename");
    writeString("media-title", "Media title");
    writeString("file-format", "Container");
    writeDouble("duration", "Duration (s)");
    writeDouble("bitrate", "Overall bitrate");
    writeString("video-codec", "Video codec");
    writeString("video-format", "Video format");
    writeDouble("video-params/w", "Video width");
    writeDouble("video-params/h", "Video height");
    writeDouble("container-fps", "Container FPS");
    writeDouble("video-bitrate", "Video bitrate");
    writeString("video-params/pixelformat", "Pixel format");
    writeString("video-params/chroma-location", "Chroma location");
    writeString("video-params/colormatrix", "Color matrix");
    writeString("video-params/primaries", "Color primaries");
    writeString("video-params/transfer", "Color transfer");
    writeDouble("video-params/rotate", "Rotation");
    writeString("audio-codec", "Audio codec");
    writeString("audio-format", "Audio format");
    writeDouble("audio-samplerate", "Audio sample rate");
    writeString("audio-channels", "Audio channels");
    writeString("audio-channel-layout", "Audio channel layout");
    writeDouble("audio-bitrate", "Audio bitrate");
    writeString("hwdec", "HW decoder setting");
    writeString("hwdec-current", "Active HW decoder");
    writeString("vo", "Video output");
    writeString("gpu-api", "GPU API");
    writeDouble("time-pos", "Position (s)");
    writeDouble("speed", "Speed");
    writeDouble("video-zoom", "Video zoom");
    writeDouble("saturation", "Saturation");
    writeDouble("brightness", "Brightness");
    writeDouble("contrast", "Contrast");
    writeDouble("video-pan-x", "Video pan X");
    writeDouble("video-pan-y", "Video pan Y");
    writeString("pause", "Paused");

    out << "\nTracks\n------\n";
    mpv_node tracks{};
    if (m_mpv && mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &tracks) >= 0 &&
        tracks.format == MPV_FORMAT_NODE_ARRAY && tracks.u.list) {
        for (int i = 0; i < tracks.u.list->num; ++i) {
            const mpv_node& track = tracks.u.list->values[i];
            if (track.format != MPV_FORMAT_NODE_MAP || !track.u.list) continue;
            out << "Track " << (i + 1)
                << ": type=" << MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "type"))
                << ", id=" << MpvNodeUtils::nodeInt(MpvNodeUtils::mapValue(track.u.list, "id"))
                << ", lang=" << MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "lang"))
                << ", title=" << MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "title"))
                << ", codec=" << MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "codec"))
                << ", external=" << MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "external-filename"))
                << ", selected=" << (MpvNodeUtils::nodeFlag(MpvNodeUtils::mapValue(track.u.list, "selected")) ? "yes" : "no")
                << "\n";
        }
        mpv_free_node_contents(&tracks);
    } else {
        out << "Unable to read track-list.\n";
    }

    out << "\nA-B / Controls\n--------------\n";
    out << "A-B start: " << "managed by MainWindow" << "\n";
    out << "Diagnostic reporter captures mpv/runtime information only.\n";
    out << "Playlist count: " << (m_playlistController ? m_playlistController->count() : 0) << "\n";
    out << "Autoplay next item: "
        << ((m_playlistController && m_playlistController->autoplay()) ? "enabled" : "disabled") << "\n";
    out << "Loop playlist: "
        << ((m_playlistController && m_playlistController->loop()) ? "enabled" : "disabled") << "\n";

    out << "\nEnd of report\n";
    file.close();

    log(QStringLiteral("DIAGNOSTIC REPORT: saved to %1").arg(path));
    QMessageBox::information(
        m_parentWidget, QStringLiteral("Log saved"),
        QStringLiteral("Diagnostic report saved to:\n%1").arg(path));
}
