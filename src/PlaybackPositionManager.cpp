#include "PlaybackPositionManager.h"

#include <QCryptographicHash>
#include <QSettings>

PlaybackPositionManager::PlaybackPositionManager(QObject* parent)
    : QObject(parent),
      m_settings(QStringLiteral("REX Player"), QStringLiteral("REX Player")) {
}

QString PlaybackPositionManager::keyFor(const QString& path) const {
    const QByteArray digest =
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("playback/positions/%1").arg(QString::fromLatin1(digest));
}

void PlaybackPositionManager::save(const QString& path, double position) {
    if (path.isEmpty()) return;

    m_settings.setValue(keyFor(path), position);
}

double PlaybackPositionManager::load(const QString& path) const {
    if (path.isEmpty()) return 0.0;

    return m_settings.value(keyFor(path), 0.0).toDouble();
}
