//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "MarkScope/Profiler.h"

//Qt includes
#include <QStringLiteral>

//Std includes
#include <vector>

namespace {
const QString kDomainName = QStringLiteral("com.cavewhere.tests.markscope");
constexpr int kTokenCount = 16;
}

TEST_CASE("Consecutive start calls return distinct tokens", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    const MarkScope::Profiler::Token first = profiler.start(QStringLiteral("first"));
    profiler.stop(first);
    const MarkScope::Profiler::Token second = profiler.start(QStringLiteral("second"));
    profiler.stop(second);

    CHECK(first != second);
    CHECK_FALSE(first == second);
}

TEST_CASE("Nested start calls return distinct tokens", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    std::vector<MarkScope::Profiler::Token> tokens;
    tokens.reserve(kTokenCount);
    for (int i = 0; i < kTokenCount; ++i) {
        tokens.push_back(profiler.start(QStringLiteral("nested %1").arg(i)));
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
        for (size_t j = i + 1; j < tokens.size(); ++j) {
            CHECK(tokens.at(i) != tokens.at(j));
        }
    }

    for (auto it = tokens.crbegin(); it != tokens.crend(); ++it) {
        profiler.stop(*it);
    }
}

TEST_CASE("Tokens compare equal to their copies", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    const MarkScope::Profiler::Token token = profiler.start(QStringLiteral("copy"));
    const MarkScope::Profiler::Token copy = token;
    CHECK(token == copy);
    profiler.stop(copy);
}

TEST_CASE("Intervals of both kinds return distinct tokens", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    const auto task = profiler.start(MarkScope::Profiler::Kind::Task, QStringLiteral("ctx=lidar"));
    const auto batch = profiler.start(MarkScope::Profiler::Kind::Batch, QStringLiteral("ctx=octree items=10"));
    CHECK(task != batch);

    profiler.stop(task);
    profiler.stop(batch);
}

TEST_CASE("Stop accepts tokens in LIFO order", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    const auto outer = profiler.start(QStringLiteral("outer"));
    const auto inner = profiler.start(QStringLiteral("inner"));
    profiler.stop(inner);
    profiler.stop(outer);

    const auto after = profiler.start(QStringLiteral("after"));
    CHECK(after != outer);
    CHECK(after != inner);
    profiler.stop(after);
}

TEST_CASE("Stop accepts tokens in non-LIFO order", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    const auto first = profiler.start(QStringLiteral("overlap a"));
    const auto second = profiler.start(QStringLiteral("overlap b"));
    profiler.stop(first);
    const auto third = profiler.start(QStringLiteral("overlap c"));
    profiler.stop(second);
    profiler.stop(third);

    CHECK(first != second);
    CHECK(second != third);
    CHECK(first != third);
}

TEST_CASE("ScopedProfiler opens and closes an interval", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    {
        MarkScope::ScopedProfiler outer(profiler, QStringLiteral("scoped outer"));
        {
            MarkScope::ScopedProfiler inner(profiler, MarkScope::Profiler::Kind::Batch,
                                            QStringLiteral("scoped inner"));
        }
    }

    const auto before = profiler.start(QStringLiteral("before scope"));
    {
        MarkScope::ScopedProfiler scoped(profiler, QStringLiteral("scoped"));
    }
    const auto after = profiler.start(QStringLiteral("after scope"));
    CHECK(before != after);
    profiler.stop(after);
    profiler.stop(before);
}

TEST_CASE("Profiler::event is callable", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    profiler.event(QStringLiteral("kind=dropped ctx=scraps gen=3"));

    const auto token = profiler.start(QStringLiteral("around event"));
    profiler.event(QStringLiteral("kind=mem footprint=1024"));
    profiler.stop(token);
    SUCCEED();
}

TEST_CASE("Profiler::enabled gates message building", "[MarkScope]")
{
    MarkScope::Profiler profiler(kDomainName);

    const QString message = profiler.enabled()
                                ? QStringLiteral("ctx=%1 wait_us=%2").arg(QStringLiteral("lidar")).arg(42)
                                : QString();
    const auto token = profiler.start(message);
    profiler.stop(token);
    SUCCEED();
}
