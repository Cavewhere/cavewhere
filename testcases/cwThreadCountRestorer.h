#ifndef CWTHREADCOUNTRESTORER_H
#define CWTHREADCOUNTRESTORER_H

//Our includes
#include "cwTask.h"

//Qt includes
#include <QThreadPool>

//! The pool width for one-thread cases: the one-thread invariant's worst case
inline constexpr int kSingleThread = 1;

//! Sets the shared worker pool's thread count and restores it on every path out of a test
class cwThreadCountRestorer {
public:
    explicit cwThreadCountRestorer(int threadCount) :
        m_previous(cwTask::threadPool()->maxThreadCount())
    {
        cwTask::threadPool()->setMaxThreadCount(threadCount);
    }

    ~cwThreadCountRestorer()
    {
        cwTask::threadPool()->setMaxThreadCount(m_previous);
    }

    cwThreadCountRestorer(const cwThreadCountRestorer&) = delete;
    cwThreadCountRestorer& operator=(const cwThreadCountRestorer&) = delete;

private:
    int m_previous;
};

#endif // CWTHREADCOUNTRESTORER_H
