/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteManager.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {

// The palette manager is an app-global singleton shared across cwRootData
// instances in a test process; each project open re-points it. Reset it to "no
// project palettes" on the way out so a sibling test that inspects the singleton
// before opening its own project never sees this test's (deleted) temp path.
struct PaletteManagerGuard {
    ~PaletteManagerGuard()
    {
        if (auto *manager = cwSymbologyPaletteManager::instance()) {
            manager->setPaletteDirectory(QString());
        }
    }
};

// The project-root palettes/ directory for a live project — the sibling of the
// (cave-scanned) data root, where forked palettes are written.
QString projectPaletteDirectory(cwProject *project)
{
    const QString projectRoot =
        QFileInfo(project->dataRootDir().absolutePath()).absolutePath();
    return QDir(projectRoot).absoluteFilePath(cwSymbologyPaletteManager::folderName());
}

} // namespace

TEST_CASE("Opening a project points the palette manager at the project's palettes dir",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();
    REQUIRE(manager != nullptr);

    // The temp project that cwRootData stands up already re-pointed the manager
    // (via cwSaveLoad::setFileName) at <projectRoot>/palettes.
    const QString expected = projectPaletteDirectory(project);
    CHECK(QDir(manager->paletteDirectory()).absolutePath() == QDir(expected).absolutePath());
    CHECK(manager->paletteDirectory().endsWith(cwSymbologyPaletteManager::folderName()));

    // With nothing forked yet, only the shipped (qrc) default is present.
    CHECK(manager->palettes().size() == 1);
    CHECK(manager->defaultPalette() != nullptr);
}

TEST_CASE("A forked palette is written under the project root",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();

    cwSymbologyPalette *fork =
        manager->duplicatePalette(manager->defaultPalette(), QStringLiteral("My Fork"));
    REQUIRE(fork != nullptr);
    CHECK(fork->isWritable());

    // The fork's files live under the project's palettes/ dir, not app-data.
    const QDir paletteDir(projectPaletteDirectory(project));
    REQUIRE(paletteDir.exists());
    const QStringList subdirs = paletteDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    REQUIRE(subdirs.size() == 1);
    CHECK(QFile::exists(paletteDir.absoluteFilePath(subdirs.first()
                                                   + QStringLiteral("/palette.json"))));

    // default + the fork.
    CHECK(manager->palettes().size() == 2);
    CHECK(manager->paletteById(fork->id()) == fork);
}

TEST_CASE("A forked palette rides into a bundled .cw and reloads",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();

    cwSymbologyPalette *fork =
        manager->duplicatePalette(manager->defaultPalette(), QStringLiteral("Bundled Fork"));
    REQUIRE(fork != nullptr);
    const QUuid forkId = fork->id();

    QTemporaryDir saveDir;
    REQUIRE(saveDir.isValid());
    const QString savePath =
        saveDir.filePath(QStringLiteral("palette-bundle-%1.cw")
                             .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(savePath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    REQUIRE(project->fileType() == cwProject::BundledGitFileType);

    // Reopen the bundle in a fresh root: extracting the .cw restores palettes/
    // (cwZip::zipDirectory packed the whole project root), and loading re-points
    // the manager at the extracted directory.
    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(savePath);
    root2->project()->waitLoadToFinish();

    auto *manager2 = root2->paletteManager();
    REQUIRE(manager2->paletteById(forkId) != nullptr);
    CHECK(manager2->paletteById(forkId)->name() == QStringLiteral("Bundled Fork"));
}

TEST_CASE("A forked palette survives a .cwproj round-trip",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();

    cwSymbologyPalette *fork =
        manager->duplicatePalette(manager->defaultPalette(), QStringLiteral("Dir Fork"));
    REQUIRE(fork != nullptr);
    const QUuid forkId = fork->id();

    QTemporaryDir saveDir;
    REQUIRE(saveDir.isValid());
    const QString projectPath =
        saveDir.filePath(QStringLiteral("palette-dir-%1.cwproj")
                             .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    // saveAs moves the .cwproj into a sibling directory; filename() is the real path.
    const QString actualPath = project->filename();

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(actualPath);
    root2->project()->waitLoadToFinish();

    auto *manager2 = root2->paletteManager();
    REQUIRE(manager2->paletteById(forkId) != nullptr);
    CHECK(manager2->paletteById(forkId)->name() == QStringLiteral("Dir Fork"));
}
