#pragma once

#include <QObject>

class RuntimeLogger final : public QObject {
    Q_OBJECT
public:
    explicit RuntimeLogger(QObject* parent = nullptr);

    bool initialize();
    void append(const QString& message);
    QString path() const;
    QString contents() const;

private:
    QString m_path;
};
