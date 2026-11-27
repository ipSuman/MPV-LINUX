#include "MprisController.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QVariantMap>
#include <QDBusObjectPath>
#include <QWidget>
#include <utility>

namespace {
constexpr auto kService = "org.mpris.MediaPlayer2.rexplayer";
constexpr auto kObjectPath = "/org/mpris/MediaPlayer2";

class RootAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
public:
    explicit RootAdaptor(QObject* parent) : QDBusAbstractAdaptor(parent) {}
    QString identity() const { return QStringLiteral("REX Player"); }
    QString desktopEntry() const { return QStringLiteral("rex-player"); }
    bool canQuit() const { return false; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
public slots:
    void Raise() { if (parent()) { auto* w = qobject_cast<QWidget*>(parent()->parent()); if (w) { w->show(); w->raise(); w->activateWindow(); } } }
    void Quit() {}
};

class PlayerAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus)
    Q_PROPERTY(double Rate READ rate)
    Q_PROPERTY(bool Shuffle READ shuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext CONSTANT)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious CONSTANT)
    Q_PROPERTY(bool CanPlay READ canPlay CONSTANT)
    Q_PROPERTY(bool CanPause READ canPause CONSTANT)
    Q_PROPERTY(bool CanSeek READ canSeek CONSTANT)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)
public:
    PlayerAdaptor(QObject* parent, std::function<void()> playPause,
                  std::function<void()> next, std::function<void()> previous,
                  std::function<void()> stop, std::function<QString()> status)
        : QDBusAbstractAdaptor(parent), m_playPause(std::move(playPause)),
          m_next(std::move(next)), m_previous(std::move(previous)),
          m_stop(std::move(stop)), m_status(std::move(status)) {}
    QString playbackStatus() const { return m_status ? m_status() : QStringLiteral("Stopped"); }
    QString loopStatus() const { return QStringLiteral("None"); }
    double rate() const { return 1.0; }
    bool shuffle() const { return false; }
    QVariantMap metadata() const { return {}; }
    double volume() const { return 1.0; }
    qlonglong position() const { return 0; }
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    bool canGoNext() const { return true; }
    bool canGoPrevious() const { return true; }
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const { return false; }
    bool canControl() const { return true; }
public slots:
    void Play() { if (playbackStatus() != QStringLiteral("Playing") && m_playPause) m_playPause(); }
    void Pause() { if (playbackStatus() == QStringLiteral("Playing") && m_playPause) m_playPause(); }
    void PlayPause() { if (m_playPause) m_playPause(); }
    void Stop() { if (m_stop) m_stop(); }
    void Next() { if (m_next) m_next(); }
    void Previous() { if (m_previous) m_previous(); }
    void Seek(qlonglong) {}
    void SetPosition(const QDBusObjectPath&, qlonglong) {}
    void OpenUri(const QString&) {}
private:
    std::function<void()> m_playPause, m_next, m_previous, m_stop;
    std::function<QString()> m_status;
};
} // namespace

MprisController::MprisController(std::function<void()> playPause,
                                 std::function<void()> next,
                                 std::function<void()> previous,
                                 std::function<void()> stop,
                                 std::function<QString()> playbackStatus,
                                 QObject* parent)
    : QObject(parent), m_playbackStatus(std::move(playbackStatus)) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return;
    new RootAdaptor(this);
    new PlayerAdaptor(this, std::move(playPause), std::move(next),
                      std::move(previous), std::move(stop), m_playbackStatus);
    m_registered = bus.registerService(QString::fromLatin1(kService));
    if (!m_registered) return;
    if (!bus.registerObject(QString::fromLatin1(kObjectPath), this,
                            QDBusConnection::ExportAdaptors)) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_registered = false;
    }
}

MprisController::~MprisController() {
    if (m_registered) {
        auto bus = QDBusConnection::sessionBus();
        bus.unregisterObject(QString::fromLatin1(kObjectPath));
        bus.unregisterService(QString::fromLatin1(kService));
    }
}

#include "MprisController.moc"
