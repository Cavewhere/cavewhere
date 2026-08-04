#ifndef CWWALLSIMPORTER_H
#define CWWALLSIMPORTER_H

//Our includes
#include "cwTreeDataImporter.h"
#include "cwFixStation.h"
#include "cwWallsImportData.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStationRenamer.h"
#include "wallsunits.h"
#include "wallsprojectparser.h"
#include "fixstation.h"
#include "vector.h"

//Qt include
#include <QObject>
#include <QStringList>
#include <QSet>
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

/**
 * The coordinate systems a Walls project's `.REF` names, as PROJ reads them:
 * \a rect for `#FIX` lines given as grid coordinates (which need the datum and
 * the zone), \a geo for the ones given as latitude and longitude (which need
 * only the datum). Both are empty when the `.REF` said nothing usable — a bare
 * .srv imported without its project file has none — and \a unmappedDatum then
 * carries the datum name the file did give, so the warning can name it.
 */
struct WallsReferenceCS {
    QString rect;
    QString geo;
    QString unmappedDatum;
};

class WallsImporterVisitor : public QObject
{
    Q_OBJECT

public:
    WallsImporterVisitor(WallsSurveyParser* parser, cwWallsImporter* importer, QString tripNamePrefix,
                         WallsReferenceCS referenceCS);

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
    WallsReferenceCS ReferenceCS;
};

class CAVEWHERE_LIB_EXPORT cwWallsImporter : public cwTreeDataImporter
{
    Q_OBJECT
public:
    enum WarningType {
        CANT_IMPORT_FIX_STATIONS = 1,
        CANT_IMPORT_REFS = 2,
        STATION_RENAMED = 3,
        HEIGHT_CORRECTIONS_APPLIED = 4,
        NO_AVERAGE_NOT_SUPPORTED = 5,
        UV_NOT_SUPPORTED = 6,
        VARIANCE_OVERRIDES_NOT_SUPPORTED = 7,
        LRUD_TYPE_NOT_SUPPORTED = 8,
        LRUD_FACING_ANGLE_NOT_SUPPORTED = 9,
        OTHER_ANGLE_UNITS_NOT_SUPPORTED = 10
    };

    explicit cwWallsImporter(QObject *parent = 0);

    friend class WallsImporterVisitor;

    bool hasParseErrors();
    QStringList parseErrors();
    bool hasImportErrors();
    QStringList importErrors();

    /**
     * Fix stations captured from `#FIX` directives, carrying the coordinate
     * system the project's `.REF` names (see WallsReferenceCS). A fix whose
     * system couldn't be determined carries none and warns through
     * importErrors(). The cave-attachment of these fixes happens in the
     * import-accept flow (follow-up).
     */
    QList<cwFixStation> capturedFixStations() const { return CapturedFixStations; }

    cwTreeImportData* data();

    static void importCalibrations(const WallsUnits units, cwTrip& trip);

public slots:
    void setInputFiles(QStringList filenames);
    void addParseError(WallsMessage message);

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
    cwTreeImportDataNode* convertTrip(cwTrip* trip, cwTreeImportDataNode* result = nullptr);

    void applyLRUDs(cwTreeImportDataNode* block);

    void addImportError(WallsMessage message);

    cwStation createStation(QString name);

    QStringList RootFilenames;

    cwWallsImportData* GlobalData;

    QStringList ParseErrors;
    QStringList ImportErrors;

    QList<cwCave*> Caves;
    cwStationRenamer StationRenamer;
    QHash<QString, QDate> StationDates;
    QHash<QString, cwStation> StationMap; // used to apply station-only LRUD lines
    QList<cwFixStation> CapturedFixStations;

    QSet<WarningType> EmittedWarnings;

    bool shouldWarn(WarningType type, bool condition = true);
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
