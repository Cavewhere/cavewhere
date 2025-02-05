#include "LoadProjectHelper.h"

//Qt includes
#include <QStandardPaths>

std::shared_ptr<cwProject> fileToProject(QString filename) {
    auto project = std::make_shared<cwProject>();
    fileToProject(project.get(), filename);
    return project;
}

void fileToProject(cwProject *project, const QString &filename) {
    QString datasetFile = copyToTempFolder(filename);

    project->loadFile(datasetFile);
    project->waitLoadToFinish();
}

QString copyToTempFolder(QString filename) {

    QFileInfo info(filename);
    QString newFileLocation = QDir::tempPath() + "/" + info.fileName();

    if(QFileInfo::exists(newFileLocation)) {
        QFile file(newFileLocation);
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadUser);

        bool couldRemove = file.remove();
        if(!couldRemove) {
            qFatal() << "Trying to remove " << newFileLocation;
        }
    }

    bool couldCopy = QFile::copy(filename, newFileLocation);
    if(!couldCopy) {
        qFatal() << "Trying to copy " << filename << " to " << newFileLocation;
    }

    bool couldPermissions = QFile::setPermissions(newFileLocation, QFile::WriteOwner | QFile::ReadOwner);
    if(!couldPermissions) {
        qFatal() << "Trying to set permissions for " << filename;
    }

    return newFileLocation;
}

QString prependTempFolder(QString filename)
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + filename;
}
