/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The transition table of one item's streaming state machine. Every case here
// is arithmetic over that state — no QRhi device, no streamer, no frame — which
// is the point of pulling it out of cwRhiTexturedItems.

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QByteArray>
#include <QSize>

//Our includes
#include "cwKtx2Codec.h"
#include "cwStreamedItemState.h"

namespace {

    constexpr int kBaseLevel = 2;
    constexpr int kFineLevel = 0;
    constexpr int kMidLevel = 1;
    constexpr int kLevelBytes = 16;
    constexpr int kLevelZeroSize = 512;
    constexpr QRhiTexture::Format kFormat = QRhiTexture::BC7;

    using Kind = cwStreamedItemState::Action::Kind;
    using StreamPriority = cwStreamedItemState::StreamPriority;

    cwCompressedTexture chain(int topLevel)
    {
        cwCompressedTexture texture;
        texture.format = kFormat;
        texture.size = QSize(kLevelZeroSize >> topLevel, kLevelZeroSize >> topLevel);
        texture.mipLevels.append(QByteArray(kLevelBytes, '\0'));
        return texture;
    }

    //! An item holding @a level with nothing open, the shape after a chain lands
    void makeResident(cwStreamedItemState& state, int level)
    {
        state.requestPinnedBase(level);
        state.beginUpload(chain(level), level);
        CHECK(state.takeUploadedTexture() == nullptr);
        REQUIRE(state.residentTopLevel() == level);
        REQUIRE_FALSE(state.hasOpenRequest());
    }
}

TEST_CASE("A fresh item asks for its pinned base before anything else",
          "[StreamedItemState]") {
    cwStreamedItemState state;

    CHECK(state.needsPinnedBase());
    CHECK(state.residentTopLevel() == cwStreamedItemState::kNoLevel);
    CHECK(state.requestedTopLevel() == cwStreamedItemState::kNoLevel);
    CHECK(state.isBelowDesired());

    const auto action = state.requestPinnedBase(kBaseLevel);

    CHECK(action.kind == Kind::Request);
    CHECK(action.level == kBaseLevel);
    CHECK(action.priority == cwStreamedItemState::kPinnedBasePriority);
    CHECK(state.requestedTopLevel() == kBaseLevel);
    CHECK(state.targetTopLevel() == kBaseLevel);
    CHECK_FALSE(state.isDemotionInFlight());
    CHECK_FALSE(state.needsPinnedBase());
}

TEST_CASE("A landed chain becomes resident and closes the request",
          "[StreamedItemState]") {
    cwStreamedItemState state;
    state.requestPinnedBase(kBaseLevel);

    CHECK(state.wantsLevel(kBaseLevel));
    CHECK_FALSE(state.wantsLevel(kFineLevel));
    CHECK_FALSE(state.hasPendingUpload());

    state.beginUpload(chain(kBaseLevel), kBaseLevel);

    CHECK(state.hasPendingUpload());
    CHECK(state.pendingUpload().readyTopLevel == kBaseLevel);
    CHECK(state.pendingUpload().nextLevelToUpload == 0);
    //Still the old level: nothing is resident until the whole chain has landed
    CHECK(state.residentTopLevel() == cwStreamedItemState::kNoLevel);

    CHECK(state.takeUploadedTexture() == nullptr);

    CHECK(state.residentTopLevel() == kBaseLevel);
    CHECK_FALSE(state.hasOpenRequest());
    CHECK_FALSE(state.hasPendingUpload());
    CHECK(state.targetTopLevel() == kBaseLevel);
}

TEST_CASE("Selection asks for detail the item is not heading for",
          "[StreamedItemState]") {
    cwStreamedItemState state;
    makeResident(state, kBaseLevel);

    SECTION("a finer level opens a refinement ranked by its deficit") {
        const auto action = state.refineTo(kFineLevel, StreamPriority::Live);

        CHECK(action.kind == Kind::Request);
        CHECK(action.level == kFineLevel);
        CHECK(action.priority == quint64(kBaseLevel - kFineLevel));
        CHECK(state.desiredTopLevel() == kFineLevel);
        CHECK(state.requestedTopLevel() == kFineLevel);
        CHECK_FALSE(state.isDemotionInFlight());
        CHECK(state.isBelowDesired());
    }

    SECTION("an export refinement outranks every live one") {
        const auto action = state.refineTo(kFineLevel, StreamPriority::Export);

        CHECK(action.kind == Kind::Request);
        CHECK(action.priority == cwStreamedItemState::kExportPriority);
    }

    SECTION("a level the item already holds asks for nothing") {
        const auto action = state.refineTo(kBaseLevel, StreamPriority::Live);

        CHECK(action.kind == Kind::None);
        CHECK(state.desiredTopLevel() == kBaseLevel);
        CHECK_FALSE(state.hasOpenRequest());
        CHECK_FALSE(state.isBelowDesired());
    }

    SECTION("a coarser level asks for nothing — giving detail back is the budget's job") {
        const auto action = state.refineTo(kBaseLevel + 1, StreamPriority::Live);

        CHECK(action.kind == Kind::None);
        CHECK_FALSE(state.hasOpenRequest());
        CHECK(state.residentTopLevel() == kBaseLevel);
    }

    SECTION("a refinement already open for that level asks for nothing again") {
        REQUIRE(state.refineTo(kFineLevel, StreamPriority::Live).kind == Kind::Request);

        const auto action = state.refineTo(kFineLevel, StreamPriority::Live);

        CHECK(action.kind == Kind::None);
        CHECK(state.requestedTopLevel() == kFineLevel);
    }
}

TEST_CASE("The budget demotes an item back to its pinned base", "[StreamedItemState]") {
    cwStreamedItemState state;
    makeResident(state, kFineLevel);

    const auto action = state.requestDemotion(kBaseLevel);

    CHECK(action.kind == Kind::Request);
    CHECK(action.level == kBaseLevel);
    CHECK(action.priority == cwStreamedItemState::kDemotionPriority);
    CHECK(state.isDemotionInFlight());
    CHECK(state.requestedTopLevel() == kBaseLevel);
    //The fine texture keeps drawing until the coarse chain has landed
    CHECK(state.residentTopLevel() == kFineLevel);

    state.beginUpload(chain(kBaseLevel), kBaseLevel);
    CHECK(state.takeUploadedTexture() == nullptr);

    CHECK(state.residentTopLevel() == kBaseLevel);
    CHECK_FALSE(state.isDemotionInFlight());
}

TEST_CASE("Selection during a demotion in flight", "[StreamedItemState]") {
    cwStreamedItemState state;
    makeResident(state, kFineLevel);
    REQUIRE(state.requestDemotion(kBaseLevel).kind == Kind::Request);

    SECTION("a camera back on the resident level cancels it instead of reloading") {
        const auto action = state.refineTo(kFineLevel, StreamPriority::Live);

        CHECK(action.kind == Kind::Cancel);
        CHECK_FALSE(state.hasOpenRequest());
        CHECK_FALSE(state.isDemotionInFlight());
        CHECK(state.residentTopLevel() == kFineLevel);
    }

    SECTION("a camera wanting something between supersedes it with a refinement") {
        makeResident(state, kMidLevel);
        REQUIRE(state.requestDemotion(kBaseLevel).kind == Kind::Request);

        const auto action = state.refineTo(kFineLevel, StreamPriority::Live);

        CHECK(action.kind == Kind::Request);
        CHECK(action.level == kFineLevel);
        CHECK(action.priority == quint64(kBaseLevel - kFineLevel));
        CHECK_FALSE(state.isDemotionInFlight());
        CHECK(state.requestedTopLevel() == kFineLevel);
    }
}

TEST_CASE("A failed load reopens the request slot", "[StreamedItemState]") {
    cwStreamedItemState state;
    makeResident(state, kBaseLevel);
    REQUIRE(state.refineTo(kFineLevel, StreamPriority::Live).kind == Kind::Request);

    SECTION("abandoning the request keeps what is resident") {
        state.beginUpload(chain(kFineLevel), kFineLevel);

        state.abandonRequest();

        CHECK_FALSE(state.hasOpenRequest());
        //A chain that has already landed is left to finish uploading
        CHECK(state.hasPendingUpload());
        CHECK(state.residentTopLevel() == kBaseLevel);
        //Selection is free to ask again on the next frame
        CHECK(state.refineTo(kFineLevel, StreamPriority::Live).kind == Kind::Request);
    }

    SECTION("a chain that cannot be uploaded is dropped the same way") {
        state.beginUpload(chain(kFineLevel), kFineLevel);
        REQUIRE(state.hasPendingUpload());

        state.failUpload();

        CHECK_FALSE(state.hasPendingUpload());
        CHECK_FALSE(state.hasOpenRequest());
        CHECK(state.residentTopLevel() == kBaseLevel);
    }

    SECTION("dropping a chain leaves a request opened after it landed open") {
        cwStreamedItemState uploading;
        makeResident(uploading, kBaseLevel);
        REQUIRE(uploading.refineTo(kMidLevel, StreamPriority::Live).kind == Kind::Request);
        uploading.beginUpload(chain(kMidLevel), kMidLevel);
        REQUIRE(uploading.refineTo(kFineLevel, StreamPriority::Live).kind == Kind::Request);

        uploading.failUpload();

        CHECK_FALSE(uploading.hasPendingUpload());
        CHECK(uploading.requestedTopLevel() == kFineLevel);
        CHECK(uploading.wantsLevel(kFineLevel));
    }
}

TEST_CASE("A request opened while a chain uploads outlives the swap",
          "[StreamedItemState]") {
    //A chain lands one budgeted level per frame, so selection and the budget
    //both get a say between the landing and the swap.
    cwStreamedItemState state;
    makeResident(state, kBaseLevel);
    REQUIRE(state.refineTo(kMidLevel, StreamPriority::Live).kind == Kind::Request);
    state.beginUpload(chain(kMidLevel), kMidLevel);

    SECTION("a finer level asked for mid-upload is still wanted when it lands") {
        REQUIRE(state.refineTo(kFineLevel, StreamPriority::Live).kind == Kind::Request);

        CHECK(state.takeUploadedTexture() == nullptr);

        CHECK(state.residentTopLevel() == kMidLevel);
        CHECK(state.hasOpenRequest());
        CHECK(state.requestedTopLevel() == kFineLevel);
        CHECK(state.wantsLevel(kFineLevel));
    }

    SECTION("a demotion asked for mid-upload survives the swap") {
        REQUIRE(state.requestDemotion(kBaseLevel).kind == Kind::Request);

        CHECK(state.takeUploadedTexture() == nullptr);

        CHECK(state.residentTopLevel() == kMidLevel);
        CHECK(state.isDemotionInFlight());
        CHECK(state.requestedTopLevel() == kBaseLevel);
    }
}

TEST_CASE("The budget demotes an item whose refinement is in flight",
          "[StreamedItemState]") {
    cwStreamedItemState state;
    makeResident(state, kMidLevel);
    REQUIRE(state.refineTo(kFineLevel, StreamPriority::Live).kind == Kind::Request);

    const auto action = state.requestDemotion(kBaseLevel);

    CHECK(action.kind == Kind::Request);
    CHECK(action.level == kBaseLevel);
    CHECK(action.priority == cwStreamedItemState::kDemotionPriority);
    CHECK(state.isDemotionInFlight());
    CHECK(state.requestedTopLevel() == kBaseLevel);
    CHECK(state.targetTopLevel() == kBaseLevel);
    CHECK(state.wantsLevel(kBaseLevel));
    CHECK_FALSE(state.wantsLevel(kFineLevel));

    //The camera settling back on what the item holds drops the demotion
    CHECK(state.refineTo(kMidLevel, StreamPriority::Live).kind == Kind::Cancel);
}

TEST_CASE("Releasing gives every level back", "[StreamedItemState]") {
    cwStreamedItemState state;
    makeResident(state, kFineLevel);
    REQUIRE(state.refineTo(kFineLevel, StreamPriority::Live).kind == Kind::None);
    REQUIRE(state.requestDemotion(kBaseLevel).kind == Kind::Request);
    state.beginUpload(chain(kBaseLevel), kBaseLevel);

    state.releaseAll();

    CHECK(state.residentTopLevel() == cwStreamedItemState::kNoLevel);
    CHECK_FALSE(state.hasOpenRequest());
    CHECK_FALSE(state.isDemotionInFlight());
    CHECK_FALSE(state.hasPendingUpload());
    //A released item is back where a fresh one starts: its base comes first
    CHECK(state.needsPinnedBase());

    state.forgetDesired();
    CHECK(state.desiredTopLevel() == cwStreamedItemState::kNoLevel);
    CHECK(state.isBelowDesired());
}

TEST_CASE("A landing chain is matched to the open request by its level",
          "[StreamedItemState]") {
    cwStreamedItemState state;
    makeResident(state, kBaseLevel);
    REQUIRE(state.refineTo(kFineLevel, StreamPriority::Live).kind == Kind::Request);

    CHECK(state.wantsLevel(kFineLevel));
    //The level the item used to hold, and a level nothing asked for
    CHECK_FALSE(state.wantsLevel(kBaseLevel));
    CHECK_FALSE(state.wantsLevel(kMidLevel));

    state.abandonRequest();
    CHECK_FALSE(state.wantsLevel(kFineLevel));
}
