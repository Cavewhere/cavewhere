/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwProgressNode.h"

//Qt includes
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFuture>
#include <QtConcurrent>

//Std includes
#include <functional>
#include <memory>

namespace {

//Promise updates travel through queued events, so pump the event loop until
//the condition holds (or the deadline passes).
bool processEventsUntil(const std::function<bool()>& condition, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while(!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return condition();
}

bool atMaximum(const QFuture<void>& future)
{
    return future.progressValue() == future.progressMaximum();
}

} // namespace

TEST_CASE("cwProgressNode root drives a running future", "[cwProgressNode]")
{
    auto root = cwProgressNode::createRoot(QStringLiteral("Updating Scraps"));

    CHECK(root->name().toStdString() == "Updating Scraps");
    CHECK(root->isLeaf());
    CHECK(!root->isFinished());

    const QFuture<void> future = root->future();
    CHECK(future.isRunning());
    CHECK(future.progressMinimum() == 0);
    CHECK(future.progressMaximum() == cwProgressNode::kPromiseUnits);

    root->finish();

    CHECK(root->isFinished());
    CHECK(future.isFinished());
    CHECK(atMaximum(future));

    //A second finish changes nothing
    root->finish();
    CHECK(future.isFinished());
    CHECK(atMaximum(future));
}

TEST_CASE("cwProgressNode leaf fraction is monotone and clamped", "[cwProgressNode]")
{
    auto root = cwProgressNode::createRoot(QStringLiteral("Run"));
    auto leaf = root->addChild(QStringLiteral("Cropping"));

    CHECK(leaf->fraction() == cwProgressNode::kUnknownFraction);
    CHECK(leaf->isLeaf());

    leaf->setTotal(4);
    leaf->report(1);
    CHECK(leaf->fraction() == Catch::Approx(0.25));

    leaf->report(0);
    CHECK(leaf->fraction() == Catch::Approx(0.25));
    CHECK(leaf->done() == 1);

    leaf->report(9);
    CHECK(leaf->fraction() == Catch::Approx(1.0));
    CHECK(leaf->total() == 4);

    auto opaque = root->addChild(QStringLiteral("Compressing texture"));
    CHECK(opaque->fraction() == cwProgressNode::kUnknownFraction);
}

TEST_CASE("cwProgressNode parent with a hint fills slot by slot", "[cwProgressNode]")
{
    auto root = cwProgressNode::createRoot(QStringLiteral("Run"));
    auto parent = root->addChild(QStringLiteral("Scrap 0"));
    parent->expectChildren(4);

    auto first = parent->addChild(QStringLiteral("Cropping"));
    first->setTotal(2);
    first->report(1);
    CHECK(parent->fraction() == Catch::Approx(0.125));

    first->finish();
    CHECK(parent->fraction() == Catch::Approx(0.25));
    CHECK(parent->activeChildren().isEmpty());

    parent->addChild(QStringLiteral("Triangulating"))->finish();
    parent->addChild(QStringLiteral("Building mesh"))->finish();
    CHECK(parent->fraction() == Catch::Approx(0.75));

    parent->addChild(QStringLiteral("Morphing"))->finish();
    CHECK(parent->fraction() == Catch::Approx(1.0));
    CHECK(!parent->isLeaf());
}

TEST_CASE("cwProgressNode parent without a hint stalls instead of stepping back",
          "[cwProgressNode]")
{
    auto root = cwProgressNode::createRoot(QStringLiteral("Run"));

    root->addChild(QStringLiteral("First"))->finish();
    CHECK(root->fraction() == Catch::Approx(1.0));
    CHECK(processEventsUntil([&root]() { return atMaximum(root->future()); }));

    auto second = root->addChild(QStringLiteral("Second"));
    CHECK(root->fraction() == Catch::Approx(0.5));

    //The bar holds its last value while the computed fraction catches up
    CHECK(atMaximum(root->future()));

    second->finish();
    root->finish();
}

TEST_CASE("cwProgressNode parent that finishes first deadens its subtree",
          "[cwProgressNode]")
{
    auto root = cwProgressNode::createRoot(QStringLiteral("Run"));
    auto parent = root->addChild(QStringLiteral("Note.glb"));
    auto child = parent->addChild(QStringLiteral("Loading glTF"));
    child->setTotal(10);
    child->report(2);

    parent->finish();

    CHECK(parent->isFinished());
    CHECK(child->isFinished());
    CHECK(parent->activeChildren().isEmpty());

    child->report(7);
    CHECK(child->done() == 2);
    CHECK(parent->fraction() == Catch::Approx(1.0));

    auto late = parent->addChild(QStringLiteral("Morphing"));
    late->setTotal(100);
    late->report(50);
    CHECK(late->done() == 0);
    CHECK(parent->activeChildren().isEmpty());
    CHECK(parent->fraction() == Catch::Approx(1.0));

    root->finish();
}

TEST_CASE("cwProgressNode cancel deadens the tree", "[cwProgressNode]")
{
    auto root = cwProgressNode::createRoot(QStringLiteral("Run"));
    auto worker = root->addChild(QStringLiteral("Morphing"));
    worker->setTotal(100);
    worker->report(10);

    root->cancel();

    CHECK(root->future().isCanceled());
    CHECK(processEventsUntil([&root]() { return root->future().isFinished(); }));

    worker->advance(5);
    CHECK(worker->done() == 10);

    auto late = worker->addChild(QStringLiteral("Mesh 0"));
    CHECK(worker->activeChildren().isEmpty());

    //The tree destructs cleanly once the worker drops its handles
    std::weak_ptr<cwProgressNode> rootObserver = root;
    std::weak_ptr<cwProgressNode> workerObserver = worker;

    late.reset();
    worker.reset();
    root.reset();

    CHECK(workerObserver.expired());
    CHECK(rootObserver.expired());
}

TEST_CASE("cwProgressNode counts concurrent advances", "[cwProgressNode]")
{
    constexpr int kWorkerCount = 8;
    constexpr int kAdvancesPerWorker = 10000;

    auto root = cwProgressNode::createRoot(QStringLiteral("Run"));
    auto leaf = root->addChild(QStringLiteral("Morphing"));
    leaf->setTotal(qsizetype(kWorkerCount) * kAdvancesPerWorker);

    QList<QFuture<void>> workers;
    workers.reserve(kWorkerCount);
    for(int i = 0; i < kWorkerCount; i++) {
        workers.append(QtConcurrent::run([leaf]() {
            for(int step = 0; step < kAdvancesPerWorker; step++) {
                leaf->advance();
            }
        }));
    }

    for(QFuture<void>& worker : workers) {
        worker.waitForFinished();
    }

    CHECK(leaf->done() == qsizetype(kWorkerCount) * kAdvancesPerWorker);
    CHECK(leaf->fraction() == Catch::Approx(1.0));
    CHECK(processEventsUntil([&root]() { return atMaximum(root->future()); }));

    leaf->finish();
    root->finish();
}

TEST_CASE("cwProgressScope is null-safe and finishes its node", "[cwProgressNode]")
{
    cwProgressScope nullScope;
    CHECK(nullScope.isNull());
    CHECK(nullScope.node() == nullptr);

    nullScope.setTotal(10);
    nullScope.report(5);
    nullScope.advance();
    nullScope.expectChildren(3);
    nullScope.finish();

    cwProgressScope childOfNull(nullScope, QStringLiteral("Parsing"));
    CHECK(childOfNull.isNull());

    auto root = cwProgressNode::createRoot(QStringLiteral("Run"));
    cwProgressNodePtr node;
    {
        cwProgressScope scope(root->addChild(QStringLiteral("Checksum")));
        node = scope.node();
        scope.setTotal(4);
        scope.advance(2);
        CHECK(node->fraction() == Catch::Approx(0.5));
    }
    CHECK(node->isFinished());

    cwProgressNodePtr movedNode;
    {
        cwProgressScope source(root->addChild(QStringLiteral("Parsing")));
        movedNode = source.node();
        cwProgressScope destination(std::move(source));

        CHECK(source.isNull());
        source.finish();
        CHECK(!movedNode->isFinished());

        CHECK(destination.node() == movedNode);
    }
    CHECK(movedNode->isFinished());

    root->finish();
}
