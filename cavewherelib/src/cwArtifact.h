// cwArtifact.h
#ifndef CWARTIFACT_H
#define CWARTIFACT_H

#include <QObject>
#include <QString>

/**
 * @brief The cwArtifact class represents data that flows between rules in the pipeline
 *
 * This is a lightweight class that serves as an identifier for data in the pipeline.
 * It emits a dataChanged signal when its underlying data (managed externally) changes.
 */
class cwArtifact : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

public:
    explicit cwArtifact(QObject *parent = nullptr);
    explicit cwArtifact(const QString &name, QObject *parent = nullptr);
    virtual ~cwArtifact();

    QString name() const;
    void setName(const QString &name);

signals:
    void nameChanged();

private:
    QString m_name;
};

#endif
