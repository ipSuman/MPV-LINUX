#pragma once

#include <QObject>
#include <functional>

class QWidget;
struct mpv_handle;

class DisplayController final : public QObject {
    Q_OBJECT
public:
    struct Values {
        int saturation = 0;
        int brightness = 0;
        int contrast = 0;
    };

    DisplayController(mpv_handle* mpv, QWidget* parentWidget, Values* values, const std::function<void(const char*, double)>& setProperty, QObject* parent = nullptr);

    void showDialog();

signals:
    void logMessage(const QString& message);

private:
    QWidget* m_parentWidget = nullptr;
    Values* m_values = nullptr;
    std::function<void(const char*, double)> m_setProperty;
};
