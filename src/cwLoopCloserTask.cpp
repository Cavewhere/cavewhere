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
#include "cwMath.h"
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
    //    printEdges(edges);

    //For Debugging, this makes sure that station don't exist in the middle of the edge
    checkBasicEdges(edges);

    //---------------------------- Detect loops and legs -----------------------------

    //Split loops out from survey legs
    cwLoopDetector loopDetector;
    loopDetector.process(edges);

    //    printLoops(loopDetector.loops());

    //---------------------------- Process all the shot directions -------------------
    cwShotProcessor shotProcessor;
    shotProcessor.process(edges);

    //---------------------------- Process least squares -----------------------------
    cwLeastSquares leastSquares;
    leastSquares.process(edges);

    //Set the cave's station postions
    cave->setStationPositionLookup(leastSquares.stationPositionLookup());

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
//    //Debugging for the resulting edges
//    foreach(cwEdgeSurveyChunk* edge, edges) {
//        qDebug() << "--Edge:" << edge;

//        for(int i = 0; i < edge->stations().size() - 1; i++) {
//            cwStation toStation = edge->stations().at(i + 1);
//            cwStation fromStation = edge->stations().at(i);
//            QList<cwShotVector> shotVectors = shotVector(i);

//            qDebug() << "\t" << fromStation.name() + "to" + toStation.name() + " there are "  + shotVectors.size() + "shots";

//            foreach(cwShotVector vector, shotVectors) {
//                qDebug() << "\t\t Dir:" << vector.direction() << "vec:" << vector.vector();
//                qDebug() << "\t\t covarience:" << vector.covarienceMatrix();
//            }
//        }
//    }
//    qDebug() << "*********** end of edges ***********";
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

    //Find all the uniqueLoops (this finds all the possible loops)
    QList<cwLoop> uniqueLoops = findUniqueLoops(initialLoops);

    //Do gassiumn elimanation on loops to find basic loops
    Loops = minimizeLoops(uniqueLoops);
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

        if(!hasValidFrontSite && !hasValidBackSite) {
            //Special case, where shot is a mix of front and back sites

            if(hasClino && hasBackCompass) {
                //Reverse the back compass so it's a front site
                shotVector = shotTransform(shot.distance(), shot.backCompass() - 180.0, shot.clino(), shot.clinoState());
            } else if(hasBackClino && hasCompass) {
                //Reverse the back clino so it's a front site
                shotVector = shotTransform(shot.distance(), shot.compass(), -shot.backClino(), shot.backClinoState());
            } else {
                Q_ASSERT(false); //This should never get here
            }

            shotVector.setDirection(cwShotVector::FrontSite);
            shotVector.setFromStation(fromStationName);
            shotVector.setToStation(toStationName);
            chunk->addShotVector(shotVector);
            return;
        }

        if(hasValidFrontSite) {
            shotVector = shotTransform(shot.distance(), shot.compass(), shot.clino(), shot.clinoState());
            shotVector.setDirection(cwShotVector::FrontSite);
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
            shotVector = shotTransform(shot.distance(), shot.backCompass(), shot.backClino(), shot.backClinoState());
            shotVector.setDirection(cwShotVector::BackSite);
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
    double distanceStd = 0.05; //FIXME: use shot std for each shot, assume the units are what ever in shot?
    double azimithStd; //= 0.017; //1 Degree std in radians
    double clinoStd; //= 0.017; //1 degree std in radians

    Q_ASSERT(clinoState != cwClinoStates::Empty);
    switch(clinoState) {
        case cwClinoStates::Valid:
        azimithStd = degreeToRadians() * 1.0; //1.25 Degree std in radians
        clinoStd = degreeToRadians() * 1.0; //1.25 Degree std in radians
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

    //Convert from degrees to radians
    azimith = azimith * acos(-1) / 180.0;
    clino = clino * acos(-1) / 180.0;

    double maxClino = degreeToRadians() * 89.9;

    //Calculate the vector
    arma::mat vector = arma::mat::fixed<3, 1>();
    vector(0,0) = sin(azimith) * cos(clino) * distance; //x
    vector(1,0) = cos(azimith) * cos(clino) * distance; //y
    vector(2,0) = sin(clino) * distance; //z

    //Calculate the convarience for the shot
    arma::mat jacobian = arma::mat::fixed<3, 3>();

    if(fabs(clino) < maxClino) {
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
    dacCovariance(0, 0) = distanceStd;
    dacCovariance(1, 1) = azimithStd;
    dacCovariance(2, 2) = clinoStd;

    arma::mat xyzCovariance = jacobian * dacCovariance * jacobian;

    cwLoopCloserTask::cwShotVector shotVector;
    shotVector.setVector(vector);

//    qDebug() << "jacobian:" << jacobian;
//    qDebug() << "dacConvariance:" << dacCovariance;
//    qDebug() << "jacobian.t():" << jacobian.t();
//    qDebug() << "Maginuted: " << arma::det(xyzCovariance);
//    qDebug() << "Inverse:" << arma::inv(xyzCovariance);
//    qDebug() << "Mag inverse" << arma::det(arma::inv(xyzCovariance));
    qDebug() << "Converiance Matrix:" << xyzCovariance;

    shotVector.setCovarienceMatrix(xyzCovariance);
    return shotVector;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::setVector
 * @param vector a 1x3 matrix, that stores the direction of the shot
 */
void cwLoopCloserTask::cwShotVector::setVector(arma::mat vector) {
    Q_ASSERT(vector.n_cols == 1);
    Q_ASSERT(vector.n_rows == 3);
    Vector = vector;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::vector
 * @return The shot's direction a 1x3 matrix
 */
const arma::mat& cwLoopCloserTask::cwShotVector::vector() const {
    return Vector;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::setConvarienceMatrix
 * @param convarienceMatrix a 3x3 matrix, that holds the convarience matrix of the shot
 */
void cwLoopCloserTask::cwShotVector::setCovarienceMatrix(arma::mat convarienceMatrix) {
    Q_ASSERT(convarienceMatrix.n_cols == 3);
    Q_ASSERT(convarienceMatrix.n_rows == 3);
    CovarianceMatrix = convarienceMatrix;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::convarienceMatrix
 * @return Returns the CovarianceMatrix for the shot
 */
arma::mat cwLoopCloserTask::cwShotVector::covarienceMatrix() const {
    return CovarianceMatrix;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::fromStation
 * @return
 */
QString cwLoopCloserTask::cwShotVector::fromStation() const {
    return FromStation;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::setFromStation
 * @param fromStation
 */
void cwLoopCloserTask::cwShotVector::setFromStation(QString fromStation) {
    FromStation = fromStation;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::toStation
 * @return
 */
QString cwLoopCloserTask::cwShotVector::toStation() const {
    return ToStation;
}

/**
 * @brief cwLoopCloserTask::cwShotVector::setToStation
 * @param toStation
 */
void cwLoopCloserTask::cwShotVector::setToStation(QString toStation) {
    ToStation = toStation;
}

/**
 * @brief setDirection
 * @param direction - The direction of this shot vector.
 */
void cwLoopCloserTask::cwShotVector::setDirection(ShotDirection direction) {
    Direction = direction;
}

/**
 * @brief direction
 * @return Returns the direction of this shot vector
 */
cwLoopCloserTask::cwShotVector::ShotDirection cwLoopCloserTask::cwShotVector::direction() const {
    return Direction;
}

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
        qDebug() << "ShotMatrix:" << shotMatrix;
        qDebug() << "Submatrix:" << shotMatrix.submat(arma::span(ii,ii+2), arma::span(0,0));
        qDebug() << "Shot vector:" << shot.vector();
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
        arma::mat shotWeightMatrix = arma::inv(shot.covarienceMatrix());

        qDebug() << "WeightMatrix sub:" <<         weightsMatrix.submat(arma::span(ii, ii+2),
                                                                        arma::span(ii, ii+2));
        qDebug() << "ShotWeightMatrix:" << shotWeightMatrix;

        weightsMatrix.submat(arma::span(ii, ii+2),
                             arma::span(ii, ii+2)) = shotWeightMatrix;
    }

    qDebug() << "Weight matrix: " << weightsMatrix;
    qDebug() << "Observation matrix:" << observationMatrix;
    qDebug() << "Observation t matrix:" << observationMatrix.t();
    qDebug() << "multi:" << observationMatrix.t() * weightsMatrix;
    qDebug() << "Everything together:" << (observationMatrix.t() * weightsMatrix * observationMatrix);
    qDebug() << "ShotMatrix:" << shotMatrix;
    qDebug() << "Determinate:" << arma::det(observationMatrix.t() * weightsMatrix * observationMatrix);

    arma::mat observationMatrixTranspose = observationMatrix.t();
    arma::mat everything = observationMatrixTranspose * weightsMatrix * observationMatrix;

    if(arma::det(everything) != 0.0) {
        //Calculate the station positions with giant matrixes (this is the least square aka weighting)

        arma::mat stationPositions =
                arma::inv(everything) *
                observationMatrixTranspose * weightsMatrix * shotMatrix;


        qDebug() << "StationPositions:" << stationPositions;

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

            qDebug() << "Station:" << stationName << stationMat(0, 0) << stationMat(1, 0) << stationMat(2, 0);
        }
    } else {
        qDebug() << "No solution!";
    }
}

/**
 * @brief cwLoopCloserTask::cwLeastSquares::stationPositionLookup
 * @return The output of the least squares algorithm
 */
cwStationPositionLookup cwLoopCloserTask::cwLeastSquares::stationPositionLookup() const
{
    return PositionLookup;
}
