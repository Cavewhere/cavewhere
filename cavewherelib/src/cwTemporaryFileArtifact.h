#ifndef CW_TEMPORARY_FILE_ARTIFACT_H
#define CW_TEMPORARY_FILE_ARTIFACT_H

#include "cwFileArtifact.h"
#include <QTemporaryFile>

class cwTemporaryFileArtifact : public cwFileNameArtifact
{
    Q_OBJECT

public:
    explicit cwTemporaryFileArtifact(QObject *parent = nullptr);
    virtual ~cwTemporaryFileArtifact();

private:
    QTemporaryFile* m_tempFile;
};

#endif // CW_TEMPORARY_FILE_ARTIFACT_H
