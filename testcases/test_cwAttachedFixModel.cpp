/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAttachedFixModel.h"
#include "cwCave.h"
#include "cwCoordinateText.h"
#include "cwFixStation.h"
#include "cwFixStationDiagnosticsModel.h"
#include "cwFixStationModel.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QConcatenateTablesProxyModel>
#include <QSignalSpy>

//Std includes
#include <cmath>

namespace {

cwAttachedFix bareFix(const QString& station)
{
    return {station, QStringLiteral("0 0 0"), QString(), QStringLiteral("survex_blocks.svx")};
}

cwAttachedFix utm13NFix(const QString& station)
{
    return {station, QStringLiteral("478000 4430000 1655"), QStringLiteral("EPSG:32613"),
            QStringLiteral("other.svx")};
}

cwFixStation nodeFix(const QString& station)
{
    cwFixStation fix;
    fix.setStationName(station);
    fix.setInputCS(QStringLiteral("EPSG:32613"));
    fix.setCoordinate(478000.0, 4430000.0, 1655.0);
    return fix;
}

constexpr double kGridEasting = 500200.0;
constexpr double kGridNorthing = 3000000.0;
constexpr double kGridElevation = 100.0;

struct Components
{
    double easting;
    double northing;
    double elevation;
};

Components componentsOf(const cwAttachedFixModel& model, int row)
{
    const QModelIndex index = model.index(row);
    return {model.data(index, cwFixStationModel::EastingRole).toDouble(),
            model.data(index, cwFixStationModel::NorthingRole).toDouble(),
            model.data(index, cwFixStationModel::ElevationRole).toDouble()};
}

} // namespace

TEST_CASE("An attached fix model's rows follow setAttachedFixes", "[cwAttachedFixModel]")
{
    cwAttachedFixModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &cwAttachedFixModel::countChanged);
    CHECK(model.rowCount() == 0);

    model.setAttachedFixes({bareFix(QStringLiteral("doghill.d1")), utm13NFix(QStringLiteral("a.p1"))});
    CHECK(model.rowCount() == 2);
    CHECK(resetSpy.count() == 1);
    CHECK(countSpy.count() == 1);

    // The same list again changes nothing.
    model.setAttachedFixes(model.attachedFixes());
    CHECK(resetSpy.count() == 1);

    model.setAttachedFixes({utm13NFix(QStringLiteral("a.p1"))});
    CHECK(model.rowCount() == 1);
    CHECK(resetSpy.count() == 2);
    CHECK(countSpy.count() == 2);
}

TEST_CASE("An attached fix row's system names the system and the code that places it",
          "[cwAttachedFixModel]")
{
    const auto placedFix = [](const QString& station, const QString& system, const QString& placement) {
        cwAttachedFix fix{station, QStringLiteral("0 0 0"), system, QStringLiteral("attached.wpj")};
        fix.placementSystem = placement;
        return fix;
    };

    cwAttachedFixModel model;
    model.setAttachedFixes({placedFix(QStringLiteral("M1"), QStringLiteral("NAD27 CONUS, UTM zone 16N"),
                                      QStringLiteral("EPSG:26716")),
                            placedFix(QStringLiteral("L1"), QStringLiteral("NAD27 CONUS, latitude/longitude"),
                                      QStringLiteral("EPSG:4267")),
                            placedFix(QStringLiteral("A1"), QStringLiteral("North American 1927, UTM zone 13N"),
                                      QStringLiteral("EPSG:26713")),
                            utm13NFix(QStringLiteral("a.p1"))});
    REQUIRE(model.rowCount() == 4);

    CHECK(model.data(model.index(0), cwFixStationModel::InputCSRole).toString().toStdString()
          == "NAD27 CONUS, UTM zone 16N (EPSG:26716)");
    CHECK(model.data(model.index(1), cwFixStationModel::InputCSRole).toString().toStdString()
          == "NAD27 CONUS, latitude/longitude (EPSG:4267)");
    CHECK(model.data(model.index(2), cwFixStationModel::InputCSRole).toString().toStdString()
          == "North American 1927, UTM zone 13N (EPSG:26713)");
    // A Survex *cs is its own code, so its row shows it once.
    CHECK(model.data(model.index(3), cwFixStationModel::InputCSRole).toString().toStdString()
          == "EPSG:32613");
}

TEST_CASE("An attached fix model answers the fix station roles read-only", "[cwAttachedFixModel]")
{
    cwAttachedFixModel model;
    model.setAttachedFixes({bareFix(QStringLiteral("doghill.d1")), utm13NFix(QStringLiteral("a.p1"))});

    const QModelIndex bare = model.index(0);
    CHECK(model.data(bare, cwFixStationModel::StationNameRole).toString() == QStringLiteral("doghill.d1"));
    CHECK(model.data(bare, cwFixStationModel::InputCSRole).toString().isEmpty());
    CHECK(model.data(bare, cwFixStationModel::CoordinateTextRole).toString() == QStringLiteral("0 0 0"));
    CHECK(model.data(bare, cwAttachedFixModel::SourceFileRole).toString()
          == QStringLiteral("survex_blocks.svx"));
    CHECK(model.data(bare, cwAttachedFixModel::ReadOnlyRole).toBool());
    CHECK(model.data(model.index(1), cwFixStationModel::InputCSRole).toString()
          == QStringLiteral("EPSG:32613"));

    // Every diagnostics role is defined, with its empty value.
    CHECK(model.data(bare, cwFixStationDiagnosticsModel::StationErrorRole) == QVariant(QString()));
    CHECK(model.data(bare, cwFixStationDiagnosticsModel::DomainErrorRole) == QVariant(QString()));
    CHECK(model.data(bare, cwFixStationDiagnosticsModel::CoordinateErrorRole) == QVariant(QString()));
    CHECK(model.data(bare, cwFixStationDiagnosticsModel::EastingDomainErrorRole) == QVariant(false));
    CHECK(model.data(bare, cwFixStationDiagnosticsModel::NorthingDomainErrorRole) == QVariant(false));
    CHECK(model.data(bare, cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole) == QVariant(false));
    CHECK(model.data(bare, cwFixStationDiagnosticsModel::DatumEnabledRole) == QVariant(false));
    CHECK(model.data(bare, cwFixStationDiagnosticsModel::AvailableDatumsRole)
          == QVariant(QStringList()));

    CHECK_FALSE(model.flags(bare).testFlag(Qt::ItemIsEditable));
    CHECK_FALSE(model.setData(bare, QStringLiteral("x1"), cwFixStationModel::StationNameRole));
    CHECK(model.data(bare, cwFixStationModel::StationNameRole).toString() == QStringLiteral("doghill.d1"));

    CHECK(model.firstFixWithoutReadInSystem() == 0);
    model.setAttachedFixes({utm13NFix(QStringLiteral("a.p1")), bareFix(QStringLiteral("doghill.d1"))});
    CHECK(model.firstFixWithoutReadInSystem() == 1);
    model.setAttachedFixes({utm13NFix(QStringLiteral("a.p1"))});
    CHECK(model.firstFixWithoutReadInSystem() == -1);

    cwAttachedFix readIn = bareFix(QStringLiteral("doghill.d0"));
    readIn.readInSystem = QStringLiteral("EPSG:32648");
    model.setAttachedFixes({readIn, bareFix(QStringLiteral("doghill.d1"))});
    CHECK(model.firstFixWithoutReadInSystem() == 1);
}

TEST_CASE("A node's fix station table lists its own fixes, then the attached ones",
          "[cwAttachedFixModel]")
{
    cwCave cave;
    cave.fixStations()->appendFixStation(nodeFix(QStringLiteral("G")));
    cave.fixStations()->appendFixStation(nodeFix(QStringLiteral("H")));
    cave.setAttachedFixes({bareFix(QStringLiteral("doghill.d1")), utm13NFix(QStringLiteral("a.p1"))});

    QConcatenateTablesProxyModel* table = cave.fixStationTable();
    const int nodeFixCount = cave.fixStations()->rowCount();
    REQUIRE(table->rowCount() == nodeFixCount + cave.attachedFixes()->rowCount());

    for (int i = 0; i < cave.attachedFixes()->rowCount(); ++i) {
        CHECK(table->mapFromSource(cave.attachedFixes()->index(i)).row() == nodeFixCount + i);
    }

    const QHash<int, QByteArray> roleNames = table->roleNames();
    CHECK(roleNames.value(cwAttachedFixModel::ReadOnlyRole) == QByteArrayLiteral("readOnly"));
    CHECK(roleNames.value(cwAttachedFixModel::SourceFileRole) == QByteArrayLiteral("sourceFile"));
    CHECK(roleNames.value(cwFixStationDiagnosticsModel::StationErrorRole)
          == QByteArrayLiteral("stationError"));

    const QModelIndex ownRow = table->index(0, 0);
    CHECK(table->data(ownRow, cwFixStationModel::StationNameRole).toString() == QStringLiteral("G"));
    CHECK(table->data(ownRow, cwAttachedFixModel::ReadOnlyRole) == QVariant(false));
    CHECK(table->data(ownRow, cwAttachedFixModel::SourceFileRole) == QVariant(QString()));
    CHECK(table->flags(ownRow).testFlag(Qt::ItemIsEditable));

    const QModelIndex attachedRow = table->index(nodeFixCount, 0);
    CHECK(table->data(attachedRow, cwFixStationModel::StationNameRole).toString()
          == QStringLiteral("doghill.d1"));
    CHECK(table->data(attachedRow, cwAttachedFixModel::ReadOnlyRole) == QVariant(true));
    CHECK_FALSE(table->flags(attachedRow).testFlag(Qt::ItemIsEditable));
    CHECK_FALSE(table->setData(attachedRow, QStringLiteral("x1"), cwFixStationModel::StationNameRole));
    CHECK(table->data(attachedRow, cwFixStationModel::StationNameRole).toString()
          == QStringLiteral("doghill.d1"));

    // A node fix's edit still reaches the node's model through the table.
    CHECK(table->setData(ownRow, QStringLiteral("G2"), cwFixStationModel::StationNameRole));
    CHECK(cave.fixStations()->fixStationAt(0).stationName() == QStringLiteral("G2"));

    // Removing a node fix shifts the attached rows up with the table.
    cave.fixStations()->removeAt(0);
    CHECK(table->rowCount() == 3);
    CHECK(table->mapFromSource(cave.attachedFixes()->index(0)).row() == 1);
}

TEST_CASE("An attached file's reference fix reads back through the reference role",
          "[AttachedFixModel][ReferenceFix]")
{
    auto fixture = makeSavedProject(QStringLiteral("attached-reference-fix"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCave* root = attachSourceUnderRegion(
        fixture.get(), fixturePath(QStringLiteral("survex_reference_fix/reference_fix.svx")));
    drainPipelines(fixture.get());

    // The file's fixes sit inside *begin ref, a leaf block with shots, so the
    // derived trip "ref" lists them.
    const cwTrip* holder = nullptr;
    for (const cwTrip* trip : root->trips()) {
        if (trip->name() == QStringLiteral("ref")) {
            holder = trip;
        }
    }
    REQUIRE(holder != nullptr);
    const cwAttachedFixModel* model = holder->attachedFixes();
    REQUIRE(model->rowCount() == 2);
    const QList<cwAttachedFix>& fixes = model->attachedFixes();
    for (int row = 0; row < model->rowCount(); ++row) {
        INFO("row " << row << ": " << fixes.at(row).station.toStdString());
        CHECK(model->data(model->index(row), cwAttachedFixModel::ReferenceRole).toBool()
              == fixes.at(row).reference);
    }

    const QModelIndex marked = model->index(0);
    CHECK(model->data(marked, cwFixStationModel::StationNameRole).toString() == QStringLiteral("ref.A"));
    const QString markedCoordinate =
        model->data(marked, cwFixStationModel::CoordinateTextRole).toString();
    INFO("A's coordinate: " << markedCoordinate.toStdString());
    CHECK(markedCoordinate == QStringLiteral("10 20 30"));
    CHECK(model->data(marked, cwAttachedFixModel::ReferenceRole) == QVariant(true));

    const QModelIndex plain = model->index(1);
    CHECK(model->data(plain, cwFixStationModel::StationNameRole).toString() == QStringLiteral("ref.B"));
    CHECK(model->data(plain, cwAttachedFixModel::ReferenceRole) == QVariant(false));

    // The page binds the node's concatenated table, which carries the role by name.
    CHECK(root->fixStationTable()->roleNames().value(cwAttachedFixModel::ReferenceRole)
          == QByteArrayLiteral("reference"));
}

TEST_CASE("An attached grid fix exposes its coordinate's numbers in meters",
          "[AttachedFixModel][cwAttachedFixModel]")
{
    cwAttachedFix withStandardErrors{QStringLiteral("S2"), QStringLiteral("500200 3000000 100 0.05 0.1"),
                                     QStringLiteral("UTM48N"), QStringLiteral("own_cs_fix.svx")};

    cwAttachedFixModel model;
    model.setAttachedFixes({{QStringLiteral("S1"), QStringLiteral("500200 3000000 100"),
                             QStringLiteral("UTM48N"), QStringLiteral("own_cs_fix.svx")},
                            bareFix(QStringLiteral("doghill.d1")),
                            withStandardErrors});
    REQUIRE(model.rowCount() == 3);

    const Components grid = componentsOf(model, 0);
    CHECK(grid.easting == kGridEasting);
    CHECK(grid.northing == kGridNorthing);
    CHECK(grid.elevation == kGridElevation);

    // The page formats those numbers the way it does a native row's.
    CHECK(cwCoordinateText::format(grid.easting, grid.northing, grid.elevation, cwUnits::Imperial,
                                   cwCoordinateText::EastingNorthing).toStdString()
          == "500200, 3000000, 328.08ft");
    CHECK(cwCoordinateText::format(grid.easting, grid.northing, grid.elevation, cwUnits::Metric,
                                   cwCoordinateText::EastingNorthing).toStdString()
          == "500200, 3000000, 100.000m");

    // A bare fix sits in the local grid.
    const Components bare = componentsOf(model, 1);
    CHECK(bare.easting == 0.0);
    CHECK(bare.northing == 0.0);
    CHECK(bare.elevation == 0.0);

    // Survex's standard errors after the three numbers are no part of the position.
    const Components spread = componentsOf(model, 2);
    CHECK(spread.easting == kGridEasting);
    CHECK(spread.northing == kGridNorthing);
    CHECK(spread.elevation == kGridElevation);

    // The text stays as the file writes it.
    CHECK(model.data(model.index(2), cwFixStationModel::CoordinateTextRole).toString()
          == QStringLiteral("500200 3000000 100 0.05 0.1"));
}

TEST_CASE("An attached fix in degrees or in unreadable text exposes no numbers",
          "[AttachedFixModel][cwAttachedFixModel]")
{
    cwAttachedFix wallsLatLong{QStringLiteral("L1"), QStringLiteral("-97.5 31.2 100"),
                               QStringLiteral("NAD27 CONUS, latitude/longitude"),
                               QStringLiteral("LATLONG.SRV")};
    wallsLatLong.placementSystem = QStringLiteral("EPSG:4267");

    cwAttachedFixModel model;
    model.setAttachedFixes({wallsLatLong,
                            {QStringLiteral("L2"), QStringLiteral("-97.5 31.2 100"),
                             QStringLiteral("LONG-LAT"), QStringLiteral("long_lat.svx")},
                            {QStringLiteral("C1"), QStringLiteral("f,1,2,3"), QString(),
                             QStringLiteral("cave.mak")}});
    REQUIRE(model.rowCount() == 3);

    for (int row = 0; row < model.rowCount(); ++row) {
        INFO("row " << row << ": "
                    << model.data(model.index(row), cwFixStationModel::CoordinateTextRole)
                           .toString().toStdString());
        const Components components = componentsOf(model, row);
        CHECK(std::isnan(components.easting));
        CHECK(std::isnan(components.northing));
        CHECK(std::isnan(components.elevation));
    }
}
