/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

#include "ProjectFilenameTestHelper.h"

#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwImage.h"
#include "cwNote.h"
#include "cwNoteLiDAR.h"
#include "cwProject.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"

#include <QFileInfo>

namespace {
QDir projectRootDirForFile(const QString& projectFileName)
{
    QFileInfo info(projectFileName);
    return info.absoluteDir();
}
}

QString ProjectFilenameTestHelper::sanitizeFileName(QString input)
{
    const QString forbiddenChars = R"(\/:*?"<>|)";
    for (const QChar& ch : forbiddenChars) {
        input.replace(ch, "_");
    }

    input = input.trimmed();
    while (input.startsWith('.') || input.endsWith('.')) {
        input = input.mid(1).chopped(1);
    }

    if (input.isEmpty()) {
        input = "untitled";
    }

    return input;
}

QString ProjectFilenameTestHelper::defaultDataRoot(const QString& projectName)
{
    return sanitizeFileName(projectName);
}

QDir ProjectFilenameTestHelper::projectDir(const cwProject* project)
{
    if (project == nullptr) {
        return QDir();
    }

    QDir rootDir = projectRootDirForFile(project->filename());
    QString dataRootName = project->dataRoot();
    if (dataRootName.isEmpty()) {
        dataRootName = defaultDataRoot(project->cavingRegion() ? project->cavingRegion()->name() : QString());
    }
    return QDir(rootDir.absoluteFilePath(dataRootName));
}

QString ProjectFilenameTestHelper::fileName(const cwCavingRegion* region)
{
    return sanitizeFileName(region->name() + QStringLiteral(".cwproj"));
}

QString ProjectFilenameTestHelper::absolutePath(const cwCavingRegion* region)
{
    return dir(region).absoluteFilePath(fileName(region));
}

QDir ProjectFilenameTestHelper::dir(const cwCavingRegion* region)
{
    if (region->parentProject()) {
        return projectDir(region->parentProject());
    }
    return QDir();
}

QString ProjectFilenameTestHelper::fileName(const cwCave* cave)
{
    return sanitizeFileName(cave->name() + QStringLiteral(".cwcave"));
}

QString ProjectFilenameTestHelper::absolutePath(const cwCave* cave)
{
    return dir(cave).absoluteFilePath(fileName(cave));
}

QDir ProjectFilenameTestHelper::dir(const cwCave* cave)
{
    if (cave->parentRegion() && cave->parentRegion()->parentProject()) {
        QDir projDir = projectDir(cave->parentRegion()->parentProject());
        return caveDirHelper(projDir, cave);
    }
    return QDir();
}

QString ProjectFilenameTestHelper::fileName(const cwTrip* trip)
{
    return sanitizeFileName(trip->name() + QStringLiteral(".cwtrip"));
}

QString ProjectFilenameTestHelper::absolutePath(const cwTrip* trip)
{
    return dir(trip).absoluteFilePath(fileName(trip));
}

QDir ProjectFilenameTestHelper::dir(const cwTrip* trip)
{
    if (trip->parentCave()) {
        return tripDirHelper(dir(trip->parentCave()), trip);
    }
    return QDir();
}

QDir ProjectFilenameTestHelper::dir(cwSurveyNoteModel* notes)
{
    if (notes->parentTrip()) {
        return noteDirHelper(dir(notes->parentTrip()));
    }
    return QDir();
}

QDir ProjectFilenameTestHelper::dir(cwSurveyNoteLiDARModel* notes)
{
    if (notes->parentTrip()) {
        return noteDirHelper(dir(notes->parentTrip()));
    }
    return QDir();
}

QString ProjectFilenameTestHelper::fileName(const cwNote* note)
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote"));
}

cwImage ProjectFilenameTestHelper::absolutePathNoteImage(const cwNote* note)
{
    if (!note) {
        return cwImage();
    }

    const QString path = absolutePath(note, note->image().path());
    if (path.isEmpty()) {
        return cwImage();
    }

    cwImage image = note->image();
    image.setPath(path);
    return image;
}

QString ProjectFilenameTestHelper::absolutePath(const cwNote* note)
{
    return dir(note).absoluteFilePath(fileName(note));
}

QString ProjectFilenameTestHelper::absolutePath(const cwNote* note, const QString& imageFilename)
{
    if (note == nullptr) {
        return QString();
    }

    if (!imageFilename.isEmpty()) {
        return dir(note).absoluteFilePath(imageFilename);
    }

    return QString();
}

QDir ProjectFilenameTestHelper::dir(const cwNote* note)
{
    if (note->parentTrip()) {
        return noteDirHelper(dir(note->parentTrip()));
    }
    return QDir();
}

QString ProjectFilenameTestHelper::fileName(const cwNoteLiDAR* note)
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote3d"));
}

QString ProjectFilenameTestHelper::absolutePath(const cwNoteLiDAR* note)
{
    return dir(note).absoluteFilePath(fileName(note));
}

QString ProjectFilenameTestHelper::absolutePath(const cwNoteLiDAR* note, const QString& lidarFilename)
{
    if (note == nullptr) {
        return QString();
    }

    if (lidarFilename.isEmpty()) {
        return QString();
    }

    return dir(note).absoluteFilePath(lidarFilename);
}

QDir ProjectFilenameTestHelper::dir(const cwNoteLiDAR* note)
{
    if (note->parentTrip()) {
        return noteDirHelper(dir(note->parentTrip()));
    }
    return QDir();
}

QDir ProjectFilenameTestHelper::caveDirHelper(const QDir& projectDir, const cwCave* cave)
{
    QString caveDirName = sanitizeFileName(cave->name());
    return QDir(projectDir.absoluteFilePath(caveDirName));
}

QDir ProjectFilenameTestHelper::tripDirHelper(const QDir& caveDir, const cwTrip* trip)
{
    return QDir(caveDir.absoluteFilePath(QStringLiteral("trips/") + sanitizeFileName(trip->name())));
}

QDir ProjectFilenameTestHelper::noteDirHelper(const QDir& tripDir)
{
    return QDir(tripDir.absoluteFilePath("notes"));
}
