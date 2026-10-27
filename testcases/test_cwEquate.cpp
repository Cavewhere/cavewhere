/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cavewhere.pb.h"
#include "cwCave.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwProtoUtils.h"
#include "cwStationHandle.h"
#include "cwTrip.h"

// Qt
#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QUuid>

namespace {

// A minimal, structurally-valid two-station equate for model tests, where the
// container ids don't need to resolve to any real cave/trip.
cwEquate makeEquate(const QString& tailA = QStringLiteral("1"),
                    const QString& tailB = QStringLiteral("2"))
{
    const QUuid container = QUuid::createUuid();
    return cwEquate({
        cwStationHandle(cwStationHandle::NativeCave, container, tailA),
        cwStationHandle(cwStationHandle::NativeCave, container, tailB)
    });
}

} // namespace

TEST_CASE("cwStationHandle validity and equality", "[Equate]")
{
    const QUuid container = QUuid::createUuid();

    SECTION("A handle needs a container and a tail to be valid") {
        CHECK_FALSE(cwStationHandle().isValid());
        CHECK_FALSE(cwStationHandle(cwStationHandle::NativeCave, QUuid(), QStringLiteral("1")).isValid());
        CHECK_FALSE(cwStationHandle(cwStationHandle::NativeCave, container, QString()).isValid());
        CHECK(cwStationHandle(cwStationHandle::NativeCave, container, QStringLiteral("1")).isValid());
    }

    SECTION("Equality is by scope, container, and tail") {
        const cwStationHandle a(cwStationHandle::Trip, container, QStringLiteral("1"));
        CHECK(a == cwStationHandle(cwStationHandle::Trip, container, QStringLiteral("1")));
        CHECK(a != cwStationHandle(cwStationHandle::NativeCave, container, QStringLiteral("1")));
        CHECK(a != cwStationHandle(cwStationHandle::Trip, QUuid::createUuid(), QStringLiteral("1")));
        CHECK(a != cwStationHandle(cwStationHandle::Trip, container, QStringLiteral("2")));
    }
}

TEST_CASE("cwEquate structural validity", "[Equate]")
{
    const QUuid caveId = QUuid::createUuid();
    const QUuid tripId = QUuid::createUuid();

    const cwStationHandle nativeStation(cwStationHandle::NativeCave, caveId, QStringLiteral("1"));
    const cwStationHandle tripStation(cwStationHandle::Trip, tripId, QStringLiteral("1"));

    SECTION("Fewer than two stations is invalid") {
        CHECK_FALSE(cwEquate().isValid());
        CHECK_FALSE(cwEquate({nativeStation}).isValid());
    }

    SECTION("Two distinct valid stations is valid") {
        CHECK(cwEquate({nativeStation, tripStation}).isValid());
    }

    SECTION("A station equated only to itself ties nothing") {
        CHECK_FALSE(cwEquate({nativeStation, nativeStation}).isValid());
    }

    SECTION("An invalid handle poisons the whole equate") {
        CHECK_FALSE(cwEquate({nativeStation, cwStationHandle()}).isValid());
    }

    SECTION("N-way (three stations) is valid") {
        const cwStationHandle another(cwStationHandle::NativeCave, caveId, QStringLiteral("2"));
        CHECK(cwEquate({nativeStation, tripStation, another}).isValid());
    }
}

TEST_CASE("cwEquateModel starts empty", "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    CHECK(model.count() == 0);
    CHECK(model.rowCount() == 0);
    CHECK(model.equates().isEmpty());
}

TEST_CASE("cwEquateModel appendEquate adds a row and signals it", "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy countSpy(&model, &cwEquateModel::countChanged);

    const cwEquate equate = makeEquate();
    model.appendEquate(equate);

    REQUIRE(model.count() == 1);
    CHECK(model.rowCount() == 1);
    CHECK(model.equateAt(0) == equate);

    // The persistence save-trigger hangs off rowsInserted, so the row range
    // must be reported correctly (both first and last == the new row 0).
    REQUIRE(insertSpy.count() == 1);
    CHECK(insertSpy.first().at(1).toInt() == 0);
    CHECK(insertSpy.first().at(2).toInt() == 0);
    CHECK(countSpy.count() == 1);
}

TEST_CASE("cwEquateModel removeAt drops a row and signals it", "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    model.appendEquate(makeEquate());
    model.appendEquate(makeEquate());
    REQUIRE(model.count() == 2);

    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy countSpy(&model, &cwEquateModel::countChanged);

    model.removeAt(0);
    CHECK(model.count() == 1);
    REQUIRE(removeSpy.count() == 1);
    CHECK(removeSpy.first().at(1).toInt() == 0);
    CHECK(countSpy.count() == 1);
}

TEST_CASE("cwEquateModel removeAt out of range is a silent no-op", "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    model.appendEquate(makeEquate());

    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy countSpy(&model, &cwEquateModel::countChanged);

    model.removeAt(-1);
    model.removeAt(5);

    CHECK(model.count() == 1);
    CHECK(removeSpy.count() == 0);
    CHECK(countSpy.count() == 0);
}

TEST_CASE("cwEquateModel exposes role names for QML", "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    const QHash<int, QByteArray> roles = model.roleNames();
    CHECK(roles.value(cwEquateModel::EquateRole) == "equate");
    CHECK(roles.value(cwEquateModel::StationCountRole) == "stationCount");
}

TEST_CASE("cwEquateModel data serves each role and rejects bad indexes", "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    const cwEquate equate = makeEquate();
    model.appendEquate(equate);

    const QModelIndex idx = model.index(0);

    SECTION("EquateRole returns the stored equate") {
        CHECK(model.data(idx, cwEquateModel::EquateRole).value<cwEquate>() == equate);
    }

    SECTION("StationCountRole returns the handle count") {
        CHECK(model.data(idx, cwEquateModel::StationCountRole).toInt() == 2);
    }

    SECTION("An unknown role yields an invalid QVariant") {
        CHECK_FALSE(model.data(idx, Qt::DisplayRole).isValid());
    }

    SECTION("An invalid or out-of-range index yields an invalid QVariant") {
        CHECK_FALSE(model.data(QModelIndex(), cwEquateModel::EquateRole).isValid());
        CHECK_FALSE(model.data(model.index(5), cwEquateModel::EquateRole).isValid());
    }
}

TEST_CASE("cwEquateModel equateAt out of range returns a default equate", "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    model.appendEquate(makeEquate());
    CHECK(model.equateAt(-1) == cwEquate());
    CHECK(model.equateAt(9) == cwEquate());
}

TEST_CASE("cwEquateModel setEquates replaces contents and resets", "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    model.appendEquate(makeEquate());

    const cwEquate a = makeEquate(QStringLiteral("A1"), QStringLiteral("A2"));
    const cwEquate b = makeEquate(QStringLiteral("B1"), QStringLiteral("B2"));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setEquates({a, b});

    REQUIRE(model.count() == 2);
    CHECK(model.equateAt(0) == a);
    CHECK(model.equateAt(1) == b);
    CHECK(resetSpy.count() == 1);
}

TEST_CASE("cwEquateModel setEquates emits countChanged only when the size changes",
          "[Equate][cwEquateModel]")
{
    cwEquateModel model;
    model.appendEquate(makeEquate());
    REQUIRE(model.count() == 1);

    SECTION("Same size, different content: no countChanged (reset still fires)") {
        QSignalSpy countSpy(&model, &cwEquateModel::countChanged);
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        model.setEquates({makeEquate(QStringLiteral("X"), QStringLiteral("Y"))});
        CHECK(countSpy.count() == 0);
        CHECK(resetSpy.count() == 1);
    }

    SECTION("Different size: countChanged fires") {
        QSignalSpy countSpy(&model, &cwEquateModel::countChanged);
        model.setEquates({});
        CHECK(countSpy.count() == 1);
    }
}

TEST_CASE("cwEquate proto round-trip preserves scope, container, and tail", "[Equate][proto]")
{
    const QUuid caveId = QUuid::createUuid();
    const QUuid tripId = QUuid::createUuid();

    // N-way (three handles) spanning both scopes exercises the full mapping:
    // scope int, container_uuid bytes, and tail string.
    const cwEquate original({
        cwStationHandle(cwStationHandle::NativeCave, caveId, QStringLiteral("1")),
        cwStationHandle(cwStationHandle::Trip, tripId, QStringLiteral("2")),
        cwStationHandle(cwStationHandle::NativeCave, caveId, QStringLiteral("entrance"))
    });

    CavewhereProto::Equate proto;
    cwProtoUtils::saveEquate(&proto, original);

    const cwEquate restored = cwProtoUtils::fromProtoEquate(proto);
    CHECK(restored == original);
    REQUIRE(restored.stations().size() == 3);
    CHECK(restored.stations().at(0).scope() == cwStationHandle::NativeCave);
    CHECK(restored.stations().at(1).scope() == cwStationHandle::Trip);
    CHECK(restored.stations().at(1).containerId() == tripId);
    CHECK(restored.stations().at(2).tail() == QStringLiteral("entrance"));
}

TEST_CASE("cwCave::validate accepts within-cave ties and rejects strangers", "[Equate]")
{
    cwCave cave;
    cave.addTrip();
    cwTrip* trip = cave.trip(0);
    REQUIRE(trip != nullptr);

    const QUuid caveId = cave.id();
    const QUuid tripId = trip->id();

    SECTION("Native station in this cave equated to a trip in this cave") {
        const cwEquate equate({
            cwStationHandle(cwStationHandle::NativeCave, caveId, QStringLiteral("1")),
            cwStationHandle(cwStationHandle::Trip, tripId, QStringLiteral("1"))
        });
        CHECK(cave.validate(equate));
    }

    SECTION("A native handle pointing at a foreign cave is rejected") {
        const cwEquate equate({
            cwStationHandle(cwStationHandle::NativeCave, caveId, QStringLiteral("1")),
            cwStationHandle(cwStationHandle::NativeCave, QUuid::createUuid(), QStringLiteral("2"))
        });
        CHECK_FALSE(cave.validate(equate));
    }

    SECTION("A trip handle pointing at a trip not in this cave is rejected") {
        const cwEquate equate({
            cwStationHandle(cwStationHandle::NativeCave, caveId, QStringLiteral("1")),
            cwStationHandle(cwStationHandle::Trip, QUuid::createUuid(), QStringLiteral("1"))
        });
        CHECK_FALSE(cave.validate(equate));
    }

    SECTION("A structurally invalid equate never validates") {
        const cwEquate equate({
            cwStationHandle(cwStationHandle::NativeCave, caveId, QStringLiteral("1"))
        });
        CHECK_FALSE(cave.validate(equate));
    }
}
