#ifndef CWFUTURE_H
#define CWFUTURE_H

//Qt includes
#include <QFuture>
#include <QString>

//Our includes
#include "cwProgressNode.h"

class cwFuture {
public:
    cwFuture() {}
    cwFuture(const QFuture<void>& future, const QString& jobName, cwProgressNodePtr tree = {}) :
        mFuture(future),
        mName(jobName),
        mTree(std::move(tree))
    { }

    QFuture<void> future() const { return mFuture; }
    QString name() const { return mName; }

    //! The run's progress tree, when it has one. Null for every other job.
    cwProgressNodePtr tree() const { return mTree; }

private:
    QFuture<void> mFuture;
    QString mName;
    cwProgressNodePtr mTree;
};

#endif // CWFUTURE_H
