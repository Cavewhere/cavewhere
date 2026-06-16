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
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwError.h"
#include "cwSymbologyErrorCodes.h"

// Qt
#include <QCoreApplication>
#include <QDir>
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

} // namespace

TEST_CASE("Manager always provides the seed palette", "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path()); // empty dir: only the seed

    REQUIRE(manager.palettes().size() == 1);
    CHECK(manager.seedPalette() != nullptr);
    CHECK(manager.paletteById(cwSymbologyPaletteSeed::id()) == manager.seedPalette());
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

    // seed + two installed.
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

    // seed + exactly one of the two duplicates.
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

    // The palette is not installed (seed only), and the failure is reported.
    CHECK(manager.paletteById(id) == nullptr);
    CHECK_FALSE(manager.errorModel()->errors()->isEmpty());
}
