/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Cavewhere includes
#include "cwScrap.h"
#include "cwProject.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwScrap.h"
#include "cwNoteTranformation.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwCavingRegion.h"

//Our includes
#include "TestHelper.h"

//Qt includes
#include <QtGlobal>

TEST_CASE("Auto Calculate Note Transform", "[ScrapTest]") {

    class TestRow {
    public:
        TestRow(QString filename, double rotation, double scale) :
            Filename(filename),
            Rotation(rotation),
            Scale(scale) {}
        TestRow() {}

        QString Filename;
        double Rotation;
        double Scale;
    };

    QList<TestRow> rows;
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileRotate.cw", 89.7529184495, 176.349));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileRotateMirror.cw", 89.5297778044, 176.696));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfile.cw", -0.1062471752, 175.592));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileMirror.cw", 0.0034680627, 176.721));

    foreach(TestRow row, rows) {
        cwProject* project = fileToProject(row.Filename);

        REQUIRE(project->cavingRegion()->caveCount() == 1);
        cwCave* cave = project->cavingRegion()->cave(0);

        REQUIRE(cave->tripCount() == 1);
        cwTrip* trip = cave->trip(0);
        cwSurveyNoteModel* noteModel = trip->notes();

        REQUIRE(noteModel->rowCount() == 1);
        cwNote* note = noteModel->notes().first();

        REQUIRE(note->scraps().size() == 1);
        cwScrap* scrap = note->scrap(0);

        //Force recalculation
        scrap->setCalculateNoteTransform(false);
        scrap->setCalculateNoteTransform(true);

        cwNoteTranformation* transform = scrap->noteTransformation();

        double realScale = 1.0 / transform->scale();

        INFO("Calc Scale:" << realScale << " Row Scale:" << row.Scale );
        CHECK(realScale == Approx(row.Scale));
        INFO("Calc Up:" << transform->northUp() << " Row Up:" << row.Rotation );
        CHECK(transform->northUp() == Approx(row.Rotation));

        delete project;
    }


}
