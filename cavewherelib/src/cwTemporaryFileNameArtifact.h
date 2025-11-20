#ifndef CW_TEMPORARY_FILE_ARTIFACT_H
#define CW_TEMPORARY_FILE_ARTIFACT_H

//Our includes
#include "cwFileNameArtifact.h"

//Qt includes
#include <QTemporaryFile>
#include <QQmlEngine>

class cwTemporaryFileNameArtifact : public cwFileNameArtifact
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TemporaryFileNameArtifact)

    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix NOTIFY suffixChanged BINDABLE bindableSuffix)

public:
    explicit cwTemporaryFileNameArtifact(QObject *parent = nullptr);
    virtual ~cwTemporaryFileNameArtifact();

    QString suffix() const { return m_suffix.value(); }
    void setSuffix(const QString& suffix) { m_suffix = suffix; }
    QBindable<QString> bindableSuffix() { return &m_suffix; }

signals:
    void suffixChanged();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwTemporaryFileNameArtifact, QString, m_suffix, QString(), &cwTemporaryFileNameArtifact::suffixChanged);

    // QTemporaryFile* m_tempFile;
};

#endif // CW_TEMPORARY_FILE_ARTIFACT_H
