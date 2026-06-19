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
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSymbologyGlyph.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteIO.h"
#include "cwSymbologyPaletteManager.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointF>
#include <QSignalSpy>
#include <QTemporaryDir>

namespace {

// A minimal one-stroke glyph with the given name.
cwSymbologyGlyph makeGlyph(const QString &name)
{
    cwPenPoint start;
    start.position = QPointF(0.0, 0.0);
    cwPenPoint end;
    end.position = QPointF(1.0, 0.0);

    cwPenStroke stroke;
    stroke.brushName = QStringLiteral("wall");
    stroke.points = {start, end};

    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = name;
    glyph.strokes = {stroke};
    return glyph;
}

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

    // The fork is appended in memory immediately; its files are written
    // asynchronously as first-class save jobs (palette.json + every glyph and
    // brush), so wait for the save queue to drain before inspecting disk.
    CHECK(manager->palettes().size() == 2);
    CHECK(manager->paletteById(fork->id()) == fork);
    project->waitSaveToFinish();

    // The fork's files live under the project's palettes/ dir, not app-data.
    const QDir paletteDir(projectPaletteDirectory(project));
    REQUIRE(paletteDir.exists());
    const QStringList subdirs = paletteDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    REQUIRE(subdirs.size() == 1);
    CHECK(QFile::exists(paletteDir.absoluteFilePath(subdirs.first()
                                                   + QStringLiteral("/palette.json"))));
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

TEST_CASE("Editing a glyph auto-saves it as a first-class file job",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();

    cwSymbologyPalette *fork =
        manager->duplicatePalette(manager->defaultPalette(), QStringLiteral("Editable"));
    REQUIRE(fork != nullptr);

    const cwSymbologyGlyph glyph = makeGlyph(QStringLiteral("my-tick"));
    REQUIRE(manager->saveGlyph(fork, glyph));

    // The edit marks the project modified (mirrors a cave/note edit).
    CHECK(project->modified());

    // The glyph file isn't on disk until the async save flush completes.
    const QString glyphFile =
        QDir(fork->directory()).absoluteFilePath(QStringLiteral("glyphs/my-tick.cwglyph"));
    project->waitSaveToFinish();
    REQUIRE(QFile::exists(glyphFile));

    // The bytes are exactly what the single-sourced serializer produces, so a
    // file written through the job path is identical to one cwSymbologyPaletteIO
    // would write (conflict-free merges depend on this).
    QFile file(glyphFile);
    REQUIRE(file.open(QIODevice::ReadOnly));
    CHECK(file.readAll() == cwSymbologyPaletteIO::glyphToJson(glyph));
}

TEST_CASE("Removing a glyph deletes its file via the save queue",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();

    cwSymbologyPalette *fork =
        manager->duplicatePalette(manager->defaultPalette(), QStringLiteral("Trimmed"));
    REQUIRE(fork != nullptr);

    REQUIRE(manager->saveGlyph(fork, makeGlyph(QStringLiteral("doomed"))));
    const QString glyphFile =
        QDir(fork->directory()).absoluteFilePath(QStringLiteral("glyphs/doomed.cwglyph"));
    project->waitSaveToFinish();
    REQUIRE(QFile::exists(glyphFile));

    REQUIRE(manager->removeGlyph(fork, QStringLiteral("doomed")));
    project->waitSaveToFinish();
    CHECK_FALSE(QFile::exists(glyphFile));
}

TEST_CASE("Removing a palette tears down its directory via the save queue",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();

    cwSymbologyPalette *fork =
        manager->duplicatePalette(manager->defaultPalette(), QStringLiteral("Doomed"));
    REQUIRE(fork != nullptr);
    const QUuid forkId = fork->id();

    // The async full write creates the palette directory and its files.
    const QString dir = fork->directory();
    project->waitSaveToFinish();
    REQUIRE(QDir(dir).exists());
    REQUIRE(QFile::exists(QDir(dir).absoluteFilePath(QStringLiteral("palette.json"))));

    // Removing it drops it from the manager and enqueues a recursive directory
    // teardown. `fork` is dangling after this returns true — use forkId/dir.
    REQUIRE(manager->removePalette(fork));
    CHECK(manager->paletteById(forkId) == nullptr);
    CHECK(manager->palettes().size() == 1); // only the shipped default remains

    project->waitSaveToFinish();
    CHECK_FALSE(QDir(dir).exists());
}

TEST_CASE("Removing a null or read-only palette is refused",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *manager = root->paletteManager();

    // The shipped default is read-only; nothing should remove it (or a nullptr).
    CHECK_FALSE(manager->removePalette(nullptr));
    CHECK_FALSE(manager->removePalette(manager->defaultPalette()));
    CHECK(manager->palettes().size() == 1);
    CHECK(manager->defaultPalette() != nullptr);
}

TEST_CASE("Reloading the palette manager is load-only and never writes back",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();

    cwSymbologyPalette *fork =
        manager->duplicatePalette(manager->defaultPalette(), QStringLiteral("Stable"));
    REQUIRE(fork != nullptr);
    const QUuid forkId = fork->id();
    project->waitSaveToFinish();

    // A reconciling reload re-applies each surviving palette's value through
    // setData(). With the auto-save wiring dropped around the reload (manager
    // aboutToReload -> cwSaveLoad disconnect), that setData must not re-enter the
    // write path: no file is rewritten and no local mutation is recorded. If it
    // did, a git/external sync that reloads after a pull would rewrite the
    // just-pulled palette files and re-dirty the project.
    QSignalSpy mutationSpy(project, &cwProject::localMutationOccurred);
    manager->reload();
    CHECK(manager->paletteById(forkId) != nullptr); // pointer survives the reload
    project->waitSaveToFinish();
    CHECK(mutationSpy.count() == 0);
}

TEST_CASE("A path-unsafe glyph name never escapes the palette directory",
          "[cwSymbologyPaletteProjectStorage]")
{
    PaletteManagerGuard guard;

    auto root = std::make_unique<cwRootData>();
    auto *project = root->project();
    auto *manager = root->paletteManager();

    cwSymbologyPalette *fork =
        manager->duplicatePalette(manager->defaultPalette(), QStringLiteral("Guarded"));
    REQUIRE(fork != nullptr);

    // Setting a glyph whose name isn't kebab-case mutates the in-memory palette
    // and emits glyphChanged, but connectPalette's path-safety guard refuses to
    // turn it into a file — so nothing is written, in or out of the tree.
    fork->setGlyph(makeGlyph(QStringLiteral("../escape")));
    project->waitSaveToFinish();

    const QDir paletteDir(fork->directory());
    const QString inTree =
        paletteDir.absoluteFilePath(QStringLiteral("glyphs/../escape.cwglyph"));
    const QString escaped = QFileInfo(paletteDir.absoluteFilePath(QStringLiteral(".."))
                                          ).absoluteFilePath() + QStringLiteral("/escape.cwglyph");
    CHECK_FALSE(QFile::exists(inTree));
    CHECK_FALSE(QFile::exists(escaped));
}
