/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwPassageTubeCases.h"
#include "cwPassageTubeGalleryModel.h"
#include "cwRenderLinePlot.h"
#include "cwRenderPassageTube.h"
#include "cwScene.h"

//Qt includes
#include <QSignalSpy>

namespace {

using Model = cwPassageTubeGalleryModel;

const QString kSymmetricT = QStringLiteral("1. symmetric T");
const QString kJunctionFreeCase = QStringLiteral("no arms, through profile 0, bend 0, same section");

QString nameAt(const Model& model, int row)
{
    return model.data(model.index(row), Model::NameRole).toString();
}

} // namespace

TEST_CASE("cwPassageTubeGalleryModel lists the catalog", "[PassageTube][cwPassageTubeGalleryModel]")
{
    const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();

    Model model;
    CHECK(model.currentIndex() == -1);

    SECTION("without the sweep, the catalog's hand-made cases in order")
    {
        QStringList expected;
        for (const cwPassageTubeCases::Case& entry : catalog) {
            if (!entry.tags.contains(QStringLiteral("sweep"))) {
                expected.append(entry.name);
            }
        }
        REQUIRE(model.rowCount() == expected.size());
        for (int i = 0; i < expected.size(); ++i) {
            CHECK(nameAt(model, i) == expected.at(i));
        }
    }

    SECTION("with the sweep, every catalog case")
    {
        model.setIncludeSweep(true);
        REQUIRE(model.rowCount() == catalog.size());
        CHECK(nameAt(model, model.rowCount() - 1) == catalog.constLast().name);
    }
}

TEST_CASE("cwPassageTubeGalleryModel builds the selected case", "[PassageTube][cwPassageTubeGalleryModel]")
{
    cwScene scene;
    Model model;
    model.setScene(&scene);
    REQUIRE(model.tube() != nullptr);
    REQUIRE(model.linePlot() != nullptr);

    const int row = model.indexOf(kSymmetricT);
    REQUIRE(row >= 0);
    CHECK(nameAt(model, row).startsWith(QStringLiteral("1. Symmetric T")));

    QSignalSpy currentSpy(&model, &Model::currentChanged);
    model.setCurrentIndex(row);
    CHECK(currentSpy.count() == 1);

    CHECK(model.currentName() == nameAt(model, row));
    CHECK_FALSE(model.mesh().isEmpty());
    CHECK(model.reportText().isEmpty() == model.report().issues.isEmpty());
    CHECK(model.issueTexts().size() == model.report().issues.size());
    CHECK(model.issuePositions().size() == model.report().issues.size());
    CHECK(model.data(model.index(row), Model::IssueCountRole).toInt() == model.report().issues.size());

    CHECK(model.tube()->positions() == model.mesh().positions);
    CHECK_FALSE(model.linePlot()->points().isEmpty());

    CHECK(model.junction() == QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(model.boundsRadius() > 0.0);
    CHECK(qFuzzyCompare(model.armDirection().length(), 1.0f));
    CHECK(qFuzzyIsNull(model.armDirection().z()));
}

TEST_CASE("cwPassageTubeGalleryModel appends random surveys", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    const int rowsBefore = model.rowCount();
    QSignalSpy countSpy(&model, &Model::countChanged);

    model.addRandom(7, 3);

    REQUIRE(model.rowCount() == rowsBefore + 1);
    CHECK(countSpy.count() == 1);
    const QString name = nameAt(model, rowsBefore);
    CHECK(name.contains(QStringLiteral("seed 7")));

    model.setCurrentIndex(rowsBefore);
    CHECK_FALSE(model.mesh().isEmpty());

    SECTION("random rows survive a catalog reload")
    {
        model.setIncludeSweep(true);
        CHECK(nameAt(model, model.rowCount() - 1) == name);
    }
}

TEST_CASE("cwPassageTubeGalleryModel counts no issues on a junction-free case", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    model.setIncludeSweep(true);

    const int row = model.indexOf(kJunctionFreeCase);
    REQUIRE(row >= 0);
    CHECK(model.data(model.index(row), Model::IssueCountRole).toInt() == 0);
    CHECK(model.data(model.index(row), Model::IssueSummaryRole).toString().isEmpty());
}
