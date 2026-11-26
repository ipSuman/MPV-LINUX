#include "GamepadController.h"

#include <QSettings>
#include <QMetaObject>
#include <SDL.h>

#include <algorithm>

namespace {
const QStringList kActions = {
    QStringLiteral("play_pause"), QStringLiteral("stop"),
    QStringLiteral("previous"), QStringLiteral("next"),
    QStringLiteral("seek_back"), QStringLiteral("seek_forward"),
    QStringLiteral("volume_up"), QStringLiteral("volume_down"),
    QStringLiteral("mute"), QStringLiteral("fullscreen"),
    QStringLiteral("playlist"), QStringLiteral("screenshot"),
    QStringLiteral("frame_back"), QStringLiteral("frame_forward"),
    QStringLiteral("speed_up"), QStringLiteral("speed_down"),
    QStringLiteral("subtitles"), QStringLiteral("loop_start"),
    QStringLiteral("loop_end"), QStringLiteral("loop_clear"),
    QStringLiteral("zoom_in"), QStringLiteral("zoom_out"),
    QStringLiteral("zoom_reset"), QStringLiteral("rotate"),
    QStringLiteral("subtitle_up"), QStringLiteral("subtitle_down")
};
}

GamepadController::GamepadController(QObject* parent) : QObject(parent) {
    m_sdlReady = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) == 0;
    if (m_sdlReady) SDL_JoystickEventState(SDL_ENABLE);
    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    m_enabled = settings.value(QStringLiteral("gamepad/enabled"), false).toBool();
    const auto saved = settings.value(QStringLiteral("gamepad/bindings")).toMap();
    for (auto it = saved.cbegin(); it != saved.cend(); ++it)
        m_bindings.insert(it.key(), it.value().toString());
    if (m_bindings.isEmpty()) resetBindings();
    if (m_enabled && m_sdlReady) openFirstDevice();
    m_timer.setInterval(8);
    connect(&m_timer, &QTimer::timeout, this, &GamepadController::poll);
    if (m_sdlReady) m_timer.start();
}

GamepadController::~GamepadController() {
    m_timer.stop();
    closeDevice();
    if (m_sdlReady) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS);
}

bool GamepadController::isAvailable() const { return m_sdlReady; }
bool GamepadController::isEnabled() const { return m_enabled; }
QString GamepadController::deviceName() const { return m_deviceName; }
QStringList GamepadController::actions() const { return kActions; }
QString GamepadController::binding(const QString& action) const { return m_bindings.value(action); }
bool GamepadController::isCapturing() const { return !m_captureAction.isEmpty(); }

void GamepadController::setEnabled(bool enabled) {
    m_enabled = enabled && m_sdlReady;
    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    settings.setValue(QStringLiteral("gamepad/enabled"), m_enabled);
    if (m_enabled) openFirstDevice(); else closeDevice();
    emit statusMessage(m_sdlReady ? (m_enabled ? QStringLiteral("Gamepad support enabled") : QStringLiteral("Gamepad support disabled"))
                                  : QStringLiteral("Gamepad support is unavailable in this build"));
}

void GamepadController::setBinding(const QString& action, const QString& input) {
    if (!kActions.contains(action)) return;
    if (input.isEmpty()) m_bindings.remove(action); else m_bindings[action] = input;
    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    settings.setValue(QStringLiteral("gamepad/bindings"), QVariant::fromValue(m_bindings));
}

void GamepadController::clearBinding(const QString& action) { setBinding(action, QString()); }

void GamepadController::resetBindings() {
    m_bindings.clear();
    // Standard SDL GameController button identifiers; unrecognised controllers
    // remain fully usable through the raw-input capture interface.
    const QList<QPair<QString, QString>> defaults = {
        {QStringLiteral("play_pause"), QStringLiteral("gc:0")},
        {QStringLiteral("stop"), QStringLiteral("gc:1")},
        {QStringLiteral("previous"), QStringLiteral("gc:2")},
        {QStringLiteral("next"), QStringLiteral("gc:3")},
        {QStringLiteral("seek_back"), QStringLiteral("gc:13")},
        {QStringLiteral("seek_forward"), QStringLiteral("gc:14")},
        {QStringLiteral("volume_down"), QStringLiteral("gc:11")},
        {QStringLiteral("volume_up"), QStringLiteral("gc:12")},
        {QStringLiteral("fullscreen"), QStringLiteral("gc:6")},
        {QStringLiteral("playlist"), QStringLiteral("gc:4")}
    };
    for (const auto& item : defaults) m_bindings.insert(item.first, item.second);
    QSettings settings(QStringLiteral("REX Player"), QStringLiteral("REX Player"));
    settings.setValue(QStringLiteral("gamepad/bindings"), QVariant::fromValue(m_bindings));
}

void GamepadController::beginCapture(const QString& action) {
    if (!kActions.contains(action)) return;
    m_captureAction = action;
    emit statusMessage(QStringLiteral("Press a gamepad button, D-pad direction or stick direction…"));
}
void GamepadController::cancelCapture() { m_captureAction.clear(); }

void GamepadController::openFirstDevice() {
    if (!m_enabled || !m_sdlReady || m_joystick) return;
    SDL_JoystickUpdate();
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* controller = SDL_GameControllerOpen(i);
            if (controller) {
                m_controller = controller;
                m_joystick = SDL_GameControllerGetJoystick(controller);
                m_deviceName = QString::fromUtf8(SDL_GameControllerName(controller));
                break;
            }
        } else {
            SDL_Joystick* joystick = SDL_JoystickOpen(i);
            if (joystick) {
                m_joystick = joystick;
                m_deviceName = QString::fromUtf8(SDL_JoystickName(joystick));
                break;
            }
        }
    }
    emit deviceChanged(m_deviceName.isEmpty() ? QStringLiteral("No controller connected") : m_deviceName);
}

void GamepadController::closeDevice() {
    if (m_controller) SDL_GameControllerClose(static_cast<SDL_GameController*>(m_controller));
    else if (m_joystick) SDL_JoystickClose(static_cast<SDL_Joystick*>(m_joystick));
    m_controller = nullptr;
    m_joystick = nullptr;
    m_deviceName.clear();
    m_axisDown.clear();
    emit deviceChanged(QStringLiteral("No controller connected"));
}

QString GamepadController::describeInput(const QString& input) const {
    if (input.startsWith(QStringLiteral("gc:"))) {
        const int id = input.mid(3).toInt();
        static const QStringList names = {QStringLiteral("A"),QStringLiteral("B"),QStringLiteral("X"),QStringLiteral("Y"),
            QStringLiteral("Back"),QStringLiteral("Guide"),QStringLiteral("Start"),QStringLiteral("Left stick"),
            QStringLiteral("Right stick"),QStringLiteral("Left shoulder"),QStringLiteral("Right shoulder"),
            QStringLiteral("D-pad up"),QStringLiteral("D-pad down"),QStringLiteral("D-pad left"),QStringLiteral("D-pad right")};
        return id >= 0 && id < names.size() ? names[id] : input;
    }
    if (input.startsWith(QStringLiteral("b:"))) return QStringLiteral("Button %1").arg(input.mid(2));
    if (input.startsWith(QStringLiteral("h:"))) {
        const QStringList p = input.split(':');
        if (p.size() == 3) return QStringLiteral("Hat %1 direction %2").arg(p[1], p[2]);
    }
    if (input.startsWith(QStringLiteral("a:"))) return QStringLiteral("Axis %1 %2").arg(input.mid(2));
    return input;
}

void GamepadController::handleInput(const QString& input) {
    if (!m_captureAction.isEmpty()) {
        const QString action = m_captureAction;
        m_captureAction.clear();
        setBinding(action, input);
        emit captureFinished(action, input);
        return;
    }
    if (!m_enabled) return;
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it)
        if (it.value() == input) emit actionTriggered(it.key());
}

void GamepadController::poll() {
    if (!m_sdlReady || !m_enabled) return;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_JOYDEVICEADDED || event.type == SDL_CONTROLLERDEVICEADDED) {
            if (!m_joystick) openFirstDevice();
        } else if (event.type == SDL_JOYDEVICEREMOVED || event.type == SDL_CONTROLLERDEVICEREMOVED) {
            const SDL_JoystickID removed = event.type == SDL_JOYDEVICEREMOVED ? event.jdevice.which : event.cdevice.which;
            if (m_joystick && SDL_JoystickInstanceID(static_cast<SDL_Joystick*>(m_joystick)) == removed) {
                closeDevice();
                openFirstDevice();
            }
        } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            handleInput(QStringLiteral("gc:%1").arg(static_cast<int>(event.cbutton.button)));
        } else if (event.type == SDL_JOYBUTTONDOWN) {
            if (!m_controller) handleInput(QStringLiteral("b:%1").arg(event.jbutton.button));
        } else if (event.type == SDL_JOYHATMOTION && !m_controller && event.jhat.value) {
            handleInput(QStringLiteral("h:%1:%2").arg(event.jhat.hat).arg(event.jhat.value));
        } else if (event.type == SDL_JOYAXISMOTION && !m_controller) {
            const int value = event.jaxis.value;
            const QString base = QStringLiteral("a:%1").arg(event.jaxis.axis);
            const QString positive = base + QStringLiteral("+");
            const QString negative = base + QStringLiteral("-");
            const bool pos = value > 18000;
            const bool neg = value < -18000;
            if (pos && !m_axisDown.value(positive)) handleInput(positive);
            if (neg && !m_axisDown.value(negative)) handleInput(negative);
            m_axisDown[positive] = pos;
            m_axisDown[negative] = neg;
        }
    }
    if (!m_joystick && m_enabled) openFirstDevice();
}
