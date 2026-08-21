/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCavingRegion.h"
#include "cwGeoReference.h"
#include "cwProject.h"
#include "cwRootData.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QUuid>

namespace {

const QString kLocalCS = QStringLiteral(
    "+proj=tmerc +lat_0=37.1832 +lon_0=-84.0947 +k=1 +x_0=0 +y_0=0 "
    "+datum=WGS84 +units=m +no_defs +type=crs");
const QString kOtherLocalCS = QStringLiteral(
    "+proj=tmerc +lat_0=38.0 +lon_0=-85.0 +k=1 +x_0=0 +y_0=0 "
    "+datum=WGS84 +units=m +no_defs +type=crs");

QString saveProject(cwRootData* root, const QTemporaryDir& tempDir, const QString& base)
{
    const QString projectPath = QDir(tempDir.path())
        .filePath(QStringLiteral("%1-%2.cwproj").arg(base).arg(QCoreApplication::applicationPid()));
    REQUIRE(root->project()->saveAs(projectPath));
    root->project()->waitSaveToFinish();
    return root->project()->filename();
}

std::unique_ptr<cwRootData> reload(const QString& path)
{
    auto reloaded = std::make_unique<cwRootData>();
    reloaded->project()->loadFile(path);
    reloaded->project()->waitLoadToFinish();
    return reloaded;
}

} // namespace

TEST_CASE("An anchored local projection survives save/load", "[cwGeoReference]")
{
    // The LDP is the one piece of geo-reference state that is deliberately not
    // recomputed from the data — two machines with different PROJ data must not
    // disagree about where the project is — so losing it on disk loses the
    // project's frame outright.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const cwGeoReference::Anchor anchor{cwGeoReference::Anchor::FixStation,
                                        QUuid::createUuid()};

    auto root = std::make_unique<cwRootData>();
    auto* geoReference = root->project()->cavingRegion()->geoReference();
    geoReference->anchorTo(anchor, kLocalCS);
    geoReference->setVerticalDatum(QStringLiteral("NAVD88"));

    const QString path = saveProject(root.get(), tempDir, QStringLiteral("ldp-anchored"));

    auto reloadedRoot = reload(path);
    auto* reloaded = reloadedRoot->project()->cavingRegion()->geoReference();
    CHECK(reloaded->state() == cwGeoReference::Anchored);
    CHECK(reloaded->localCoordinateSystem() == kLocalCS);
    CHECK(reloaded->anchor() == anchor);
    CHECK(reloaded->verticalDatum() == QStringLiteral("NAVD88"));
}

TEST_CASE("A frozen local projection survives save/load without an anchor",
          "[cwGeoReference]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* geoReference = root->project()->cavingRegion()->geoReference();
    geoReference->anchorTo({cwGeoReference::Anchor::LazLayer, QUuid::createUuid()},
                           kLocalCS);
    geoReference->freeze();

    const QString path = saveProject(root.get(), tempDir, QStringLiteral("ldp-frozen"));

    auto reloadedRoot = reload(path);
    auto* reloaded = reloadedRoot->project()->cavingRegion()->geoReference();
    CHECK(reloaded->state() == cwGeoReference::Frozen);
    CHECK(reloaded->localCoordinateSystem() == kLocalCS);
    CHECK_FALSE(reloaded->anchor().isValid());
}

TEST_CASE("A project with no local projection loads ungeoreferenced",
          "[cwGeoReference]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    const QString path = saveProject(root.get(), tempDir, QStringLiteral("ldp-none"));

    auto reloadedRoot = reload(path);
    auto* reloaded = reloadedRoot->project()->cavingRegion()->geoReference();
    CHECK(reloaded->state() == cwGeoReference::Ungeoreferenced);
    CHECK(reloaded->localCoordinateSystem().isEmpty());
    CHECK_FALSE(reloaded->anchor().isValid());
}

TEST_CASE("A vertical datum survives save/load with no local projection",
          "[cwGeoReference]")
{
    // The vertical datum isn't part of the frame: elevations, and whatever they
    // are heights above, exist before anything anchors the project. Persisting
    // it only alongside an LDP would drop what lidar declared about a project
    // that hasn't been georeferenced yet.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    root->project()->cavingRegion()->geoReference()->setVerticalDatum(
        QStringLiteral("NAVD88"));

    const QString path = saveProject(root.get(), tempDir, QStringLiteral("ldp-datum-only"));

    auto reloadedRoot = reload(path);
    auto* reloaded = reloadedRoot->project()->cavingRegion()->geoReference();
    CHECK(reloaded->verticalDatum() == QStringLiteral("NAVD88"));
    CHECK(reloaded->state() == cwGeoReference::Ungeoreferenced);
    CHECK(reloaded->localCoordinateSystem().isEmpty());
}

TEST_CASE("A local projection change reaches disk without an explicit save",
          "[cwGeoReference]")
{
    // The LDP lives in the project metadata file, so — like unitSystem and the
    // global CS before it — the save pipeline has to watch it change. Without
    // that wiring the frame a re-derive produced is dropped on close.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const cwGeoReference::Anchor anchor{cwGeoReference::Anchor::FixStation,
                                        QUuid::createUuid()};

    auto root = std::make_unique<cwRootData>();
    auto* geoReference = root->project()->cavingRegion()->geoReference();
    geoReference->anchorTo(anchor, kLocalCS);

    const QString path = saveProject(root.get(), tempDir, QStringLiteral("ldp-incremental"));

    SECTION("a re-derived frame")
    {
        geoReference->anchorTo(anchor, kOtherLocalCS);
        root->project()->waitSaveToFinish();

        auto reloadedRoot = reload(path);
        auto* reloaded = reloadedRoot->project()->cavingRegion()->geoReference();
        CHECK(reloaded->localCoordinateSystem() == kOtherLocalCS);
    }

    SECTION("a vertical datum learned after the fact")
    {
        geoReference->setVerticalDatum(QStringLiteral("NGVD29"));
        root->project()->waitSaveToFinish();

        auto reloadedRoot = reload(path);
        auto* reloaded = reloadedRoot->project()->cavingRegion()->geoReference();
        CHECK(reloaded->verticalDatum() == QStringLiteral("NGVD29"));
    }
}
