#include "TrackController.h"

#include <QAction>
#include <QCursor>
#include <QFileInfo>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QStringList>
#include <QWidget>
#include <utility>

#include <cstdint>

#include "MpvNodeUtils.h"
#include "RuntimeLogger.h"

#include <mpv/client.h>

TrackController::TrackController(mpv_handle* mpv, RuntimeLogger* logger, QObject* parent)
    : QObject(parent), m_mpv(mpv), m_logger(logger) {}

void TrackController::setSaveAudioCallback(std::function<void()> callback) {
    m_saveAudioCallback = std::move(callback);
}

void TrackController::log(const QString& message) const {
    if (m_logger) m_logger->append(message);
}

void TrackController::showMenu(QWidget* anchor) {
    if (!m_mpv) return;

    mpv_node tracks{};
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &tracks) < 0 ||
        tracks.format != MPV_FORMAT_NODE_ARRAY || !tracks.u.list) {
        mpv_free_node_contents(&tracks);
        log(QStringLiteral("TRACK MENU: could not read track-list"));
        return;
    }

    auto* menu = new QMenu(anchor);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    auto* audioMenu = menu->addMenu(QStringLiteral("Audio"));
    auto* subtitleMenu = menu->addMenu(QStringLiteral("Subtitles"));

    auto* saveAudioAction = menu->addAction(QStringLiteral("Save Audio"));
    saveAudioAction->setEnabled(static_cast<bool>(m_saveAudioCallback));
    connect(saveAudioAction, &QAction::triggered, this, [this] {
        if (m_saveAudioCallback) m_saveAudioCallback();
    });

    const mpv_node_list* list = tracks.u.list;
    bool hasAudio = false;
    bool hasSubtitles = false;

    auto addTrack = [this](QMenu* target, const QString& label, int id, bool selected, const char* property) {
        auto* action = target->addAction(label);
        action->setCheckable(true);
        action->setChecked(selected);
        connect(action, &QAction::triggered, this, [this, id, property] {
            if (id < 0) {
                static char noTrack[] = "no";
                char* value = noTrack;
                const int result = mpv_set_property_async(
                    m_mpv, 0, property, MPV_FORMAT_STRING, &value);
                log(QStringLiteral("TRACK SELECT: %1=no result=%2")
                        .arg(QString::fromUtf8(property)).arg(result));
            } else {
                int64_t value = id;
                const int result = mpv_set_property_async(
                    m_mpv, 0, property, MPV_FORMAT_INT64, &value);
                log(QStringLiteral("TRACK SELECT: %1=%2 result=%3")
                        .arg(QString::fromUtf8(property)).arg(id).arg(result));
            }
        });
    };

    addTrack(audioMenu, QStringLiteral("Auto"), -1, false, "aid");

    mpv_node aid{};
    if (mpv_get_property(m_mpv, "aid", MPV_FORMAT_NODE, &aid) >= 0 &&
        aid.format == MPV_FORMAT_INT64) {
        audioMenu->actions().first()->setChecked(aid.u.int64 < 0);
    }
    mpv_free_node_contents(&aid);

    auto* autoSubtitle = subtitleMenu->addAction(QStringLiteral("Off"));
    autoSubtitle->setCheckable(true);

    mpv_node sid{};
    if (mpv_get_property(m_mpv, "sid", MPV_FORMAT_NODE, &sid) >= 0 &&
        sid.format == MPV_FORMAT_INT64) {
        autoSubtitle->setChecked(sid.u.int64 < 0);
    }
    mpv_free_node_contents(&sid);

    connect(autoSubtitle, &QAction::triggered, this, [this] {
        static char noSubtitle[] = "no";
        char* value = noSubtitle;
        const int result = mpv_set_property_async(
            m_mpv, 0, "sid", MPV_FORMAT_STRING, &value);
        log(QStringLiteral("TRACK SELECT: sid=no result=%1").arg(result));
    });

    for (int i = 0; i < list->num; ++i) {
        const mpv_node& track = list->values[i];
        if (track.format != MPV_FORMAT_NODE_MAP || !track.u.list) continue;

        const QString type =
            MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "type"));
        const int id =
            MpvNodeUtils::nodeInt(MpvNodeUtils::mapValue(track.u.list, "id"));
        if (id < 0) continue;

        const QString lang =
            MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "lang"));
        const QString title =
            MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "title"));
        const QString external =
            MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "external-filename"));
        const bool selected =
            MpvNodeUtils::nodeFlag(MpvNodeUtils::mapValue(track.u.list, "selected"));

        QString label = title;
        if (label.isEmpty()) label = lang;
        if (label.isEmpty() && !external.isEmpty()) label = QFileInfo(external).fileName();
        if (label.isEmpty()) label = QStringLiteral("Track %1").arg(id);
        if (!lang.isEmpty() && title != lang) label += QStringLiteral(" (%1)").arg(lang);

        if (type == QStringLiteral("audio")) {
            addTrack(audioMenu, label, id, selected, "aid");
            hasAudio = true;
        } else if (type == QStringLiteral("sub")) {
            auto* action = subtitleMenu->addAction(label);
            action->setCheckable(true);
            action->setChecked(selected);
            connect(action, &QAction::triggered, this, [this, id] {
                int64_t value = id;
                const int result = mpv_set_property_async(
                    m_mpv, 0, "sid", MPV_FORMAT_INT64, &value);
                log(QStringLiteral("TRACK SELECT: sid=%1 result=%2").arg(id).arg(result));
            });
            hasSubtitles = true;
        }
    }

    audioMenu->setEnabled(hasAudio);
    subtitleMenu->setEnabled(hasSubtitles || subtitleMenu->actions().size() > 0);

    if (anchor) {
        menu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
    } else {
        menu->popup(QCursor::pos());
    }

    mpv_free_node_contents(&tracks);
}

void TrackController::cycleSubtitles() {
    if (!m_mpv) return;

    QList<int> subtitleIds;
    mpv_node tracks{};
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &tracks) >= 0 &&
        tracks.format == MPV_FORMAT_NODE_ARRAY && tracks.u.list) {
        for (int i = 0; i < tracks.u.list->num; ++i) {
            const mpv_node& track = tracks.u.list->values[i];
            if (track.format != MPV_FORMAT_NODE_MAP || !track.u.list) continue;
            const QString type =
                MpvNodeUtils::nodeString(MpvNodeUtils::mapValue(track.u.list, "type"));
            if (type != QStringLiteral("sub")) continue;
            const int id =
                MpvNodeUtils::nodeInt(MpvNodeUtils::mapValue(track.u.list, "id"));
            if (id >= 0) subtitleIds.append(id);
        }
    }
    mpv_free_node_contents(&tracks);

    char* sidValue = nullptr;
    QString currentSid = QStringLiteral("no");
    if (mpv_get_property(m_mpv, "sid", MPV_FORMAT_STRING, &sidValue) >= 0) {
        if (sidValue) currentSid = QString::fromUtf8(sidValue);
        mpv_free(sidValue);
    }

    QString nextSid;
    if (currentSid == QStringLiteral("no") || currentSid == QStringLiteral("auto")) {
        nextSid = subtitleIds.isEmpty()
            ? QStringLiteral("no")
            : QString::number(subtitleIds.first());
    } else {
        bool ok = false;
        const int currentId = currentSid.toInt(&ok);
        int nextIndex = -1;
        if (ok) nextIndex = subtitleIds.indexOf(currentId) + 1;
        nextSid = (nextIndex >= 0 && nextIndex < subtitleIds.size())
            ? QString::number(subtitleIds.at(nextIndex))
            : QStringLiteral("no");
    }

    const QByteArray encoded = nextSid.toUtf8();
    const char* args[] = {"set", "sid", encoded.constData(), nullptr};
    const int result = mpv_command_async(m_mpv, 0, args);
    log(QStringLiteral("SUBTITLE CYCLE: current=%1 next=%2 result=%3")
            .arg(currentSid).arg(nextSid).arg(result));
}
