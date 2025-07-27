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

    bool operator==(const Obj& other) const {
        return id == other.id
               && key == other.key
               && value == other.value;
    }
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
            obj.id = QUuid::createUuid();

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

struct LineObj {
    QUuid id;
    int size;
    Obj obj;
    QList<Obj> list;

    bool operator==(const LineObj& other) {
        return id == other.id
               && size == other.size
               && obj == other.obj
               && list == other.list;
    }
};

// namespace cwDiff {
Obj merge(const Obj& base,
          const Obj& ours,
          const Obj& theirs,
          cwDiff::MergeStrategy strategy)
{
    Q_ASSERT(base.id == ours.id);
    Q_ASSERT(base.id == theirs.id);

    return Obj {
        base.id,
        cwDiff::merge(base.key, ours.key, theirs.key, strategy),
        cwDiff::merge(base.value, ours.value, theirs.value, strategy)
    };
}

template<typename Comparator = std::equal_to<Obj>>
QList<Obj> mergeList(const QList<Obj>& base,
                 const QList<Obj>& ours,
                 const QList<Obj>& theirs,
                 cwDiff::MergeStrategy strategy,
                 Comparator objEquals)
{
    auto idEquals = [](const Obj& a, const Obj& b) {
        return a.id == b.id;
    };

    auto oursEdits = cwDiff::diff<Obj>(base, ours, idEquals);
    auto theirsEdits = cwDiff::diff<Obj>(base, theirs, idEquals);

    // Resulting merged list
    QList<Obj> result;
    result.reserve(std::max(base.size(), std::max(ours.size(), theirs.size())));

    // Index into base, ours, and theirs
    int baseIndex = 0;
    int oursIndex = 0;
    int theirsIndex = 0;

    // Iterators for edit scripts
    std::size_t oursEditIndex = 0;
    std::size_t theirsEditIndex = 0;

    while (baseIndex < base.size()
           || oursEditIndex < oursEdits.size()
           || theirsEditIndex < theirsEdits.size())
    {
        bool oursDeleted = (oursEditIndex < oursEdits.size() &&
                            oursEdits[oursEditIndex].operation == cwDiff::EditOperation::Delete &&
                            oursEdits[oursEditIndex].positionOld == baseIndex);

        bool theirsDeleted = (theirsEditIndex < theirsEdits.size() &&
                              theirsEdits[theirsEditIndex].operation == cwDiff::EditOperation::Delete &&
                              theirsEdits[theirsEditIndex].positionOld == baseIndex);

        bool oursInserted = (oursEditIndex < oursEdits.size() &&
                             oursEdits[oursEditIndex].operation == cwDiff::EditOperation::Insert &&
                             oursEdits[oursEditIndex].positionOld == baseIndex);

        bool theirsInserted = (theirsEditIndex < theirsEdits.size() &&
                               theirsEdits[theirsEditIndex].operation == cwDiff::EditOperation::Insert &&
                               theirsEdits[theirsEditIndex].positionOld == baseIndex);

        // Handle insertions first
        if (oursInserted && theirsInserted) {
            // Both inserted at the same spot – resolve conflict

            const Obj& oursObj = ours.at(oursEdits.at(oursEditIndex).positionNew);
            const Obj& theirsObj = theirs.at(theirsEdits.at(theirsEditIndex).positionNew);

            //Unlikely case that they are the same
            if(idEquals(oursObj, theirsObj)) {
                if(objEquals(oursObj, theirsObj)) {
                    //Same same
                    result.append(oursObj);
                } else if (strategy == cwDiff::MergeStrategy::UseOursOnConflict) {
                    result.append(oursObj);
                } else if (strategy == cwDiff::MergeStrategy::UseTheirsOnConflict) {
                    result.append(theirsObj);
                }
            } else {
                // Concat objects
                result.append(oursObj);
                result.append(theirsObj);
            }

            ++oursEditIndex;
            ++theirsEditIndex;
            continue;
        }

        if (oursInserted) {
            result.append(ours[oursEdits[oursEditIndex].positionNew]);
            ++oursEditIndex;
            continue;
        }

        if (theirsInserted) {
            result.append(theirs[theirsEdits[theirsEditIndex].positionNew]);
            ++theirsEditIndex;
            continue;
        }

        // Handle deletes or unchanged
        if (oursDeleted && theirsDeleted) {
            // Both deleted: respect deletion
            ++baseIndex;
            ++oursEditIndex;
            ++theirsEditIndex;
            continue;
        }

        if (oursDeleted && !theirsDeleted) {
            //Is theirs different that the base?
            if(!objEquals(base.at(baseIndex), theirs.at(theirsIndex))) {
                if (strategy == cwDiff::MergeStrategy::UseOursOnConflict) {
                    ++baseIndex;
                    ++oursEditIndex;
                    continue;
                } else {
                    result.append(theirs[theirsIndex]);
                }
            } else {
                //Theirs is equal to the base, we can delete it
                ++baseIndex;
                ++oursEditIndex;
                continue;
            }
        }

        if (!oursDeleted && theirsDeleted) {
            if(!objEquals(base.at(baseIndex), ours.at(theirsIndex))) {
                if (strategy == cwDiff::MergeStrategy::UseTheirsOnConflict) {
                    ++baseIndex;
                    ++theirsEditIndex;
                    continue;
                } else {
                    result.append(ours[oursIndex]);
                }
            } else {
                //Ours is equal to the base, we can delete it
                ++baseIndex;
                ++theirsEditIndex;
                continue;
            }
        }

        if (!oursDeleted && !theirsDeleted && baseIndex < base.size()) {
            // No changes – keep from either side (prefer ours)
            // Do object level merge
            result.append(merge(base.at(baseIndex), ours.at(oursIndex), theirs.at(theirsIndex), strategy));
        }

        // Advance all indexes
        ++baseIndex;
        ++oursIndex;
        ++theirsIndex;

        if (oursDeleted) ++oursEditIndex;
        if (theirsDeleted) ++theirsEditIndex;
    }

    return result;
}

LineObj merge(const LineObj& base,
              const LineObj& ours,
              const LineObj& thiers,
              cwDiff::MergeStrategy strategy)
{
    Q_ASSERT(base.id == ours.id);
    Q_ASSERT(base.id == thiers.id);

    auto objEquals = [](const Obj& obj1, const Obj& obj2) {
        return obj1.key == obj2.key
               && obj1.value == obj2.value;
    };

    return LineObj {
        base.id,
        cwDiff::merge(base.size, ours.size, thiers.size, strategy),
        cwDiff::merge(base.obj, ours.obj, thiers.obj, strategy, objEquals),
        mergeList(base.list, ours.list, thiers.list, strategy, objEquals)
    };
}

static const QUuid ID_BASE("{00000000-0000-0000-0000-000000000000}");
static const QUuid ID_OTHER("{11111111-1111-1111-1111-111111111111}");
static const QUuid ID_CONFLICT("{22222222-2222-2222-2222-222222222222}");

static Obj makeObj(const QUuid& id, const QString& key, int value) {
    return Obj{ id, key, value };
}

static LineObj makeLineObj(const QUuid& id,
                           int size,
                           const Obj& obj,
                           const QList<Obj>& list)
{
    return LineObj{ id, size, obj, list };
}

TEST_CASE("3-way merge of LineObj — no changes", "[merge]") {
    auto base  = makeLineObj(ID_BASE, 1, makeObj(ID_OTHER, "k", 10), { makeObj(ID_OTHER, "k", 10) });
    auto ours  = base;
    auto theirs= base;

    auto result = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
    CHECK(result == base);
    CHECK(result.size == base.size);
    CHECK(result.obj.value == base.obj.value);
    CHECK(result.list == base.list);
}

TEST_CASE("Single-side non-conflicting changes", "[merge]") {
    auto base = makeLineObj(ID_BASE, 1,
                            makeObj(ID_OTHER, "k", 10),
                            {
                                makeObj(QUuid::createUuid(), "1", 9),
                                makeObj(QUuid::createUuid(), "2", 11),
                            });

    auto ours = base;
    auto theirs = base;

    SECTION("ours only change size") {
        ours.size = 5;
        auto merged = merge(base, ours, base, MergeStrategy::UseTheirsOnConflict);
        CHECK(merged.size == 5);

        // Everything else unchanged from base
        CHECK(merged.id        == base.id);
        CHECK(merged.obj.id    == base.obj.id);
        CHECK(merged.obj.key   == base.obj.key);
        CHECK(merged.obj.value == base.obj.value);

        // List unchanged
        CHECK(merged.list.size() == base.list.size());
        CHECK(merged.list      == base.list);
    }

    SECTION("theirs only change size") {
        theirs.size = 6;
        auto merged = merge(base, ours, theirs, MergeStrategy::UseTheirsOnConflict);
        CHECK(merged.size == 6);

        // Everything else unchanged from base
        CHECK(merged.id        == base.id);
        CHECK(merged.obj.id    == base.obj.id);
        CHECK(merged.obj.key   == base.obj.key);
        CHECK(merged.obj.value == base.obj.value);

        // List unchanged
        CHECK(merged.list.size() == base.list.size());
        CHECK(merged.list      == base.list);
    }

    SECTION("our only change nested obj.value") {
        ours.obj.value = 20;
        auto merged = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
        CHECK(merged.obj.value == 20);

        // Everything else unchanged from base
        CHECK(merged.id        == base.id);
        CHECK(merged.size      == base.size);
        CHECK(merged.obj.id    == base.obj.id);
        CHECK(merged.obj.key   == base.obj.key);
        // CHECK(merged.obj.value == base.obj.value);

        // List unchanged
        CHECK(merged.list.size() == base.list.size());
        CHECK(merged.list      == base.list);
    }

    SECTION("theirs only change nested obj.value") {
        theirs.obj.value = 20;
        auto merged = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
        CHECK(merged.obj.value == 20);

        // Everything else unchanged from base
        CHECK(merged.id        == base.id);
        CHECK(merged.size      == base.size);
        CHECK(merged.obj.id    == base.obj.id);
        CHECK(merged.obj.key   == base.obj.key);
        // CHECK(merged.obj.value == base.obj.value);

        // List unchanged
        CHECK(merged.list.size() == base.list.size());
        CHECK(merged.list      == base.list);
    }

    SECTION("our only change nested obj.key") {
        ours.obj.key = "sauce";
        auto merged = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
        CHECK(merged.obj.key == "sauce");

        // Everything else unchanged from base
        CHECK(merged.id        == base.id);
        CHECK(merged.size      == base.size);
        CHECK(merged.obj.id    == base.obj.id);
        // CHECK(merged.obj.key   == base.obj.key);
        CHECK(merged.obj.value == base.obj.value);

        // List unchanged
        CHECK(merged.list.size() == base.list.size());
        CHECK(merged.list      == base.list);
    }

    SECTION("theirs only change nested obj.key") {
        theirs.obj.key = "sauce";
        auto merged = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
        CHECK(merged.obj.key == "sauce");

        // Everything else unchanged from base
        CHECK(merged.id        == base.id);
        CHECK(merged.size      == base.size);
        CHECK(merged.obj.id    == base.obj.id);
        // CHECK(merged.obj.key   == base.obj.key);
        CHECK(merged.obj.value == base.obj.value);

        // List unchanged
        CHECK(merged.list.size() == base.list.size());
        CHECK(merged.list      == base.list);
    }
}

TEST_CASE("Independent non-conflicting changes are merged together", "[merge]") {
    // Base with a couple list elements so we can also test list‐related merges
    auto base = makeLineObj(
        ID_BASE,
        1,
        makeObj(ID_OTHER, "k", 10),
        {
            makeObj(QUuid::createUuid(), "1", 9),
            makeObj(QUuid::createUuid(), "2", 11),
        }
        );

    //
    // Scenario A: ours changes size, theirs changes obj.key
    //
    {
        auto ours   = base;
        ours.size     = 7;

        auto theirs = base;
        theirs.obj.key = "changed";

        auto checkAll = [&](const LineObj& merged) {
            // the two intended diffs:
            CHECK(merged.size      == 7);
            CHECK(merged.obj.key   == "changed");
            // everything else still base
            CHECK(merged.id        == base.id);
            CHECK(merged.obj.id    == base.obj.id);
            CHECK(merged.obj.value == base.obj.value);
            CHECK(merged.list.size() == base.list.size());
            CHECK(merged.list      == base.list);
        };

        SECTION("Strategy::UseOursOnConflict") {
            auto m = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
            checkAll(m);
        }
        SECTION("Strategy::UseTheirsOnConflict") {
            auto m = merge(base, ours, theirs, MergeStrategy::UseTheirsOnConflict);
            checkAll(m);
        }
        SECTION("Strategy::UseBaseOnConflict") {
            auto m = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
            checkAll(m);
        }
    }

    //
    // Scenario B: ours changes nested obj.value, theirs appends one item to list
    //
    {
        auto newItem = makeObj(QUuid::createUuid(), "X", 99);

        auto ours   = base;
        ours.obj.value = 42;
        auto theirs = base;
        theirs.list.append(newItem);

        auto checkAll = [&](const LineObj& merged) {
            // the two intended diffs:
            CHECK(merged.obj.value        == 42);
            REQUIRE(merged.list.size()    == base.list.size() + 1);
            CHECK(merged.list.back().id   == newItem.id);
            CHECK(merged.list.back().key  == newItem.key);
            CHECK(merged.list.back().value== newItem.value);

            // everything else still base
            CHECK(merged.id        == base.id);
            CHECK(merged.size      == base.size);
            CHECK(merged.obj.id    == base.obj.id);
            CHECK(merged.obj.key   == base.obj.key);
            // the other list elements still intact & in order
            for (int i = 0; i < base.list.size(); ++i)
                CHECK(merged.list[i] == base.list[i]);
        };

        SECTION("Strategy::UseOursOnConflict") {
            auto m = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
            checkAll(m);
        }
        SECTION("Strategy::UseTheirsOnConflict") {
            auto m = merge(base, ours, theirs, MergeStrategy::UseTheirsOnConflict);
            checkAll(m);
        }
        SECTION("Strategy::UseBaseOnConflict") {
            auto m = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
            checkAll(m);
        }
    }
}


TEST_CASE("Independent non-conflicting changes are merged together - simple", "[merge]") {
    auto base = makeLineObj(ID_BASE, 1, makeObj(ID_OTHER, "k", 10), {});
    auto ours = base;   ours.size = 7;
    auto theirs = base; theirs.obj.key = "changed";

    auto merged = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
    CHECK(merged.size == 7);
    CHECK(merged.obj.key == "changed");
}

TEST_CASE("Identical changes on both sides", "[merge]") {
    auto base = makeLineObj(ID_BASE, 1, makeObj(ID_OTHER, "k", 10), {});
    auto ours = base; ours.size = 42;
    auto theirs = base; theirs.size = 42;

    auto merged = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
    CHECK(merged.size == 42);
}

TEST_CASE("Scalar conflict resolution – size", "[merge]") {
    auto base   = makeLineObj(ID_BASE, 1, makeObj(ID_OTHER, "k", 10), {});
    auto ours   = base; ours.size = 10;
    auto theirs = base; theirs.size = 20;

    SECTION("UseOursOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
        CHECK(m.size == 10);
    }
    SECTION("UseTheirsOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseTheirsOnConflict);
        CHECK(m.size == 20);
    }
    SECTION("UseBaseOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
        CHECK(m.size == base.size);
    }
}

TEST_CASE("Nested conflict resolution – obj.value", "[merge]") {
    auto base   = makeLineObj(ID_BASE, 1, makeObj(ID_OTHER, "k", 10), {});
    auto ours   = base; ours.obj.value = 5;
    auto theirs = base; theirs.obj.value = 7;

    SECTION("UseOursOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
        CHECK(m.obj.value == 5);
    }
    SECTION("UseTheirsOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseTheirsOnConflict);
        CHECK(m.obj.value == 7);
    }
    SECTION("UseBaseOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
        CHECK(m.obj.value == 10);
    }
}

TEST_CASE("List additions (non-conflicting)", "[merge]") {
    Obj A{ ID_OTHER, "A", 1 }, B{ ID_CONFLICT, "B", 2 }, C{ ID_BASE, "C", 3 };
    auto base   = makeLineObj(ID_BASE, 1, makeObj(ID_OTHER, "k", 10), { A });
    auto ours   = base; ours.list.append(B);
    auto theirs = base; theirs.list.append(C);

    auto merged = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
    CHECK(merged.list.contains(A));
    CHECK(merged.list.contains(B));
    CHECK(merged.list.contains(C));
}

TEST_CASE("List duplicate addition is idempotent", "[merge]") {
    Obj D{ ID_CONFLICT, "D", 4 };
    auto base   = makeLineObj(ID_BASE, 1, makeObj(ID_OTHER, "k", 10), {});
    auto ours   = base; ours.list.append(D);
    auto theirs = base; theirs.list.append(D);

    auto merged = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
    CHECK(std::count_if(merged.list.begin(), merged.list.end(),
                          [&](auto const& o){ return o.id == D.id; }) == 1);
}

TEST_CASE("List element modification conflict", "[merge]") {
    Obj X{ ID_OTHER, "X", 1 };
    auto base = makeLineObj(ID_BASE, 1, X, { X });
    auto ours   = base; ours.list[0].value = 2;
    auto theirs = base; theirs.list[0].value = 3;

    SECTION("UseOursOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseOursOnConflict);
        CHECK(m.list[0].value == 2);
    }
    SECTION("UseTheirsOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseTheirsOnConflict);
        CHECK(m.list[0].value == 3);
    }
    SECTION("UseBaseOnConflict") {
        auto m = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
        CHECK(m.list[0].value == 1);
    }
}

TEST_CASE("Mixed nested & list changes", "[merge]") {
    auto base   = makeLineObj(ID_BASE, 1, makeObj(ID_OTHER, "k", 10), {});
    auto ours   = base;   ours.size = 8;
    auto theirs = base;   theirs.list.append(makeObj(ID_CONFLICT, "Z", 99));

    auto merged = merge(base, ours, theirs, MergeStrategy::UseBaseOnConflict);
    CHECK(merged.size == 8);
    CHECK(merged.list.size() == 1);
    CHECK(merged.list[0].value == 99);
}

//3 way diff
//
//struct Obj {
// QUuid id;
// QString key;
// int value;
//};
//Call merge for each object
//merge(Obj base, Obj ours, Obj yours, stategy)
//base == ours && base == yours
// - No changes
//base != ours && base == yours
// - take our changes
//base == ours && base != yours
// - take your changes
//base != our && base != yours
// - Both change, take statagy

//Vector merge
//merge(QVector<Obj> base, QVector<Obj> ours, QVector<Obj> yours, idEquals, statagy, objectEquals, objectMergeFunc, )
//Concate on insert
//delete, if other is unchanged, and edit is delete
//keep and keep do objectEquals, if unequal, do objectLevel merge with objectMergeFunc




