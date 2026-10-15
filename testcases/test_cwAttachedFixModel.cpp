/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAttachedFixModel.h"
#include "cwCave.h"
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
