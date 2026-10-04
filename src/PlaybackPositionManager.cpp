#include "PlaybackPositionManager.h"

#include <QCryptographicHash>
#include <QSettings>

PlaybackPositionManager::PlaybackPositionManager(QObject* parent)
    : QObject(parent) {
}

QString PlaybackPositionManager::keyFor(const QString& path) const {
    const QByteArray digest =
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("playback/positions/%1").arg(QString::fromLatin1(digest));
}

void PlaybackPositionManager::save(const QString& path, double position) {
    if (path.isEmpty()) return;

    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    settings.setValue(keyFor(path), position);
    settings.sync();
}

double PlaybackPositionManager::load(const QString& path) const {
    if (path.isEmpty()) return 0.0;

    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    return settings.value(keyFor(path), 0.0).toDouble();
}
