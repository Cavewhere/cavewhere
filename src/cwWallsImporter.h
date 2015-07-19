#ifndef CWWALLSIMPORTER_H
#define CWWALLSIMPORTER_H

//Our includes
#include "cwTask.h"
#include "cwCave.h"
#include "cwStationRenamer.h"
#include "wallsvisitor.h"

//Qt include
#include <QRegExp>
#include <QStringList>
class QFile;

class cwWallsImporter : public cwTask
{
    Q_OBJECT
public:
    explicit cwWallsImporter(QObject *parent = 0);

    void setWallsDataFiles(QStringList filename);

    QList<cwCave> caves() const;

protected:
    void runTask();
    bool verifyFileExists(QString filename);
    bool parseFile(QString filename, QList<cwTrip*>& tripsOut);

private:
    QStringList WallsDataFiles;
    QList<cwCave> Caves;
    cwStationRenamer StationRenamer;
};

/**
 * @brief cwWallsImporter::setWallsDataFiles
 * @param filenames - Sets the compass data file list
 */
inline void cwWallsImporter::setWallsDataFiles(QStringList filenames)
{
    WallsDataFiles = filenames;
}

inline QList<cwCave> cwWallsImporter::caves() const
{
    return Caves;
}

#endif // CWWALLSIMPORTER_H
