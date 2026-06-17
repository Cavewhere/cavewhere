/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwRootData.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QUuid>

#include <memory>

namespace {

// Saves the project to a unique path under tempDir, then loads it into a fresh
// cwRootData and returns the reloaded project. The PID-stamped filename keeps
// concurrent test processes from colliding.
std::unique_ptr<cwRootData> saveAndReload(cwProject *project, const QTemporaryDir &tempDir,
                                          const QString &base)
{
    const QString projectPath =
        QDir(tempDir.path())
            .filePath(QStringLiteral("%1-%2.cwproj").arg(base).arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    // saveAs moves the .cwproj into a sibling directory; filename() is the real path.
    const QString actualPath = project->filename();

    auto reloaded = std::make_unique<cwRootData>();
    reloaded->project()->loadFile(actualPath);
    reloaded->project()->waitLoadToFinish();
    return reloaded;
}

} // namespace

TEST_CASE("Region defaultPaletteId round-trips through save/load",
          "[cwCavingRegionPaletteRoundTrip]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    cwProject *project = root->project();

    cwCavingRegion *region = project->cavingRegion();
    region->addCave();

    const QUuid paletteId = QUuid::createUuid();
    region->setDefaultPaletteId(paletteId);

    auto reloaded = saveAndReload(project, tempDir, QStringLiteral("palette-rt"));

    CHECK(reloaded->project()->cavingRegion()->defaultPaletteId() == paletteId);
}

TEST_CASE("A cleared region defaultPaletteId loads back as null",
          "[cwCavingRegionPaletteRoundTrip]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    cwProject *project = root->project();

    cwCavingRegion *region = project->cavingRegion();
    region->addCave();
    REQUIRE(region->defaultPaletteId().isNull()); // fresh region defaults to null
    region->setDefaultPaletteId(QUuid::createUuid());
    region->setDefaultPaletteId(QUuid());

    auto reloaded = saveAndReload(project, tempDir, QStringLiteral("palette-rt-cleared"));

    CHECK(reloaded->project()->cavingRegion()->defaultPaletteId().isNull());
}
