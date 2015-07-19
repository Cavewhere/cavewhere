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

class WallsImporterVisitor : public CapturingWallsVisitor
{
public:
    WallsImporterVisitor(WallsParser* parser, cwStationRenamer* stationRenamer, QString tripNamePrefix)
        : Parser(parser),
          StationRenamer(stationRenamer),
          TripNamePrefix(tripNamePrefix),
          Trips(QList<cwTrip*>()),
          CurrentTrip(NULL)
    {

    }

    void ensureValidTrip()
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

    virtual void endFixLine()
    {
        ensureValidTrip();
        // TODO
    }

    virtual void endVectorLine()
    {
        ensureValidTrip();

        cwStation fromStation = StationRenamer->createStation(Parser->units()->processStationName(from));

        LengthUnit d_unit = Parser->units()->d_unit;

        cwStation* lrudStation;

        if (Parser->units()->vectorType == VectorType::RECT)
        {
            Parser->units()->rectToCt(north, east, rectUp, distance, frontsightAzimuth, frontsightInclination);
            std::cout << "Converted RECT shot:" << std::endl;
            std::cout << "  distance: " << distance << std::endl;
            std::cout << "  azm:      " << frontsightAzimuth << std::endl;
            std::cout << "  inc:      " << frontsightInclination << std::endl;
        }

        if (distance.isValid())
        {
            cwStation toStation = StationRenamer->createStation(Parser->units()->processStationName(to));
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

            // TODO I think the following may be causing segfaults.
            // How do you put a station by itself in a trip?

//            if (CurrentTrip->chunks().isEmpty() ||
//                CurrentTrip->chunks().last()->stations().last().name() != fromStation.name())
//            {
//                CurrentTrip->addNewChunk();
//                cwSurveyChunk* lastChunk = CurrentTrip->chunks().last();
//                lastChunk->stations() << fromStation;
//                lrudStation = &fromStation;
//            }
//            else
//            {
//                cwSurveyChunk* lastChunk = CurrentTrip->chunks().last();
//                lrudStation = &lastChunk->stations().last();
//            }
        }

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

    virtual void beginUnitsLine()
    {
        priorUnits = *Parser->units();
    }

    virtual void endUnitsLine()
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

    virtual void visitDateLine()
    {
        // when the next vector or fix line sees that
        // CurrentTrip is null, it will create a new one
        CurrentTrip = NULL;
    }

    inline QList<cwTrip*> trips() { return Trips; }

private:
    WallsUnits priorUnits;
    WallsParser* Parser;
    cwStationRenamer* StationRenamer;
    QString TripNamePrefix;
    QList<cwTrip*> Trips;
    cwTrip* CurrentTrip;
    cwSurveyChunk* CurrentChunk;
};

cwWallsImporter::cwWallsImporter(QObject *parent) :
    cwTask(parent)
{

}

void cwWallsImporter::runTask()
{
    Length::init();
    Angle::init();

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
    WallsImporterVisitor visitor(&parser, &StationRenamer, justFilename + '-');
    parser.setVisitor(&visitor);

    bool failed = false;

    int lineNumber = 0;
    while (!file.atEnd())
    {
        QString line = file.readLine();
        if (file.error() != QFile::NoError)
        {
            emit statusMessage(QString("Error reading from file %1 at line %2: %3")
                               .arg(filename)
                               .arg(lineNumber)
                               .arg(file.errorString()));
            failed = true;
            break;
        }

        parser.reset(Segment(line, filename, lineNumber, 0));

        try
        {
            parser.parseLine();
        }
        catch (const SegmentParseExpectedException& ex)
        {
            emit statusMessage(ex.message());
            failed = true;
            break;
        }
        catch (const SegmentParseException& ex)
        {
            emit statusMessage(ex.message());
            failed = true;
            break;
        }

        lineNumber++;
    }

    file.close();

    if (!failed)
    {
        tripsOut << visitor.trips();
    }
    else
    {
        foreach (cwTrip* trip, visitor.trips())
        {
            delete trip;
        }
    }

    return !failed;
}
