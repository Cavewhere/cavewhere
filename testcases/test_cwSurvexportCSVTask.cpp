//Catch includes
#include "catch.hpp"

//Our includes
#include "cwSurvexportCSVTask.h"

//Qt includes
#include <QVector>

TEST_CASE("cwSurvexportCSVTask should initilize correctly", "[cwSurvexportCSVTask]") {
    cwSurvexportCSVTask task;
    CHECK(task.stationPositions().positions().isEmpty() == true);
}

TEST_CASE("cwSurvexportCSVTask should parse survexport CSV output correctly", "[cwSurvexportCSVTask]") {
    cwSurvexportCSVTask task;
    task.setSurvexportCSVFile(QStringLiteral("://datasets/test_cwSurvexportCSVTask/cwSurvexport_data.3d.csv"));
    task.start();
    task.waitToFinish();

    class Station {
    public:
        Station() {}
        Station(QString name, QVector3D position) :
            name(name),
            position(position)
        {
        }

        QString name;
        QVector3D position;
    };

    QVector<Station> stations {
        Station("26", QVector3D(0.0, 0.0, 0.0)),
                Station("26a", QVector3D(0.93, -5.40, 2.32)),
                Station("26b", QVector3D(0.92, -9.94, 1.88)),
                Station("26c", QVector3D(6.56, -11.66, 2.85)),
                Station("26d", QVector3D(12.32, -16.50, 3.29)),
                Station("26e", QVector3D(4.44, -5.02, 0.47))
    };

    cwStationPositionLookup lookup = task.stationPositions();
    for(auto station : stations) {
        CHECK(lookup.hasPosition(station.name));
        QVector3D lookupPos = lookup.position(station.name);
        CHECK(lookupPos.x() == Approx(station.position.x()));
        CHECK(lookupPos.y() == Approx(station.position.y()));
        CHECK(lookupPos.z() == Approx(station.position.z()));
    }

    CHECK(lookup.positions().size() == stations.size());
}
