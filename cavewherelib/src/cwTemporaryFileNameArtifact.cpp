#include "cwTemporaryFileNameArtifact.h"

//Qt includes
#include <QDir>
#include <QCoreApplication>

cwTemporaryFileNameArtifact::cwTemporaryFileNameArtifact(QObject *parent)
    : cwFileNameArtifact(parent)
{
    setName("Temporary file name");

    bindableFilename().setBinding([this](){
        QTemporaryFile file;
        QString suffix = m_suffix;
        if(!suffix.isEmpty()) {
            file.setFileTemplate(QDir::tempPath()
                                 + QStringLiteral("/")
                                 + QCoreApplication::instance()->applicationName()
                                 + QStringLiteral(".XXXXXX.")
                                 + m_suffix);
        }
        file.setAutoRemove(false);
        file.open();
        return file.fileName();
    });
}

cwTemporaryFileNameArtifact::~cwTemporaryFileNameArtifact()
{
    // m_tempFile is parented, so it will be automatically deleted.
}
