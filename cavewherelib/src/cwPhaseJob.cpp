/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPhaseJob.h"

cwPhaseJob::~cwPhaseJob()
{
    finish();
}

void cwPhaseJob::start(const QString& name, int numberOfSteps, cwFutureManagerToken token)
{
    Q_ASSERT(!m_started);

    m_future = m_promise.future();
    m_promise.start();
    m_promise.setProgressRange(0, numberOfSteps);
    m_started = true;

    if(token.isValid()) {
        token.addJob({m_future, name});
    }
}

void cwPhaseJob::setProgressValue(int value)
{
    Q_ASSERT(m_started);
    if(!m_finished) {
        m_promise.setProgressValue(value);
    }
}

void cwPhaseJob::finish()
{
    if(m_started && !m_finished) {
        m_promise.finish();
        m_finished = true;
    }
}
