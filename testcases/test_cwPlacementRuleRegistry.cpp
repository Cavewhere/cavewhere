/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwPlacementRule.h"
#include "cwPlacementRuleRegistry.h"

// Qt
#include <QStringList>

TEST_CASE("Registry resolves the iter-1 rules by name", "[cwPlacementRuleRegistry]")
{
    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();

    const QStringList expected = {
        QStringLiteral("Uniform spacing"),
        QStringLiteral("Explicit point"),
        QStringLiteral("Align to tangent"),
        QStringLiteral("Rigid stamp"),
        QStringLiteral("Jointed stamp"),
        QStringLiteral("Bending stamp"),
        QStringLiteral("Trace offset polyline"),
    };

    for (const QString &name : expected) {
        INFO("rule: " << name.toStdString());
        REQUIRE(registry.rule(name) != nullptr);
        CHECK(registry.rule(name)->displayName() == name);
    }
}

TEST_CASE("Registry returns nullptr for an unknown rule name", "[cwPlacementRuleRegistry]")
{
    CHECK(cwPlacementRuleRegistry::instance().rule(QStringLiteral("not-a-rule")) == nullptr);
    // "Point stamp" collapsed into "Rigid stamp" (Explicit point seeds the single
    // position); it is no longer a registered rule.
    CHECK(cwPlacementRuleRegistry::instance().rule(QStringLiteral("Point stamp")) == nullptr);
}

TEST_CASE("Each rule reports its stage", "[cwPlacementRuleRegistry]")
{
    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();

    CHECK(registry.rule(QStringLiteral("Uniform spacing"))->stage() == cwPlacementRule::Generate);
    CHECK(registry.rule(QStringLiteral("Explicit point"))->stage() == cwPlacementRule::Generate);
    CHECK(registry.rule(QStringLiteral("Align to tangent"))->stage() == cwPlacementRule::MutatePerLayer);
    CHECK(registry.rule(QStringLiteral("Rigid stamp"))->stage() == cwPlacementRule::Terminal);
    CHECK(registry.rule(QStringLiteral("Jointed stamp"))->stage() == cwPlacementRule::Terminal);
    CHECK(registry.rule(QStringLiteral("Bending stamp"))->stage() == cwPlacementRule::Terminal);
    CHECK(registry.rule(QStringLiteral("Trace offset polyline"))->stage() == cwPlacementRule::Terminal);
}

TEST_CASE("Terminal rules report their output kind; others report none",
          "[cwPlacementRuleRegistry]")
{
    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();

    CHECK(registry.rule(QStringLiteral("Rigid stamp"))->outputKind()
          == cwPlacementRule::OutputKind::Stamps);
    CHECK(registry.rule(QStringLiteral("Jointed stamp"))->outputKind()
          == cwPlacementRule::OutputKind::Stamps);
    CHECK(registry.rule(QStringLiteral("Bending stamp"))->outputKind()
          == cwPlacementRule::OutputKind::Stamps);
    CHECK(registry.rule(QStringLiteral("Trace offset polyline"))->outputKind()
          == cwPlacementRule::OutputKind::Polylines);

    CHECK(registry.rule(QStringLiteral("Uniform spacing"))->outputKind()
          == cwPlacementRule::OutputKind::None);
    CHECK(registry.rule(QStringLiteral("Align to tangent"))->outputKind()
          == cwPlacementRule::OutputKind::None);
}
