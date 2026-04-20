//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwSketch.h"
#include "cwTrip.h"

TEST_CASE("cwScrap defaults to NoteParent with nullptr note", "[cwScrap][sketchParent]") {
    cwScrap scrap;
    CHECK(scrap.parentKind()    == cwScrap::NoteParent);
    CHECK(scrap.parentNote()    == nullptr);
    CHECK(scrap.parentSketch()  == nullptr);
    CHECK(scrap.parentTrip()    == nullptr);
    CHECK(scrap.parentCave()    == nullptr);
}

TEST_CASE("cwScrap set to a sketch reports SketchParent and null note", "[cwScrap][sketchParent]") {
    cwCave cave;
    cwTrip trip;
    trip.setParentCave(&cave);

    cwSketch sketch;
    sketch.setParentTrip(&trip);

    cwScrap scrap;
    scrap.setParentSketch(&sketch);

    CHECK(scrap.parentKind()   == cwScrap::SketchParent);
    CHECK(scrap.parentSketch() == &sketch);
    CHECK(scrap.parentNote()   == nullptr);
    CHECK(scrap.parentTrip()   == &trip);
    CHECK(scrap.parentCave()   == &cave);
}

TEST_CASE("cwScrap with a sketch whose trip is not yet attached degrades safely",
          "[cwScrap][sketchParent]") {
    cwSketch sketch; // no parent trip set
    cwScrap scrap;
    scrap.setParentSketch(&sketch);

    CHECK(scrap.parentKind() == cwScrap::SketchParent);
    CHECK(scrap.parentTrip() == nullptr);
    CHECK(scrap.parentCave() == nullptr);
}

TEST_CASE("cwScrap with a note still routes parentTrip/parentCave through the note",
          "[cwScrap][sketchParent]") {
    cwCave cave;
    cwTrip trip;
    trip.setParentCave(&cave);

    cwNote note;
    note.setParentTrip(&trip);

    cwScrap scrap;
    scrap.setParentNote(&note);

    CHECK(scrap.parentKind()   == cwScrap::NoteParent);
    CHECK(scrap.parentNote()   == &note);
    CHECK(scrap.parentSketch() == nullptr);
    CHECK(scrap.parentTrip()   == &trip);
    CHECK(scrap.parentCave()   == &cave);
}

TEST_CASE("Setting a sketch parent detaches any existing note parent",
          "[cwScrap][sketchParent]") {
    cwCave noteCave;
    cwTrip noteTrip;
    noteTrip.setParentCave(&noteCave);
    cwNote note;
    note.setParentTrip(&noteTrip);

    cwCave sketchCave;
    cwTrip sketchTrip;
    sketchTrip.setParentCave(&sketchCave);
    cwSketch sketch;
    sketch.setParentTrip(&sketchTrip);

    cwScrap scrap;
    scrap.setParentNote(&note);
    scrap.setParentSketch(&sketch);

    CHECK(scrap.parentKind()   == cwScrap::SketchParent);
    CHECK(scrap.parentNote()   == nullptr);
    CHECK(scrap.parentSketch() == &sketch);
    CHECK(scrap.parentTrip()   == &sketchTrip);
    CHECK(scrap.parentCave()   == &sketchCave);
}

TEST_CASE("Setting a note parent detaches any existing sketch parent",
          "[cwScrap][sketchParent]") {
    cwCave cave;
    cwTrip trip;
    trip.setParentCave(&cave);
    cwNote note;
    note.setParentTrip(&trip);
    cwSketch sketch;
    sketch.setParentTrip(&trip);

    cwScrap scrap;
    scrap.setParentSketch(&sketch);
    REQUIRE(scrap.parentKind() == cwScrap::SketchParent);

    scrap.setParentNote(&note);

    CHECK(scrap.parentKind()   == cwScrap::NoteParent);
    CHECK(scrap.parentNote()   == &note);
    CHECK(scrap.parentSketch() == nullptr);
}

TEST_CASE("Clearing the sketch parent reverts to unparented", "[cwScrap][sketchParent]") {
    cwCave cave;
    cwTrip trip;
    trip.setParentCave(&cave);
    cwSketch sketch;
    sketch.setParentTrip(&trip);

    cwScrap scrap;
    scrap.setParentSketch(&sketch);
    REQUIRE(scrap.parentKind() == cwScrap::SketchParent);

    scrap.setParentSketch(nullptr);

    CHECK(scrap.parentKind()   == cwScrap::NoteParent);
    CHECK(scrap.parentSketch() == nullptr);
    CHECK(scrap.parentNote()   == nullptr);
    CHECK(scrap.parentTrip()   == nullptr);
    CHECK(scrap.parentCave()   == nullptr);
}
