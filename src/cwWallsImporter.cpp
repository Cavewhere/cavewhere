#include "cwWallsImporter.h"

#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwLength.h"
#include "wallssurveyparser.h"
#include "wallsprojectparser.h"
#include "cwSurvexGlobalData.h"
#include "cwSurvexBlockData.h"

//Qt includes
#include <QFileInfo>
#include <QDir>

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
        CurrentTrip->setDate(Parser->date());

        cwWallsImporter::importCalibrations(Parser->units(), *CurrentTrip);
    }
}

void WallsImporterVisitor::parsedFixStation(FixStation station)
{
    Q_UNUSED(station);
    ensureValidTrip();
    // TODO
}

void WallsImporterVisitor::parsedVector(Vector v)
{
    ensureValidTrip();
    if (Trips.isEmpty() || Trips.last() != CurrentTrip) Trips << CurrentTrip;

    WallsUnits units = Parser->units();

    cwStation fromStation = Importer->StationRenamer.createStation(units.processStationName(v.from()));
    cwStation toStation;
    cwShot shot;

    Length::Unit dUnit = units.dUnit();

    cwStation* lrudStation;

    if (units.vectorType() == VectorType::RECT)
    {
        ULength distance;
        UAngle frontAzimuth;
        UAngle frontInclination;
        units.rectToCt(v.north(), v.east(), v.rectUp(), distance, frontAzimuth, frontInclination);
        // rect correction is not supported so it's added here.
        // decl doesn't apply v.to() rect lines, so it's pre-subtracted here so that when the trip declination
        // is added back, the result agrees with the Walls data.
        frontAzimuth += units.rect() - units.decl();
        v.setDistance(distance);
        v.setFrontAzimuth(frontAzimuth);
        v.setFrontInclination(frontInclination);
    }
    else if (v.distance().isValid())
    {
        toStation = Importer->StationRenamer.createStation(units.processStationName(v.to()));

        // apply Walls corrections that Cavewhere doesn't support
        ULength distance = v.distance();
        UAngle frontInclination = v.frontInclination();
        UAngle backInclination = v.backInclination();
        units.applyHeightCorrections(distance, frontInclination, backInclination, v.instHeight(), v.targetHeight());

        shot.setDistance(distance.get(dUnit));
        if (v.frontAzimuth().isValid())
        {
            shot.setCompass(v.frontAzimuth().get(Angle::Degrees));
        }
        else
        {
            shot.setCompassState(cwCompassStates::Empty);
        }
        if (frontInclination.isValid())
        {
            shot.setClino(frontInclination.get(Angle::Degrees));
            if (shot.clino() == 90.0)
            {
                shot.setClinoState(cwClinoStates::Up);
            }
            else if (shot.clino() == -90.0)
            {
                shot.setClinoState(cwClinoStates::Down);
            }
        }
        else
        {
            shot.setClinoState(cwClinoStates::Empty);
        }
        if (v.backAzimuth().isValid())
        {
            shot.setBackCompass(v.backAzimuth().get(Angle::Degrees));
        }
        else
        {
            shot.setBackCompassState(cwCompassStates::Empty);
        }
        if (backInclination.isValid())
        {
            shot.setBackClino(backInclination.get(Angle::Degrees));
            if (shot.backClino() == 90.0)
            {
                shot.setBackClinoState(cwClinoStates::Up);
            }
            else if (shot.backClino() == -90.0)
            {
                shot.setBackClinoState(cwClinoStates::Down);
            }
        }
        else
        {
            shot.setBackClinoState(cwClinoStates::Empty);
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
        lrudStation->setLeft(v.left().get(dUnit));
    }
    else
    {
        lrudStation->setLeftInputState(cwDistanceStates::Empty);
    }
    if (v.right().isValid())
    {
        lrudStation->setRight(v.right().get(dUnit));
    }
    else
    {
        lrudStation->setRightInputState(cwDistanceStates::Empty);
    }
    if (v.up().isValid())
    {
        lrudStation->setUp(v.up().get(dUnit));
    }
    else
    {
        lrudStation->setUpInputState(cwDistanceStates::Empty);
    }
    if (v.down().isValid())
    {
        lrudStation->setDown(v.down().get(dUnit));
    }
    else
    {
        lrudStation->setDownInputState(cwDistanceStates::Empty);
    }

    // save the latest LRUDs associated with each station so that we can apply them in the end
    if (Parser->date().isValid())
    {
        if (!Importer->StationDates.contains(lrudStation->name()) ||
            Parser->date() >= Importer->StationDates[lrudStation->name()]) {
            Importer->StationDates[lrudStation->name()] = Parser->date();
            Importer->StationMap[lrudStation->name()] = *lrudStation;
        }
    }
    else if (!Importer->StationDates.contains(lrudStation->name()))
    {
        Importer->StationMap[lrudStation->name()] = *lrudStation;
    }

    if (v.distance().isValid())
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
    Importer->addError(message);
}

cwWallsImporter::cwWallsImporter(QObject *parent) :
    cwTreeDataImporter(parent),
    GlobalData(new cwSurvexGlobalData(this))
{
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
    importWalls(RootFilename);
    done();
}

/**
  \brief Returns true if errors have accured.
  \returns The list of errors
  */
bool cwWallsImporter::hasErrors() {
    return !Errors.isEmpty();
}

/**
  \brief Gets the errors of the importer
  \return Returns the errors if any.  Will be empty if HasErrors() returns false
  */
QStringList cwWallsImporter::errors() {
    return Errors;
}

/**
  \brief Clears all the current data in the object
  */
void cwWallsImporter::clear() {
    Errors.clear();
    StationMap.clear();
}

void cwWallsImporter::importWalls(QString filename) {
    clear();

    WallsProjectParser projParser;
    QObject::connect(&projParser, &WallsProjectParser::message, this, &cwWallsImporter::emitMessage);

    WpjBookPtr rootBook = projParser.parseFile(filename);
    cwSurvexBlockData* rootBlock = convertEntry(rootBook);
    if (rootBlock != nullptr) {
        applyLRUDs(rootBlock);
        QList<cwSurvexBlockData*> blocks;
        blocks << rootBlock;
        GlobalData->setBlocks(blocks);
    }
}

void cwWallsImporter::applyLRUDs(cwSurvexBlockData* block) {
    // apply StationMap replacements to support Walls' station-LRUD lines
    foreach (cwSurveyChunk* chunk, block->chunks())
    {
        for (int i = 0; i < chunk->stationCount(); i++)
        {
            QString name = chunk->stations()[i].name();
            if (StationMap.contains(name))
            {
                chunk->setStation(StationMap[name], i);
            }
        }
    }
    foreach (cwSurvexBlockData* childBlock, block->childBlocks())
    {
        applyLRUDs(childBlock);
    }
}

cwSurvexBlockData* cwWallsImporter::convertEntry(WpjEntryPtr entry) {
    if (entry.isNull()) {
        return nullptr;
    }
    if (entry->isBook()) {
        return convertBook(entry.staticCast<WpjBook>());
    }
    else if (entry->isSurvey()) {
        return convertSurvey(entry);
    }
    return nullptr;
}

cwSurvexBlockData* cwWallsImporter::convertBook(WpjBookPtr book) {
    cwSurvexBlockData* result = new cwSurvexBlockData();

    try {
        result->setName(book->Title);
        foreach (WpjEntryPtr child, book->Children) {
            cwSurvexBlockData* childBlock = convertEntry(child);
            if (childBlock) {
                result->addChildBlock(childBlock);
            }
        }

        return result;
    }
    catch (...) {
        delete result;
        return nullptr;
    }
}

cwSurvexBlockData* cwWallsImporter::convertSurvey(WpjEntryPtr survey) {
    cwSurvexBlockData* result = new cwSurvexBlockData();

    try {
        result->setName(survey->Title);
        result->IncludeDistance = true;

        QList<cwTripPtr> trips;
        if (!parseSrvFile(survey, trips)) {
            // jump to catch block
            throw std::exception();
        }

        if (!trips.isEmpty()) {
            *result->calibration() = *trips[0]->calibrations();
        }

        foreach (cwTripPtr trip, trips) {
            foreach (cwSurveyChunk* chunk, trip->chunks()) {
                result->addChunk(new cwSurveyChunk(*chunk));
            }

            foreach (cwTeamMember member, trip->team()->teamMembers()) {
                result->team()->addTeamMember(member);
            }
        }

        return result;
    }
    catch (...) {
        delete result;
        return nullptr;
    }
}

void cwWallsImporter::emitMessage(WallsMessage _message)
{
    addError(_message);
}

void cwWallsImporter::addError(WallsMessage _message)
{
    Errors << _message.toString();
    emit message(_message);
}

bool cwWallsImporter::verifyFileExists(QString filename, Segment segment)
{
    QFileInfo fileInfo(filename);
    if(!fileInfo.exists()) {
        addError(WallsMessage("error",
                                 QString("file doesn't exist: %1").arg(filename),
                                 segment));
        return false;
    }

    if(!fileInfo.isReadable()) {
        addError(WallsMessage("error",
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
        addError(WallsMessage("error",
                              QString("couldn't open file %1: %2").arg(filename).arg(file.errorString()),
                              survey->Name));
        return false;
    }

    QString justFilename = filename.mid(std::max(0, filename.lastIndexOf('/') + 1));

    WallsSurveyParser parser;
    WallsImporterVisitor visitor(&parser, this, justFilename);

    foreach (Segment options, survey->allOptions()) {
        try
        {
            parser.parseUnitsOptions(options);
        }
        catch (const SegmentParseException& ex)
        {
            addError(WallsMessage(ex));
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
            addError(WallsMessage("error",
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
                surveyors = visitor.comment().trimmed().split(QRegExp("\\s*;\\s*"));
            }
        }
        catch (const SegmentParseException& ex)
        {
            addError(WallsMessage(ex));
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
