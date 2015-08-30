#ifndef CWWALLSIMPORTER_H
#define CWWALLSIMPORTER_H

//Our includes
#include "cwTask.h"
#include "cwCave.h"
#include "cwStationRenamer.h"
#include "wallsvisitor.h"
#include "wallsunits.h"

//Qt include
#include <QObject>
#include <QRegExp>
#include <QStringList>
class QFile;

namespace dewalls {
    class WallsParser;
    class SegmentParseExpectedException;
    class SegmentParseException;
}

using namespace dewalls;

class cwWallsImporter;

class WallsImporterVisitor : public QObject, public CapturingWallsVisitor
{
    Q_OBJECT

public:
    WallsImporterVisitor(WallsParser* parser, cwWallsImporter* importer, QString tripNamePrefix);

    void clearTrip();
    void ensureValidTrip();
    virtual void endFixLine();
    virtual void endVectorLine();
    virtual void beginUnitsLine();
    virtual void endUnitsLine();
    virtual void visitDateLine(QDate date);
    virtual void warn(QString message);
    virtual void message(WallsMessage message);
    inline QList<cwTrip*> trips() const { return Trips; }

signals:
    void warning(QString message);

private:
    WallsUnits priorUnits;
    WallsParser* Parser;
    cwWallsImporter* Importer;
    QString TripNamePrefix;
    QList<cwTrip*> Trips;
    cwTrip* CurrentTrip;
    cwSurveyChunk* CurrentChunk;
};

class cwWallsImporter : public cwTask
{
    Q_OBJECT
public:
    explicit cwWallsImporter(QObject *parent = 0);

    void setWallsDataFiles(QStringList filename);

    QList<cwCave> caves() const;

    friend class WallsImporterVisitor;

signals:
    void message(QString severity, QString message, QString source,
                 int startLine, int startColumn, int endLine, int endColumn);

public slots:
    void warning(QString message);

protected:
    void emitMessage(const SegmentParseExpectedException& exception);
    void emitMessage(const SegmentParseException& exception);
    void emitMessage(WallsMessage message);
    void runTask();
    bool verifyFileExists(QString filename);
    bool parseFile(QString filename, QList<cwTrip*>& tripsOut);

private:
    QStringList WallsDataFiles;
    QList<cwCave> Caves;
    cwStationRenamer StationRenamer;
    QHash<QString, QDate> StationDates;
    QHash<QString, cwStation> StationMap; // used to apply station-only LRUD lines
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
