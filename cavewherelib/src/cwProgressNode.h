/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPROGRESSNODE_H
#define CWPROGRESSNODE_H

//Qt includes
#include <QElapsedTimer>
#include <QFuture>
#include <QList>
#include <QMutex>
#include <QPromise>
#include <QString>

//Std includes
#include <atomic>
#include <memory>
#include <optional>

//Our includes
#include "cwGlobals.h"

class cwProgressNode;
using cwProgressNodePtr = std::shared_ptr<cwProgressNode>;

//A thread-safe node in a run's progress tree. Created through cwProgressScope
//or cwProgressNode::createRoot; never constructed directly.
//
//A run owns a root node whose fraction drives a QPromise<void>, so the task
//list and the ring see a normal future. Code that holds a node may hang a
//named child off it, drive it as a leaf (setTotal/report/advance) or grow
//children under it, and finish it when the work is done. Nobody declares
//child counts up front; a loop that already knows its count may hint it with
//expectChildren(), which only smooths the interior. A parent that grows past
//its hint stalls the bar (the promise drops lower values) rather than moving
//it backward.
class CAVEWHERE_LIB_EXPORT cwProgressNode : public std::enable_shared_from_this<cwProgressNode>
{
    struct CreateTag { explicit CreateTag() = default; };

public:
    //Sentinel fraction for "alive, nothing measured yet" (an opaque leaf)
    static constexpr double kUnknownFraction = -1.0;
    //Resolution of the root's promise range: fraction * kPromiseUnits
    static constexpr int kPromiseUnits = 1000;

    cwProgressNode(CreateTag, const QString& name, cwProgressNodePtr root,
                   std::weak_ptr<cwProgressNode> parent);
    ~cwProgressNode();

    cwProgressNode(const cwProgressNode&) = delete;
    cwProgressNode& operator=(const cwProgressNode&) = delete;

    static cwProgressNodePtr createRoot(const QString& name);

    //Attaches a new active child. Returns a detached (dead) node when this
    //node is finished or its root is canceled, so late callers stay no-ops.
    cwProgressNodePtr addChild(const QString& name);

    //Leaf API. A node is a leaf until addChild() is called on it.
    void setTotal(qsizetype total);            //0 keeps the leaf opaque
    void report(qsizetype done);               //monotone: max(done, previous)
    void advance(qsizetype delta = 1);

    //Parent hint: a loop that knows its count says so, so the fraction is
    //computed over `count` slots from the first child on. Optional.
    void expectChildren(int count);

    void finish();                             //idempotent; finishes active children
    void cancel();                             //root only: cancels the promise, marks the tree

    QString name() const { return m_name; }
    double fraction() const;                   //[0, 1] or kUnknownFraction
    qsizetype done() const;                    //leaf counters, 0 for a parent
    qsizetype total() const;
    bool isFinished() const { return m_finished.load(); }
    bool isLeaf() const { return !m_isParent.load(); }
    bool isCanceled() const { return rootNode()->m_canceled.load(); }
    qint64 ageMs() const { return m_born.elapsed(); }
    QList<cwProgressNodePtr> activeChildren() const;

    QFuture<void> future() const;              //root only; default QFuture otherwise

private:
    mutable QMutex m_mutex;                    //m_children, m_finishedChildren, m_expectedChildren
    QList<cwProgressNodePtr> m_children;       //active children only
    int m_finishedChildren = 0;
    int m_expectedChildren = 0;
    std::atomic<qsizetype> m_done{0};
    std::atomic<qsizetype> m_total{0};
    std::atomic<bool> m_finished{false};
    std::atomic<bool> m_isParent{false};
    std::weak_ptr<cwProgressNode> m_parent;
    cwProgressNodePtr m_root;                  //null on the root itself
    QElapsedTimer m_born;
    QString m_name;

    //Root only
    mutable QMutex m_promiseMutex;
    std::optional<QPromise<void>> m_promise;
    QFuture<void> m_future;
    std::atomic<int> m_lastPromiseValue{0};
    std::atomic<bool> m_canceled{false};

    //Live enough to record progress: still running under a running root
    bool isActive() const { return !isFinished() && !isCanceled(); }

    cwProgressNode* rootNode() { return m_root ? m_root.get() : this; }
    const cwProgressNode* rootNode() const { return m_root ? m_root.get() : this; }

    void publish();
    void childFinished(const cwProgressNode* child);
};

//RAII, null-safe, move-only handle. What functions take and pass down. A
//default-constructed scope is a null scope: every call is a no-op and every
//child of it is null, so library code below the task layer can take a scope
//by default argument and stay independent of the task manager.
class CAVEWHERE_LIB_EXPORT cwProgressScope
{
public:
    cwProgressScope() = default;
    explicit cwProgressScope(cwProgressNodePtr node);
    cwProgressScope(const cwProgressScope& parent, const QString& name);
    cwProgressScope(const cwProgressNodePtr& parent, const QString& name);
    cwProgressScope(cwProgressScope&& other) noexcept;
    cwProgressScope& operator=(cwProgressScope&& other) noexcept;
    ~cwProgressScope();

    cwProgressScope(const cwProgressScope&) = delete;
    cwProgressScope& operator=(const cwProgressScope&) = delete;

    void setTotal(qsizetype total);
    void report(qsizetype done);
    void advance(qsizetype delta = 1);
    void expectChildren(int count);
    void finish();

    //Capture this by value into a lambda that outlives the scope
    cwProgressNodePtr node() const { return m_node; }
    bool isNull() const { return m_node == nullptr; }

private:
    cwProgressNodePtr m_node;
};

#endif // CWPROGRESSNODE_H
