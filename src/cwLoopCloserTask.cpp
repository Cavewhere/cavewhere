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
#include "cwCave.h"
#include "cwDebug.h"

//Qt includes
#include <QElapsedTimer>

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

    //Print out edge results
    printEdges(edges);

    //For Debugging, this makes sure that station don't exist in the middle of the edge
    checkBasicEdges(edges);

    //---------------------------- Detect loops and legs -----------------------------

    //Split loops out from survey legs
    cwLoopDetector loopDetector;
    loopDetector.process(edges);

    printLoops(loopDetector.loops());


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
        foreach(cwStation station, edge->stations()) {
            qDebug() << "\t" << station.name();
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

            //Add survey chunk, split if nessarcy
            cwEdgeSurveyChunk* edgeChunk = new cwEdgeSurveyChunk();
            edgeChunk->setShots(shots);
            edgeChunk->setStations(stations);

            addEdgeChunk(edgeChunk);
        }
    }

    return ResultingEdges.toList();
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

        for(int i = 0; i < stations.size(); i++) {
            cwStation station = stations.at(i);

//            qDebug() << "Lookup contains station:" << station.name() << Lookup.contains(station.name());

            if(Lookup.contains(station.name())) {
                //Station is an intersection

                if(i == 0 || i == stations.size() - 1) {
                    //This is the first or last station in the edgeChunk
                    splitOnStation(station);
                    //                qDebug() << "Appending newChunk:" << newChunk << LOCATION;
                    //                Q_ASSERT(!ResultingEdges.contains(newChunk));
                    ResultingEdges.insert(chunk);

                } else {
                    //This is a middle station in the newChunk, split it and potentially another chunk
                    //Split new chunk
                    cwEdgeSurveyChunk* otherNewHalf = chunk->split(station.name());

                    //Split the other station alread in the lookup on station.name(), if it can
                    splitOnStation(station.name());
                    //                qDebug() << "Appending newChunk:" << newChunk << LOCATION;
                    //                Q_ASSERT(!ResultingEdges.contains(newChunk));
                    ResultingEdges.insert(chunk);
                    addStationInEdgeChunk(chunk);

                    //Update the station, and restart the search with the other half
                    stations = otherNewHalf->stations();
                    chunk = otherNewHalf;
                    i = -1;
                }
            }
        }

        //    if(Lookup.isEmpty()) {
        //        qDebug() << "Appending newChunk:" << newChunk << LOCATION;
        //        Q_ASSERT(!ResultingEdges.contains(newChunk));
        ResultingEdges.insert(chunk);
        addStationInEdgeChunk(chunk);
        //    }
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

//    qDebug() << "Split on station:" << station.name();

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
            ResultingEdges.insert(otherHalf);
        }
    }
}

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
 * @brief cwLoopCloserTask::cwChunkLookup::addStationInEdgeChunk
 * @param chunk
 *
 * Adds all the station from chunk into this lookup
 */
void cwLoopCloserTask::cwMainEdgeProcessor::addStationInEdgeChunk(cwLoopCloserTask::cwEdgeSurveyChunk *chunk)
{
    foreach(cwStation station, chunk->stations()) {
//        qDebug() << "Inserting station name into lookup:" << station.name() << "chunk:" << chunk;
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
            return newChunk;
        }
    }

    Q_ASSERT(false); //Couldn't find station in chunk

    return NULL;
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

    //Find all the loops from the edges, Start with the first station
    QString firstStation = edges.first()->stations().first().name();
    QList<cwLoop> initialLoops = findLoops(firstStation);

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

        //Add the loops that we just found, but make sure they're are unqiue
        //To thread this this must be moved out of this foreach loop, this is combind part of the threading
        foreach(cwLoop loop, loops) {
            if(!uniqueLoops.contains(loop)) {
                uniqueLoops.append(loop);
            }
        }
    }

    printLoops(uniqueLoops);

    //Do gassiumn elimanation on loops to find basic loops
    minimizeLoops();
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
//    qDebug() << "Finding loop for station:" << station;

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

//        qDebug() << "Added loop--";
//        printEdges(loopPath);

        return;
    }

    VisitedStations.insert(station);

    QList<cwEdgeSurveyChunk*> edges = EdgeLookup.values(station);

    foreach(cwEdgeSurveyChunk* edge, edges) {
        if(!VisitedEdges.contains(edge)) {

            VisitedEdges.insert(edge);

//            qDebug() << "VisitedEdge:" << edge << "for station" << station;
//            printEdges(QList<cwEdgeSurveyChunk*>() << edge);

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

/**
 * @brief cwLoopCloserTask::cwLoopDetector::minimizeLoops
 *
 * This uses gaussian elimination to find all the minimal loops
 */
void cwLoopCloserTask::cwLoopDetector::minimizeLoops() {
    //Find all the loop edges
    QHash<cwEdgeSurveyChunk*, int> loopEdges;
    int index = 0;
    foreach(cwLoop loop, Loops) {
        foreach(cwEdgeSurveyChunk* edge, loop.edges()) {
            if(!loopEdges.contains(edge)) {
                loopEdges.insert(edge, index);
                ++index;
            }
        }
    }

//    //Populate the matrix
//    QList< cwLoopCloserTask::EdgeMatrixRow > matrix;
//    foreach(cwLoop loop, Loops) {
//        cwLoopCloserTask::EdgeMatrixRow row;
//        row.fill(false, loopEdges.size());

//        foreach(cwEdgeSurveyChunk* edge, loop.edges()) {
//            int index = loopEdges.value(edge);
//            row[index] = true;
//        }

//        matrix.row
//    }


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


bool cwLoopCloserTask::cwLoop::operator ==(const cwLoopCloserTask::cwLoop &other) const
{
    return other.edges() == edges();
}

bool cwLoopCloserTask::cwLoop::operator !=(const cwLoopCloserTask::cwLoop &other) const
{
    return other.edges() != edges();
}
