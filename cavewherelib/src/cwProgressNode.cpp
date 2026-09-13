/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProgressNode.h"

//Qt includes
#include <QtGlobal>

//Std includes
#include <algorithm>
#include <atomic>

namespace {

//Raises value to candidate when candidate is larger. Returns true when it moved.
template <typename T>
bool storeMax(std::atomic<T>& value, T candidate)
{
    T current = value.load();
    while(candidate > current && !value.compare_exchange_weak(current, candidate)) { }
    return candidate > current;
}

} // namespace

cwProgressNode::cwProgressNode(CreateTag, const QString& name, cwProgressNodePtr root,
                               std::weak_ptr<cwProgressNode> parent) :
    m_parent(std::move(parent)),
    m_root(std::move(root)),
    m_name(name)
{
    m_born.start();
}

cwProgressNode::~cwProgressNode()
{
    //Safety net: an abandoned tree can't leave a stuck row behind
    finish();
}

cwProgressNodePtr cwProgressNode::createRoot(const QString& name)
{
    auto root = std::make_shared<cwProgressNode>(CreateTag(), name, cwProgressNodePtr(),
                                                 std::weak_ptr<cwProgressNode>());
    root->m_promise.emplace();
    root->m_future = root->m_promise->future();
    root->m_promise->start();
    root->m_promise->setProgressRange(0, kPromiseUnits);
    return root;
}

cwProgressNodePtr cwProgressNode::addChild(const QString& name)
{
    const cwProgressNodePtr root = m_root ? m_root : shared_from_this();

    if(isActive()) {
        auto child = std::make_shared<cwProgressNode>(CreateTag(), name, root, weak_from_this());

        bool attached = false;
        {
            //Under the mutex, so a child that races finish() either lands in
            //finish()'s snapshot or sees the finished flag and detaches
            QMutexLocker locker(&m_mutex);
            if(isActive()) {
                m_children.append(child);
                attached = true;
            }
        }

        if(attached) {
            m_isParent.store(true);
            publish();

            return child;
        }
    }

    //Detached: nothing links it to the tree, so its whole subtree is a no-op
    auto detached = std::make_shared<cwProgressNode>(CreateTag(), name, root,
                                                     std::weak_ptr<cwProgressNode>());
    detached->m_finished.store(true);
    return detached;
}

void cwProgressNode::setTotal(qsizetype total)
{
    Q_ASSERT(isLeaf());
    if(isActive()) {
        m_total.store(total);
        publish();
    }
}

void cwProgressNode::report(qsizetype done)
{
    Q_ASSERT(isLeaf());
    if(isActive()) {
        storeMax(m_done, done);
        publish();
    }
}

void cwProgressNode::advance(qsizetype delta)
{
    Q_ASSERT(isLeaf());
    if(isActive()) {
        m_done.fetch_add(delta);
        publish();
    }
}

void cwProgressNode::expectChildren(int count)
{
    {
        QMutexLocker locker(&m_mutex);
        m_expectedChildren = count;
    }
    publish();
}

void cwProgressNode::finish()
{
    if(m_finished.exchange(true)) {
        return;
    }

    for(const cwProgressNodePtr& child : activeChildren()) {
        child->finish();
    }

    if(cwProgressNodePtr parent = m_parent.lock()) {
        parent->childFinished(this); //publishes for us
    } else if(m_root) {
        publish();
    } else {
        QMutexLocker locker(&m_promiseMutex);
        if(m_promise.has_value() && !m_canceled.load()) {
            m_promise->setProgressValue(kPromiseUnits);
            m_promise->finish();
        }
    }
}

void cwProgressNode::cancel()
{
    Q_ASSERT(!m_root); //root only

    m_canceled.store(true);

    //Children hold the root, so detaching them is what lets an abandoned
    //tree free itself when its workers never finish their own nodes
    for(const cwProgressNodePtr& child : activeChildren()) {
        child->finish();
    }

    QMutexLocker locker(&m_promiseMutex);
    if(m_promise.has_value()) {
        m_future.cancel();
        m_promise->finish();
    }
}

double cwProgressNode::fraction() const
{
    if(m_finished.load()) {
        return 1.0;
    }

    if(isLeaf()) {
        const qsizetype total = m_total.load();
        if(total <= 0) {
            return kUnknownFraction;
        }
        return double(std::min(m_done.load(), total)) / double(total);
    }

    QList<cwProgressNodePtr> children;
    int finishedChildren = 0;
    int expectedChildren = 0;
    {
        QMutexLocker locker(&m_mutex);
        children = m_children;
        finishedChildren = m_finishedChildren;
        expectedChildren = m_expectedChildren;
    }

    const int slotCount = std::max(expectedChildren,
                                   finishedChildren + int(children.size()));
    if(slotCount <= 0) {
        return kUnknownFraction;
    }

    double sum = finishedChildren;
    for(const cwProgressNodePtr& child : std::as_const(children)) {
        sum += std::max(0.0, child->fraction());
    }

    return std::min(1.0, sum / double(slotCount));
}

qsizetype cwProgressNode::done() const
{
    return isLeaf() ? m_done.load() : 0;
}

qsizetype cwProgressNode::total() const
{
    return isLeaf() ? m_total.load() : 0;
}

QList<cwProgressNodePtr> cwProgressNode::activeChildren() const
{
    QMutexLocker locker(&m_mutex);
    return m_children;
}

QFuture<void> cwProgressNode::future() const
{
    return m_future;
}

void cwProgressNode::publish()
{
    cwProgressNode* root = rootNode();

    const double fraction = root->fraction();
    if(fraction < 0.0) {
        return;
    }

    const int value = int(fraction * kPromiseUnits);
    if(!storeMax(root->m_lastPromiseValue, value)) {
        //An unhinted parent that grew stalls the bar instead of stepping back
        return;
    }

    QMutexLocker locker(&root->m_promiseMutex);
    if(root->m_promise.has_value() && !root->m_finished.load()) {
        root->m_promise->setProgressValue(value);
    }
}

void cwProgressNode::childFinished(const cwProgressNode* child)
{
    {
        QMutexLocker locker(&m_mutex);
        const qsizetype removed = m_children.removeIf([child](const cwProgressNodePtr& node) {
            return node.get() == child;
        });
        if(removed == 0) {
            return;
        }
        m_finishedChildren += int(removed);
    }

    publish();
}

cwProgressScope::cwProgressScope(cwProgressNodePtr node) :
    m_node(std::move(node))
{
}

cwProgressScope::cwProgressScope(const cwProgressScope& parent, const QString& name) :
    m_node(parent.m_node ? parent.m_node->addChild(name) : cwProgressNodePtr())
{
}

cwProgressScope::cwProgressScope(cwProgressScope&& other) noexcept :
    m_node(std::move(other.m_node))
{
}

cwProgressScope& cwProgressScope::operator=(cwProgressScope&& other) noexcept
{
    if(this != &other) {
        finish();
        m_node = std::move(other.m_node);
    }
    return *this;
}

cwProgressScope::~cwProgressScope()
{
    finish();
}

void cwProgressScope::setTotal(qsizetype total)
{
    if(m_node) {
        m_node->setTotal(total);
    }
}

void cwProgressScope::report(qsizetype done)
{
    if(m_node) {
        m_node->report(done);
    }
}

void cwProgressScope::advance(qsizetype delta)
{
    if(m_node) {
        m_node->advance(delta);
    }
}

void cwProgressScope::expectChildren(int count)
{
    if(m_node) {
        m_node->expectChildren(count);
    }
}

void cwProgressScope::finish()
{
    if(m_node) {
        m_node->finish();
    }
}
