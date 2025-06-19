//Catch includes
#include <catch2/catch_test_macros.hpp>

//Std includes
#include <iostream>
#include <random>

//Our includes
#include "cwDiff.h"

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

template<typename T>
void checkApplyScript(const std::vector<T>& A,
                      const std::vector<T>& B,
                      const std::vector<Edit<T>>& diffScript) {

    auto result = applyEditScript(diffScript,
                                  A,
                                  B);

    CHECK(result.size() == B.size());
    for(size_t i = 0; i < result.size() && i < B.size(); i++) {
        INFO("i:" << i);
        CHECK(B.at(i) == result.at(i));
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
        qDebug() << "Script length:" << script.size();
        printEditSequence<char>(A, B, script);
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





// void checkEditScript(const std::vector<cwDiff<int>::Edit>& expected, const std::vector<cwDiff<int>::Edit>& accual) {
//     CHECK(accual.size() == expected.size());
//     for (size_t i = 0; i < expected.size() && i < accual.size(); ++i) {
//         INFO("i:" << i);
//         CHECK(accual[i].m_type == expected[i].m_type);
//         CHECK(accual[i].m_item == expected[i].m_item);
//     }
// }

// // --- Catch2 Test Cases for cwDiff<int> ---
// TEST_CASE("Myers diff identity sequence", "[cwDiff]") {
//     std::vector<int> original{1,2,3,4};
//     auto script = cwDiff<int>::diff(original, original);
//     REQUIRE(script.empty());
// }

// TEST_CASE("Myers diff single element", "[cwDiff]") {
//     // SECTION("insert") {
//     //     std::vector<int> A{}, B{1};
//     //     auto script = cwDiff<int>::diff(A, B);
//     //     std::vector<cwDiff<int>::Edit> expected{
//     //                                             {cwDiff<int>::EditType::Insert, 1},
//     //                                             };
//     //     checkEditScript(expected, script);
//     // }

//     // SECTION("delete") {
//     //     std::vector<int> A{1}, B{};
//     //     auto script = cwDiff<int>::diff(A, B);
//     //     std::vector<cwDiff<int>::Edit> expected{
//     //                                             {cwDiff<int>::EditType::Delete, 1},
//     //                                             };
//     //     checkEditScript(expected, script);
//     // }

//     // SECTION("same") {
//     //     std::vector<int> A{1}, B{1};
//     //     auto script = cwDiff<int>::diff(A, B);
//     //     std::vector<cwDiff<int>::Edit> expected{
//     //                                             {cwDiff<int>::EditType::Keep, 1},
//     //                                             };
//     //     checkEditScript(expected, script);
//     // }

//     SECTION("delete and insert") {
//         std::vector<int> A{1}, B{2};
//         auto script = cwDiff<int>::diff(A, B);
//         std::vector<cwDiff<int>::Edit> expected{
//                                                 {cwDiff<int>::EditType::Delete, 1},
//                                                 {cwDiff<int>::EditType::Insert, 2},
//                                                 };
//         checkEditScript(expected, script);
//     }
// }

// TEST_CASE("Myers diff single element delete", "[cwDiff]") {
// }

// TEST_CASE("Myers diff deletes element", "[cwDiff]") {
//     std::vector<int> A{1,2,3}, B{1,3};
//     auto script = cwDiff<int>::diff(A, B);
//     std::vector<cwDiff<int>::Edit> expected{
//         {cwDiff<int>::EditType::Keep,   1},
//         {cwDiff<int>::EditType::Delete, 2},
//         {cwDiff<int>::EditType::Keep,   3}
//     };
//     REQUIRE(script.size() == expected.size());
//     for(size_t i=0;i<script.size();++i) {
//         REQUIRE(script[i].m_type == expected[i].m_type);
//         REQUIRE(script[i].m_item == expected[i].m_item);
//     }
// }

// TEST_CASE("Myers diff inserts elements", "[cwDiff]") {
//     std::vector<int> A{4,1};
//     std::vector<int> B{4,5,6,1};
//     auto script = cwDiff<int>::diff(A, B);

//     std::vector<cwDiff<int>::Edit> expected = {
//         {cwDiff<int>::EditType::Keep, 4},
//         {cwDiff<int>::EditType::Insert, 5},
//         {cwDiff<int>::EditType::Insert, 6},
//         {cwDiff<int>::EditType::Keep,   1},
//     };
//     CHECK(script.size() == expected.size());
//     for (size_t i = 0; i < expected.size() && i < script.size(); ++i) {
//         INFO("i:" << i);
//         CHECK(script[i].m_type == expected[i].m_type);
//         CHECK(script[i].m_item == expected[i].m_item);
//     }
// }

// TEST_CASE("Myers diff complex insert+delete", "[cwDiff]") {
//     std::vector<int> A{1,2,3,4,5};
//     std::vector<int> B{2,4,6,5};
//     // A->B: delete 1, keep 2, delete 3, keep 4, insert 6, keep 5
//     auto script = cwDiff<int>::diff(A, B);
//     std::vector<cwDiff<int>::Edit> expected = {
//         {cwDiff<int>::EditType::Delete, 1},
//         {cwDiff<int>::EditType::Keep,   2},
//         {cwDiff<int>::EditType::Delete, 3},
//         {cwDiff<int>::EditType::Keep,   4},
//         {cwDiff<int>::EditType::Insert, 6},
//         {cwDiff<int>::EditType::Keep,   5}
//     };
//     CHECK(script.size() == expected.size());
//     for (size_t i = 0; i < expected.size() && i < script.size(); ++i) {
//         INFO("i:" << i);
//         CHECK(script[i].m_type == expected[i].m_type);
//         CHECK(script[i].m_item == expected[i].m_item);
//     }
// }

// // TEST_CASE("Three-way merge inserts from both sides", "[cwDiff]") {
// //     std::vector<int> base{1,2};
// //     std::vector<int> local{1,2,3};
// //     std::vector<int> remote{1,2,4};
// //     auto localScript  = cwDiff<int>::diff(base, local);
// //     auto remoteScript = cwDiff<int>::diff(base, remote);
// //     auto result = cwDiff<int>::merge(localScript, remoteScript);
// //     REQUIRE(result.m_mergedOrder == std::vector<int>{1,2,3,4});
// //     REQUIRE(result.m_conflicts   == std::vector<int>{3,4});
// // }
