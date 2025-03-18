#include "cwTemporaryFileArtifact.h"

cwTemporaryFileArtifact::cwTemporaryFileArtifact(QObject *parent)
    : cwFileNameArtifact(parent)
    , m_tempFile(new QTemporaryFile(this))
{
    m_tempFile->open();
    m_tempFile->setAutoRemove(false);
    m_tempFile->close();

    setFilename(m_tempFile->fileName());
}

cwTemporaryFileArtifact::~cwTemporaryFileArtifact()
{
    // m_tempFile is parented, so it will be automatically deleted.
}
