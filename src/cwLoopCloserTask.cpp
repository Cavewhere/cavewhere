/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwLoopCloserTask.h"
#include "cwCavingRegion.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwCave.h"
#include "cwMath.h"
#include "cwDebug.h"
#include "cwStationPosition.h"

//Qt includes
#include <QElapsedTimer>
#include <QLinkedList>
#include <QMatrix3x3>

cwLoopCloserTask::cwLoopCloserTask(QObject *parent) :
    cwTask(parent)
{
}

/**
 * @brief cwLoopCloserTask::setRegion
 * @param region
 *
 * Sets the caving region of all the caves and surveyes to process
 */
void cwLoopCloserTask::setRegion(cwCavingRegion *region)
{
    Region = region;
}

QMatrix3x3 cwLoopCloserTask::toQMatrix3x3(const arma::mat& matrix)
{
    Q_ASSERT(matrix.n_rows == 3);
    Q_ASSERT(matrix.n_cols == 3);

    QMatrix3x3 newMatrix;
    for(int r = 0; r < 3; r++) {
        for(int c = 0; c < 3; c++) {
            newMatrix(r, c) = matrix(r, c);
        }
    }

    return newMatrix;
}

/**
 * @brief cwLoopCloserTask::runTask
 */
void cwLoopCloserTask::runTask()
{
    QElapsedTimer timer;
    timer.start();

    //Do the loop closure for all caves
    foreach(cwCave* cave, Region->caves()) {
        processCave(cave);
    }

    qDebug() << "Loop closure task done in:" << timer.elapsed() << "ms";

    done();
}

/**
 * @brief cwLoopCloserTask::processCave
 * @param cave
 *
 * This process a unconnected cave
 */
void cwLoopCloserTask::processCave(cwCave *cave)
{
    //--------------------------- Split all chunks at intersections ------------------
    //Find all main edges, (main edge only store intersection at the first and last station
    cwMainEdgeProcessor mainEdgeProcessor;
    QList<cwEdgeSurveyChunk*> edges = mainEdgeProcessor.mainEdges(cave);

    if(edges.isEmpty()) { return; }

    //Print out edge results
    printEdges(edges);

    //For Debugging, this makes sure that station don't exist in the middle of the edge
    checkBasicEdges(edges);

    //---------------------------- Process all the shot directions -------------------
    cwShotProcessor shotProcessor;
    shotProcessor.setShotStd(Region->shotStd()->data());
    shotProcessor.process(edges);

    //---------------------------- Detect loops and legs -----------------------------
    //Split loops out from survey legs
    cwLoopDetector loopDetector;
    loopDetector.process(edges);
    //    printLoops(loopDetector.loops());

    //--------------------------- Setup fixed position ------------------------------
    //This is currently a hack, but this should be replaced with fixed position
    //entered by the user, if user didn't enter any fixed positons, then set the
    //first station to (0, 0, 0);
    cwStationPositionLookup fixedStationLookup;
    QString firstStation = edges.first()->stations().first().name();
    fixedStationLookup.setPosition(firstStation, QVector3D(0.0, 0.0, 0.0));

    //---------------------------- Solve the network ---------------------------------
    cwNetworkSolver networkSolver;
    networkSolver.setEdgeLegs(loopDetector.legEdges());
    networkSolver.setLoops(loopDetector.loops());
    networkSolver.setInitialFixedStations(fixedStationLookup);
    networkSolver.process();

    //---------------------------- Process least squares -----------------------------
    //    cwLeastSquares leastSquares;
    //    leastSquares.process(edges);

    //Set the cave's station postions
    //    cave->setStationPositionLookup(leastSquares.stationPositionLookup());
    cave->setStationPositionLookup(networkSolver.stationPositionLookup());

    //    printEdges(edges);

    //---------------------------- Process all Legs ----------------------------------


    //---------------------------- Find standard devation for all loops ---------------

    //---------------------------- Sort loops by stand devation ----------------------

    //---------------------------- Close loops in order of best fit to worst fit -----

    //---------------------------- Offset all loops and legs -------------------------

    //---------------------------- Return results ------------------------------------

}

/**
 * @brief cwLoopCloserTask::printEdges
 * @param edges
 *
 * This is for debugging, this print's out all the edges and stations
 */
void cwLoopCloserTask::printEdges(QList<cwLoopCloserTask::cwEdgeSurveyChunk *> edges) const
{
    //Debugging for the resulting edges
    foreach(cwEdgeSurveyChunk* edge, edges) {
        qDebug() << "--Edge:" << edge;

        for(int i = 0; i < edge->stations().size() - 1; i++) {
            cwStation toStation = edge->stations().at(i + 1);
            cwStation fromStation = edge->stations().at(i);
//            QList<cwShotVector> shotVectors = shotVector(i);

            qDebug() << "\t" << fromStation.name() + "to" + toStation.name(); //" there are "  + shotVectors.size() + "shots";

//            foreach(cwShotVector vector, shotVectors) {
//                qDebug() << "\t\t Dir:" << vector.direction() << "vec:" << vector.vector();
//                qDebug() << "\t\t covarience:" << vector.covarienceMatrix();
//            }
        }
    }
    qDebug() << "*********** end of edges ***********";
}

/**
 * @brief cwLoopCloserTask::checkBasicEdges
 * @param edges
 *
 * For Debugging, this makes sure that station don't exist in the middle of the edge
 * End points on the survey chunk are the intersections.  The middle station in the edge
 * exist only once in the cave. This makes sure the middle stations only exist once in
 * the whole cave.
 */
void cwLoopCloserTask::checkBasicEdges(QList<cwEdgeSurveyChunk*> edges) const {
    foreach(cwEdgeSurveyChunk* edge, edges) {
        foreach(cwEdgeSurveyChunk* edge2, edges) {
            if(edge != edge2) {
                for(int i = 1; i < edge->stations().size() - 1; i++) {
                    for(int ii = 1; ii < edge2->stations().size() - 1; ii++) {
                        QString stationName1 = edge->stations().at(i).name();
                        QString stationName2 = edge2->stations().at(ii).name();
                        if(stationName1.compare(stationName2, Qt::CaseInsensitive) == 0) {
                            qDebug() << "This is a bug!, stations shouldn't be repeated in the middle of the survey chunk" << stationName1 << "==" << stationName2 << LOCATION;
                        }
                    }
                }
            } else {
                if(!edge->stations().isEmpty()) {
                    QString firstStation = edge->stations().first().name();
                    QString lastStation = edge->stations().last().name();

                    for(int i = 1; i < edge->stations().size() - 1; i++) {
                        QString station = edge->stations().at(i).name();
                        if(firstStation.compare(station, Qt::CaseInsensitive) == 0) {
                            qDebug() << "This is a bug!, a mid station is being repeated, chunk should be broken into bits" << firstStation << station << LOCATION;
                        }

                        if(lastStation.compare(station, Qt::CaseInsensitive) == 0) {
                            qDebug() << "This is a bug!, a mid station is being repeated, chunk should be broken into bits" << lastStation << station << LOCATION;
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief cwLoopCloserTask::printLoops
 * @param loops
 */
void cwLoopCloserTask::printLoops(QList<cwLoopCloserTask::cwLoop> loops) const
{
    int count = 0;
    foreach(cwLoop loop, loops) {
        qDebug() << "-----***-----Loop" << count << "-------***----------";
        printEdges(loop.edges().toList());
        qDebug() << "-----***-----End of Loop--------***------------";
        count++;
    }
}

/**
 * @brief cwLoopCloserTask::mainEdges
 * @param cave
 * @return All the main survey edges in the cave.
 *
 * This will break the survey chunks into small chunks. The smaller chunk has no intersections
 * in the middle of the chunk.  All intersection happend at stations at the beginning and the
 * end of the chunks.  (A survey chunk is made up of consectutive stations and shots)
 *
 * Finding the main edges is useful for finding basic (smallest) loops in the cave. This drastically
 * reduces computationial effort of the loop dectection algroithm
 */
QList<cwLoopCloserTask::cwEdgeSurveyChunk*> cwLoopCloserTask::cwMainEdgeProcessor::mainEdges(cwCave *cave)
{

    foreach(cwTrip* trip, cave->trips()) {
        foreach(cwSurveyChunk* chunk, trip->chunks()) {

            if(!chunk->isValid()) {
                continue;
            }

            if(chunk->stations().size() == 2 &&
                    chunk->stations().first().name().isEmpty() &&
                    chunk->stations().last().name().isEmpty() &&
                    !chunk->shots().first().isValid())
            {
                continue;
            }

            QList<cwStation> stations = chunk->stations();
            QList<cwShot> shots = chunk->shots();

            if(stations.last().name().isEmpty() &&
                    !shots.last().isValid())
            {
                stations.removeLast();
                shots.removeLast();
            }

            if(shots.isEmpty()) {
                continue;
            }

            //Make sure all the shots are valid
            bool foundInvalidShot = false;
            foreach(cwShot shot, shots) {
                if(!shot.isValid()) {
                    qDebug() << "Shot is invalid!";
                    //TODO We need to emit an error the the user
                    foundInvalidShot = true;
                    continue;
                }
            }

            if(foundInvalidShot) { continue; }

            //Add survey chunk, split if nessarcy
            cwEdgeSurveyChunk* edgeChunk = new cwEdgeSurveyChunk();
            edgeChunk->setShots(shots);
            edgeChunk->setStations(stations);
            edgeChunk->setCalibration(trip->calibrations());

            addEdgeChunk(edgeChunk);
        }
    }

    splitOnRepeatingEdges();

    return ResultingEdges; //.toList();
}

/**
 * @brief cwLoopCloserTask::addEdgeChunk
 * @param chunk
 */
void cwLoopCloserTask::cwMainEdgeProcessor::addEdgeChunk(cwLoopCloserTask::cwEdgeSurveyChunk* newChunk)
{
    //This removes internal loops inside of newChunk
    QList<cwLoopCloserTask::cwEdgeSurveyChunk*> newChunks = splitChunkLocally(newChunk);

    foreach(cwLoopCloserTask::cwEdgeSurveyChunk* chunk, newChunks) {

        QList<cwStation> stations = chunk->stations();

        bool newChunk = true;

        for(int i = 0; i < stations.size(); i++) {
            cwStation station = stations.at(i);

            qDebug() << "Lookup contains station:" << station.name() << Lookup.contains(station.name());

            if(Lookup.contains(station.name())) {
                //Station is an intersection

                if(i == 0 || i == stations.size() - 1) {
                    //This is the first or last station in the edgeChunk
                    splitOnStation(station);
                } else {
                    //This is a middle station in the newChunk, split it and potentially another chunk
                    //Split new chunk
                    cwEdgeSurveyChunk* otherNewHalf = chunk->split(station.name());

                    //Split the other station alread in the lookup on station.name(), if it can
                    splitOnStation(station.name());

                    ResultingEdges.append(chunk);
                    addStationInEdgeChunk(chunk);

                    //Update the station, and restart the search with the other half
                    stations = otherNewHalf->stations();
                    chunk = otherNewHalf;
                    i = -1;
                    newChunk = false;
                }
            }
        }

        if(newChunk) {
            ResultingEdges.append(chunk);
            addStationInEdgeChunk(chunk);
        }
    }
}

/**
 * @brief cwLoopCloserTask::splitOnStation
 * @param station
 * @param chunkLoopup
 */
void cwLoopCloserTask::cwMainEdgeProcessor::splitOnStation(cwStation station)
{
    QList<cwEdgeSurveyChunk*> edgeChunks = Lookup.values(station.name());

    qDebug() << "Split on station:" << station.name();

    foreach(cwEdgeSurveyChunk* chunk, edgeChunks) {
        if(chunk->stations().first().name() != station.name() &&
                chunk->stations().last().name() != station.name())
        {
            //Split otherChunk into two seperate bits, if station falls in the middle
            cwEdgeSurveyChunk* otherHalf = chunk->split(station.name());

            //Remove all occurancies of station found in otherHalf from chunk
            QListIterator<cwStation> iter(otherHalf->stations());
            iter.next(); //Skip first station, because it's still in chunk as the last station
            while(iter.hasNext()) {
                cwStation station = iter.next();
                Lookup.remove(station.name(), chunk);
            }

            addStationInEdgeChunk(otherHalf);
            //            qDebug() << "Appending newChunk:" << otherHalf << LOCATION;
            //            Q_ASSERT(!ResultingEdges.contains(otherHalf));
            ResultingEdges.append(otherHalf);
        }
    }
}

/**
 * @brief cwLoopCloserTask::cwMainEdgeProcessor::splitOnRepeatingEdges
 *
 * This splits the loop on repeating edge.
 *
 * For example two end points could have two edges connecting them:
 *    _
 *  /   \
 * A  -  B
 * |     |
 *  \ C /
 *
 * A and B have two edges connecting them (top one, and the middle one). This can
 * cause problems with loop detector algorithm, because it can't detect basic loops
 * from this.  This function will split at least one of them so that it works:
 *
 *    D
 *  /   \
 * A  -  B
 * |     |
 *  \ C /
 *
 * You can see that after running this function station D split one of the paths
 * between A and B.
 */
void cwLoopCloserTask::cwMainEdgeProcessor::splitOnRepeatingEdges()
{
    QMultiHash<QString, cwEdgeSurveyChunk*> endStations;

    //Add all the edge to the resulting edges
    foreach(cwEdgeSurveyChunk* edge, ResultingEdges) {
        QString key1 = edge->stations().first().name() + edge->stations().last().name();
        QString key2 = edge->stations().last().name() + edge->stations().first().name();
        endStations.insertMulti(key1, edge);
        endStations.insertMulti(key2, edge);
    }

    QSet<cwEdgeSurveyChunk*> processed;

    //Go through all the end stations
    foreach(QString firstLastStationKey, endStations.keys()) {
        QList<cwEdgeSurveyChunk*> edges = endStations.values(firstLastStationKey);
        if(edges.size() >= 2) {
            foreach(cwEdgeSurveyChunk* edge, edges) {

                if(processed.contains(edge)) {
                    //Already processed these edges
                    break;
                }

                //Break the edge into pieces
                if(edge->stations().size() > 2) {
                    //Break the edge on the second station
                    QString secondStation = edge->stations().at(1).name();
                    cwEdgeSurveyChunk* newEdge = edge->split(secondStation);
                    ResultingEdges.append(newEdge);
                }

                processed.insert(edge);
            }
        }
    }
}

/**
 * @brief cwLoopCloserTask::cwMainEdgeProcessor::mergeChunk
 * @param chunk
 * @return
 *
 * This will attempt to merge chunk, with all the other pre-existing chunks.
 *
 * The chunk can only be merged with another chunk, if the first of last station matches with another
 * already existing chunks, first and last station.
 *
 * If this returns true, the chunk has been delete, and it's data has been merge into an already existing
 * chunk.
 */
//bool cwLoopCloserTask::cwMainEdgeProcessor::mergeChunk(cwLoopCloserTask::cwEdgeSurveyChunk *chunk)
//{
//    bool merged = mergeChunkUsingStation(chunk->stations().first(), chunk);
//    if(!merged) {
//        merged = mergeChunkUsingStation(chunk->stations().last(), chunk);
//    }
//    return merged;
//}

//bool cwLoopCloserTask::cwMainEdgeProcessor::mergeChunkUsingStation(cwStation station, cwLoopCloserTask::cwEdgeSurveyChunk *chunk)
//{
//    if(Lookup.contains(station.name())) {
//        QList<cwEdgeSurveyChunk*> chunks = Lookup.values(station.name());
//        foreach(cwEdgeSurveyChunk* currentChunk, chunks) {
//            if(chunk != currentChunk) {

//                cwStation firstStation = currentChunk->stations().first();
//                cwStation lastStation = currentChunk->stations().last();

//                //Does the current chunk make an internal loop
//                if(firstStation == lastStation) {
//                    //It makes an internal loop in itself, can't merge with a loop
//                    continue;
//                }

//                if(station == firstStation) {

//                } else if(station == lastStation) {

//                }

//            }
//        }
//    }
//    return false;
//}

/**
 * @brief cwLoopCloserTask::cwMainEdgeProcessor::splitChunkLocally
 * @param chunk
 * @return
 *
 * This splits the chunk if the chunk has a loop inside of it. It will split chunk at local loop in the chunk. Usually this will
 * return a list with just one element, because chunk usually doesn't have any internal loops, but if it does, it will return multiple chunks.
 */
QList<cwLoopCloserTask::cwEdgeSurveyChunk *> cwLoopCloserTask::cwMainEdgeProcessor::splitChunkLocally(cwLoopCloserTask::cwEdgeSurveyChunk *chunk)
{
    QList<cwLoopCloserTask::cwEdgeSurveyChunk *> splitChunks;

    //Split on first station
    QString firstStation = chunk->stations().first().name();
    QString lastStation = chunk->stations().last().name();
    for(int i = 1; i < chunk->stations().size() - 1; i++) {
        QString currentStationName = chunk->stations().at(i).name();

        bool stationEqualsFirstStation = firstStation.compare(currentStationName, Qt::CaseInsensitive) == 0;
        bool stationEqualsLastStation = lastStation.compare(currentStationName, Qt::CaseInsensitive) == 0;

        if(stationEqualsFirstStation || stationEqualsLastStation) {
            //Split on station
            cwEdgeSurveyChunk* otherHalf;
            if(stationEqualsFirstStation) {
                otherHalf = chunk->split(firstStation);
            } else if(stationEqualsLastStation) {
                otherHalf = chunk->split(lastStation);
            } else {
                otherHalf = NULL;
                Q_ASSERT(false);
            }

            splitChunks.append(chunk);

            //Restart the for-loop
            chunk = otherHalf;
            i = 1;
        }
    }

    splitChunks.append(chunk);

    return splitChunks;
}

/**
 * @brief cwLoopCloserTask::cwEdgeSurveyChunk::cwEdgeSurveyChunk
 */
cwLoopCloserTask::cwEdgeSurveyChunk::cwEdgeSurveyChunk() :
    Calibration(NULL)
{

}

cwStation &cwLoopCloserTask::cwEdgeSurveyChunk::firstStation()
{
    return Stations.first();
}

/**
 * @brief cwLoopCloserTask::cwEdgeSurveyChunk::setCalibration
 * @param calibration
 */
void cwLoopCloserTask::cwEdgeSurveyChunk::setCalibration(cwTripCalibration *calibration)
{
    Calibration = calibration;
}

/**
 * @brief cwLoopCloserTask::cwEdgeSurveyChunk::calibration
 * @return
 */
cwTripCalibration *cwLoopCloserTask::cwEdgeSurveyChunk::calibration() const
{
    return Calibration;
}

/**
 * @brief cwLoopCloserTask::cwChunkLookup::addStationInEdgeChunk
 * @param chunk
 *
 * Adds all the station from chunk into this lookup
 */
void cwLoopCloserTask::cwMainEdgeProcessor::addStationInEdgeChunk(cwLoopCloserTask::cwEdgeSurveyChunk *chunk)
{
    foreach(cwStation station, chunk->stations()) {
        qDebug() << "Inserting station name into lookup:" << station.name() << "chunk:" << chunk;
        Lookup.insert(station.name(), chunk);
    }
}

/**
 * @brief cwLoopCloserTask::cwEdgeSurveyChunk::split
 * @param stationName
 * @return Returns the second half of the survey chunk.  If the chunk couldn't be split this return's NULL
 *
 * The chunk will be split by stationName.  The current chunk's last station will have stationName, and
 * first station in chunk that's return will have the station name.  This function does nothing, and returns
 * NULL, if stationName is first or last station in the chunk. This returns NULL if there's no station
 * in the current chunk.
 */
cwLoopCloserTask::cwEdgeSurveyChunk *cwLoopCloserTask::cwEdgeSurveyChunk::split(QString stationName)
{
    //Make sure the station isn't the beginning or the last station
    Q_ASSERT(!Stations.isEmpty());
    //    Q_ASSERT(Stations.first().name().compare(stationName, Qt::CaseInsensitive) != 0);
    //    Q_ASSERT(Stations.last().name().compare(stationName, Qt::CaseInsensitive) != 0);

    for(int i = 1; i < Stations.size() - 1; i++) {
        if(Stations.at(i).name().compare(stationName, Qt::CaseInsensitive) == 0) {
            //Split at the current station
            QList<cwStation> newChunkStations = Stations.mid(i);
            Stations.erase(Stations.begin() + i + 1, Stations.end());

            QList<cwShot> newChunkShots = Shots.mid(i);
            Shots.erase(Shots.begin() + i, Shots.end());

            cwEdgeSurveyChunk* newChunk = new cwEdgeSurveyChunk();
            newChunk->setStations(newChunkStations);
            newChunk->setShots(newChunkShots);
            newChunk->setCalibration(calibration());
            return newChunk;
        }
    }

    Q_ASSERT(false); //Couldn't find station in chunk

    return NULL;
}

/**
 * @brief cwLoopCloserTask::cwEdgeSurveyChunk::addShotVector
 * @param shotIndex
 * @param direction
 *
 * Adds the shot vector. This is the vector's direction and covarience of the shot.
 */
void cwLoopCloserTask::cwEdgeSurveyChunk::addShotVector(cwLoopCloserTask::cwShotVector direction)
{
    ShotVectors.append(direction);
}

/**
 * @brief cwLoopCloserTask::cwEdgeSurveyChunk::shotVector
 * @param shotIndex
 * @return
 */
QList<cwLoopCloserTask::cwShotVector> cwLoopCloserTask::cwEdgeSurveyChunk::shotVectors() const {
    return ShotVectors;
}

cwLoopCloserTask::cwShotVector &cwLoopCloserTask::cwEdgeSurveyChunk::firstShotVector()
{
    return ShotVectors.first();
}


/**
 * @brief cwLoopCloserTask::cwLoopDetector::loopEdges
 * @return Returns all the edges that belong to at least one loop
 *
 * This uses Holton method for minimal cycle basis
 */
void cwLoopCloserTask::cwLoopDetector::process(QList<cwLoopCloserTask::cwEdgeSurveyChunk *> edges)
{
    if(edges.isEmpty()) { return; }

    Edges = edges;

    qDebug() << "Edges:" << Edges.size();

    //Find all the loops from the edges, Start with the first station
    QString firstStation = edges.first()->stations().first().name();
    QList<cwLoop> initialLoops = findLoops(firstStation);

    qDebug() << "InitialLoops:" << initialLoops.size();

    //Find all the uniqueLoops (this finds all the possible loops)
    QList<cwLoop> uniqueLoops = findUniqueLoops(initialLoops);

    qDebug() << "UniqueLoops:" << uniqueLoops.size();

    //Do gassiumn elimanation on loops to find basic loops
//    QList<cwLoop> basicLoops = minimizeLoops(uniqueLoops);

    //Group uniqueLoops
    QList<cwLoopGroup> loopGroups = groupLoops(uniqueLoops);

//    maximizeLoops(loopGroups);

    //Debugging print out all the loopGroups
//    int count = 0;
//    qDebug() << "There are " << loopGroups.size() << "loop groups.";
//    foreach(cwLoopGroup group, loopGroups) {
//        qDebug() << "########## Loop group" << count << "########";
//        group.printLoops();
//        count++;
//    }

    //Sort the groups by stdev
    sortLoopGroups(loopGroups);

    //Find leg edges
    LegEdges = findLegEdges(edges, uniqueLoops);
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::createEdgeLookup
 *
 * This goes through all the edges and maps the first and last station to the multi hash.  This will allow use
 * to lookup all the edges quickly
 */
void cwLoopCloserTask::cwLoopDetector::createEdgeLookup()
{
    foreach(cwEdgeSurveyChunk* edge, Edges) {
        if(edge->stations().size() >= 2) {
            QString firstStation = edge->stations().first().name();
            QString lastStation = edge->stations().last().name();
            EdgeLookup.insert(firstStation, edge);
            EdgeLookup.insert(lastStation, edge);
        }
    }
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::findLoops
 * @param station
 * @return
 */
QList<cwLoopCloserTask::cwLoop> cwLoopCloserTask::cwLoopDetector::findLoops(QString station)
{
    QList<cwEdgeSurveyChunk*> path;
    QList<cwLoop> resultLoops;

    //Create the lookup for all the edges
    createEdgeLookup();

    findLoopsHelper(station, resultLoops, path);

    VisitedEdges.clear();
    VisitedStations.clear();

    return resultLoops;
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::findLoops
 * @param station
 *
 * This does a depth first search to find all the loops in the graph. This will populate Loops data.
 */
void cwLoopCloserTask::cwLoopDetector::findLoopsHelper(QString station, QList<cwLoop>& resultLoops, QList<cwEdgeSurveyChunk *> path)
{
    qDebug() << "Finding loop for station:" << station;

    //    printEdges(path);

    if(VisitedStations.contains(station)) {
        qDebug() << "Station alread vistied:" << station;

        //Already visited this station, created a loop
        cwLoop loop;

        QSet<cwEdgeSurveyChunk *> loopPath;

        if(path.size() == 1) {
            //Handles the case where path 1 and first and last stations are equal to each other.
            //The edge forms a loop on itself
            cwEdgeSurveyChunk* edge = path.first();
            QString firstStation = edge->stations().first().name();
            QString lastStation = edge->stations().last().name();

            if(firstStation.compare(lastStation, Qt::CaseInsensitive) == 0) {
                loopPath.insert(edge);
            }
        } else {
            //Go up the path until we find the station
            for(int i = path.size() - 2; i >= 0; i--) {
                cwEdgeSurveyChunk* edge = path.at(i);
                QString firstStation = edge->stations().first().name();
                QString lastStation = edge->stations().last().name();
                if(firstStation.compare(station, Qt::CaseInsensitive) == 0 ||
                        lastStation.compare(station, Qt::CaseInsensitive) == 0) {
                    //                qDebug() << "Middle is =" << i << path.size();
                    loopPath = QSet<cwEdgeSurveyChunk*>::fromList(path.mid(i));
                    break;
                }
            }
        }

        qDebug() << "Path size:" << path.size();
        Q_ASSERT(!loopPath.isEmpty()); //The loop shouldn't be empty!  You should have found a path for the loop

        loop.setEdges(loopPath);
        resultLoops.append(loop);

        qDebug() << "Added loop--";
        printEdges(loopPath.toList());

        return;
    }

    VisitedStations.insert(station);

//    qDebug() << "Visiting station:" << station;

    QList<cwEdgeSurveyChunk*> edges = EdgeLookup.values(station);

    foreach(cwEdgeSurveyChunk* edge, edges) {
        if(!VisitedEdges.contains(edge)) {

            VisitedEdges.insert(edge);

            qDebug() << "VisitedEdge:" << edge << "for station" << station;
            printEdges(QList<cwEdgeSurveyChunk*>() << edge);

            QList<cwEdgeSurveyChunk*> newPath = path;
            newPath.append(edge);

            QString firstStation = edge->stations().first().name();
            QString secondStation = edge->stations().last().name();

            if(station.compare(firstStation, Qt::CaseInsensitive) != 0) {
                findLoopsHelper(firstStation, resultLoops, newPath); //Recursive Calls
            } else {
                findLoopsHelper(secondStation, resultLoops, newPath); //Recursive Calls
            }
        }
    }
}

QList<cwLoopCloserTask::cwLoop> cwLoopCloserTask::cwLoopDetector::findUniqueLoops(QList<cwLoopCloserTask::cwLoop> initialLoops)
{
    QList<cwLoop> uniqueLoops; //All the loops from each station in the initialLoops
    QSet<QString> stationsInLoops;
    QSet<cwEdgeSurveyChunk*> edgesInLoops;

    //Find stations that exist in the loops
    foreach(cwLoop loop, initialLoops) {
        foreach(cwEdgeSurveyChunk* edge, loop.edges()) {
            stationsInLoops.insert(edge->stations().first().name());
            stationsInLoops.insert(edge->stations().last().name());
            edgesInLoops.insert(edge);
        }
    }

    foreach(QString station, stationsInLoops) {
        //Create a new loop detector and find all the new loops, using a different station
        //This section could be threaded
        cwLoopDetector loopDetector;
        loopDetector.Edges = edgesInLoops.toList();
        QList<cwLoop> loops = loopDetector.findLoops(station);

        qDebug() << "Found loops for: " << station;
        printLoops(loops);
        qDebug() << "------------------------------ ##########";

        //Add the loops that we just found, but make sure they're are unqiue
        //To thread this this must be moved out of this foreach loop, this is combind part of the threading
        foreach(cwLoop loop, loops) {
            if(!uniqueLoops.contains(loop)) {
                uniqueLoops.append(loop);
            }
        }
    }

    return uniqueLoops;
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::groupUniqueLoops
 * @param uniqueLoops
 * @return
 *
 * This groups the unique loops together.  A loop group is where two or more loops share an edge.
 * If group only has one loop in it, then it doesn't share any edges with any other loops.
 *
 * A loop can't be in two seperate loop groups. A loop is own by one and only one loop group
 */
QList<cwLoopCloserTask::cwLoopGroup> cwLoopCloserTask::cwLoopDetector::groupLoops(QList<cwLoopCloserTask::cwLoop> uniqueLoops)
{

    //Copy unique loops to a linked list
    QLinkedList<cwLoop> loops;
    QList<cwLoop>::iterator iter = uniqueLoops.begin();
    for(; iter != uniqueLoops.end(); iter++) {
        loops.append(*iter);
    }

    QList<cwLoopGroup> loopGroups;

    qDebug() << "Number of loops:" << loops.size() << uniqueLoops.size();

    while(!loops.isEmpty()) {
        cwLoop loop = loops.first();
        loops.removeFirst();

        cwLoopGroup loopGroup;
        loopGroup.appendLoop(loop);

        bool hasMoreToProcess = true;
        while(hasMoreToProcess) {

            QLinkedList<cwLoop>::iterator iter = loops.begin();
            hasMoreToProcess = false;
            for(; iter != loops.end(); iter++) {
                cwLoop currentLoop = *iter;
                if(loopGroup.canAddLoop(currentLoop)) {
                    loopGroup.appendLoop(currentLoop);
                    iter = loops.erase(iter); //Remove the add group from the list of loops
                    hasMoreToProcess = true;
                }
            }
        }

        loopGroups.append(loopGroup);
    }

    return loopGroups;
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::minimizeLoops
 *
 * This uses gaussian elimination to find all the minimal loops
 */
QList<cwLoopCloserTask::cwLoop> cwLoopCloserTask::cwLoopDetector::minimizeLoops(QList<cwLoop> loops) {

    //Sort edges by weight
    qStableSort(loops);

    //Find all the loop edges, and index them
    QHash<cwEdgeSurveyChunk*, int> loopEdgesToColumnIndex;
    QHash<int, cwEdgeSurveyChunk*> columnIndexToLoopEdges;
    int index = 0;
    foreach(cwLoop loop, loops) {
        foreach(cwEdgeSurveyChunk* edge, loop.edges()) {
            if(!loopEdgesToColumnIndex.contains(edge)) {
                loopEdgesToColumnIndex.insert(edge, index);
                columnIndexToLoopEdges.insert(index, edge);
                ++index;
            }
        }
    }

    //Create the bool matrix
    QVector<char> matrix; //Matrix with bytes
    int columns = loopEdgesToColumnIndex.size();
    int rows = loops.size();
    matrix.fill(0, columns * rows); //Fill with zeros to start out with

    //Populate the matrix
    for(int i = 0; i < loops.size(); i++) {
        cwLoop loop = loops.at(i);
        foreach(cwEdgeSurveyChunk* edge, loop.edges()) {
            int column = loopEdgesToColumnIndex.value(edge);
            matrix[i * columns + column] = 1; //Set the matrix
        }
    }

    printMatrix(matrix, columns, rows, columnIndexToLoopEdges);

    QVector<char> tempRow;
    tempRow.resize(columns);

    //Do gaussian elimination to in minimal cycle basics
    //    int currentColumn = 0;
    int lastPivotRow = 0;
    int rowWithZeros = -1;
    for(int i = 0; i < columns && rowWithZeros < 0; i++) {

        //Search each row's at the currentColumn for a pivit point
        for(int pivotRow = lastPivotRow; pivotRow < rows; pivotRow++) {
            int index = pivotRow * columns + i;
            if(matrix.at(index)) {

                //Swap the pivotRow with the lastPivotRow
                if(lastPivotRow != pivotRow) {
                    char* lastPivotRowData = &matrix[lastPivotRow * columns];
                    char* pivotRowData = &matrix[pivotRow * columns];
                    memcpy(tempRow.data(), lastPivotRowData, columns);
                    memcpy(lastPivotRowData, pivotRowData, columns);
                    memcpy(pivotRowData, tempRow.data(), columns);
                }

                pivotRow = lastPivotRow;

                //Subtract all lower rows that also have 1 in the current column
                for(int r = pivotRow + 1; r < rows && rowWithZeros < 0; r++) {
                    if(matrix.at(r * columns + i)) {
                        //Add pivotRow and the current row at r
                        for(int c = i; c < columns; c++) {
                            matrix[r * columns + c] = (matrix[r * columns + c] + matrix[pivotRow * columns + c]) % 2;
                        }

                        //Check if the current row is all zeros
                        bool foundTrueColumn = false;
                        for(int c = i; c < columns; c++) {
                            if(matrix[r * columns + c]) {
                                foundTrueColumn = true;
                            }
                        }

                        if(!foundTrueColumn) {
                            rowWithZeros = r;
                        }
                    }
                }

                lastPivotRow++;
            }
        }

        printMatrix(matrix, columns, rows, columnIndexToLoopEdges);

    }

    //Repopulate minimum loops
    QList<cwLoop> minimumLoops;
    rowWithZeros = rowWithZeros == -1 ? rows : rowWithZeros;
    for(int r = 0; r < rowWithZeros; r++) {
        QSet<cwEdgeSurveyChunk*> edges;
        for(int c = 0; c < columns; c++) {
            if(matrix.at(r * columns + c)) {
                cwEdgeSurveyChunk* edge = columnIndexToLoopEdges.value(c);
                edges.insert(edge);
            }
        }

        cwLoop loop;
        loop.setEdges(edges);
        minimumLoops.append(loop);
    }


    return minimumLoops;
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::maximizeLoops
 * @param loopGroups
 *
 * This will maximize the all the loops in a loop group.  This assumes that the loop group only contains
 * basic cycles.
 */
void cwLoopCloserTask::cwLoopDetector::maximizeLoops(QList<cwLoopCloserTask::cwLoopGroup> &loopGroups)
{
    for(int i = 0; i < loopGroups.size(); i++) {
        loopGroups[i].maximize();
    }
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::findLegEdges
 * @param edges
 * @param uniqueLoops
 * @return
 *
 * This takes all the edges and all the uniqueLoops and compares the to.  All edges, that don't exist
 * in the loops, are the leg Edges.  This function return the leg edges.
 */
QList<cwLoopCloserTask::cwEdgeSurveyChunk *> cwLoopCloserTask::cwLoopDetector::findLegEdges(QList<cwLoopCloserTask::cwEdgeSurveyChunk *> edges,
                                                                                            QList<cwLoopCloserTask::cwLoop> loops) const
{
    //Create a total loopEdge loopup
    QSet<cwEdgeSurveyChunk*> loopEdges;
    foreach(cwLoop loop, loops) {
        loopEdges.unite(loop.edges());
    }

    //Find all the legLeges
    QList<cwEdgeSurveyChunk*> legEdges;
    foreach(cwEdgeSurveyChunk* edge, edges) {
        if(!loopEdges.contains(edge)) {
            legEdges.append(edge);
        }
    }

    qDebug() << "LegEdges:" << legEdges.size();

    return legEdges;
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::sortLoopGroups
 * @param groups
 *
 * This sorts all the loop groups in the loop closure task
 */
void cwLoopCloserTask::cwLoopDetector::sortLoopGroups(QList<cwLoopCloserTask::cwLoopGroup> &groups)
{
    for(int i = 0; i < groups.size(); i++) {
        cwLoopGroup& group = groups[i];
        group.sortLoops();
    }
}

void cwLoopCloserTask::cwLoopDetector::printEdges(QList<cwLoopCloserTask::cwEdgeSurveyChunk *> edges) const
{
    //Debugging for the resulting edges
    foreach(cwEdgeSurveyChunk* edge, edges) {
        qDebug() << "\t--Edge:" << edge;
        foreach(cwStation station, edge->stations()) {
            qDebug() << "\t\t" << station.name();
        }
    }
    qDebug() << "\t*********** end of edges ***********";
}

/**
 * @brief cwLoopCloserTask::cwLoopDetector::printLoops
 * @param loops
 */
void cwLoopCloserTask::cwLoopDetector::printLoops(QList<cwLoopCloserTask::cwLoop> loops) const
{
    int count = 0;
    foreach(cwLoop loop, loops) {
        qDebug() << "-----***-----Loop" << count << "-------***----------";
        printEdges(loop.edges().toList());
        qDebug() << "-----***-----End of Loop--------***------------";
        count++;
    }
}

/**
 * @brief printMatrix
 * @param matrix
 * @param columns
 * @param rows
 * @param columnIndexToLoopEdges
 *
 * THis is a helper function for debugging the gaussian elimination
 */
void cwLoopCloserTask::cwLoopDetector::printMatrix(QVector<char> matrix,
                                                   int columns,
                                                   int rows,
                                                   QHash<int, cwEdgeSurveyChunk*> columnIndexToLoopEdges) {

    qDebug() << "============ Printing out Matrix =============";

    //Print out the header
    QString headerString;
    for(int i = 0; i < columns; i++) {
        cwEdgeSurveyChunk* edge = columnIndexToLoopEdges.value(i, NULL);
        headerString += edge->stations().first().name() + edge->stations().last().name() + " ";
    }
    qDebug() << qPrintable(headerString);

    for(int i = 0; i < rows; i++) {
        QString rowString;
        for(int j = 0; j < columns; j++) {
            rowString += QString(" %1 ").arg((int)matrix.at(i * columns + j));
        }

        qDebug() << qPrintable(rowString);
    }
}


/**
 * @brief cwLoopCloserTask::cwLoop::calculateStd
 *
 * This calculates the std devation of the loop
 */
void cwLoopCloserTask::cwLoop::calculateStd()
{
    Q_ASSERT(!Edges.empty());

    //This will break the loop, as see how far apart stations are
    QSet<cwEdgeSurveyChunk*>::iterator iter = Edges.begin();
    cwEdgeSurveyChunk* firstEdge = *iter;
    QString firstStation = firstEdge->stations().first().name();
    QString newStationName = firstStation + "_";

    //Rename the first station to break the loop
    qDebug() << "Shot vectors:" << firstEdge->shotVectors().first().fromStation() << newStationName;

    firstEdge->firstShotVector().setFromStation(newStationName);
    firstEdge->firstStation().setName(newStationName);

    //Create fix the first station to (0, 0, 0)
    cwStationPositionLookup fixedStationLookup;
    fixedStationLookup.setPosition(newStationName, QVector3D());

    //Create a network solver
    cwNetworkSolver networkSolver;
    networkSolver.setEdgeLegs(Edges.toList()); //Set the network solver edges, there are no loops in this
    networkSolver.setInitialFixedStations(fixedStationLookup);
    networkSolver.process();

    //Get the locations
    cwStationPositionLookup locations = networkSolver.stationPositionLookup();
    QMatrix3x3 loopConvariance = locations.covariance(firstStation); //The is the original position, aka at the end of the loop

    //Calculate the absolute error
    QVector3D p1 = locations.position(firstStation);
    QVector3D p2 = locations.position(newStationName);

    qDebug() << "Loop convariance:" << loopConvariance;
    qDebug() << "p1:" << p1 << "p2:" << p2 << (p2-p1);

    QVector3D std(sqrt(loopConvariance(0,0)), sqrt(loopConvariance(1,1)), sqrt(loopConvariance(2,2)));
    QVector3D absoluteError = p2 - p1;
    QVector3D loopStd = QVector3D(absoluteError.x() / std.x(), absoluteError.y() / std.y(), absoluteError.z() / std.z());
    qDebug() << "Loop std:" <<  absoluteError.length() / std.length() << std.lengthSquared() << absoluteError.length();

    AbsoluteError = absoluteError;
    StdError = loopStd;

    //Reset the first station the first and last station
    firstEdge->firstShotVector().setFromStation(firstStation);
    firstEdge->firstStation().setName(firstStation);

}

QVector3D cwLoopCloserTask::cwLoop::loopLength() const
{
    return QVector3D();
}

QVector3D cwLoopCloserTask::cwLoop::absoluteError() const
{
    return AbsoluteError;
}

QVector3D cwLoopCloserTask::cwLoop::stdError() const
{
    return StdError;
}

bool cwLoopCloserTask::cwLoop::operator ==(const cwLoopCloserTask::cwLoop &other) const
{
    return other.edges() == edges();
}

bool cwLoopCloserTask::cwLoop::operator !=(const cwLoopCloserTask::cwLoop &other) const
{
    return other.edges() != edges();
}

/**
 * @brief cwLoopCloserTask::cwLoop::operator <
 * @param other
 * @return This returns the number of edges in the loop. This is function compares the weight of the loop.
 * The weight is equal to number of edges in the loop. This is used to sort the edges by weight
 */
bool cwLoopCloserTask::cwLoop::operator <(const cwLoopCloserTask::cwLoop &other) const
{
    return edges().size() < other.edges().size();
}

/**
 * @brief cwLoopCloserTask::cwLoop::printEdges
 *
 * Prints all the edges in the shot
 */
void cwLoopCloserTask::cwLoop::printEdges()
{
    //Debugging for the resulting edges
    foreach(cwEdgeSurveyChunk* edge, Edges) {
        qDebug() << "--Edge:" << edge;

        for(int i = 0; i < edge->stations().size() - 1; i++) {
            cwStation toStation = edge->stations().at(i + 1);
            cwStation fromStation = edge->stations().at(i);
//            QList<cwShotVector> shotVectors = shotVector(i);

            qDebug() << "\t" << fromStation.name() + "to" + toStation.name(); // + " there are "  + shotVectors.size() + "shots";

//            foreach(cwShotVector vector, shotVectors) {
//                qDebug() << "\t\t Dir:" << vector.direction() << "vec:" << vector.vector();
//                qDebug() << "\t\t covarience:" << vector.covarienceMatrix();
//            }
        }
    }
    qDebug() << "*********** end of edges ***********";
}

/**
 * @brief cwLoopCloserTask::cwLoop::canCombine
 * @param l1
 * @param l2
 * @return True if l1 can be combined with l2, and false if it can't be combined
 */
bool cwLoopCloserTask::cwLoop::canCombine(const cwLoopCloserTask::cwLoop &l1, const cwLoopCloserTask::cwLoop &l2)
{
    cwLoop smallerLoop;
    cwLoop largerLoop;

    if(l1.edges().size() < l2.edges().size()) {
        smallerLoop = l1;
        largerLoop = l2;
    } else {
        smallerLoop = l2;
        largerLoop = l1;
    }

    int matchCount = 0;
    foreach(cwEdgeSurveyChunk* edgeChunk, smallerLoop.edges()) {
        if(largerLoop.edges().contains(edgeChunk)) {
            matchCount++;
        }
    }

    return matchCount != 0 && matchCount != smallerLoop.edges().size();
}

/**
 * @brief cwLoopCloserTask::cwLoop::combine
 * @param l1
 * @param l2
 * @return Returns a combine loop that contains l1 and l2
 */
cwLoopCloserTask::cwLoop cwLoopCloserTask::cwLoop::combine(const cwLoopCloserTask::cwLoop &l1, const cwLoopCloserTask::cwLoop &l2)
{
    Q_ASSERT(canCombine(l1, l2));

    QSet<cwEdgeSurveyChunk*> allEdges = l1.edges().unite(l2.edges());
    QSet<cwEdgeSurveyChunk*> sharedEdges = l1.edges().intersect(l2.edges());
    QSet<cwEdgeSurveyChunk*> combineLoopEdges = allEdges.subtract(sharedEdges);

    cwLoop combineLoop;
    combineLoop.setEdges(combineLoopEdges);

    return combineLoop;
}

/**
 * @brief cwLoopCloserTask::cwShotProcessor::process
 * @param edges
 * @return Go through all the shots, and modifies the cwEdgeSurveyChunk shotDirection
 */
void cwLoopCloserTask::cwShotProcessor::process(QList<cwEdgeSurveyChunk*> edges) {

    foreach(cwEdgeSurveyChunk* chunk, edges) {
        QList<cwShot> shots = chunk->shots();
        for(int i = 0; i < shots.size(); i++) {
            processShot(chunk, i);
        }
    }
}

/**
 * @brief cwLoopCloserTask::cwShotProcessor::processShot
 * @param shot
 */
void cwLoopCloserTask::cwShotProcessor::processShot(cwEdgeSurveyChunk* chunk, int shotIndex)  {
    const cwShot& shot = chunk->shots().at(shotIndex);
    QString fromStationName = chunk->stations().at(shotIndex).name();
    QString toStationName = chunk->stations().at(shotIndex + 1).name();

    if(shot.distanceState() == cwDistanceStates::Valid) {

        bool hasClino = cwShot::clinoValid(shot.clinoState());
        bool hasCompass = cwShot::compassValid(shot.compassState());
        bool hasBackClino = cwShot::clinoValid(shot.backClinoState());
        bool hasBackCompass = cwShot::compassValid(shot.backCompassState());
        bool clinoUpDown = shot.clinoState() == cwClinoStates::Up || shot.clinoState() == cwClinoStates::Down;
        bool backClinoUpDown = shot.backClinoState() == cwClinoStates::Up || shot.backClinoState() == cwClinoStates::Down;

        bool hasValidFrontSite = (hasClino && hasCompass) || clinoUpDown;
        bool hasValidBackSite = (hasBackClino && hasBackCompass) || backClinoUpDown;

        cwShotVector shotVector; //The shot direction and xyz covarience for the shot

        //Apply calibrations
        cwShot calibratedShot = applyCalibration(chunk->calibration(), shot);

        if(!hasValidFrontSite && !hasValidBackSite) {
            //Special case, where shot is a mix of front and back sites

            if(hasClino && hasBackCompass) {
                //Reverse the back compass so it's a front site
                shotVector = shotTransform(calibratedShot.distance(),
                                           calibratedShot.backCompass() - 180.0,
                                           calibratedShot.clino(),
                                           calibratedShot.clinoState());
            } else if(hasBackClino && hasCompass) {
                //Reverse the back clino so it's a front site
                shotVector = shotTransform(calibratedShot.distance(),
                                           calibratedShot.compass(),
                                           -calibratedShot.backClino(),
                                           calibratedShot.backClinoState());
            } else {
                Q_ASSERT(false); //This should never get here
            }

            //            shotVector.setDirection(cwShotVector::FrontSite);
            shotVector.setFromStation(fromStationName);
            shotVector.setToStation(toStationName);
            chunk->addShotVector(shotVector);
            return;
        }

        if(hasValidFrontSite) {
            shotVector = shotTransform(calibratedShot.distance(),
                                       calibratedShot.compass(),
                                       calibratedShot.clino(),
                                       calibratedShot.clinoState());
            //            shotVector.setDirection(cwShotVector::FrontSite);
            shotVector.setFromStation(fromStationName);
            shotVector.setToStation(toStationName);
            chunk->addShotVector(shotVector);

        } else if(!hasValidBackSite) {
            //Error in shot

            if(hasCompass) {
                //TODO: Send error to the user that there's no clino
                qDebug() << "There's no clino";
            } else if(hasClino) {
                //TODO: Send error to the user that there's no compass
                qDebug() << "There's no compass";
            } else {
                //TODO: Send error to the user that there's not back clino and compass
                qDebug() << "There's no back clino or compass";
            }

            return;
        }

        if(hasValidBackSite) {
            shotVector = shotTransform(calibratedShot.distance(),
                                       calibratedShot.backCompass(),
                                       calibratedShot.backClino(),
                                       calibratedShot.backClinoState());
            //            shotVector.setDirection(cwShotVector::BackSite);
            shotVector.setFromStation(toStationName); //Reverse the station because, this is a backsite
            shotVector.setToStation(fromStationName);
            chunk->addShotVector(shotVector);

        } else if(!hasValidFrontSite) {
            //Error in shot

            if(hasBackCompass) {
                //TODO: Send error to the user that there's no back clino
                qDebug() << "There's no back clino";
            } else if(hasBackClino) {
                //TODO: Send error to the user that there's no back compass
                qDebug() << "There's no back compass";
            } else {
                //TODO: Send error to the user that there's not back clino and compass
                qDebug() << "There's no back clino or compass";
            }

            return;
        }

    } else {
        //TODO: Send error to the user that there's no distance value
        qDebug() << "Error! shot doesn't have distance! You need to send error to user.";

    }

}

/**
 * @brief normalShotTransform
 * @param shot
 * @return The direction of the shot based on (x,y,z)
 *
 * Send in a shot, and this calculate the direction of the shot as (x, y, z) vector.
 *
 * X =   sine(A) * cosine(I) * D
 * Y = cosine(A) * cosine(I) * D
 * Z =               sine(I) * D
 *
 * This will also calculate the convarience matrix for (x, y, z) vector.  The convarience matrix is used as
 * weight shots.
 *
 * [covariance matrix of (XYZ)]
 * =
 * [Jacobian of TRANS]
 * [covariance matrix of DAI]
 * [Transpose of Jacobian of TRANS]
 *
 * Jacobian for normal shot transform
 * [  sin(A)*cos(I)         cos(A)*cos(I)*D     -sin(A)*sin(I)*D ]
 * [  cos(A)*cos(I)        -sin(A)*cos(I)*D     -cos(A)*sin(I)*D ]
 * [         sin(I)        0                            cos(I)*D ]
 *
 */
cwLoopCloserTask::cwShotVector cwLoopCloserTask::cwShotProcessor::shotTransform(double distance,
                                                                                double azimith,
                                                                                double clino,
                                                                                cwClinoStates::State clinoState)
{
    //Variance for distance, compass (azimith), clino
    double distanceStd = ShotStdev.DistanceStd; //0.086 FIXME: use shot std for each shot, assume the units are what ever in shot?
    double azimithStd; //= 0.017; //1 Degree std in radians
    double clinoStd; //= 0.017; //1 degree std in radians

    Q_ASSERT(clinoState != cwClinoStates::Empty);
    switch(clinoState) {
    case cwClinoStates::Valid:
        azimithStd = degreeToRadians() * ShotStdev.CompassStd; //1.25; //1.25 Degree std in radians
        clinoStd = degreeToRadians() * ShotStdev.ClinoStd; //1.25; //1.25 Degree std in radians
        break;
    case cwClinoStates::Down:
        azimithStd = 0.0001;
        clinoStd = 0.0001;
        azimith = 0.0;
        clino = -90.0;
        break;
    case cwClinoStates::Up:
        azimithStd = 0.0001;
        clinoStd = 0.0001;
        azimith = 0.0;
        clino = 90.0;
        break;
    default:
        return cwLoopCloserTask::cwShotVector();
    }

    qDebug() << "Radians:" << degreeToRadians() * ShotStdev.CompassStd << degreeToRadians() * ShotStdev.ClinoStd;

    //Convert from degrees to radians
    azimith = azimith * degreeToRadians();
    clino = clino * degreeToRadians();

    double maxClino = degreeToRadians() * 89.9;

    //Calculate the vector
    arma::mat vector = arma::mat::fixed<3, 1>();
    vector(0,0) = sin(azimith) * cos(clino) * distance; //x
    vector(1,0) = cos(azimith) * cos(clino) * distance; //y
    vector(2,0) = sin(clino) * distance; //z

    //Calculate the convarience for the shot
    arma::mat jacobian = arma::mat::fixed<3, 3>();

    if(fabs(clino) < maxClino && distance > 0.0) {
        jacobian(0, 0) = sin(azimith) * cos(clino);
        jacobian(0, 1) = cos(azimith) * cos(clino) * distance;
        jacobian(0, 2) = -sin(azimith) * sin(clino) * distance;

        jacobian(1, 0) = cos(azimith) * cos(clino);
        jacobian(1, 1) = -sin(azimith) * cos(clino) * distance;
        jacobian(1, 2) = -cos(azimith) * sin(clino) * distance;

        jacobian(2, 0) = sin(clino);
        jacobian(2, 1) = 0;
        jacobian(2, 2) = cos(clino) * distance;
    } else {
        jacobian.eye();
    }

    arma::mat dacCovariance = arma::mat::fixed<3, 3>();
    dacCovariance.zeros();
    dacCovariance(0, 0) = distanceStd * distanceStd;
    dacCovariance(1, 1) = azimithStd * azimithStd;
    dacCovariance(2, 2) = clinoStd * clinoStd;

    arma::mat xyzCovariance = jacobian * dacCovariance * jacobian.t();

    cwLoopCloserTask::cwShotVector shotVector;
    shotVector.setVector(vector);

    //    qDebug() << "jacobian:" << jacobian;
    //    qDebug() << "dacConvariance:" << dacCovariance;
    //    qDebug() << "jacobian.t():" << jacobian.t();
    //    qDebug() << "Maginuted: " << arma::det(xyzCovariance);
    //    qDebug() << "Inverse:" << arma::inv(xyzCovariance);
    //    qDebug() << "Mag inverse" << arma::det(arma::inv(xyzCovariance));
    qDebug() << "Converiance Matrix:" << xyzCovariance;

    qDebug() << "std xyz: " << sqrt(xyzCovariance(0,0)) << sqrt(xyzCovariance(1,1)) << sqrt(xyzCovariance(2,2));

    shotVector.setCovarienceMatrix(xyzCovariance);
    return shotVector;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::setVector
 * @param vector a 1x3 matrix, that stores the direction of the shot
 */
cwLoopCloserTask::cwShotVector::cwShotVector() :
    d(new cwShotVectorData())
{

}

void cwLoopCloserTask::cwShotVector::setVector(arma::mat vector) {
    Q_ASSERT(vector.n_cols == 1);
    Q_ASSERT(vector.n_rows == 3);
    d->Vector = vector;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::vector
 * @return The shot's direction a 1x3 matrix
 */
const arma::mat& cwLoopCloserTask::cwShotVector::vector() const {
    return d->Vector;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::setConvarienceMatrix
 * @param convarienceMatrix a 3x3 matrix, that holds the convarience matrix of the shot
 */
void cwLoopCloserTask::cwShotVector::setCovarienceMatrix(arma::mat convarienceMatrix) {
    Q_ASSERT(convarienceMatrix.n_cols == 3);
    Q_ASSERT(convarienceMatrix.n_rows == 3);
    d->CovarianceMatrix = convarienceMatrix;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::convarienceMatrix
 * @return Returns the CovarianceMatrix for the shot
 */
arma::mat cwLoopCloserTask::cwShotVector::covarienceMatrix() const {
    return d->CovarianceMatrix;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::fromStation
 * @return
 */
QString cwLoopCloserTask::cwShotVector::fromStation() const {
    return d->FromStation;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::setFromStation
 * @param fromStation
 */
void cwLoopCloserTask::cwShotVector::setFromStation(QString fromStation) {
    d->FromStation = fromStation;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::toStation
 * @return
 */
QString cwLoopCloserTask::cwShotVector::toStation() const {
    return d->ToStation;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::setToStation
 * @param toStation
 */
void cwLoopCloserTask::cwShotVector::setToStation(QString toStation) {
    d->ToStation = toStation;
}

///**
// * @brief setDirection
// * @param direction - The direction of this shot vector.
// */
//void cwLoopCloserTask::cwShotVector::setDirection(ShotDirection direction) {
//    d->Direction = direction;
//}

///**
// * @brief direction
// * @return Returns the direction of this shot vector
// */
//cwLoopCloserTask::cwShotVector::ShotDirection cwLoopCloserTask::cwShotVector::direction() const {
//    return d->Direction;
//}

/**
 * @brief cwLoopCloserTask::cwLeastSquares::process
 * @param edges
 *
 * Wahoo! Least Squares
 */
void cwLoopCloserTask::cwLeastSquares::process(QList<cwEdgeSurveyChunk*> edges) {

    if(edges.isEmpty()) { return; }

    //Index all the stations name, so we can put them into a matrix
    QHash<QString, int> stationToIndex; //Stores the station name and maps them to column int
    int stationCount = 0;

    //The list of shots the list of edges
    QList<cwShotVector> shots;

    //FIXME: We should probably check if there's previously fixed stations, for now we just fixed the first station
    arma::mat zeroMat = arma::mat::fixed<3,1>(); //The fixed position
    zeroMat.zeros();
    arma::mat fixedWeight = arma::mat::fixed<3,3>(); //Fixed weight
    fixedWeight.eye(); //Identity

    cwStation firstStation = edges.first()->stations().first();
    cwShotVector fixShotVector;
    fixShotVector.setToStation(firstStation.name());
    fixShotVector.setVector(zeroMat);
    fixShotVector.setCovarienceMatrix(fixedWeight);
    shots.append(fixShotVector);

    foreach(cwEdgeSurveyChunk* edge, edges) {
        foreach(cwStation station, edge->stations()) {
            if(!stationToIndex.contains(station.name())) {
                stationToIndex.insert(station.name(), stationCount);
                ++stationCount; //Increase the station count
            }
        }

        //Append all the shot vectors from the shots
        shots.append(edge->shotVectors());
    }

    //Build the shot matrix, observation matrix, weights. All have 3 components and must be times by 3
    arma::mat shotMatrix = arma::mat(shots.size() * 3, 1); //Holds all the shot vectors
    arma::mat observationMatrix = arma::mat(shots.size() * 3, stationToIndex.size() * 3);
    arma::mat weightsMatrix = arma::mat(shots.size() * 3, shots.size() * 3);

    //Fill the fromStationMatrix with -1 and toStationMatrix to 1
    arma::mat fromStationMatrix = arma::mat::fixed<3,3>();
    arma::mat toStationMatrix = arma::mat::fixed<3,3>();
    fromStationMatrix.zeros();
    toStationMatrix.zeros();
    for(int i = 0; i < 3; i++) {
        fromStationMatrix(i,i) = -1;
        toStationMatrix(i,i) = 1;
    }

    //Set the observation and weightsMatrix to all zeros
    observationMatrix.zeros();
    weightsMatrix.zeros();

    //Go through all the shots and populate all the observational and weights matrix
    for(int i = 0; i < shots.size(); i++) {
        const cwShotVector& shot = shots.at(i);

        int ii = 3 * i; //Matrix index

        //Add the shot to the shot matrix
        //        qDebug() << "ShotMatrix:" << shotMatrix;
        //        qDebug() << "Submatrix:" << shotMatrix.submat(arma::span(ii,ii+2), arma::span(0,0));
        //        qDebug() << "Shot vector:" << shot.vector();
        shotMatrix.submat(arma::span(ii,ii+2), arma::span(0,0)) = shot.vector();

        //Add the shot to the observation matrix
        if(!shot.fromStation().isEmpty()) { //We need to test this because of fixed shots, that only have fromStations
            int fromStationIndex = 3 * stationToIndex.value(shot.fromStation(), -1);
            Q_ASSERT(fromStationIndex >= 0);
            observationMatrix.submat(arma::span(ii, ii+2),
                                     arma::span(fromStationIndex, fromStationIndex+2)) = fromStationMatrix;
        }

        if(!shot.toStation().isEmpty()) {
            int toStationIndex = 3 * stationToIndex.value(shot.toStation(), -1);
            Q_ASSERT(toStationIndex >= 0);
            observationMatrix.submat(arma::span(ii, ii+2),
                                     arma::span(toStationIndex, toStationIndex+2)) = toStationMatrix;
        }

        //Add the shot to the weights matrix
        //        qDebug() << "fromStation:" << shot.fromStation() << "To:" << shot.toStation() << "CovarienceMatrix:" << shot.covarienceMatrix();
        arma::mat shotWeightMatrix = arma::inv(shot.covarienceMatrix());

        //        qDebug() << "WeightMatrix sub:" <<         weightsMatrix.submat(arma::span(ii, ii+2),
        //                                                                        arma::span(ii, ii+2));
        //        qDebug() << "ShotWeightMatrix:" << shotWeightMatrix;

        weightsMatrix.submat(arma::span(ii, ii+2),
                             arma::span(ii, ii+2)) = shotWeightMatrix;
    }

    //    qDebug() << "Weight matrix: " << weightsMatrix;
    //    qDebug() << "Observation matrix:" << observationMatrix;
    //    qDebug() << "Observation t matrix:" << observationMatrix.t();
    //    qDebug() << "multi:" << observationMatrix.t() * weightsMatrix;
    //    qDebug() << "Everything together:" << (observationMatrix.t() * weightsMatrix * observationMatrix);
    //    qDebug() << "ShotMatrix:" << shotMatrix;
    //    qDebug() << "Determinate:" << arma::det(observationMatrix.t() * weightsMatrix * observationMatrix);

    arma::mat observationMatrixTranspose = observationMatrix.t();
    arma::mat everything = observationMatrixTranspose * weightsMatrix * observationMatrix;

    if(arma::det(everything) != 0.0) {
        //Calculate the station positions with giant matrixes (this is the least square aka weighting)

        arma::mat stationPositions = arma::solve(everything,
                                                 observationMatrixTranspose * weightsMatrix * shotMatrix);


        //                arma::inv(everything) *
        //                observationMatrixTranspose * weightsMatrix * shotMatrix;


        //        qDebug() << "StationPositions:" << stationPositions;

        //For each station get the data for it
        QHashIterator<QString, int> iter(stationToIndex);
        while(iter.hasNext()) {
            iter.next();
            QString stationName = iter.key();
            int stationIndex = 3 * iter.value();

            arma::mat stationMat = stationPositions.submat(arma::span(stationIndex, stationIndex+2),
                                                           arma::span(0, 0));

            double x = roundToDecimal(stationMat(0,0), 2);
            double y = roundToDecimal(stationMat(1,0), 2);
            double z = roundToDecimal(stationMat(2,0), 2);

            QVector3D stationPosition(x, y, z);
            PositionLookup.setPosition(stationName, stationPosition);

            //            qDebug() << "Station:" << stationName << stationMat(0, 0) << stationMat(1, 0) << stationMat(2, 0);
        }
    } else {
        qDebug() << "No solution!";
    }
}

/**
 * @brief cwLoopCloserTask::cwLeastSquares::setShotStd
 * @param shotStdev
 */
void cwLoopCloserTask::cwShotProcessor::setShotStd(cwShotStdev shotStdev)
{
    ShotStdev = shotStdev;
}

/**
 * @brief cwLoopCloserTask::cwShotProcessor::applyCalibration
 * @param calibration
 * @param shot
 * @return
 *
 *  This applys the calibration to the shot.  This will create a copy of the original shot.
 */
cwShot cwLoopCloserTask::cwShotProcessor::applyCalibration(cwTripCalibration *calibration, const cwShot &shot) const
{
    cwShot calibratedShot = shot;

    bool hasClino = cwShot::clinoValid(shot.clinoState());
    bool hasCompass = cwShot::compassValid(shot.compassState());
    bool hasBackClino = cwShot::clinoValid(shot.backClinoState());
    bool hasBackCompass = cwShot::compassValid(shot.backCompassState());
    bool clinoUpDown = shot.clinoState() == cwClinoStates::Up || shot.clinoState() == cwClinoStates::Down;
    //    bool backClinoUpDown = shot.backClinoState() == cwClinoStates::Up || shot.backClinoState() == cwClinoStates::Down;

    //Calibrate the distance and convert it to meters
    double calibratedDistance = shot.distance() + calibration->tapeCalibration();
    calibratedDistance = cwUnits::convert(calibratedDistance, calibration->distanceUnit(), cwUnits::Meters);
    calibratedShot.setDistance(calibratedDistance);

    //Calibrate the front compass
    if(hasCompass) {
        double compass = shot.compass();
        compass += calibration->frontCompassCalibration() + calibration->declination();
        calibratedShot.setCompass(compass);
    }

    //Calibrate the back compass
    if(hasBackCompass) {
        double backCompass = shot.backCompass();
        if(calibration->hasCorrectedCompassBacksight()) {
            backCompass += 180.0;
        }

        backCompass += calibration->backCompassCalibration() + calibration->declination();
        backCompass = fmod(backCompass, 360.0); //Make sure the shot is greater that -360.0 and less than 360.0
        calibratedShot.setBackCompass(backCompass);
    }

    //Calibrate the front clino
    if(hasClino && !clinoUpDown) {
        //All clino shot's that aren't measured with up and down
        double clino = shot.clino();
        clino += calibration->frontClinoCalibration();
        clino = qMin(90.0, qMax(-90.0, clino)); //clamp the clino to -90 and 90
        calibratedShot.setClino(clino);
    }

    //Calibrate the back clino
    if(hasBackClino) {
        double backClino = shot.backClino();
        backClino += calibration->backClinoCalibration();

        if(calibration->hasCorrectedClinoBacksight()) {
            switch (shot.backClinoState()) {
            case cwClinoStates::Up:
                //Swap the back for the front
                calibratedShot.setBackClinoState(cwClinoStates::Down);
                break;
            case cwClinoStates::Down:
                //Swap the back for the front
                calibratedShot.setBackClinoState(cwClinoStates::Up);
                break;
            case cwClinoStates::Valid:
                calibratedShot.setBackClino(-backClino);
                break;
            default:
                Q_ASSERT(false); //We shouldn't get here
                break;
            }
        } else {
            calibratedShot.setBackClino(backClino);
        }
    }

    //Return shot
    return calibratedShot;
}

/**
 * @brief cwLoopCloserTask::cwLeastSquares::stationPositionLookup
 * @return The output of the least squares algorithm
 */
cwStationPositionLookup cwLoopCloserTask::cwLeastSquares::stationPositionLookup() const
{
    return PositionLookup;
}


/**
 * @brief cwLoopCloserTask::cwNetworkSolver::process
 *
 * This will process all the loops and legs, to generate the final station positions.
 *
 * The final station potions will be placed in PositionLookup.
 */
void cwLoopCloserTask::cwNetworkSolver::process()
{
    //Create an loopup for all the leg edges (edges that aren't in a loop)
    indexLegStations();

    Q_ASSERT(!PositionLookup.positions().isEmpty()); //You need to have at least 1 fixed station

    //Start at the first fix position
    QString firstStationName = PositionLookup.positions().firstKey();

    QList<QString> fixedStations; //The fixed stations that it's neigbors need to be calculated
    fixedStations.append(firstStationName);

    //For each station, calculate the positions of the neighboring stations
    while(!fixedStations.isEmpty()) {
        qDebug() << "Current fixed stations:" << fixedStations;
        fixedStations = calcNeigboringStations(fixedStations);
    }

}

void cwLoopCloserTask::cwNetworkSolver::setEdgeLegs(QList<cwLoopCloserTask::cwEdgeSurveyChunk *> edgeLegs)
{
    EdgeLegs = edgeLegs;
}

void cwLoopCloserTask::cwNetworkSolver::setLoops(QList<cwLoopCloserTask::cwLoop> loops)
{
    Loops = loops;
}

/**
 * @brief cwLoopCloserTask::cwNetworkSolver::setInitialFixedStations
 * @param fixedPositions
 *
 * This sets the initial fixed position. You can have multiple stations that are already fixed
 */
void cwLoopCloserTask::cwNetworkSolver::setInitialFixedStations(cwStationPositionLookup fixedPositions)
{
    PositionLookup = fixedPositions;
}

cwStationPositionLookup cwLoopCloserTask::cwNetworkSolver::stationPositionLookup() const
{
    return PositionLookup;
}

/**
 * @brief cwLoopCloserTask::cwNetworkSolver::indexEdgeLegs
 *
 * This will go through all the edge legs, and index them based on the from station name.
 */
void cwLoopCloserTask::cwNetworkSolver::indexLegStations()
{
    StationsToLegs.clear();

    foreach(cwEdgeSurveyChunk* chunk, EdgeLegs) {
        foreach(cwShotVector vector, chunk->shotVectors()) {
            StationsToLegs.insertMulti(vector.fromStation(), vector);
            StationsToLegs.insertMulti(vector.toStation(), vector);
        }
    }
}

/**
 * @brief cwLoopCloserTask::cwNetworkSolver::indexLoopStations
 *
 * This will go through all the loops, and index the station name to each of them
 */
void cwLoopCloserTask::cwNetworkSolver::indexLoopStations()
{

}

/**
 * @brief cwLoopCloserTask::cwNetworkSolver::calcNeigboringStations
 * @param fixedStations
 * @return
 *
 * Using the fixedStations (passed as the parameter) this function calculates the positions
 * of the fixedStations neigbors. The neigbors are then added to a list and returned. If
 * all positions are assign, and there's no more processing, this return an empty list
 */
QList<QString> cwLoopCloserTask::cwNetworkSolver::calcNeigboringStations(QList<QString> fixedStations)
{
    QList<QString> newFixedStations;

    foreach(QString station, fixedStations) {

        //Process the legs
        QList<cwShotVector> shots = StationsToLegs.values(station);

        //Have a local list off all the station positions - for averaging backsites
        QMultiHash<QString, cwStationPosition> localStationPosition;

        foreach(cwShotVector shot, shots) {

            bool calcFrontSite = (station == shot.fromStation() && !PositionLookup.hasStation(shot.toStation()));
            bool calcBackSite = (station == shot.toStation() && !PositionLookup.hasStation(shot.fromStation()));

            cwStationPosition stationPosition;

            //Shots between to and from station hasn't been processed
            if(calcFrontSite) {
                //We don't have the position for the toStation,
                //Calculate the new position
                arma::mat vector = shot.vector();
                QVector3D shotDirection(vector(0,0), vector(1,0), vector(2, 0));

                QVector3D fromPosition = PositionLookup.position(shot.fromStation());
                QVector3D toPosition = fromPosition + shotDirection;

                QMatrix3x3 fromConvariance = PositionLookup.covariance(shot.fromStation());
                QMatrix3x3 toConvariance = fromConvariance + cwLoopCloserTask::toQMatrix3x3(shot.covarienceMatrix());

                stationPosition.setPosition(toPosition);
                stationPosition.setConvariance(toConvariance);

                localStationPosition.insertMulti(shot.toStation(), stationPosition);

            } else if(calcBackSite) {
                //This is a backsite
                arma::mat vector = -shot.vector(); //Inverse the direction because it's a backsite
                QVector3D shotDirection(vector(0,0), vector(1,0), vector(2, 0));

                QVector3D fromPosition = PositionLookup.position(shot.toStation());
                QVector3D toPosition = fromPosition + shotDirection;

                QMatrix3x3 fromConvariance = PositionLookup.covariance(shot.toStation());
                QMatrix3x3 toConvariance = fromConvariance + cwLoopCloserTask::toQMatrix3x3(shot.covarienceMatrix());

                stationPosition.setPosition(toPosition);
                stationPosition.setConvariance(toConvariance);

                localStationPosition.insertMulti(shot.fromStation(), stationPosition);
            }
        }

        //Average the localStationPositions
        foreach(QString key, localStationPosition.keys()) {
            QList<cwStationPosition> positions = localStationPosition.values(key);
            cwStationPosition averagePosition = average(positions);

            PositionLookup.insertStation(key, averagePosition);
            newFixedStations.append(key);
        }
    }

    QList<QString> possibleLoopStations = fixedStations + newFixedStations;

    foreach(QString station, possibleLoopStations) {


    }

    return newFixedStations;
}

/**
 * @brief cwLoopCloserTask::cwNetworkSolver::average
 * @param positions
 * @return The average position of the positions
 */
cwStationPosition cwLoopCloserTask::cwNetworkSolver::average(QList<cwStationPosition> stations) const
{
    if(stations.size() == 1) {
        return stations.first();
    }

    QVector3D averagePosition;
    foreach(cwStationPosition station, stations) {
        averagePosition += station.position();
    }
    double size = stations.size();
    averagePosition = QVector3D(averagePosition.x() / size,
                                averagePosition.y() / size,
                                averagePosition.z() / size);


    cwStationPosition averageStation;
    averageStation.setPosition(averagePosition);
    averageStation.setConvariance(stations.first().convariance());

    return averageStation;
}

/**
 * @brief cwLoopCloserTask::cwLoopGroup::setLoops
 * @param loops
 */
void cwLoopCloserTask::cwLoopGroup::setLoops(QList<cwLoopCloserTask::cwLoop> loops)
{
    Loops = loops;
}

/**
 * @brief cwLoopCloserTask::cwLoopGroup::appendLoop
 * @param loop
 *
 * Addes the loop to the loop group
 */
void cwLoopCloserTask::cwLoopGroup::appendLoop(cwLoopCloserTask::cwLoop loop)
{
    Q_ASSERT(canAddLoop(loop));
    Loops.append(loop);
}

/**
 * @brief cwLoopCloserTask::cwLoopGroup::maximize
 *
 * This function assumes that all the loops in the loop group are basic cycles.
 *
 * This function will take all the basic loops add them together to all the loop
 * combinations. Finding all the loop combinations can find bad legs in a survey
 * loop.
 */
void cwLoopCloserTask::cwLoopGroup::maximize()
{
    QList<cwLoop> allCombinedLoops;

    typedef QPair<QSet<int>, cwLoop> LoopBuildup;

    QList<LoopBuildup> lastCombinedLoops;
    QList<LoopBuildup> currentCombinedLoops;

    while(!lastCombinedLoops.isEmpty()) {
        for(int ii = 0; ii < lastCombinedLoops.size(); ii++) {
            const LoopBuildup& l = lastCombinedLoops.at(ii);
            QSet<int> basicLoops = l.first;

            for(int i = 0; i < Loops.size(); i++) {
                if(basicLoops.contains(i)) {
                    //This loop already contains Loop.at(i)
                    continue;
                }

                cwLoop l1 = l.second;
                cwLoop l2 = Loops.at(i);


                if(cwLoop::canCombine(l1, l2)) {
                    cwLoop combineLoop = cwLoop::combine(l1, l2);
                    basicLoops.insert(i);

                    LoopBuildup newCombineLoop(basicLoops, combineLoop);

                    //Add the new loop to
                    currentCombinedLoops.append(newCombineLoop);
                }
            }
        }

        //Add currentCombineLoops to allLoops
        foreach(LoopBuildup b, currentCombinedLoops) {
            allCombinedLoops.append(b.second);
        }

        lastCombinedLoops = currentCombinedLoops;
    }

    //Add all of the together
    Loops += allCombinedLoops;

}

/**
 * @brief lessThanStd
 * @param loop1
 * @param loop2
 * @return Returns true if loop1's std is less than loop2 std
 */
bool cwLoopCloserTask::cwLoopGroup::lessThanStd(const cwLoopCloserTask::cwLoop& loop1, const cwLoopCloserTask::cwLoop& loop2) {
    qDebug() << "Less than:" << loop1.stdError().lengthSquared() << loop2.stdError().lengthSquared();
    qDebug() << "Less than2:" << loop1.stdError().length() << loop2.stdError().length();
    return loop1.stdError().lengthSquared() < loop2.stdError().lengthSquared();
}

/**
 * @brief cwLoopCloserTask::cwLoopGroup::sortLoops
 *
 * This goes through all the loops in the loop group an calculate the loop's
 * std-dev and sorts the loops by theier total std-dev
 */
void cwLoopCloserTask::cwLoopGroup::sortLoops()
{
    //Find std-dev
    for(int i = 0; i < Loops.size(); i++) {
        Loops[i].calculateStd();
    }

    qSort(Loops.begin(), Loops.end(), &cwLoopGroup::lessThanStd);
}

/**
 * @brief cwLoopCloserTask::cwLoopGroup::canAddLoop
 * @param loop
 * @return
 *
 * Test to see if the loop can be added to the loop group.
 */
bool cwLoopCloserTask::cwLoopGroup::canAddLoop(const cwLoopCloserTask::cwLoop &loop) const
{
    if(Loops.isEmpty()) {
        return true;
    }

    foreach(cwEdgeSurveyChunk* edge, loop.edges()) {
        foreach(cwLoop loop, Loops) {
            if(loop.edges().contains(edge)) {
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief cwLoopCloserTask::cwLoopGroup::printLoops
 *
 * Prints all the loops in the loop group
 */
void cwLoopCloserTask::cwLoopGroup::printLoops() const
{
    int count = 0;
    foreach(cwLoop loop, Loops) {
        qDebug() << "-----***-----Loop" << count << "-------***----------";
        loop.printEdges();
        qDebug() << "-----***-----End of Loop--------***------------";
        count++;
    }
}
