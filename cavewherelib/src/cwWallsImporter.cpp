#include "cwWallsImporter.h"

#include "cwCoordinateTransform.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwLength.h"
#include "cwMath.h"
#include "wallssurveyparser.h"
#include "wallsprojectparser.h"
#include "wallstypes.h"
#include "cwTreeImportData.h"
#include "cwTreeImportDataNode.h"
#include "img.h"

//Qt includes
#include <QFileInfo>
#include <QDir>

#include <cstdlib>
#include <iostream>

using namespace dewalls;

typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle> UAngle;

cwUnits::LengthUnit cwUnit(Length::Unit dewallsUnit)
{
    return dewallsUnit == Length::Feet ? cwUnits::LengthUnit::Feet : cwUnits::LengthUnit::Meters;
}

namespace {

//! The img datum for a Walls datum name.
//!
//! img.c matches Compass' spellings exactly and Walls spells several datums
//! differently, so the ones survex's own Walls reader special-cases are
//! repeated here (survex/src/datain.c). Walls carries a handful of regional
//! NAD27 realizations; they differ in how they transform to WGS84, not in which
//! CRS a coordinate given in them belongs to, so all of them name NAD27.
img_datum wallsDatum(const QString& name)
{
    const QByteArray latin1 = name.toLatin1();
    const img_datum compassDatum = img_parse_compass_datum_string(latin1.constData(),
                                                                  static_cast<size_t>(latin1.size()));
    if (compassDatum != img_DATUM_UNKNOWN) {
        return compassDatum;
    }

    if (name.startsWith(QStringLiteral("NAD27"))) {
        return img_DATUM_NAD27;
    }
    if (name == QStringLiteral("NAD83")) {
        return img_DATUM_NAD83;
    }
    if (name == QStringLiteral("Geodetic Datum `49")) {
        return img_DATUM_NZGD49;
    }
    if (name == QStringLiteral("Hu-Tzu-Shan")) {
        return img_DATUM_HUTZUSHAN1950;
    }
    return img_DATUM_UNKNOWN;
}

//! What \a reference says the project's `#FIX` coordinates are in.
WallsReferenceCS wallsReferenceCS(const GeoReferencePtr& reference)
{
    WallsReferenceCS referenceCS;

    if (reference.isNull()) {
        return referenceCS;
    }

    const QString datumName = reference->datumName.trimmed();
    const img_datum datum = wallsDatum(datumName);
    if (datum == img_DATUM_UNKNOWN) {
        referenceCS.unmappedDatum = datumName;
        return referenceCS;
    }

    //dewalls signs the zone the way img wants it: positive north, negative
    //south. img returns null for a zone of 0 — what a .REF with no zone gives
    //us — for the polar UPS zones, and for a zone outside the datum's area.
    if (char* utm = img_compass_utm_proj_str(datum, reference->zone)) {
        referenceCS.rect = QString::fromLatin1(utm);
        std::free(utm);
    }

    //Every datum img knows has a geodetic CRS; only img_DATUM_UNKNOWN, which
    //returned above, has none.
    referenceCS.geo = QStringLiteral("EPSG:%1").arg(img_compass_longlat_epsg_code(datum));

    return referenceCS;
}

//! Why \a referenceCS names no system, phrased to finish "#FIX A1 gives grid
//! coordinates, but ...". The zone reason is reachable only from the rect side;
//! a lat/long fix needs the datum alone.
QString missingCSReason(const WallsReferenceCS& referenceCS)
{
    if (!referenceCS.unmappedDatum.isEmpty()) {
        return QString("its datum \"%1\" isn't one CaveWhere can map")
            .arg(referenceCS.unmappedDatum);
    }
    if (referenceCS.geo.isEmpty()) {
        return QStringLiteral("the project names no datum");
    }
    return QStringLiteral("the project names no UTM zone they belong to");
}

}

WallsImporterVisitor::WallsImporterVisitor(WallsSurveyParser* parser, cwWallsImporter* importer, QString tripNamePrefix,
                                           WallsReferenceCS referenceCS)
    : Parser(parser),
      Importer(importer),
      TripNamePrefix(std::move(tripNamePrefix)),
      Trips(QList<cwTripPtr>()),
      CurrentTrip(),
      ReferenceCS(std::move(referenceCS))
{
    QObject::connect(parser, &WallsSurveyParser::parsedVector, this, &WallsImporterVisitor::parsedVector);
    QObject::connect(parser, &WallsSurveyParser::parsedFixStation, this, &WallsImporterVisitor::parsedFixStation);
    QObject::connect(parser, &WallsSurveyParser::parsedDate, this, &WallsImporterVisitor::parsedDate);
    QObject::connect(parser, &WallsSurveyParser::willParseUnits, this, &WallsImporterVisitor::willParseUnits);
    QObject::connect(parser, &WallsSurveyParser::parsedUnits, this, &WallsImporterVisitor::parsedUnits);
    QObject::connect(parser, &WallsSurveyParser::parsedComment, this, &WallsImporterVisitor::parsedComment);
    QObject::connect(parser, &WallsSurveyParser::message, this, &WallsImporterVisitor::message);
}

void WallsImporterVisitor::clearTrip()
{
    flushSplays();
    CurrentTrip.clear();
}

void WallsImporterVisitor::finishParsing()
{
    flushSplays();
}

/**
 * @brief WallsImporterVisitor::flushSplays
 *
 * Hangs the current trip's buffered splays on the stations they were shot from
 */
void WallsImporterVisitor::flushSplays()
{
    if (CurrentTrip.isNull()) {
        Splays.clear();
        return;
    }

    const QList<cwStation> skipped = Splays.attachTo(CurrentTrip->chunks());

    for (const cwStation& station : skipped) {
        Importer->addImportError(WallsMessage("warning",
            QString("Skipping %1 splay(s) at %2 because no shot in this trip reaches that station")
                .arg(station.splayCount())
                .arg(station.name())));
    }
}

void WallsImporterVisitor::ensureValidTrip()
{
    if (CurrentTrip.isNull())
    {
        CurrentTrip = cwTripPtr(new cwTrip());
        CurrentTrip->setName(QString("%1 (%2)").arg(TripNamePrefix).arg(Trips.size()));
        CurrentTrip->setDate(QDateTime(Parser->date(), QTime()));

        cwWallsImporter::importCalibrations(Parser->units(), *CurrentTrip);
    }
}

void WallsImporterVisitor::parsedFixStation(FixStation station)
{
    ensureValidTrip();

    cwFixStation fix;
    const cwStation renamed = Importer->createStation(station.name());
    fix.setStationName(renamed.name());

    // The coordinate system comes from the project's .REF line, which names the
    // datum and UTM zone the #FIX coordinates were surveyed in.
    const bool hasRect = station.east().isValid() && station.north().isValid();
    const bool hasGeo  = station.latitude().isValid() && station.longitude().isValid();

    QString inputCS;
    double easting = 0.0;
    double northing = 0.0;
    QString warning;

    if (hasRect) {
        inputCS = ReferenceCS.rect;
        easting = station.east().get(Length::Meters);
        northing = station.north().get(Length::Meters);
        if (inputCS.isEmpty()) {
            warning = QString("#FIX %1 gives grid coordinates, but %2, so they place nothing until "
                              "you set the coordinate system on the fix in CaveWhere.")
                          .arg(station.name(), missingCSReason(ReferenceCS));
        }
    } else if (hasGeo) {
        // FixStation::longitude is east, latitude is north.
        inputCS = ReferenceCS.geo;
        easting = station.longitude().get(Angle::Degrees);
        northing = station.latitude().get(Angle::Degrees);
        if (inputCS.isEmpty()) {
            // Any two datums put the same latitude and longitude within a couple
            // hundred meters of each other, so assuming one still leaves a usable
            // coordinate — unlike a grid coordinate, which means nothing without
            // its zone.
            inputCS = cwCoordinateTransform::Wgs84;
            warning = QString("#FIX %1 gives latitude and longitude and %2, so WGS84 (EPSG:4326) "
                              "is assumed. Change the coordinate system on the fix if it was "
                              "surveyed in another datum.")
                          .arg(station.name(), missingCSReason(ReferenceCS));
        }
    } else {
        Importer->addImportError(WallsMessage("warning",
            QString("#FIX %1 has neither rect nor geo coordinates; skipped.")
                .arg(station.name())));
        return;
    }

    fix.setInputCS(inputCS);
    //All three components at once: a fix whose CS we couldn't determine has no
    //axis order to read them back under, so the one-at-a-time setters would
    //each erase what the one before it stored (see cwFixStation).
    fix.setCoordinate(easting, northing,
                      station.rectUp().isValid() ? station.rectUp().get(Length::Meters) : 0.0);

    if (!warning.isEmpty()) {
        Importer->addImportError(WallsMessage("warning", warning));
    }

    Importer->CapturedFixStations.append(fix);
}

void WallsImporterVisitor::parsedVector(Vector v)
{
    ensureValidTrip();
    if (Trips.isEmpty() || Trips.last() != CurrentTrip) Trips << CurrentTrip;

    WallsUnits units = v.units();

    cwStation fromStation = Importer->createStation(units.processStationName(v.from()));
    cwStation toStation;
    cwShot shot;

    Length::Unit dUnit = units.dUnit();

    cwStation* lrudStation;

    //A leg with one end omitted is a wall shot: it hangs on the end the line
    //does name, since it has no destination station to chain to. The raw names
    //say which end that is — a #prefix would turn the omitted one into "PP:".
    const bool fromOmitted = v.from().isEmpty();
    const bool oneEndOmitted = fromOmitted != v.to().isEmpty();

    if (units.vectorType() == VectorType::RECT && v.north().isValid())
    {
        v.deriveCtFromRect();
        // rect correction is not supported so it's added here.
        // decl doesn't apply v.to() rect lines, so it's pre-subtracted here so that when the trip declination
        // is added back, the result agrees with the Walls data.
        v.setFrontAzimuth(v.frontAzimuth() + units.rect() - units.decl());
    }

    if (v.distance().isValid())
    {
        if (Importer->shouldWarn(cwWallsImporter::VARIANCE_OVERRIDES_NOT_SUPPORTED,
                                 !v.horizVariance().isNull() || !v.vertVariance().isNull())) {
            Importer->addImportError(WallsMessage("warning", "Walls variance overrides are not supported by Cavewhere"));
        }
        if (Importer->shouldWarn(cwWallsImporter::LRUD_FACING_ANGLE_NOT_SUPPORTED,
                                 v.lrudAngle().isValid())) {
            Importer->addImportError(WallsMessage("warning", "LRUD facing angles are not currently supported by Cavewhere"));
        }

        toStation = Importer->createStation(units.processStationName(v.to()));

        // apply Walls corrections that Cavewhere doesn't support
        if (Importer->shouldWarn(cwWallsImporter::HEIGHT_CORRECTIONS_APPLIED, v.applyHeightCorrections())) {
            Importer->addImportError(WallsMessage("warning", "This data contains shots with instrument/target heights and/or INCH correction.  Since these quantities are not stored in Cavewhere, the distance and inclination of such shots have been changed to reflect the same vector."));
        }
        ULength distance = v.distance();
        UAngle frontInclination = v.frontInclination();
        UAngle backInclination = v.backInclination();

        if (Importer->shouldWarn(cwWallsImporter::OTHER_ANGLE_UNITS_NOT_SUPPORTED,
                                 (v.frontAzimuth().isValid() && v.frontAzimuth().unit() != Angle::Degrees) ||
                                 (v.backAzimuth().isValid() && v.backAzimuth().unit() != Angle::Degrees) ||
                                 (frontInclination.isValid() && frontInclination.unit() != Angle::Degrees) ||
                                 (backInclination.isValid() && backInclination.unit() != Angle::Degrees))) {
            Importer->addImportError(WallsMessage("warning", "This data contains azimuths and/or inclinations in units other than degrees; all have been converted to degrees for Cavewhere import"));
        }

        if (!frontInclination.isValid() && !backInclination.isValid())
        {
            frontInclination = UAngle(0, Angle::Degrees);
        }

        shot.setDistance(cwDistanceReading(distance.get(dUnit)));
        if (v.frontAzimuth().isValid())
        {
            shot.setCompass(cwCompassReading(v.frontAzimuth().get(Angle::Degrees)));
        }

        if (frontInclination.isValid())
        {
            shot.setClino(cwClinoReading(frontInclination.get(Angle::Degrees)));
            // if (shot.clino() == 90.0)
            // {
            //     shot.setClinoState(cwClinoReading::State::Up);
            // }
            // else if (shot.clino() == -90.0)
            // {
            //     shot.setClinoState(cwClinoReading::State::Down);
            // }
        }
        if (v.backAzimuth().isValid())
        {
            shot.setBackCompass(cwCompassReading(v.backAzimuth().get(Angle::Degrees)));
        }

        if (backInclination.isValid())
        {
            shot.setBackClino(cwClinoReading(backInclination.get(Angle::Degrees)));
            // if (shot.backClino() == 90.0)
            // {
            //     shot.setBackClinoState(cwClinoReading::State::Up);
            // }
            // else if (shot.backClino() == -90.0)
            // {
            //     shot.setBackClinoState(cwClinoReading::State::Down);
            // }
        }

        // TODO: exclude length flag/segment

        if (oneEndOmitted)
        {
            //A splay hangs on the one end the line names, and the LRUDs on that
            //line describe it too — the wall point has no passage around it
            lrudStation = fromOmitted ? &toStation : &fromStation;

            //A station carries foresights only, so a reading the line gives as
            //a backsight has to be turned around before it can hang there
            UAngle azimuth = v.frontAzimuth();
            if (!azimuth.isValid() && v.backAzimuth().isValid())
            {
                azimuth = units.typeabCorrected()
                        ? v.backAzimuth()
                        : v.backAzimuth() + UAngle(cwHalfTurnDegrees, Angle::Degrees);
            }

            UAngle inclination = frontInclination;
            if (!inclination.isValid() && backInclination.isValid())
            {
                inclination = units.typevbCorrected()
                        ? backInclination
                        : -backInclination;
            }

            //A plumbed shot leaves the azimuth out entirely
            cwCompassReading compass;
            if (azimuth.isValid())
            {
                compass = cwCompassReading(cwWrapDegrees360(azimuth.get(Angle::Degrees)));
            }

            const cwShotMeasurement asWritten(cwDistanceReading(distance.get(dUnit)),
                                              compass,
                                              cwClinoReading(inclination.get(Angle::Degrees)));

            if (lrudStation->isValid())
            {
                //A leg written wall-point-first reads toward the station, so
                //the reading has to be turned around to point at the wall
                Splays.add(lrudStation->name(),
                           fromOmitted ? asWritten.reversed() : asWritten);
            }
        }
        else
        {
            lrudStation = units.lrud() == LrudType::From ||
                    units.lrud() == LrudType::FB ?
                        &fromStation : &toStation;
        }
    }
    else
    {
        lrudStation = &fromStation;
    }

    v.left() = units.correctLength(v.left(), units.incs());
    v.right() += units.correctLength(v.right(), units.incs());
    v.up() += units.correctLength(v.up(), units.incs());
    v.down() += units.correctLength(v.down(), units.incs());

    //A line that measured no passage says nothing about the passage around the
    //station, so whatever an earlier line recorded stands. Storing this one
    //would blank those dimensions instead — which a splay line, having no
    //passage of its own to describe, would otherwise do to the station it hangs on
    const bool lineHasLruds = v.left().isValid() || v.right().isValid()
            || v.up().isValid() || v.down().isValid();

    if (lineHasLruds)
    {
        //A line that gives only some of the four restates the passage as a
        //whole, so the ones it leaves out are cleared rather than kept
        const auto reading = [dUnit](const ULength& length) {
            return length.isValid() ? cwDistanceReading(length.get(dUnit))
                                    : cwDistanceReading();
        };

        lrudStation->setLeft(reading(v.left()));
        lrudStation->setRight(reading(v.right()));
        lrudStation->setUp(reading(v.up()));
        lrudStation->setDown(reading(v.down()));

        // save the latest LRUDs associated with each station so that we can apply them in the end
        if (v.date().isValid())
        {
            if (!Importer->StationDates.contains(lrudStation->name()) ||
                v.date() >= Importer->StationDates[lrudStation->name()]) {
                Importer->StationDates[lrudStation->name()] = v.date();
                Importer->StationMap[lrudStation->name()] = *lrudStation;
            }
        }
        else if (!Importer->StationDates.contains(lrudStation->name()))
        {
            Importer->StationMap[lrudStation->name()] = *lrudStation;
        }
    }

    //A splay is already buffered against the end the line names, and it stays
    //out of the centerline
    if (!oneEndOmitted && v.distance().isValid()
        && !fromStation.name().isEmpty() && !toStation.name().isEmpty())
    {
        CurrentTrip->addShotToLastChunk(fromStation, toStation, shot);
    }
}

void WallsImporterVisitor::willParseUnits()
{
    priorUnits = Parser->units();
}

void WallsImporterVisitor::parsedUnits()
{
    if (Parser->units().dUnit() != priorUnits.dUnit() ||
        Parser->units().decl() != priorUnits.decl() ||
        Parser->units().incd() != priorUnits.incd() ||
        Parser->units().inca() != priorUnits.inca() ||
        Parser->units().incab() != priorUnits.incab() ||
        Parser->units().incv() != priorUnits.incv() ||
        Parser->units().incvb() != priorUnits.incvb() ||
        Parser->units().typeabCorrected() != priorUnits.typeabCorrected() ||
        Parser->units().typevbCorrected() != priorUnits.typevbCorrected())
    {
        if (Importer->shouldWarn(cwWallsImporter::NO_AVERAGE_NOT_SUPPORTED,
                                 Parser->units().typeabNoAverage() || Parser->units().typevbNoAverage())) {
            Importer->addImportError(WallsMessage("warning", "no-average backsights (e.g. #units typeab=C,2,X) are not supported by Cavewhere"));
        }
        if (Importer->shouldWarn(cwWallsImporter::UV_NOT_SUPPORTED,
                                 Parser->units().uvh() != 1.0 || Parser->units().uvv() != 1.0)) {
            Importer->addImportError(WallsMessage("warning", "unit variance (e.g. #units uv=... uvv=... uvh=...) are not supported by Cavewhere"));
        }
        if (Importer->shouldWarn(cwWallsImporter::LRUD_TYPE_NOT_SUPPORTED,
                                 Parser->units().lrud() != LrudType::From)) {
            Importer->addImportError(WallsMessage("warning", "LRUD type (e.g. #units lrud=to) is not currently supported by Cavewhere"));
        }
        // when the next vector or fix line sees that
        // CurrentTrip is null, it will create a new one
        clearTrip();
    }
}

void WallsImporterVisitor::parsedDate(QDate date)
{
    Q_UNUSED(date);

    // when the next vector or fix line sees that
    // CurrentTrip is null, it will create a new one
    clearTrip();
}

void WallsImporterVisitor::parsedComment(QString comment)
{
    Comment = comment;
}

void WallsImporterVisitor::message(WallsMessage message)
{
    Importer->addParseError(message);
}

cwWallsImporter::cwWallsImporter(QObject *parent) :
    cwTreeDataImporter(parent),
    GlobalData(new cwWallsImportData(this)),
    EmittedWarnings()
{
}

bool cwWallsImporter::shouldWarn(WarningType type, bool condition)
{
    if (condition && !EmittedWarnings.contains(type)) {
        EmittedWarnings << type;
        return true;
    }
    return false;
}

cwStation cwWallsImporter::createStation(QString name)
{   
    cwStation station = StationRenamer.createStation(name);
    if (shouldWarn(STATION_RENAMED, name != station.name())) {
        addImportError(WallsMessage("warning",
                             QString("Some stations in the imported data had to be renamed to comply with Cavewhere station name restrictions (for instance: %1 -> %2)").arg(name, station.name())));
    }
    return station;
}

void cwWallsImporter::importCalibrations(const WallsUnits units, cwTrip &trip)
{
    Length::Unit dUnit = units.dUnit();

    trip.calibrations()->setDistanceUnit(cwUnit(dUnit));
    trip.calibrations()->setCorrectedCompassBacksight(units.typeabCorrected());
    trip.calibrations()->setCorrectedClinoBacksight(units.typevbCorrected());
    trip.calibrations()->setTapeCalibration(units.incd().get(dUnit));
    trip.calibrations()->setFrontCompassCalibration(units.inca().get(Angle::Degrees));
    trip.calibrations()->setFrontClinoCalibration(units.incv().get(Angle::Degrees));
    trip.calibrations()->setBackCompassCalibration(units.incab().get(Angle::Degrees));
    trip.calibrations()->setBackClinoCalibration(units.incvb().get(Angle::Degrees));
    trip.calibrations()->setImportedDeclination(units.decl().get(Angle::Degrees));
}


void cwWallsImporter::runTask() {
    importWalls(RootFilenames);
    done();
}

/**
  \brief Returns true if errors have accured.
  \returns The list of errors
  */
bool cwWallsImporter::hasParseErrors() {
    return !ParseErrors.isEmpty();
}

/**
  \brief Gets the errors of the importer
  \return Returns the errors if any.  Will be empty if HasErrors() returns false
  */
QStringList cwWallsImporter::parseErrors() {
    return ParseErrors;
}

/**
  \brief Returns true if errors have accured.
  \returns The list of errors
  */
bool cwWallsImporter::hasImportErrors() {
    return !ImportErrors.isEmpty();
}

/**
  \brief Gets the errors of the importer
  \return Returns the errors if any.  Will be empty if HasErrors() returns false
  */
QStringList cwWallsImporter::importErrors() {
    return ImportErrors;
}

/**
  \brief Clears all the current data in the object
  */
void cwWallsImporter::clear() {
    ParseErrors.clear();
    ImportErrors.clear();
    EmittedWarnings.clear();
    StationMap.clear();
}

void cwWallsImporter::importWalls(QStringList filenames) {
    clear();

    cwTreeImportDataNode* rootBlock = new cwTreeImportDataNode(nullptr);

    foreach(QString filename, filenames) {
        cwTreeImportDataNode* block;
        QFileInfo info(filename);
        if (info.suffix().compare("srv", Qt::CaseInsensitive) == 0) {
            WpjEntryPtr entry(new WpjEntry(WpjBookPtr(), info.baseName()));
            entry->Path = info.absolutePath();
            entry->Name = info.baseName();
            block = convertSurvey(entry);
        }
        else {
            WallsProjectParser projParser;
            QObject::connect(&projParser, &WallsProjectParser::message, this, &cwWallsImporter::addParseError);

            WpjBookPtr rootBook = projParser.parseFile(filename);
            block = convertEntry(rootBook);
        }
        if (block != nullptr) {
            applyLRUDs(block);
            rootBlock->addChildNode(block);
        }
    }

    QList<cwTreeImportDataNode*> blocks;
    if (rootBlock->childNodeCount() == 1) {
        rootBlock->childNode(0)->setParent(nullptr);
        blocks << rootBlock->childNode(0);
        delete rootBlock;
        rootBlock = blocks[0];
    }
    else {
        blocks << rootBlock;
    }
    if (rootBlock->name().isEmpty()) {
        rootBlock->setName("Walls Import");
    }
    GlobalData->setNodes(blocks);
}

void cwWallsImporter::applyLRUDs(cwTreeImportDataNode* block) {
    // apply StationMap replacements to support Walls' station-LRUD lines
    foreach (cwSurveyChunk* chunk, block->chunks())
    {
        for (int i = 0; i < chunk->stationCount(); i++)
        {
            cwStation station = chunk->station(i);
            const auto lruds = StationMap.constFind(station.name());
            if (lruds != StationMap.constEnd())
            {
                //Copy the LRUDs across rather than the whole station, so the
                //station keeps the splays hanging on it and the id it already has
                station.setLeft(lruds->left());
                station.setRight(lruds->right());
                station.setUp(lruds->up());
                station.setDown(lruds->down());
                chunk->setStation(station, i);
            }
        }
    }
    foreach (cwTreeImportDataNode* childBlock, block->childNodes())
    {
        applyLRUDs(childBlock);
    }
}

cwTreeImportDataNode* cwWallsImporter::convertEntry(WpjEntryPtr entry) {
    if (entry.isNull()) {
        return nullptr;
    }
    if (shouldWarn(CANT_IMPORT_REFS, !entry->reference().isNull())) {
        addImportError(WallsMessage("warning", "This data has a .REF geographic reference. Its datum and UTM zone give "
                                               "the coordinate system for #FIX stations; the reference position itself "
                                               "isn't imported into Cavewhere"));
    }
    if (entry->isBook()) {
        return convertBook(entry.staticCast<WpjBook>());
    }
    else if (entry->isSurvey()) {
        return convertSurvey(entry);
    }
    return nullptr;
}

cwTreeImportDataNode* cwWallsImporter::convertBook(WpjBookPtr book) {
    cwTreeImportDataNode* result = new cwTreeImportDataNode();

    try {
        result->setName(book->Title);
        foreach (WpjEntryPtr child, book->Children) {
            cwTreeImportDataNode* childBlock = convertEntry(child);
            if (childBlock) {
                result->addChildNode(childBlock);
            }
        }

        return result;
    }
    catch (...) {
        delete result;
        return nullptr;
    }
}

cwTreeImportDataNode* cwWallsImporter::convertSurvey(WpjEntryPtr survey) {
    cwTreeImportDataNode* result = new cwTreeImportDataNode();

    try {
        QList<cwTripPtr> trips;
        if (!parseSrvFile(survey, trips)) {
            // jump to catch block
            throw std::exception();
        }

        if (trips.size() == 1) {
            convertTrip(trips[0].data(), result);
        }
        else {
            for (int i = 0; i < trips.size(); i++) {
                cwTreeImportDataNode* child = convertTrip(trips[i].data());
                child->setName(QString("%1 (%2)").arg(survey->Title).arg(i + 1));
                result->addChildNode(child);
            }
        }

        result->setName(survey->Title);
        result->IncludeDistance = true;

        return result;
    }
    catch (...) {
        delete result;
        return nullptr;
    }
}

cwTreeImportDataNode* cwWallsImporter::convertTrip(cwTrip* trip, cwTreeImportDataNode* result)
{
    bool createdResult = !result;
    if (!result) {
        result = new cwTreeImportDataNode();
    }

    try {
        result->IncludeDistance = true;
        result->setName(trip->name());
        result->setDate(trip->date().date());
        result->calibration()->setData(trip->calibrations()->data());
        foreach(cwSurveyChunk* chunk, trip->chunks()) {
            auto newChunk = new cwSurveyChunk();
            newChunk->setData(chunk->data());
            result->addChunk(newChunk);
        }

        foreach (cwTeamMember member, trip->team()->teamMembers()) {
            result->team()->addTeamMember(member);
        }

        return result;
    }
    catch (...) {
        if (createdResult) {
            delete result;
        }
        return nullptr;
    }
}

void cwWallsImporter::addParseError(WallsMessage _message)
{
    std::cerr << _message.toString().toStdString() << std::endl;
    ParseErrors << _message.toString();
}

void cwWallsImporter::addImportError(WallsMessage _message)
{
    ImportErrors << _message.toString();
}

bool cwWallsImporter::verifyFileExists(QString filename, Segment segment)
{
    QFileInfo fileInfo(filename);
    if(!fileInfo.exists()) {
        addParseError(WallsMessage("error",
                                 QString("file doesn't exist: %1").arg(filename),
                                 segment));
        return false;
    }

    if(!fileInfo.isReadable()) {
        addParseError(WallsMessage("error",
                                 QString("file isn't readable: %1").arg(filename),
                                 segment));
        return false;
    }

    return true;
}

bool cwWallsImporter::parseSrvFile(WpjEntryPtr survey, QList<cwTripPtr>& tripsOut)
{
    QString filename = survey->absolutePath();

    if (filename.isEmpty())
    {
        return true;
    }

    if (!verifyFileExists(filename, survey->Name))
    {
        return false;
    }

    QFile file(filename);
    if (!file.open(QFile::ReadOnly))
    {
        addParseError(WallsMessage("error",
                              QString("couldn't open file %1: %2").arg(filename, file.errorString()),
                              survey->Name));
        return false;
    }

    QString justFilename = filename.mid(std::max(qsizetype(0), filename.lastIndexOf('/') + 1));

    WallsSurveyParser parser;
    WallsImporterVisitor visitor(&parser, this, justFilename, wallsReferenceCS(survey->reference()));

    foreach (Segment options, survey->allOptions()) {
        try
        {
            parser.parseUnitsOptions(options);
        }
        catch (const SegmentParseException& ex)
        {
            addParseError(WallsMessage(ex));
            return false;
        }
    }

    QStringList segment = survey->segment();
    if (!segment.isEmpty()) {
        parser.setSegment(segment);
        parser.setRootSegment(segment);
    }

    bool failed = false;

    QString tripName;
    QStringList surveyors;

    int lineNumber = 0;
    while (!file.atEnd())
    {
        QString line = file.readLine();
        line = line.trimmed();
        if (file.error() != QFile::NoError)
        {
            addParseError(WallsMessage("error",
                                  QString("failed to read from file: %1").arg(file.errorString()),
                                  filename,
                                  lineNumber));
            failed = true;
            break;
        }

        try
        {
            parser.parseLine(Segment(line, filename, lineNumber, 0));

            if (lineNumber == 0 && !visitor.comment().isEmpty())
            {
                tripName = visitor.comment();
            }
            else if (lineNumber == 1 && !visitor.comment().isEmpty())
            {
                static const QRegularExpression regex(QStringLiteral("\\s*;\\s*"));
                surveyors = visitor.comment().trimmed().split(regex);
            }
        }
        catch (const SegmentParseException& ex)
        {
            addParseError(WallsMessage(ex));
            failed = true;
            break;
        }

        lineNumber++;
    }

    if (!survey->Title.isEmpty()) {
        tripName = survey->Title;
    }

    file.close();

    if (!failed)
    {
        visitor.finishParsing();

        if (!tripName.isEmpty())
        {
            int i = 0;
            foreach (cwTripPtr trip, visitor.trips())
            {
                if (i == 0) trip->setName(tripName);
                else trip->setName(QString("%1 (%2)").arg(tripName).arg(++i));
            }
        }
        if (!surveyors.isEmpty())
        {
            foreach (cwTripPtr trip, visitor.trips())
            {
                QList<cwTeamMember> members;
                members.reserve(surveyors.size());
                foreach (const QString& surveyor, surveyors) {
                    members.append(cwTeamMember(surveyor, QStringList()));
                }
                trip->team()->setTeamMembers(members);
            }
        }

        tripsOut << visitor.trips();
        emit statusMessage(QString("Parsed file %1").arg(filename));
    }
    else
    {
        emit statusMessage(QString("Skipping file %1 due to errors").arg(filename));
    }

    return !failed;
}
