#include "cwTemporaryFileArtifact.h"

//Qt includes
#include <QDir>
#include <QCoreApplication>

cwTemporaryFileArtifact::cwTemporaryFileArtifact(QObject *parent)
    : cwFileNameArtifact(parent)
{
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

cwTemporaryFileArtifact::~cwTemporaryFileArtifact()
{
    // m_tempFile is parented, so it will be automatically deleted.
}
