#include "cwWallsImporter.h"

#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwLength.h"
#include "wallsparser.h"
#include "wallsprojectparser.h"
#include "cwSurvexGlobalData.h"
#include "cwSurvexBlockData.h"

//Qt includes
#include <QFileInfo>
#include <QDir>

using namespace dewalls;

typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle> UAngle;
typedef const Unit<Length>* LengthUnit;
typedef const Unit<Angle>* AngleUnit;

cwUnits::LengthUnit cwUnit(const Unit<Length>* dewallsUnit)
{
    return dewallsUnit == Length::feet() ? cwUnits::Feet : cwUnits::Meters;
}

WallsImporterVisitor::WallsImporterVisitor(WallsParser* parser, cwWallsImporter* importer, QString tripNamePrefix)
    : Parser(parser),
      Importer(importer),
      TripNamePrefix(tripNamePrefix),
      Trips(QList<cwTripPtr>()),
      CurrentTrip()
{

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

        LengthUnit d_unit = Parser->units()->d_unit;

        CurrentTrip->calibrations()->setDistanceUnit(cwUnit(d_unit));
        CurrentTrip->calibrations()->setCorrectedCompassBacksight(Parser->units()->typeab_corrected);
        CurrentTrip->calibrations()->setCorrectedClinoBacksight(Parser->units()->typevb_corrected);
        CurrentTrip->calibrations()->setTapeCalibration(Parser->units()->incd.get(d_unit));
        CurrentTrip->calibrations()->setFrontCompassCalibration(Parser->units()->inca.get(Angle::degrees()));
        CurrentTrip->calibrations()->setFrontClinoCalibration(Parser->units()->incv.get(Angle::degrees()));
        CurrentTrip->calibrations()->setBackCompassCalibration(Parser->units()->incab.get(Angle::degrees()));
        CurrentTrip->calibrations()->setBackClinoCalibration(Parser->units()->incvb.get(Angle::degrees()));
        CurrentTrip->calibrations()->setDeclination(Parser->units()->decl.get(Angle::degrees()));
    }
}

void WallsImporterVisitor::endFixLine()
{
    ensureValidTrip();
    // TODO
}

void WallsImporterVisitor::endVectorLine()
{
    ensureValidTrip();
    if (Trips.isEmpty() || Trips.last() != CurrentTrip) Trips << CurrentTrip;

    cwStation fromStation = Importer->StationRenamer.createStation(Parser->units()->processStationName(from));
    cwStation toStation;
    cwShot shot;

    LengthUnit d_unit = Parser->units()->d_unit;

    cwStation* lrudStation;

    if (Parser->units()->vectorType == VectorType::RECT)
    {
        Parser->units()->rectToCt(north, east, rectUp, distance, frontsightAzimuth, frontsightInclination);
        // rect correction is not supported so it's added here.
        // decl doesn't apply to rect lines, so it's pre-subtracted here so that when the trip declination
        // is added back, the result agrees with the Walls data.
        frontsightAzimuth += Parser->units()->rect - Parser->units()->decl;
    }

    if (distance.isValid())
    {
        toStation = Importer->StationRenamer.createStation(Parser->units()->processStationName(to));

        // apply Walls corrections that Cavewhere doesn't support
        Parser->units()->applyHeightCorrections(distance, frontsightInclination, backsightInclination, instrumentHeight, targetHeight);

        shot.setDistance(distance.get(d_unit));
        if (frontsightAzimuth.isValid())
        {
            shot.setCompass(frontsightAzimuth.get(Angle::degrees()));
        }
        else
        {
            shot.setCompassState(cwCompassStates::Empty);
        }
        if (frontsightInclination.isValid())
        {
            shot.setClino(frontsightInclination.get(Angle::degrees()));
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
        if (backsightAzimuth.isValid())
        {
            shot.setBackCompass(backsightAzimuth.get(Angle::degrees()));
        }
        else
        {
            shot.setBackCompassState(cwCompassStates::Empty);
        }
        if (backsightInclination.isValid())
        {
            shot.setBackClino(backsightInclination.get(Angle::degrees()));
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

        lrudStation = Parser->units()->lrud == LrudType::From ||
                Parser->units()->lrud == LrudType::FB ?
                    &fromStation : &toStation;
    }
    else
    {
        lrudStation = &fromStation;
    }

    left += Parser->units()->incs;
    right += Parser->units()->incs;
    up += Parser->units()->incs;
    down += Parser->units()->incs;

    if (left.isValid())
    {
        lrudStation->setLeft(left.get(d_unit));
    }
    else
    {
        lrudStation->setLeftInputState(cwDistanceStates::Empty);
    }
    if (right.isValid())
    {
        lrudStation->setRight(right.get(d_unit));
    }
    else
    {
        lrudStation->setRightInputState(cwDistanceStates::Empty);
    }
    if (up.isValid())
    {
        lrudStation->setUp(up.get(d_unit));
    }
    else
    {
        lrudStation->setUpInputState(cwDistanceStates::Empty);
    }
    if (down.isValid())
    {
        lrudStation->setDown(down.get(d_unit));
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

    if (distance.isValid())
    {
        CurrentTrip->addShotToLastChunk(fromStation, toStation, shot);
    }
}

void WallsImporterVisitor::beginUnitsLine()
{
    priorUnits = *Parser->units();
}

void WallsImporterVisitor::endUnitsLine()
{
    if (Parser->units()->d_unit != priorUnits.d_unit ||
        Parser->units()->decl != priorUnits.decl ||
        Parser->units()->incd != priorUnits.incd ||
        Parser->units()->inca != priorUnits.inca ||
        Parser->units()->incab != priorUnits.incab ||
        Parser->units()->incv != priorUnits.incv ||
        Parser->units()->incvb != priorUnits.incvb ||
        Parser->units()->typeab_corrected != priorUnits.typeab_corrected ||
        Parser->units()->typevb_corrected != priorUnits.typevb_corrected)
    {
        // when the next vector or fix line sees that
        // CurrentTrip is null, it will create a new one
        clearTrip();
    }
}

void WallsImporterVisitor::visitDateLine(QDate date)
{
    Q_UNUSED(date);

    // when the next vector or fix line sees that
    // CurrentTrip is null, it will create a new one
    clearTrip();
}

void WallsImporterVisitor::message(WallsMessage message)
{
    Importer->emitMessage(message);
}

cwWallsImporter::cwWallsImporter(QObject *parent) :
    cwTreeDataImporter(parent),
    GlobalData(new cwSurvexGlobalData(this))
{
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
    QObject::connect(&projParser, &WallsProjectParser::message, this,
                     static_cast<void (cwWallsImporter::*)(QString, QString, QString, int, int, int, int)>(
                         &cwWallsImporter::emitMessage));

    WpjBookPtr rootBook = projParser.parseFile(filename);
    cwSurvexBlockData* rootBlock = convertEntry(rootBook);
    if (rootBlock != nullptr) {
        applyLRUDs(rootBlock);
        QList<cwSurvexBlockData*> blocks;
        blocks << rootBlock;
        GlobalData->setBlocks(blocks);
    }

    // TODO
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
    QString severity;
    switch(_message.severity) {
    case WallsMessage::Info:
        severity = "info";
        break;
    case WallsMessage::Warning:
        severity = "warning";
        break;
    case WallsMessage::Error:
        severity = "error";
        break;
    }

    emitMessage(severity, _message.message, _message.source,
                _message.startLine, _message.startColumn, _message.endLine, _message.endColumn);
}

void cwWallsImporter::emitMessage(QString severity, QString _message, QString source,
                                  int startLine, int startColumn, int endLine, int endColumn)
{
    addError(severity, _message, source, startLine, startColumn, endLine, endColumn);
    emit message(severity, _message, source, startLine, startColumn, endLine, endColumn);
}

void cwWallsImporter::emitMessage(const SegmentParseException &ex) {
    emitMessage("error", ex.detailMessage(), ex.segment().source(),
                ex.segment().startLine(), ex.segment().startCol(), ex.segment().endLine(), ex.segment().endCol());
}

void cwWallsImporter::addError(QString severity, QString message, QString source,
                               int startLine, int startColumn, int endLine, int endColumn)
{
    Q_UNUSED(endLine);
    Q_UNUSED(endColumn);

    if (source.isNull()) {
        Errors << QString("%1: %2").arg(severity, message);
    }
    else if (startLine < 0) {
        Errors << QString("%1: %2 (%3)").arg(severity, message, source);
    }
    else if (startColumn < 0) {
        Errors << QString("%1: %2 (%3, line %4)").arg(severity, message).arg(source).arg(startLine + 1);
    }
    else {
        Errors << QString("%1: %2 (%3, line %4, col %5)").arg(severity, message).arg(source).arg(startLine + 1).arg(startColumn + 1);
    }
}

bool cwWallsImporter::verifyFileExists(QString filename)
{
    QFileInfo fileInfo(filename);
    if(!fileInfo.exists()) {
        //TODO: Fix error message
        emit statusMessage(QString("I can't parse %1 because it does not exist!").arg(filename));
        return false;
    }

    if(!fileInfo.isReadable()) {
        //TODO: Fix error message
        emit statusMessage(QString("I can't parse %1 because it's not readable, change the permissions?").arg(filename));
        return false;
    }

    return true;
}

bool cwWallsImporter::parseSrvFile(WpjEntryPtr survey, QList<cwTripPtr>& tripsOut)
{
    QString filename = survey->absolutePath();

    if (!verifyFileExists(filename))
    {
        return false;
    }

    QFile file(filename);
    if (!file.open(QFile::ReadOnly))
    {
        emit statusMessage(QString("I couldn't open %1").arg(filename));
        return false;
    }

    QString justFilename = filename.mid(std::max(0, filename.lastIndexOf('/') + 1));

    WallsParser parser;
    WallsImporterVisitor visitor(&parser, this, justFilename);
    parser.setVisitor(&visitor);

    foreach (Segment options, survey->allOptions()) {
        try {
            parser.parseUnitsOptions(options);
        }
        catch (const SegmentParseException& ex)
        {
            emitMessage(ex);
            return false;
        }
    }

//    PrintingWallsVisitor printingVisitor;
//    MultiWallsVisitor multiVisitor({&printingVisitor, &visitor});
//    parser.setVisitor(&multiVisitor);

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
            emit statusMessage(QString("Error reading from file %1 at line %2: %3")
                               .arg(filename)
                               .arg(lineNumber)
                               .arg(file.errorString()));
            failed = true;
            break;
        }

        try
        {
            parser.parseLine(Segment(line, filename, lineNumber, 0));

            if (lineNumber == 0 && !visitor.comment.isEmpty())
            {
                tripName = visitor.comment;
            }
            else if (lineNumber == 1 && !visitor.comment.isEmpty())
            {
                surveyors = visitor.comment.trimmed().split(QRegExp("\\s*;\\s*"));
            }
        }
        catch (const SegmentParseException& ex)
        {
            emitMessage(ex);
            failed = true;
            break;
        }

        lineNumber++;
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
