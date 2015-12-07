#ifndef CWWALLSIMPORTER_H
#define CWWALLSIMPORTER_H

//Our includes
#include "cwTreeDataImporter.h"
#include "cwCave.h"
#include "cwSurveyChunk.h"
#include "cwStationRenamer.h"
#include "wallsunits.h"
#include "wallsprojectparser.h"
#include "fixstation.h"
#include "vector.h"

//Qt include
#include <QObject>
#include <QRegExp>
#include <QStringList>
class QFile;

class cwSurvexBlockData;
class cwSurvexGlobalData;

namespace dewalls {
    class WallsParser;
    class SegmentParseExpectedException;
    class SegmentParseException;
}

using namespace dewalls;

typedef QSharedPointer<cwTrip> cwTripPtr;

class cwWallsImporter;

class WallsImporterVisitor : public QObject
{
    Q_OBJECT

public:
    WallsImporterVisitor(WallsParser* parser, cwWallsImporter* importer, QString tripNamePrefix);

    void clearTrip();
    void ensureValidTrip();
    inline QList<cwTripPtr> trips() const { return Trips; }
    inline QString comment() const { return Comment; }

public slots:
    void parsedFixStation(FixStation station);
    void parsedVector(Vector vector);
    void willParseUnits();
    void parsedUnits();
    void parsedDate(QDate date);
    void parsedComment(QString comment);
    void message(WallsMessage message);

private:
    WallsUnits priorUnits;
    WallsParser* Parser;
    cwWallsImporter* Importer;
    QString Comment;
    QString TripNamePrefix;
    QList<cwTripPtr> Trips;
    cwTripPtr CurrentTrip;
};

class cwWallsImporter : public cwTreeDataImporter
{
    Q_OBJECT
public:
    explicit cwWallsImporter(QObject *parent = 0);

    friend class WallsImporterVisitor;

    bool hasErrors();
    QStringList errors();

    cwSurvexGlobalData* data();

    static void importCalibrations(WallsUnits units, cwTrip& trip);

signals:
    void message(WallsMessage message);

public slots:
    void emitMessage(WallsMessage message);
    void setSurvexFile(QString filename);

protected:
    bool verifyFileExists(QString filename, Segment segment);
    bool parseSrvFile(WpjEntryPtr survey, QList<cwTripPtr>& tripsOut);

private:
    virtual void runTask();
    void importWalls(QString filename);
    void clear();

    cwSurvexBlockData* convertEntry(WpjEntryPtr entry);
    cwSurvexBlockData* convertBook(WpjBookPtr book);
    cwSurvexBlockData* convertSurvey(WpjEntryPtr survey);

    void applyLRUDs(cwSurvexBlockData* block);

    void addError(WallsMessage message);

    QString RootFilename;

    cwSurvexGlobalData* GlobalData;

    QStringList Errors;

    QList<cwCave*> Caves;
    cwStationRenamer StationRenamer;
    QHash<QString, QDate> StationDates;
    QHash<QString, cwStation> StationMap; // used to apply station-only LRUD lines
};

/**
  \brief Gets all the data from the importer
  */
inline cwSurvexGlobalData* cwWallsImporter::data() {
    return GlobalData;
}

/**
  \brief Sets the root file for the survex
  */
inline void cwWallsImporter::setSurvexFile(QString filename) {
    RootFilename = filename;
}

#endif // CWWALLSIMPORTER_H
