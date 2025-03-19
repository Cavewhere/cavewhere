#ifndef CW_TEMPORARY_FILE_ARTIFACT_H
#define CW_TEMPORARY_FILE_ARTIFACT_H

#include "cwFileNameArtifact.h"
#include <QTemporaryFile>

class cwTemporaryFileArtifact : public cwFileNameArtifact
{
    Q_OBJECT

    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix NOTIFY suffixChanged BINDABLE bindableSuffix)

public:
    explicit cwTemporaryFileArtifact(QObject *parent = nullptr);
    virtual ~cwTemporaryFileArtifact();

    QString suffix() const { return m_suffix.value(); }
    void setSuffix(const QString& suffix) { m_suffix = suffix; }
    QBindable<QString> bindableSuffix() { return &m_suffix; }

signals:
    void suffixChanged();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwTemporaryFileArtifact, QString, m_suffix, QString(), &cwTemporaryFileArtifact::suffixChanged);

    // QTemporaryFile* m_tempFile;
};

#endif // CW_TEMPORARY_FILE_ARTIFACT_H
