/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSymbologyPaletteManager.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyPaletteIO.h"
#include "cwLineBrush.h"
#include "cwDecorationLayer.h"
#include "cwPlacementRuleData.h"
#include "cwSymbologyGlyph.h"
#include "cwPenStroke.h"
#include "cwPenPoint.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwError.h"
#include "cwSymbologyErrorCodes.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPointF>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// Writes a one-brush palette with the given id into <root>/<subdir>.
void writePalette(const QString &root, const QString &subdir, const QUuid &id,
                  const QString &displayName)
{
    cwLineBrush brush;
    brush.name = QStringLiteral("wall");

    cwSymbologyPaletteData palette;
    palette.id = id;
    palette.name = displayName;
    palette.lineBrushes = {brush};

    const QString dir = QDir(root).filePath(subdir);
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(palette, dir).hasError());
}

// Writes a one-brush palette whose single decoration layer has the given rule
// stack (by display name). save() does not validate, so this can write a
// malformed stack for load-time validation to catch.
void writeRuleStackPalette(const QString &root, const QString &subdir, const QUuid &id,
                           const QStringList &ruleNames)
{
    cwDecorationLayer layer;
    for (const QString &ruleName : ruleNames) {
        layer.rules.append(cwPlacementRuleData{ruleName, {}});
    }

    cwLineBrush brush;
    brush.name = QStringLiteral("wall");
    brush.decorations = {layer};

    cwSymbologyPaletteData palette;
    palette.id = id;
    palette.name = QStringLiteral("Stack Test");
    palette.lineBrushes = {brush};

    REQUIRE_FALSE(cwSymbologyPaletteIO::save(palette, QDir(root).filePath(subdir)).hasError());
}

bool modelHasWarning(cwErrorModel *model, SymbologyErrorCode code)
{
    const QList<cwError> errors = model->errors()->toList();
    for (const cwError &error : errors) {
        if (error.errorTypeId() == static_cast<int>(code) && error.type() == cwError::Warning) {
            return true;
        }
    }
    return false;
}

// A minimal two-point glyph whose stroke uses the seed's "wall" brush, so it
// resolves (and stays acyclic) inside a fork of the seed palette.
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

} // namespace

TEST_CASE("Manager always provides the default palette", "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path()); // empty dir: only the default

    REQUIRE(manager.palettes().size() == 1);

    cwSymbologyPalette *defaultPalette = manager.defaultPalette();
    REQUIRE(defaultPalette != nullptr);

    // It loads from the embedded qrc keyed by the seed id, and is listed first.
    CHECK(manager.paletteById(cwSymbologyPaletteSeed::id()) == defaultPalette);
    CHECK(manager.palettes().constFirst() == defaultPalette);

    // Shipped read-only — the user edits a copy, never the built-in.
    CHECK_FALSE(defaultPalette->isWritable());

    // The qrc payload carries the seed's brushes, so the cross-palette
    // substitution keys resolve out of the box.
    CHECK(defaultPalette->brush(cwSymbologyPaletteSeed::wallBrushName()).has_value());
    CHECK(defaultPalette->brush(cwSymbologyPaletteSeed::featureBrushName()).has_value());
    CHECK(defaultPalette->brush(cwSymbologyPaletteSeed::scrapOutlineBrushName()).has_value());
    CHECK(defaultPalette->brush(cwSymbologyPaletteSeed::floorStepBrushName()).has_value());

    CHECK(manager.errorModel()->errors()->isEmpty());
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);
}

TEST_CASE("Manager enumerates installed palettes by Palette.id, not directory name",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid idA = QUuid::createUuid();
    const QUuid idB = QUuid::createUuid();
    // Directory names deliberately differ from the palettes' display names.
    writePalette(temp.path(), QStringLiteral("dir-one"), idA, QStringLiteral("Alpha"));
    writePalette(temp.path(), QStringLiteral("dir-two"), idB, QStringLiteral("Beta"));

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    // default + two installed.
    CHECK(manager.palettes().size() == 3);

    auto *a = manager.paletteById(idA);
    auto *b = manager.paletteById(idB);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a->name() == QStringLiteral("Alpha"));
    CHECK(b->name() == QStringLiteral("Beta"));
}

TEST_CASE("Manager resolves duplicate palette ids first-scanned-wins",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid sharedId = QUuid::createUuid();
    // entryInfoList scans by name, so "a-first" precedes "b-second".
    writePalette(temp.path(), QStringLiteral("a-first"), sharedId, QStringLiteral("First"));
    writePalette(temp.path(), QStringLiteral("b-second"), sharedId, QStringLiteral("Second"));

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    // default + exactly one of the two duplicates.
    CHECK(manager.palettes().size() == 2);
    auto *kept = manager.paletteById(sharedId);
    REQUIRE(kept != nullptr);
    CHECK(kept->name() == QStringLiteral("First"));

    // The dropped duplicate is reported, not silently swallowed.
    CHECK_FALSE(manager.errorModel()->errors()->isEmpty());
    CHECK(manager.errorModel()->warningCount() >= 1);
}

TEST_CASE("Manager surfaces a typed rule-stack warning in its error model",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid id = QUuid::createUuid();
    // A placement rule under a polyline terminal is dead — a non-fatal warning.
    writeRuleStackPalette(temp.path(), QStringLiteral("dead-rule"), id,
                          {QStringLiteral("Uniform spacing"), QStringLiteral("Trace")});

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    // Non-fatal: the palette still loads.
    CHECK(manager.paletteById(id) != nullptr);

    // The typed problem reaches the owned model with severity and code intact —
    // the dividend of routing through cwErrorModel rather than flattened strings.
    cwErrorModel *model = manager.errorModel();
    CHECK(model->fatalCount() == 0);
    CHECK(model->warningCount() >= 1);
    CHECK(modelHasWarning(model, SymbologyErrorCode::DeadRulesUnderTrace));
}

TEST_CASE("Manager refuses a palette with a fatal rule-stack problem",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid id = QUuid::createUuid();
    // Two terminal rules — the engine would resolve the ambiguity arbitrarily, so
    // the load is fatal and the whole palette is refused.
    writeRuleStackPalette(temp.path(), QStringLiteral("two-terminals"), id,
                          {QStringLiteral("Rigid stamp"), QStringLiteral("Trace")});

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    // The palette is not installed (default only), and the failure is reported.
    CHECK(manager.paletteById(id) == nullptr);
    CHECK_FALSE(manager.errorModel()->errors()->isEmpty());
}

TEST_CASE("Manager forks a read-only palette into a writable copy",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    cwSymbologyPalette *seed = manager.defaultPalette();
    REQUIRE(seed != nullptr);
    REQUIRE_FALSE(seed->isWritable());
    const QUuid seedId = seed->id();
    const int seedBrushCount = seed->lineBrushes().size();
    const int seedGlyphCount = seed->glyphs().size();

    cwSymbologyPalette *fork = manager.duplicatePalette(seed, QStringLiteral("My Palette"));
    REQUIRE(fork != nullptr);

    // Fresh identity, editable, renamed.
    CHECK(fork->id() != seedId);
    CHECK(fork->isWritable());
    CHECK(fork->name() == QStringLiteral("My Palette"));

    // Content carried across verbatim (names are the in-palette keys).
    CHECK(fork->lineBrushes().size() == seedBrushCount);
    CHECK(fork->glyphs().size() == seedGlyphCount);
    CHECK(fork->brush(cwSymbologyPaletteSeed::wallBrushName()).has_value());

    // A new subdirectory was written and rediscovered: default + fork.
    CHECK(manager.palettes().size() == 2);
    CHECK(QDir(temp.path()).exists(QStringLiteral("my-palette")));
}

TEST_CASE("Manager de-duplicates fork directory names",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    // defaultPalette() is re-fetched each time: duplicatePalette() reloads and
    // invalidates the prior pointers.
    cwSymbologyPalette *first =
        manager.duplicatePalette(manager.defaultPalette(), QStringLiteral("Caves"));
    cwSymbologyPalette *second =
        manager.duplicatePalette(manager.defaultPalette(), QStringLiteral("Caves"));
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    CHECK(first->id() != second->id());
    CHECK(QDir(temp.path()).exists(QStringLiteral("caves")));
    CHECK(QDir(temp.path()).exists(QStringLiteral("caves-2")));
    CHECK(manager.palettes().size() == 3); // default + two forks
}

TEST_CASE("Manager saves a glyph into a writable palette",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    cwSymbologyPalette *fork =
        manager.duplicatePalette(manager.defaultPalette(), QStringLiteral("Editable"));
    REQUIRE(fork != nullptr);
    const QUuid forkId = fork->id();
    const int glyphsBefore = fork->glyphs().size();

    const cwSymbologyGlyph glyph = makeGlyph(QStringLiteral("my-tick"));
    REQUIRE(manager.saveGlyph(fork, glyph));

    // Refreshed in place — same pointer, now carrying the glyph.
    CHECK(manager.paletteById(forkId) == fork);
    CHECK(fork->glyphs().size() == glyphsBefore + 1);
    const auto saved = fork->glyph(QStringLiteral("my-tick"));
    REQUIRE(saved.has_value());
    CHECK(saved.value() == glyph);

    // The glyph file landed under the fork's glyphs/ subdir.
    const QString glyphFile =
        QDir(fork->directory()).absoluteFilePath(QStringLiteral("glyphs/my-tick.cwglyph"));
    CHECK(QFile::exists(glyphFile));
}

TEST_CASE("Manager refuses to save a glyph into a read-only palette",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    cwSymbologyPalette *seed = manager.defaultPalette();
    REQUIRE(seed != nullptr);
    REQUIRE_FALSE(seed->isWritable());

    const int errorsBefore = manager.errorModel()->errors()->size();
    CHECK_FALSE(manager.saveGlyph(seed, makeGlyph(QStringLiteral("nope"))));

    // Nothing added, problem reported.
    CHECK_FALSE(seed->glyph(QStringLiteral("nope")).has_value());
    CHECK(manager.errorModel()->errors()->size() > errorsBefore);
}

TEST_CASE("Manager removes a glyph from a writable palette",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    cwSymbologyPalette *fork =
        manager.duplicatePalette(manager.defaultPalette(), QStringLiteral("Trimmed"));
    REQUIRE(fork != nullptr);
    REQUIRE(manager.saveGlyph(fork, makeGlyph(QStringLiteral("doomed"))));
    REQUIRE(fork->glyph(QStringLiteral("doomed")).has_value());

    REQUIRE(manager.removeGlyph(fork, QStringLiteral("doomed")));
    CHECK_FALSE(fork->glyph(QStringLiteral("doomed")).has_value());

    const QString glyphFile =
        QDir(fork->directory()).absoluteFilePath(QStringLiteral("glyphs/doomed.cwglyph"));
    CHECK_FALSE(QFile::exists(glyphFile));
}

TEST_CASE("Manager rejects a path-unsafe glyph name on remove",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    cwSymbologyPalette *fork =
        manager.duplicatePalette(manager.defaultPalette(), QStringLiteral("Guarded"));
    REQUIRE(fork != nullptr);

    // A path-traversal name is rejected on the kebab-case guard before any
    // filesystem touch, and the problem is reported.
    const int errorsBefore = manager.errorModel()->errors()->size();
    CHECK_FALSE(manager.removeGlyph(fork, QStringLiteral("../palette")));
    CHECK(manager.errorModel()->errors()->size() > errorsBefore);
}
