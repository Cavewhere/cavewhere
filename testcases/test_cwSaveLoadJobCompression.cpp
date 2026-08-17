#include <catch2/catch_test_macros.hpp>

#include <QObject>
#include <QString>

#include <memory>

#include "cwSaveLoad.h"
#include "cwSaveLoadPrivate.h"

namespace {

using Job = cwSaveLoadPrivate::Job;
using Kind = Job::Kind;
using Action = Job::Action;

// Distinct object identities for tests. A job tracks the lifetime of the
// object it names, so these are real QObjects rather than fabricated
// addresses; the compression rules themselves only ever compare the pointers.
// Function-local statics so they are built after the QApplication.
const QObject* objectA()
{
    static QObject object;
    return &object;
}

const QObject* objectB()
{
    static QObject object;
    return &object;
}

const QString kTagDefault = QString();
const QString kTagSource = QStringLiteral("source");
const QString kTagOther = QStringLiteral("other");

Job makeMove(const QObject* objectId,
             const QString& tag,
             const QString& oldPath,
             const QString& newPath,
             Kind kind = Kind::File)
{
    Job job(objectId, kind, Action::Move);
    job.tag = tag;
    job.oldPath = oldPath;
    job.path = newPath;
    return job;
}

Job makeWrite(const QObject* objectId, const QString& tag, const QString& path)
{
    Job job(objectId, Kind::File, Action::WriteFile);
    job.tag = tag;
    job.path = path;
    return job;
}

Job makeRemove(const QObject* objectId, const QString& tag, const QString& path)
{
    Job job(objectId, Kind::File, Action::Remove);
    job.tag = tag;
    job.path = path;
    return job;
}

int countJobs(const QList<Job>& jobs, Action action, const QString& tag)
{
    int count = 0;
    for (const Job& job : jobs) {
        if (job.action == action && job.tag == tag) {
            ++count;
        }
    }
    return count;
}

const Job* findJob(const QList<Job>& jobs, Action action, const QString& tag)
{
    for (const Job& job : jobs) {
        if (job.action == action && job.tag == tag) {
            return &job;
        }
    }
    return nullptr;
}

}

TEST_CASE("compressPendingJobs: default tag preserves legacy single-group collapse",
          "[cwSaveLoad][JobCompression]") {
    cwSaveLoadPrivate d;
    d.m_pendingJobs = {
        makeMove(objectA(), kTagDefault, "/proj/A.foo", "/proj/B.foo"),
        makeMove(objectA(), kTagDefault, "/proj/B.foo", "/proj/C.foo"),
    };

    d.compressPendingJobs();

    REQUIRE(d.m_pendingJobs.size() == 1);
    REQUIRE(d.m_pendingJobs.at(0).action == Action::Move);
    REQUIRE(d.m_pendingJobs.at(0).oldPath == QStringLiteral("/proj/A.foo"));
    REQUIRE(d.m_pendingJobs.at(0).path == QStringLiteral("/proj/C.foo"));
    REQUIRE(d.m_pendingJobs.at(0).tag.isEmpty());
}

TEST_CASE("compressPendingJobs: two tag groups collapse independently",
          "[cwSaveLoad][JobCompression]") {
    cwSaveLoadPrivate d;
    d.m_pendingJobs = {
        makeMove(objectA(), kTagDefault, "/proj/A.foo", "/proj/B.foo"),
        makeMove(objectA(), kTagSource,  "/proj/A.bar", "/proj/B.bar"),
        makeMove(objectA(), kTagDefault, "/proj/B.foo", "/proj/C.foo"),
        makeMove(objectA(), kTagSource,  "/proj/B.bar", "/proj/C.bar"),
    };

    d.compressPendingJobs();

    REQUIRE(d.m_pendingJobs.size() == 2);

    const Job* defaultMove = findJob(d.m_pendingJobs, Action::Move, kTagDefault);
    const Job* sourceMove = findJob(d.m_pendingJobs, Action::Move, kTagSource);
    REQUIRE(defaultMove != nullptr);
    REQUIRE(sourceMove != nullptr);

    REQUIRE(defaultMove->oldPath == QStringLiteral("/proj/A.foo"));
    REQUIRE(defaultMove->path    == QStringLiteral("/proj/C.foo"));
    REQUIRE(sourceMove->oldPath  == QStringLiteral("/proj/A.bar"));
    REQUIRE(sourceMove->path     == QStringLiteral("/proj/C.bar"));
}

TEST_CASE("compressPendingJobs: one tag group collapses while a singleton group stays",
          "[cwSaveLoad][JobCompression]") {
    cwSaveLoadPrivate d;
    d.m_pendingJobs = {
        makeMove(objectA(), kTagDefault, "/proj/A.foo", "/proj/B.foo"),
        makeMove(objectA(), kTagSource,  "/proj/A.bar", "/proj/B.bar"),
        makeMove(objectA(), kTagDefault, "/proj/B.foo", "/proj/C.foo"),
    };

    d.compressPendingJobs();

    REQUIRE(d.m_pendingJobs.size() == 2);

    const Job* defaultMove = findJob(d.m_pendingJobs, Action::Move, kTagDefault);
    const Job* sourceMove = findJob(d.m_pendingJobs, Action::Move, kTagSource);
    REQUIRE(defaultMove != nullptr);
    REQUIRE(sourceMove != nullptr);

    REQUIRE(defaultMove->oldPath == QStringLiteral("/proj/A.foo"));
    REQUIRE(defaultMove->path    == QStringLiteral("/proj/C.foo"));
    REQUIRE(sourceMove->oldPath  == QStringLiteral("/proj/A.bar"));
    REQUIRE(sourceMove->path     == QStringLiteral("/proj/B.bar"));
}

TEST_CASE("compressPendingJobs: cross-tag non-chaining moves are not collapsed",
          "[cwSaveLoad][JobCompression]") {
    // The exact shape that the pre-tag logic would have corrupted:
    // two Move jobs for the same (objectId, Kind) with disjoint paths.
    // Without tags they would have been collapsed to "/proj/A.foo -> /proj/D.bar",
    // silently moving the .laz over the .cwlaz.
    cwSaveLoadPrivate d;
    d.m_pendingJobs = {
        makeMove(objectA(), kTagDefault, "/proj/A.foo", "/proj/B.foo"),
        makeMove(objectA(), kTagSource,  "/proj/C.bar", "/proj/D.bar"),
    };

    d.compressPendingJobs();

    REQUIRE(d.m_pendingJobs.size() == 2);

    const Job* defaultMove = findJob(d.m_pendingJobs, Action::Move, kTagDefault);
    const Job* sourceMove = findJob(d.m_pendingJobs, Action::Move, kTagSource);
    REQUIRE(defaultMove != nullptr);
    REQUIRE(sourceMove != nullptr);

    REQUIRE(defaultMove->oldPath == QStringLiteral("/proj/A.foo"));
    REQUIRE(defaultMove->path    == QStringLiteral("/proj/B.foo"));
    REQUIRE(sourceMove->oldPath  == QStringLiteral("/proj/C.bar"));
    REQUIRE(sourceMove->path     == QStringLiteral("/proj/D.bar"));
}

TEST_CASE("compressPendingJobs: dropRedundantWrites respects tag",
          "[cwSaveLoad][JobCompression]") {
    cwSaveLoadPrivate d;
    d.m_pendingJobs = {
        makeWrite(objectA(), kTagDefault, "/proj/A.foo"),
        makeWrite(objectA(), kTagSource,  "/proj/A.bar"),
    };

    d.compressPendingJobs();

    // Different tags target different artifacts — neither is redundant.
    REQUIRE(d.m_pendingJobs.size() == 2);
    REQUIRE(countJobs(d.m_pendingJobs, Action::WriteFile, kTagDefault) == 1);
    REQUIRE(countJobs(d.m_pendingJobs, Action::WriteFile, kTagSource) == 1);

    // Adding a second write in the empty-tag group drops the earlier one,
    // but the other-tag write is untouched.
    d.m_pendingJobs.append(makeWrite(objectA(), kTagDefault, "/proj/A.foo"));
    d.compressPendingJobs();

    REQUIRE(d.m_pendingJobs.size() == 2);
    REQUIRE(countJobs(d.m_pendingJobs, Action::WriteFile, kTagDefault) == 1);
    REQUIRE(countJobs(d.m_pendingJobs, Action::WriteFile, kTagSource) == 1);
}

TEST_CASE("compressPendingJobs: dropWritesSupersededByRemove respects tag",
          "[cwSaveLoad][JobCompression]") {
    cwSaveLoadPrivate d;
    d.m_pendingJobs = {
        makeRemove(objectA(), kTagDefault, "/proj/A.foo"),
        makeWrite (objectA(), kTagSource,  "/proj/A.bar"),
    };

    d.compressPendingJobs();

    // A Remove on the primary artifact must not delete a queued write to a
    // secondary artifact: they are independent files.
    REQUIRE(d.m_pendingJobs.size() == 2);
    REQUIRE(countJobs(d.m_pendingJobs, Action::Remove, kTagDefault) == 1);
    REQUIRE(countJobs(d.m_pendingJobs, Action::WriteFile, kTagSource) == 1);
}

TEST_CASE("compressPendingJobs: three sequential moves within one tag collapse to A->D",
          "[cwSaveLoad][JobCompression]") {
    cwSaveLoadPrivate d;
    d.m_pendingJobs = {
        makeMove(objectA(), kTagSource, "/proj/A.bar", "/proj/B.bar"),
        makeMove(objectA(), kTagSource, "/proj/B.bar", "/proj/C.bar"),
        makeMove(objectA(), kTagSource, "/proj/C.bar", "/proj/D.bar"),
    };

    d.compressPendingJobs();

    REQUIRE(d.m_pendingJobs.size() == 1);
    REQUIRE(d.m_pendingJobs.at(0).action == Action::Move);
    REQUIRE(d.m_pendingJobs.at(0).tag == kTagSource);
    REQUIRE(d.m_pendingJobs.at(0).oldPath == QStringLiteral("/proj/A.bar"));
    REQUIRE(d.m_pendingJobs.at(0).path == QStringLiteral("/proj/D.bar"));
}

TEST_CASE("compressPendingJobs: different objects in same tag remain independent",
          "[cwSaveLoad][JobCompression]") {
    // Sanity guard: the GroupKey is (objectId, tag), not just tag. Two
    // objects sharing a tag must not have their jobs cross-merged.
    cwSaveLoadPrivate d;
    d.m_pendingJobs = {
        makeMove(objectA(), kTagOther, "/proj/A.foo", "/proj/B.foo"),
        makeMove(objectB(), kTagOther, "/proj/X.foo", "/proj/Y.foo"),
    };

    d.compressPendingJobs();

    REQUIRE(d.m_pendingJobs.size() == 2);
}

TEST_CASE("stateFor drops the entry when its object is destroyed",
          "[cwSaveLoad][JobCompression]") {
    // m_objectStates is keyed by pointer; without the destroyed cleanup, a
    // deleted object's entry would wait to be inherited by whatever object
    // the allocator next places at the same address.
    cwSaveLoad saveLoad;
    cwSaveLoadPrivate d;
    auto object = std::make_unique<QObject>();
    const QObject* address = object.get();

    d.stateFor(address, &saveLoad).currentPath = QStringLiteral("/proj/A.foo");
    REQUIRE(d.m_objectStates.contains(address));

    SECTION("destruction erases the entry") {
        object.reset();
        CHECK(!d.m_objectStates.contains(address));
    }

    SECTION("an entry erased for another reason is re-watched on re-insert") {
        // A directory Remove job or resetObjectStates erases entries while
        // their objects are still alive; the next stateFor must leave the
        // lifetime cleanup intact.
        d.m_objectStates.remove(address);
        d.stateFor(address, &saveLoad).currentPath = QStringLiteral("/proj/B.foo");
        REQUIRE(d.m_objectStates.contains(address));

        object.reset();
        CHECK(!d.m_objectStates.contains(address));
    }
}
