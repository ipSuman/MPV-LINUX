#pragma once

#include <QObject>
#include <QTimer>
#include <QStringList>

class RuntimeLogger final : public QObject {
    Q_OBJECT
public:
    explicit RuntimeLogger(QObject* parent = nullptr);
    ~RuntimeLogger() override;

    bool initialize();
    void append(const QString& message);
    QString path() const;
    QString contents() const;

private:
    void flush();

    QString m_path;
    QStringList m_pending;
    QTimer m_flushTimer;
};
