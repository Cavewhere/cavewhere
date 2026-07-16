//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTracked.h"

//Qt includes
#include <QVector>

//Std includes
#include <utility>

namespace {
    // Records whether it was move-assigned from, so a test can tell the rvalue
    // setValue overload apart from the copying const-ref overload.
    struct CopyMoveProbe {
        int value = 0;
        bool movedFrom = false;

        CopyMoveProbe() = default;
        explicit CopyMoveProbe(int v) : value(v) {}

        CopyMoveProbe(const CopyMoveProbe& other) = default;
        CopyMoveProbe(CopyMoveProbe&& other) noexcept
            : value(other.value) { other.movedFrom = true; }

        CopyMoveProbe& operator=(const CopyMoveProbe& other) {
            value = other.value;
            return *this;
        }
        CopyMoveProbe& operator=(CopyMoveProbe&& other) noexcept {
            value = other.value;
            other.movedFrom = true;
            return *this;
        }

        bool operator!=(const CopyMoveProbe& other) const { return value != other.value; }
    };
}

TEST_CASE("cwTracked starts unchanged", "[cwTracked]") {
    cwTracked<int> tracked(7);
    CHECK(tracked.value() == 7);
    CHECK_FALSE(tracked.isChanged());
}

TEST_CASE("cwTracked setValue marks changed only on a real change", "[cwTracked]") {
    cwTracked<int> tracked(7);

    SECTION("a new value marks it changed") {
        tracked.setValue(42);
        CHECK(tracked.value() == 42);
        CHECK(tracked.isChanged());
    }

    SECTION("an equal value leaves it unchanged") {
        tracked.setValue(7);
        CHECK(tracked.value() == 7);
        CHECK_FALSE(tracked.isChanged());
    }

    SECTION("resetChanged clears the dirty flag") {
        tracked.setValue(42);
        REQUIRE(tracked.isChanged());
        tracked.resetChanged();
        CHECK_FALSE(tracked.isChanged());
    }
}

TEST_CASE("cwTracked lvalue setValue copies the value", "[cwTracked]") {
    cwTracked<CopyMoveProbe> tracked{CopyMoveProbe(0)};

    CopyMoveProbe source(42);
    tracked.setValue(source);

    CHECK(tracked.value().value == 42);
    CHECK_FALSE(source.movedFrom);  // const-ref overload must not move
}

TEST_CASE("cwTracked rvalue setValue move-assigns the value", "[cwTracked]") {
    cwTracked<CopyMoveProbe> tracked{CopyMoveProbe(0)};

    CopyMoveProbe source(42);
    tracked.setValue(std::move(source));

    CHECK(tracked.value().value == 42);
    CHECK(source.movedFrom);  // rvalue overload steals the value
}

TEST_CASE("cwTracked rvalue setValue skips the move when the value is unchanged", "[cwTracked]") {
    cwTracked<CopyMoveProbe> tracked{CopyMoveProbe(7)};
    REQUIRE_FALSE(tracked.isChanged());

    CopyMoveProbe same(7);
    tracked.setValue(std::move(same));

    // The change-detection compare runs before the assignment, so an equal
    // value must neither move the source nor flip the dirty flag.
    CHECK_FALSE(same.movedFrom);
    CHECK_FALSE(tracked.isChanged());
}

TEST_CASE("cwTracked rvalue setValue empties a moved-from QVector", "[cwTracked]") {
    cwTracked<QVector<int>> tracked;

    QVector<int> source{1, 2, 3};
    tracked.setValue(std::move(source));

    CHECK(tracked.value() == QVector<int>{1, 2, 3});
    CHECK(source.isEmpty());  // buffer was stolen, not copied
}
