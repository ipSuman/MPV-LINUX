#pragma once

#include <QString>
#include <QStringList>

class PlaylistController {
public:
    PlaylistController();

    bool addPath(const QString& path);
    void clear();
    bool removeAt(int index);

    int count() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    int indexOf(const QString& path) const;
    QString pathAt(int index) const;

    int previousIndex() const;
    int nextIndex() const;

    bool autoplay() const { return m_autoplay; }
    bool loop() const { return m_loop; }
    void setAutoplay(bool enabled);
    void setLoop(bool enabled);

private:
    QStringList m_paths;
    int m_currentIndex = -1;
    bool m_autoplay = true;
    bool m_loop = false;
};
