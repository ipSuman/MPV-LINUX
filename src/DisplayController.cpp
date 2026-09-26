#include "DisplayController.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

DisplayController::DisplayController(
    QWidget* parentWidget,
    Values* values,
    const std::function<void(const char*, double)>& setProperty,
    QObject* parent)
    : QObject(parent),
      m_parentWidget(parentWidget),
      m_values(values),
      m_setProperty(setProperty) {}

void DisplayController::showDialog() {
    if (!m_parentWidget || !m_values) return;

    QDialog dialog(m_parentWidget);
    dialog.setWindowTitle(QStringLiteral("Display"));
    dialog.setModal(true);
    dialog.resize(460, 280);

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const int currentSaturation = std::clamp(m_values->saturation, -100, 100);
    const int currentBrightness = std::clamp(m_values->brightness, -100, 100);
    const int currentContrast = std::clamp(m_values->contrast, -100, 100);

    auto makeSlider = [&](const QString& name, int value, const char* property,
                          int* storedValue, const char* settingKey) {
        auto* row = new QWidget(&dialog);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(-100, 100);
        slider->setValue(value);
        auto* valueLabel = new QLabel(QString::number(value), row);
        valueLabel->setMinimumWidth(42);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(valueLabel);

        connect(slider, &QSlider::valueChanged, &dialog,
                [this, property, storedValue, settingKey, valueLabel](int v) {
                    valueLabel->setText(QString::number(v));
                    *storedValue = v;
                    if (m_setProperty) m_setProperty(property, v);

                    QSettings settings(QStringLiteral("REX Player"),
                                        QStringLiteral("REX Player"));
                    settings.setValue(QString::fromUtf8(settingKey), v);
                    settings.sync();
                    emit logMessage(QStringLiteral(
                        "DISPLAY: %1=%2").arg(QString::fromUtf8(property)).arg(v));
                });

        form->addRow(name, row);
        return slider;
    };

    auto* saturation = makeSlider(
        QStringLiteral("Saturation"), currentSaturation, "saturation",
        &m_values->saturation, "display/saturation");
    auto* brightness = makeSlider(
        QStringLiteral("Brightness"), currentBrightness, "brightness",
        &m_values->brightness, "display/brightness");
    auto* contrast = makeSlider(
        QStringLiteral("Contrast"), currentContrast, "contrast",
        &m_values->contrast, "display/contrast");

    layout->addLayout(form);

    auto* note = new QLabel(
        QStringLiteral("Range: −100 to +100. Changes are applied and remembered immediately."),
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* reset = buttons->addButton(
        QStringLiteral("Reset defaults"), QDialogButtonBox::ResetRole);
    layout->addWidget(buttons);

    connect(reset, &QPushButton::clicked, &dialog, [&] {
        saturation->setValue(0);
        brightness->setValue(0);
        contrast->setValue(0);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);

    dialog.adjustSize();
    const int margin = 16;
    const QSize size = dialog.size();
    const QPoint global = m_parentWidget->mapToGlobal(QPoint(
        std::max(margin, m_parentWidget->width() - size.width() - margin),
        std::max(margin, m_parentWidget->height() - size.height() - margin)));
    dialog.move(global);
    dialog.exec();
}
