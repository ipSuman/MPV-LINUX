#include "PlaylistController.h"

#include <QFileInfo>
#include <QSettings>

PlaylistController::PlaylistController() {
    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    m_autoplay = settings.value(QStringLiteral("playlist/autoplay"), true).toBool();
    m_loop = settings.value(QStringLiteral("playlist/loop"), false).toBool();
}

bool PlaylistController::addPath(const QString& path) {
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (absolute.isEmpty() || indexOf(absolute) >= 0) return false;
    m_paths.append(absolute);
    if (m_currentIndex < 0) m_currentIndex = 0;
    return true;
}

void PlaylistController::clear() {
    m_paths.clear();
    m_currentIndex = -1;
}

bool PlaylistController::removeAt(int index) {
    if (index < 0 || index >= m_paths.size()) return false;
    m_paths.removeAt(index);
    if (m_paths.isEmpty()) {
        m_currentIndex = -1;
    } else if (m_currentIndex > index) {
        --m_currentIndex;
    } else if (m_currentIndex == index) {
        m_currentIndex = index < m_paths.size() ? index : m_paths.size() - 1;
    }
    return true;
}

int PlaylistController::count() const {
    return m_paths.size();
}

int PlaylistController::currentIndex() const {
    return m_currentIndex;
}

void PlaylistController::setCurrentIndex(int index) {
    m_currentIndex = (index >= 0 && index < m_paths.size()) ? index : -1;
}

int PlaylistController::indexOf(const QString& path) const {
    const QString absolute = QFileInfo(path).absoluteFilePath();
    return m_paths.indexOf(absolute);
}

QString PlaylistController::pathAt(int index) const {
    return (index >= 0 && index < m_paths.size()) ? m_paths.at(index) : QString();
}

int PlaylistController::previousIndex() const {
    return m_currentIndex > 0 ? m_currentIndex - 1 : -1;
}

int PlaylistController::nextIndex() const {
    return m_currentIndex >= 0 && m_currentIndex + 1 < m_paths.size()
        ? m_currentIndex + 1
        : -1;
}

void PlaylistController::setAutoplay(bool enabled) {
    m_autoplay = enabled;
    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    settings.setValue(QStringLiteral("playlist/autoplay"), enabled);
    settings.sync();
}

void PlaylistController::setLoop(bool enabled) {
    m_loop = enabled;
    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    settings.setValue(QStringLiteral("playlist/loop"), enabled);
    settings.sync();
}
