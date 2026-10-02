/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPHASEJOB_H
#define CWPHASEJOB_H

//Qt includes
#include <QFuture>
#include <QPromise>
#include <QString>

//Our includes
#include "cwFutureManagerToken.h"
#include "cwGlobals.h"

//A promise-backed progress job for one phase of a long-running operation
//(e.g. an export's tile rendering), surfaced in the app's job list through
//cwFutureManagerToken. start() begins the promise, sets the step count, and
//registers the job when the token is valid — pass an invalid token to run
//the phase silently (the promise still runs, so the caller's
//completion/cancel paths stay unconditional). finish() is idempotent, and
//the destructor finishes an unfinished job so a destroyed phase can't
//leave a stuck job-list row. Owners whose lifetime is trapped (e.g. inside
//a shared_ptr cycle) must still call finish() on every exit path — the
//destructor safety net only fires if the owner is actually destroyed.
//
//Not thread-safe: construct, drive, and destroy on one thread. To resolve
//the job-list entry early (e.g. on a user cancel), cancel future() — the
//job list removes the row on cancel; finish() afterwards is still legal
//and required.
class CAVEWHERE_LIB_EXPORT cwPhaseJob
{
public:
    cwPhaseJob() = default;
    ~cwPhaseJob();

    cwPhaseJob(const cwPhaseJob&) = delete;
    cwPhaseJob& operator=(const cwPhaseJob&) = delete;

    void start(const QString& name, int numberOfSteps, cwFutureManagerToken token);
    void setProgressValue(int value);
    void finish();

    bool isStarted() const { return m_started; }

    //Only meaningful after start(). A default-constructed QFuture reports
    //canceled, which is also why start() must run before this future is
    //handed to anything that inspects it.
    QFuture<void> future() const { return m_future; }

private:
    QPromise<void> m_promise;
    QFuture<void> m_future;
    bool m_started = false;
    bool m_finished = false;
};

#endif // CWPHASEJOB_H
