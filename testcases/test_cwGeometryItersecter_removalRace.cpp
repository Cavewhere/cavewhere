//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGeometryItersecter.h"
#include "TestGeometryBuilders.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"
#include "cwTask.h"

//Qt includes
#include <QCoreApplication>
#include <QThread>
#include <QThreadPool>
#include <QVector3D>

//QMath3d
#include <qray3d.h>

namespace {

constexpr float kPickRadius = 1.0f;
constexpr uint64_t kIdA = 1;
constexpr uint64_t kIdB = 2;
const QVector3D kPositionA(0.0f, 0.0f, 0.0f);
const QVector3D kPositionB(10.0f, 0.0f, 0.0f);

cwPickQuery pointPickQuery()
{
    cwPickQuery query;
    query.kinds = cwPickQuery::Kind::Points;
    return query;
}

QRay3D rayDownThrough(const QVector3D& position)
{
    return QRay3D(position + QVector3D(0.0f, 0.0f, 50.0f),
                  QVector3D(0.0f, 0.0f, -1.0f));
}

bool hits(const cwGeometryItersecter& intersecter, const QVector3D& position)
{
    return intersecter.intersectsDetailed(rayDownThrough(position),
                                          pointPickQuery()).hit();
}

// Block until every build worker has left cwTask's pool WITHOUT spinning the
// main event loop — the queued install callback must stay undelivered.
// waitOnPool's releaseThread/reserveThread dance can zero activeThreadCount
// while the outer worker is still finishing, so require the pool to stay
// drained across a short pause before trusting it.
void drainWorkerPool()
{
    QThreadPool* pool = cwTask::threadPool();
    do {
        pool->waitForDone();
        QThread::msleep(10);
    } while (pool->activeThreadCount() > 0);
}

} // namespace

// A removal racing a BVH build that already finished computing: removal strips
// its Key from m_dirtyKeys, so without a live-Nodes filter the stale install
// callback republishes the removed slot (picks hit deleted geometry and fill
// cwRayHit::object() from a freed attribution pointer) and promotes its
// sub-BVH into the cross-rebuild cache forever (render object ids are never
// recycled, so nothing evicts the entry).
//
// The window needs the install event queued but undelivered when the removal
// lands, so the test pumps posted events by hand instead of waitForFinish().
// The rebuild the removal launches runs on the worker pool, so its own
// install can't realistically post before the second pump drains — and even
// if it ever did, it would only mask the republish CHECK; the cache CHECK
// still pins the leak, which no later install can undo.
TEST_CASE("Removal racing a finished BVH build must not republish the removed key",
          "[cwGeometryItersecter]")
{
    cwGeometryItersecter intersecter;
    intersecter.addObject(cwGeometryItersecter::Object(
        nullptr, kIdA, cwTestGeometry::points({kPositionA}), QMatrix4x4(), kPickRadius));
    intersecter.addObject(cwGeometryItersecter::Object(
        nullptr, kIdB, cwTestGeometry::points({kPositionB}), QMatrix4x4(), kPickRadius));
    const cwGeometryItersecter::Key keyA{cwRenderObjectId{0}, kIdA};

    // The restarter defers the build launch to the event loop; fire it, then
    // let the worker finish computing. Its install callback is now queued on
    // the main thread, undelivered.
    QCoreApplication::sendPostedEvents();
    drainWorkerPool();
    REQUIRE_FALSE(hits(intersecter, kPositionB)); // install must still be pending

    intersecter.removeObject(keyA);
    REQUIRE(intersecter.boundingBox(keyA).isNull());

    // Deliver the stale install (and launch the removal's rebuild).
    QCoreApplication::sendPostedEvents();

    CHECK(hits(intersecter, kPositionB));       // the stale install did land ...
    CHECK_FALSE(hits(intersecter, kPositionA)); // ... minus the removed key's slot

    intersecter.waitForFinish();
    CHECK(hits(intersecter, kPositionB));
    CHECK_FALSE(hits(intersecter, kPositionA));

    // The removed key's freshly built sub-BVH must not be promoted into the
    // cross-rebuild cache — nothing would ever evict it.
    CHECK(intersecter.debugStatistics().cachedSubBvhCount == 1);
}
