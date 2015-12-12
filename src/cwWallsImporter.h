#ifndef CWWALLSIMPORTER_H
#define CWWALLSIMPORTER_H

//Our includes
#include "cwTreeDataImporter.h"
#include "cwWallsImportData.h"
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

class cwTreeImportDataNode;
class cwTreeImportData;

namespace dewalls {
    class WallsSurveyParser;
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
    WallsImporterVisitor(WallsSurveyParser* parser, cwWallsImporter* importer, QString tripNamePrefix);

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
    WallsSurveyParser* Parser;
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

    cwTreeImportData* data();

    static void importCalibrations(WallsUnits units, cwTrip& trip);

signals:
    void message(WallsMessage message);

public slots:
    void emitMessage(WallsMessage message);
    void setInputFiles(QStringList filenames);

protected:
    bool verifyFileExists(QString filename, Segment segment);
    bool parseSrvFile(WpjEntryPtr survey, QList<cwTripPtr>& tripsOut);

private:
    virtual void runTask();
    void importWalls(QStringList filenames);
    void clear();

    cwTreeImportDataNode* convertEntry(WpjEntryPtr entry);
    cwTreeImportDataNode* convertBook(WpjBookPtr book);
    cwTreeImportDataNode* convertSurvey(WpjEntryPtr survey);

    void applyLRUDs(cwTreeImportDataNode* block);

    void addError(WallsMessage message);

    QStringList RootFilenames;

    cwWallsImportData* GlobalData;

    QStringList Errors;

    QList<cwCave*> Caves;
    cwStationRenamer StationRenamer;
    QHash<QString, QDate> StationDates;
    QHash<QString, cwStation> StationMap; // used to apply station-only LRUD lines
};

/**
  \brief Gets all the data from the importer
  */
inline cwTreeImportData* cwWallsImporter::data() {
    return GlobalData;
}

/**
  \brief Sets the root file for the survex
  */
inline void cwWallsImporter::setInputFiles(QStringList filenames) {
    RootFilenames = filenames;
}

#endif // CWWALLSIMPORTER_H
