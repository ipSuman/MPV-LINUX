#pragma once

#include <QObject>
#include <QString>
#include <QSettings>

class PlaybackPositionManager final : public QObject {
    Q_OBJECT
public:
    explicit PlaybackPositionManager(QObject* parent = nullptr);

    void save(const QString& path, double position);
    double load(const QString& path) const;

private:
    QString keyFor(const QString& path) const;

    QSettings m_settings;
};
