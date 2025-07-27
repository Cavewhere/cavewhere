#include "cwWallsImporter.h"

#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwLength.h"
#include "wallssurveyparser.h"
#include "wallsprojectparser.h"
#include "wallstypes.h"
#include "cwTreeImportData.h"
#include "cwTreeImportDataNode.h"

//Qt includes
#include <QFileInfo>
#include <QDir>

#include <iostream>

using namespace dewalls;

typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle> UAngle;

cwUnits::LengthUnit cwUnit(Length::Unit dewallsUnit)
{
    return dewallsUnit == Length::Feet ? cwUnits::LengthUnit::Feet : cwUnits::LengthUnit::Meters;
}

WallsImporterVisitor::WallsImporterVisitor(WallsSurveyParser* parser, cwWallsImporter* importer, QString tripNamePrefix)
    : Parser(parser),
      Importer(importer),
      TripNamePrefix(tripNamePrefix),
      Trips(QList<cwTripPtr>()),
      CurrentTrip()
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
    CurrentTrip.clear();
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
    Q_UNUSED(station);
    ensureValidTrip();
    if (Importer->shouldWarn(cwWallsImporter::CANT_IMPORT_FIX_STATIONS)) {
        Importer->addImportError(WallsMessage("warning", "This data contains #FIX stations, which can't currently be imported into Cavewhere"));
    }
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

        lrudStation = units.lrud() == LrudType::From ||
                units.lrud() == LrudType::FB ?
                    &fromStation : &toStation;
    }
    else
    {
        lrudStation = &fromStation;
    }

    v.left() = units.correctLength(v.left(), units.incs());
    v.right() += units.correctLength(v.right(), units.incs());
    v.up() += units.correctLength(v.up(), units.incs());
    v.down() += units.correctLength(v.down(), units.incs());

    if (v.left().isValid())
    {
        lrudStation->setLeft(cwDistanceReading(v.left().get(dUnit)));
    }
    else
    {
        lrudStation->setLeft(cwDistanceReading());
    }
    if (v.right().isValid())
    {
        lrudStation->setRight(cwDistanceReading(v.right().get(dUnit)));
    }
    else
    {
        lrudStation->setRight(cwDistanceReading());
    }
    if (v.up().isValid())
    {
        lrudStation->setUp(cwDistanceReading(v.up().get(dUnit)));
    }
    else
    {
        lrudStation->setUp(cwDistanceReading());
    }
    if (v.down().isValid())
    {
        lrudStation->setDown(cwDistanceReading(v.down().get(dUnit)));
    }
    else
    {
        lrudStation->setDown(cwDistanceReading());
    }

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

    if(fromStation.name().isEmpty() || toStation.name().isEmpty()) {
        Importer->addImportError(WallsMessage("warning", QString("Station \"%1\" to \"%2\" Walls importer currently doesn't support splay shots").arg(fromStation.name()).arg(toStation.name())));
    }

    if (v.distance().isValid() && !fromStation.name().isEmpty() && !toStation.name().isEmpty())
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
    trip.calibrations()->setDeclination(units.decl().get(Angle::Degrees));
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
            auto stations = chunk->stations();
            QString name = stations.at(i).name();
            if (StationMap.contains(name))
            {
                chunk->setStation(StationMap[name], i);
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
        addImportError(WallsMessage("warning", "This data contains geographic references, which can't currently be imported into Cavewhere"));
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
    WallsImporterVisitor visitor(&parser, this, justFilename);

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
                cwTeam* team = new cwTeam(trip.data());
                foreach (QString surveyor, surveyors) {
                    team->addTeamMember(cwTeamMember(surveyor, QStringList()));
                }
                trip->setTeam(team);
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
