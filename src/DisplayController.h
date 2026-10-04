#pragma once

#include <QObject>
#include <QString>
#include <functional>

class QWidget;
struct mpv_handle;

class DisplayController final : public QObject {
    Q_OBJECT
public:
    DisplayController(QWidget* parentWidget, int* saturation, int* brightness, int* contrast,
                      const std::function<void(const char*, double)>& setProperty,
                      QObject* parent = nullptr);

    void showDialog();

signals:
    void logMessage(const QString& message);

private:
    QWidget* m_parentWidget = nullptr;
    int* m_saturation = nullptr;
    int* m_brightness = nullptr;
    int* m_contrast = nullptr;
    std::function<void(const char*, double)> m_setProperty;
};
