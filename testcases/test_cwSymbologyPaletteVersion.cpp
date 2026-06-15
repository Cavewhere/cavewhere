/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSymbologyPaletteIO.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteManager.h"
#include "cwSymbologyPalette.h"
#include "cwLineBrush.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwRegionIOTask.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUuid>

namespace {

QString uniqueDir(const QTemporaryDir &temp, const QString &base)
{
    return QDir(temp.path())
        .filePath(QStringLiteral("%1-%2").arg(base).arg(QCoreApplication::applicationPid()));
}

// A minimal valid one-brush palette saved into `dir`. Every file is stamped with
// the running build's protoVersion() by the save path.
void saveMinimalPalette(const QString &dir, const QUuid &id, const QString &name)
{
    cwLineBrush brush;
    brush.name = QStringLiteral("wall");

    cwSymbologyPaletteData palette;
    palette.id = id;
    palette.name = name;
    palette.lineBrushes = {brush};
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(palette, dir).hasError());
}

// Drops a hand-authored brush file carrying a chosen format version into the
// palette's brushes/ subdir — the only way to simulate a file written by a newer
// build, since save() always stamps the current version.
void writeVersionedBrushFile(const QString &paletteDir, const QString &name, int version)
{
    const QString brushesDir = QDir(paletteDir).filePath(cwSymbologyPaletteIO::kBrushesSubdirName);
    REQUIRE(QDir().mkpath(brushesDir));
    const QString json =
        QStringLiteral("{\n  \"name\": \"%1\",\n"
                       "  \"fileVersion\": { \"version\": %2, \"cavewhereVersion\": \"future\" }\n}\n")
            .arg(name)
            .arg(version);
    QFile file(QDir(brushesDir).filePath(name + cwSymbologyPaletteIO::kBrushFileSuffix));
    REQUIRE(file.open(QFile::WriteOnly));
    file.write(json.toUtf8());
    file.close();
}

} // namespace

TEST_CASE("A saved palette is stamped with the current format version",
          "[cwSymbologyPaletteVersion]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("stamp"));

    saveMinimalPalette(dir, QUuid::createUuid(), QStringLiteral("Stamped"));

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());
    CHECK(loaded.value().maxFileVersion == cwRegionIOTask::protoVersion());
}

TEST_CASE("Load aggregates the highest version across the palette's files",
          "[cwSymbologyPaletteVersion]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("aggregate"));

    saveMinimalPalette(dir, QUuid::createUuid(), QStringLiteral("Mixed"));
    // A newer build dropped one brush in at a higher format version.
    const int futureVersion = cwRegionIOTask::protoVersion() + 100;
    writeVersionedBrushFile(dir, QStringLiteral("future-brush"), futureVersion);

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());
    // The max folds in the newer brush, not just palette.json's stamp.
    CHECK(loaded.value().maxFileVersion == futureVersion);
}

TEST_CASE("Manager marks a forward-incompatible palette read-only and warns",
          "[cwSymbologyPaletteVersion]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = uniqueDir(temp, QStringLiteral("mgr-fwd"));

    const QUuid id = QUuid::createUuid();
    const QString paletteDir = QDir(root).filePath(QStringLiteral("future-palette"));
    saveMinimalPalette(paletteDir, id, QStringLiteral("Future Palette"));
    writeVersionedBrushFile(paletteDir, QStringLiteral("future-brush"),
                            cwRegionIOTask::protoVersion() + 1);

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(root); // reload posts the warning into the owned model

    auto *palette = manager.paletteById(id);
    REQUIRE(palette != nullptr);
    CHECK_FALSE(palette->isWritable()); // read-only: re-saving can't clobber newer data
    CHECK_FALSE(manager.errorModel()->errors()->isEmpty()); // surfaced to the UI's error model
}
