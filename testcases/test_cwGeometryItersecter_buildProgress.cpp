//Catch includes
#include <catch2/catch_test_macros.hpp>

// Std
#include <algorithm>
#include <cstdint>

// Qt
#include <QCoreApplication>
#include <QMatrix4x4>
#include <QModelIndex>
#include <QVector>
#include <QVector3D>

// SUT
#include "cwGeometryItersecter.h"
#include "cwFutureManagerModel.h"
#include "TestGeometryBuilders.h"

namespace {

// pickRadius must be > 0 or Points contribute no BVH primitives
// (countNodePrimitives returns 0), so the build would have nothing to do.
constexpr float kPickRadius = 0.05f;

// Large enough that a single sub-BVH build is not instantaneous — so that if
// intra-object progress *were* reported, the QFutureWatcher would have time to
// deliver at least one intermediate update while the build is in flight.
constexpr int kPointCount = 500000;

// One monolithic point cloud — the "huge object" from the bug report. A single
// registered Object becomes a single build task, which is precisely the case
// that starves the progress meter.
cwGeometryItersecter::Object makeLargeCloud(uint64_t id)
{
    QVector<QVector3D> points;
    points.reserve(kPointCount);

    // Deterministic lattice spread over a box (no RNG — reproducible).
    constexpr int kSide = 80; // 80^3 = 512000 >= kPointCount
    int made = 0;
    for (int x = 0; x < kSide && made < kPointCount; ++x) {
        for (int y = 0; y < kSide && made < kPointCount; ++y) {
            for (int z = 0; z < kSide && made < kPointCount; ++z) {
                points.append(QVector3D(float(x), float(y), float(z)));
                ++made;
            }
        }
    }

    return cwGeometryItersecter::Object(nullptr, id,
                                        cwTestGeometry::points(points),
                                        QMatrix4x4(),
                                        kPickRadius);
}

} // namespace

// Guards the "progress sits at 0 / 1000" regression: when a single huge object
// is registered, the async BVH build must report progress between start and
// finish. The bug this pins was the job in the task panel jumping straight from
// 0 to done with nothing in between — exactly what a user loading a large point
// cloud saw.
//
// The task panel reads progress from cwFutureManagerModel (ProgressRole /
// NumberOfStepRole), fed by a QFutureWatcher over the build future — the very
// same path the UI uses. We record every progress value the model publishes
// during the build and check whether any lands strictly between 0 and the
// maximum. With the bug, none ever does.
TEST_CASE("Building a single huge object reports intermediate progress",
          "[cwGeometryItersecter][progress]")
{
    cwFutureManagerModel model;
    model.setInterval(10);

    cwGeometryItersecter intersector;
    intersector.setFutureManagerToken(model.token());

    QVector<int> observedValues;
    int observedMax = 0;

    QObject::connect(&model, &cwFutureManagerModel::dataChanged, &model,
                     [&](const QModelIndex& topLeft,
                         const QModelIndex& /*bottomRight*/,
                         const QVector<int>& roles) {
        if (roles.contains(cwFutureManagerModel::NumberOfStepRole)) {
            observedMax = (std::max)(
                observedMax,
                model.data(topLeft, cwFutureManagerModel::NumberOfStepRole).toInt());
        }
        if (roles.contains(cwFutureManagerModel::ProgressRole)) {
            observedValues.append(
                model.data(topLeft, cwFutureManagerModel::ProgressRole).toInt());
        }
    });

    const cwGeometryItersecter::Object cloud = makeLargeCloud(1);
    const cwGeometryItersecter::Key key = cloud.key();
    intersector.addObject(cloud);

    // waitForFinish() spins a nested event loop, so the QFutureWatcher inside
    // the model delivers its progress signals here while the build runs.
    intersector.waitForFinish();
    QCoreApplication::processEvents();

    // Anchor: the build actually happened and installed a pickable sub-BVH.
    REQUIRE(intersector.isObjectPickReady(key));
    // Anchor: the progress range was published (build was observed via the panel).
    REQUIRE(observedMax > 0);

    const int maxSteps = observedMax;
    bool sawIntermediate = false;
    for (int v : observedValues) {
        if (v > 0 && v < maxSteps) {
            sawIntermediate = true;
            break;
        }
    }

    INFO("progress maximum = " << maxSteps);
    INFO("observed progress updates = " << observedValues.size());
    // A single huge object must advance the meter as it builds. The regression
    // this guards against reported progress only per finished build task, so a
    // one-task build (one big cloud) jumped 0 -> maximum with nothing between,
    // pinning the panel at 0 for the whole build.
    CHECK(sawIntermediate);
}
