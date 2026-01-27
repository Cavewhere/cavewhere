/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

#ifndef PROJECT_FILENAME_TEST_HELPER_H
#define PROJECT_FILENAME_TEST_HELPER_H

#include <QDir>
#include <QString>

class cwProject;
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyNoteModel;
class cwSurveyNoteLiDARModel;
class cwNote;
class cwNoteLiDAR;
class cwImage;

class ProjectFilenameTestHelper
{
public:
    static QDir projectDir(const cwProject* project);

    static QString fileName(const cwCavingRegion* region);
    static QString absolutePath(const cwCavingRegion* region);
    static QDir dir(const cwCavingRegion* region);

    static QString fileName(const cwCave* cave);
    static QString absolutePath(const cwCave* cave);
    static QDir dir(const cwCave* cave);

    static QString fileName(const cwTrip* trip);
    static QString absolutePath(const cwTrip* trip);
    static QDir dir(const cwTrip* trip);

    static QDir dir(cwSurveyNoteModel* notes);
    static QDir dir(cwSurveyNoteLiDARModel* notes);

    static QString fileName(const cwNote* note);
    static QString absolutePath(const cwNote* note);
    static QString absolutePath(const cwNote* note, const QString& imageFilename);
    static QDir dir(const cwNote* note);

    static QString fileName(const cwNoteLiDAR* note);
    static QString absolutePath(const cwNoteLiDAR* note);
    static QString absolutePath(const cwNoteLiDAR* note, const QString& lidarFilename);
    static QDir dir(const cwNoteLiDAR* note);

    static cwImage absolutePathNoteImage(const cwNote* note);

private:
    static QString sanitizeFileName(QString input);
    static QString defaultDataRoot(const QString& projectName);

    static QDir caveDirHelper(const QDir& projectDir, const cwCave* cave);
    static QDir tripDirHelper(const QDir& caveDir, const cwTrip* trip);
    static QDir noteDirHelper(const QDir& tripDir);
};

#endif // PROJECT_FILENAME_TEST_HELPER_H
