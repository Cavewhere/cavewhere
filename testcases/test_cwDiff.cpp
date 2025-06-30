//Catch includes
#include <catch2/catch_test_macros.hpp>

//Std includes
#include <iostream>
#include <random>

//Our includes
#include "cwDiff.h"

//Qt includes
#include <QUuid>

using namespace cwDiff;

void checkEditScript(const std::vector<Edit<int>>& expected, const std::vector<Edit<int>>& acual) {
    CHECK(acual.size() == expected.size());
    for (size_t i = 0; i < expected.size() && i < acual.size(); ++i) {
        INFO("i:" << i);
        CHECK(acual[i].operation == expected[i].operation);
        CHECK(acual[i].positionOld == expected[i].positionOld);
        CHECK(acual[i].positionNew == expected[i].positionNew);
    }
}

template<typename T, typename Comparator = std::equal_to<T>>
void checkApplyScript(const std::vector<T>& A,
                      const std::vector<T>& B,
                      const std::vector<Edit<T>>& diffScript,
                      Comparator equals = Comparator{}
                      ) {

    auto result = applyEditScript(diffScript,
                                  A,
                                  B);

    CHECK(result.size() == B.size());
    for(size_t i = 0; i < result.size() && i < B.size(); i++) {
        INFO("i:" << i);
        CHECK(equals(B.at(i), result.at(i)) == true);
    }
}

template<typename T>
void printEditSequence(
    const std::vector<T>& s1,
    const std::vector<T>& s2,
    const std::vector<Edit<T>>& editScript)
{
    for (const auto& edit : editScript) {
        if (edit.operation == EditOperation::Delete) {
            std::cout
                << "Delete " << s1[edit.positionOld]
                << " from s1 at position " << edit.positionOld
                << " in s1."
                << std::endl;
        }
        else {
            std::cout
                << "Insert " << s2[edit.positionNew]
                << " from s2 before position " << edit.positionOld
                << " into s1."
                << std::endl;
        }
    }
}

TEST_CASE("Myers diff single element", "[cwDiff]") {
    SECTION("insert") {
        std::vector<int> A{}, B{1};
        auto script = diff<int>(A, B);
        std::vector<Edit<int>> expected{
            Edit<int>({EditOperation::Insert, 0, 0}),
        };
        checkEditScript(expected, script);
        checkApplyScript(A, B, script);
    }

    SECTION("delete") {
        std::vector<int> A{1}, B{};
        auto script = diff<int>(A, B);
        std::vector<Edit<int>> expected{
            Edit<int>({EditOperation::Delete, 0, 0}),
        };
        checkEditScript(expected, script);
        checkApplyScript(A, B, script);
    }
    SECTION("same") {
        std::vector<int> A{1}, B{1};
        auto script = diff<int>(A, B);
        std::vector<Edit<int>> expected{
        };
        checkEditScript(expected, script);
        checkApplyScript(A, B, script);
    }
    SECTION("delete and insert") {
        std::vector<int> A{1}, B{2};
        auto script = diff<int>(A, B);
        std::vector<Edit<int>> expected{
            Edit<int>{EditOperation::Delete, 0, 0},
            Edit<int>{EditOperation::Insert, 1, 0},
        };
        checkEditScript(expected, script);
        checkApplyScript(A, B, script);
    }
}

TEST_CASE("Complex cases", "[cwDiff]") {
    SECTION("xaxcxabc") {
        std::vector<char> A{'x','a','x','c','x','a','b','c'};
        std::vector<char> B{'a','b','c','y'};
        auto script = diff<char>(A, B);
        // printEditSequence<char>(A, B, script);
        checkApplyScript(A, B, script);
    }

    SECTION("abgdef") {
        std::vector<char> A{'a','b','g','d','e','f'};
        std::vector<char> B{'g','h'};
        auto script = diff<char>(A, B);
        checkApplyScript(A, B, script);
    }

    SECTION("wacr") {
        std::vector<char> A{'a'};
        std::vector<char> B{'w','a','c','r'};
        auto script = diff<char>(A, B);
        checkApplyScript(A, B, script);
    }

}

TEST_CASE("Randomized diff<char> tests", "[cwDiff]") {
    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<> lenDist(0, 100); // varying lengths
    std::uniform_int_distribution<> charDist(0, 25); // characters a-z

    for (int i = 0; i < 5000; ++i) {
        int lenA = lenDist(rng);
        int lenB = lenDist(rng);

        std::vector<char> A(lenA);
        std::vector<char> B(lenB);

        for (char& c : A) {
            c = static_cast<char>('a' + charDist(rng));
        }

        for (char& c : B) {
            c = static_cast<char>('a' + charDist(rng));
        }

        auto script = diff<char>(A, B);
        INFO("I:" << i);
        checkApplyScript(A, B, script);
    }
}

struct Obj {
    QUuid id;
    QString key;
    int value;
};

TEST_CASE("Randomized diff<Obj> tests with modifications", "[cwDiff]") {
    std::mt19937                    rng(42);           // fixed seed
    std::uniform_int_distribution<> lenDist(0, 10);    // A length and number of adds/removes
    std::uniform_int_distribution<> valDist(0, 1000);   // value range
    std::uniform_int_distribution<> keyLenDist(1, 10);  // key length
    std::uniform_int_distribution<> charDist(0, 25);    // a–z

    // build an initial vector<Obj> of given length
    auto buildList = [&](int len) {
        std::vector<Obj> list;
        list.reserve(len);
        for (int i = 0; i < len; ++i) {
            Obj obj;
            obj.id    = QUuid::createUuid();

            int klen = keyLenDist(rng);
            QString k;
            k.reserve(klen);
            for (int j = 0; j < klen; ++j) {
                k.append(QChar('a' + charDist(rng)));
            }
            obj.key   = k;
            obj.value = valDist(rng);

            list.push_back(obj);
        }
        return list;
    };

    // take an existing list and randomly remove and add elements to create B
    auto modifyList = [&](const std::vector<Obj>& original) {
        std::vector<Obj> mod = original;
        int               remCount = lenDist(rng) % (original.size() + 1);
        int               addCount = lenDist(rng);

        // removals
        for (int r = 0; r < remCount && !mod.empty(); ++r) {
            std::uniform_int_distribution<> idxDist(0, static_cast<int>(mod.size()) - 1);
            int idx = idxDist(rng);
            mod.erase(mod.begin() + idx);
        }

        // additions at random positions
        for (int a = 0; a < addCount; ++a) {
            Obj obj;
            obj.id    = QUuid::createUuid();

            int klen = keyLenDist(rng);
            QString k;
            k.reserve(klen);
            for (int j = 0; j < klen; ++j) {
                k.append(QChar('a' + charDist(rng)));
            }
            obj.key   = k;
            obj.value = valDist(rng);

            std::uniform_int_distribution<> posDist(0, static_cast<int>(mod.size()));
            int pos = posDist(rng);
            mod.insert(mod.begin() + pos, obj);
        }

        return mod;
    };

    for (int i = 0; i < 50; ++i) {
        auto A = buildList(lenDist(rng));
        auto B = modifyList(A);

        // comparator that only compares UUIDs
        auto equals = [](const Obj& a, const Obj& b) {
            return a.id == b.id;
        };

        auto script = diff<Obj>(A, B, equals);

        INFO("Iteration: " << i);
        checkApplyScript(A, B, script, equals);
    }
}



