// cwArtifact.h
#ifndef CWARTIFACT_H
#define CWARTIFACT_H

#include <QObject>
#include <QString>
#include <QObjectBindableProperty>
#include "CaveWhereLibExport.h"

/**
 * @brief The cwArtifact class represents data that flows between rules in the pipeline
 *
 * This is a lightweight class that serves as an identifier for data in the pipeline.
 * It emits a dataChanged signal when its underlying data (managed externally) changes.
 */
class CAVEWHERE_LIB_EXPORT cwArtifact : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged BINDABLE bindableName)


public:
    explicit cwArtifact(QObject *parent = nullptr);
    explicit cwArtifact(const QString &name, QObject *parent = nullptr);
    virtual ~cwArtifact();

    QString name() const { return m_name.value(); }
    void setName(const QString& name) { m_name = name; }
    QBindable<QString> bindableName() { return &m_name; }

signals:
    void nameChanged();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwArtifact, QString, m_name, QString(), &cwArtifact::nameChanged);
};

#endif
