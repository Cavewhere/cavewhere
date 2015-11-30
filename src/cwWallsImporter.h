#ifndef CWWALLSIMPORTER_H
#define CWWALLSIMPORTER_H

//Our includes
#include "cwTreeDataImporter.h"
#include "cwCave.h"
#include "cwSurveyChunk.h"
#include "cwStationRenamer.h"
#include "wallsvisitor.h"
#include "wallsunits.h"
#include "wallsprojectparser.h"

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
    virtual void message(WallsMessage message);
    inline QList<cwTripPtr> trips() const { return Trips; }

private:
    WallsUnits priorUnits;
    WallsParser* Parser;
    cwWallsImporter* Importer;
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

signals:
    void message(QString severity, QString message, QString source,
                 int startLine, int startColumn, int endLine, int endColumn);

public slots:
    void emitMessage(const SegmentParseException& exception);
    void emitMessage(QString severity, QString message, QString source,
                 int startLine, int startColumn, int endLine, int endColumn);
    void emitMessage(WallsMessage message);
    void setSurvexFile(QString filename);

protected:
    bool verifyFileExists(QString filename);
    bool parseSrvFile(WpjEntryPtr survey, QList<cwTripPtr>& tripsOut);

private:
    virtual void runTask();
    void importWalls(QString filename);
    void clear();

    cwSurvexBlockData* convertEntry(WpjEntryPtr entry);
    cwSurvexBlockData* convertBook(WpjBookPtr book);
    cwSurvexBlockData* convertSurvey(WpjEntryPtr survey);

    void applyLRUDs(cwSurvexBlockData* block);

    void addError(QString severity, QString message, QString source = QString(),
                 int startLine = -1, int startColumn = -1, int endLine = -1, int endColumn = -1);

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
