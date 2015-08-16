#include "cwWallsImporter.h"

#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwLength.h"
#include "wallsparser.h"

//Qt includes
#include <QFileInfo>

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
      Trips(QList<cwTrip*>()),
      CurrentTrip(NULL)
{

}

void WallsImporterVisitor::ensureValidTrip()
{
    if (!CurrentTrip)
    {
        CurrentTrip = new cwTrip();
        Trips << CurrentTrip;
        CurrentTrip->setName(QString("%1%2").arg(TripNamePrefix).arg(Trips.size()));
        CurrentTrip->setDate(Parser->units()->date);

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

    cwStation fromStation = Importer->StationRenamer.createStation(Parser->units()->processStationName(from));

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
        cwStation toStation = Importer->StationRenamer.createStation(Parser->units()->processStationName(to));
        cwShot shot;

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

        CurrentTrip->addShotToLastChunk(fromStation, toStation, shot);

        lrudStation = Parser->units()->lrud == LrudType::From ||
                Parser->units()->lrud == LrudType::FB ?
                    &fromStation : &toStation;

        if(!CurrentTrip->chunks().isEmpty()) {
            cwSurveyChunk* lastChunk = CurrentTrip->chunks().last();
            int secondToLastStation = lastChunk->stationCount() - 2;
            lastChunk->setStation(fromStation, secondToLastStation);
        }
    }
    else
    {
        lrudStation = &fromStation;
    }

    // save the latest LRUDs associated with each station so that we can apply them in the end
    if (Parser->units()->date.isValid())
    {
        if (!Importer->StationDates.contains(lrudStation->name()) ||
            Parser->units()->date >= Importer->StationDates[lrudStation->name()]) {
            Importer->StationDates[lrudStation->name()] = Parser->units()->date;
            Importer->StationMap[lrudStation->name()] = *lrudStation;
        }
    }
    else if (!Importer->StationDates.contains(lrudStation->name()))
    {
        Importer->StationMap[lrudStation->name()] = *lrudStation;
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
}

void WallsImporterVisitor::beginUnitsLine()
{
    priorUnits = *Parser->units();
}

void WallsImporterVisitor::endUnitsLine()
{
    if (Parser->units()->d_unit != priorUnits.d_unit ||
        Parser->units()->date != priorUnits.date ||
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
        CurrentTrip = NULL;
    }
}

void WallsImporterVisitor::visitDateLine()
{
    // when the next vector or fix line sees that
    // CurrentTrip is null, it will create a new one
    CurrentTrip = NULL;
}

void WallsImporterVisitor::warn( QString message )
{
    emit warning(message);
}

cwWallsImporter::cwWallsImporter(QObject *parent) :
    cwTask(parent)
{

}

void cwWallsImporter::warning(QString message)
{
    emit statusMessage("WARNING: " + message);
}

void cwWallsImporter::runTask()
{
    Length::init();
    Angle::init();

    StationMap.clear();

    Caves.clear();
    Caves.append(cwCave());
    cwCave* cave = &Caves.last();
    cave->setName("Walls Import");
    StationRenamer.setCave(cave);

    QList<cwTrip*> trips;

    foreach(QString filename, WallsDataFiles)
    {
        parseFile(filename, trips);
    }

    foreach (cwTrip* trip, trips)
    {
        // apply StationMap replacements to support Walls' station-LRUD lines
        foreach (cwSurveyChunk* chunk, trip->chunks())
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

        cave->addTrip(trip);
    }

    done();
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

bool cwWallsImporter::parseFile(QString filename, QList<cwTrip*>& tripsOut)
{
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
    WallsImporterVisitor visitor(&parser, this, justFilename + '-');
    parser.setVisitor(&visitor);

    QObject::connect(&visitor, &WallsImporterVisitor::warning, this, &cwWallsImporter::warning);
//    PrintingWallsVisitor printingVisitor;
//    MultiWallsVisitor multiVisitor({&printingVisitor, &visitor});
//    parser.setVisitor(&multiVisitor);

    bool failed = false;

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
        }
        catch (const SegmentParseExpectedException& ex)
        {
            emit statusMessage("Error: " + ex.message());
            failed = true;
            break;
        }
        catch (const SegmentParseException& ex)
        {
            emit statusMessage("Error: " + ex.message());
            failed = true;
            break;
        }

        lineNumber++;
    }

    file.close();

    if (!failed)
    {
        tripsOut << visitor.trips();
        emit statusMessage(QString("Parsed file %1").arg(filename));
    }
    else
    {
        emit statusMessage(QString("Skipping file %1 due to errors").arg(filename));
        foreach (cwTrip* trip, visitor.trips())
        {
            delete trip;
        }
    }

    return !failed;
}
