#pragma once

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QString>
#include <QStringList>

class GamepadController final : public QObject {
    Q_OBJECT
public:
    explicit GamepadController(QObject* parent = nullptr);
    ~GamepadController() override;

    bool isAvailable() const;
    bool isEnabled() const;
    void setEnabled(bool enabled);
    QString deviceName() const;
    QStringList actions() const;
    QString binding(const QString& action) const;
    QString inputLabel(const QString& input) const;
    void setBinding(const QString& action, const QString& input);
    void clearBinding(const QString& action);
    void resetBindings();
    bool isCapturing() const;
    void beginCapture(const QString& action);
    void cancelCapture();

signals:
    void actionTriggered(const QString& action);
    void deviceChanged(const QString& name);
    void captureFinished(const QString& action, const QString& input);
    void statusMessage(const QString& message);

private slots:
    void poll();

private:
    void openFirstDevice();
    void closeDevice();
    void handleInput(const QString& input);

    QTimer m_timer;
    void* m_joystick = nullptr;
    void* m_controller = nullptr;
    QString m_deviceName;
    QMap<QString, QString> m_bindings;
    QString m_captureAction;
    QMap<QString, bool> m_axisDown;
    bool m_enabled = false;
    bool m_sdlReady = false;
};
