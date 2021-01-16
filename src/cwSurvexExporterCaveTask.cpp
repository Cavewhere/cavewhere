/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterTripTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwFixedStationModel.h"
#include "cwCavingRegion.h"

//Proj4 includes
#include "proj.h"

cwSurvexExporterCaveTask::cwSurvexExporterCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
    TripExporter = new cwSurvexExporterTripTask(this);
    TripExporter->setParentSurvexExporter(this);

//    connect(TripExporter, SIGNAL(progressed(int)), SIGNAL(progressed(int)));
}

/**
  \brief Writes the cave data to the stream
  */
bool cwSurvexExporterCaveTask::writeCave(QTextStream& stream, cwCave* cave) {

    if(!checkData(cave)) {
        if(isRunning()) {
            stop();
        }
        return false;
    }

    QString caveName = cave->name().remove(" ");

    stream << "*begin " << caveName << " ;" << cave->name() << endl << endl;

    //This fucks up shit in cavern
   // stream << "*sd compass 2.0 degrees" << endl;
   // stream << "*sd clino 2.0 degrees" << endl << endl;

    //Add fix station to tie the cave down
    fixFirstStation(stream, cave);

    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        TripExporter->writeTrip(stream, trip);
        TotalProgress += trip->numberOfStations();
        stream << endl;
    }

    stream << "*end ; End of " << cave->name() << endl;

    return true;
}

/**
 * @brief cwSurvexExporterCaveTask::fixFirstStation
 * @param stream
 * @param cave
 *
 * This fixes the first station in the cave, if the cave has any stations.
 */
void cwSurvexExporterCaveTask::fixFirstStation(QTextStream &stream, cwCave *cave)
{
    struct Point {
        double easting;
        double northing;
        double altitude;

        Point operator-(const Point& other) {
            return {
                easting - other.easting,
                        northing - other.northing,
                        altitude - other.altitude
            };
        }
    };

    auto origin = cave->parentRegion()->origin().toGeoCoordinate();

    auto transform = [origin](const QGeoCoordinate& coordinate) {
        PJ_CONTEXT *C;
        PJ *P;
        PJ_COORD a, b;

        /* or you may set C=PJ_DEFAULT_CTX if you are sure you will     */
        /* use PJ objects from only one thread                          */
        C = proj_context_create();

        QString projectionStr = QString("+proj=tmerc +lat_0=0 +lon_0=%1 +k_0=1 +x_0=0 +y_0=0 +ellps=bessel +units=m")
                .arg(origin.longitude());
        QByteArray projection = projectionStr.toLocal8Bit();

        P = proj_create_crs_to_crs (C,
                                    "EPSG:4326",
                                    projection.constData(),
                                    NULL);

        if (0==P) {
            fprintf(stderr, "Oops\n");
            return Point({0.0, 0.0, 0.0});
        }

        /* a coordinate union representing Copenhagen: 55d N, 12d E    */
        /* Given that we have used proj_normalize_for_visualization(), the order of
        /* coordinates is longitude, latitude, and values are expressed in degrees. */
        a = proj_coord (coordinate.latitude(), coordinate.longitude(), 0, 0);

        /* transform to UTM zone 32, then back to geographical */
        b = proj_trans (P, PJ_FWD, a);
//        printf ("easting: %.3f, northing: %.3f\n", b.enu.e, b.enu.n);
//        b = proj_trans (P, PJ_INV, b);
//        printf ("longitude: %g, latitude: %g\n", b.lp.lam, b.lp.phi);

        /* Clean up */
        proj_destroy (P);
        proj_context_destroy (C); /* may be omitted in the single threaded case */

        return Point({b.enu.e, b.enu.n, coordinate.altitude()});
    };

    auto toXY = [origin, transform](const QGeoCoordinate& coordinate) {
        auto localOrigin = transform(origin);
        auto localPoint = transform(coordinate);
        return localPoint - localOrigin;
    };

    auto toFixedString = [](const QString& stationName, const Point& point) {
        if(stationName.isEmpty()) {
            return QString();
        }

        return QString("*fix %1 %2 %3 %4")
                .arg(stationName)
                .arg(point.easting)
                .arg(point.northing)
                .arg(point.altitude);
    };

    auto writeLine = [&stream](const QString& line) {
        if(!line.isEmpty()) {
            stream << line << Qt::endl;
        }
    };

    if(cave != nullptr) {
        if(cave->fixedStations()->isEmpty()) {
            auto firstStation = [cave]() {
                if(!cave->trips().isEmpty()) {
                    cwTrip* firstTrip = cave->trips().first();
                    if(!firstTrip->chunks().isEmpty()) {
                        cwSurveyChunk* firstChunk = firstTrip->chunks().first();
                        if(!firstChunk->stations().isEmpty()) {
                            cwStation station = firstChunk->stations().first();
                            return station;
                        }
                    }
                }
                return cwStation();
            }();
            writeLine(toFixedString(firstStation.name(), Point({0.0, 0.0, 0.0})));
        } else {
            //Use fixed station
            for(int i = 0; i < cave->fixedStations()->rowCount(); i++) {
                auto station = cave->fixedStations()->row(i);
                writeLine(toFixedString(station.stationName(), toXY(station.toGeoCoordinate())));
            }
        }
    }
}


