#include "LoadProjectHelper.h"

//Qt includes
#include <QStandardPaths>
#include <QDirIterator>

//Our includes
#include "cwZip.h"
#include "cwFutureManagerModel.h"

std::shared_ptr<cwProject> fileToProject(QString filename) {
    auto project = std::make_shared<cwProject>();
    addTokenManager(project.get());
    fileToProject(project.get(), filename);
    return project;
}

QString fileToProject(cwProject *project, const QString &filename) {
    QString datasetFile = copyToTempFolder(filename);

    project->loadOrConvert(datasetFile);
    project->waitLoadToFinish();

    return datasetFile;
}

QString copyToTempFolder(QString filename) {

    QTemporaryDir tempDir;
    tempDir.setAutoRemove(false);

    QFileInfo info(filename);
    QString newFileLocation = tempDir.path() + "/" + info.fileName();

    if(!info.exists(filename)) {
        qFatal() << "file doesnt' exist:" << filename;
    }

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

void TestHelper::loadProjectFromZip(cwProject *project, const QString &filename) {
    QString datasetFileZip = copyToTempFolder(filename);

    QFileInfo info(datasetFileZip);
    auto result = cwZip::extractAll(datasetFileZip, info.canonicalPath());

    // Find the first .cw file
    QDirIterator it(info.canonicalPath(), {"*.cw"}, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        QString filePath = it.next();

        // Optionally, load it
        project->loadOrConvert(filePath);
        project->waitLoadToFinish();
    } else {
        qFatal() << "No .cw file found in:" << info.canonicalPath();
    }

}

QString TestHelper::copyToTempDir(const QString &filename)
{
    return copyToTempFolder(filename);
}

bool TestHelper::fileExists(const QUrl &filename) const
{
    QFileInfo info(filename.toLocalFile());
    return info.exists();
}

size_t TestHelper::fileSize(const QUrl &filename) const
{
    QFileInfo info(filename.toLocalFile());
    return info.size();
}

void TestHelper::removeFile(const QUrl &filename) const
{
    QFile::remove(filename.toLocalFile());
}



void addTokenManager(cwProject *project)
{
    cwFutureManagerModel* model = new cwFutureManagerModel(project);
    project->setFutureManagerToken(model->token());
}
