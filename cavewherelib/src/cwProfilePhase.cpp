/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProfilePhase.h"
#include "cwProfileLog.h"
#include "cwTaskTrace.h"

//Qt includes
#include <QElapsedTimer>

struct cwProfilePhase::State
{
    const QLoggingCategory* category = nullptr;
    QString kind;
    QString name;
    QString fields;
    QElapsedTimer timer;
    bool logging = false;
    bool finished = false;
#if CAVEWHERE_TASK_TRACE
    bool traced = false;
    MarkScope::Profiler::Token token;
#endif

    ~State()
    {
        end();
    }

    void end()
    {
        if (finished) {
            return;
        }
        finished = true;

#if CAVEWHERE_TASK_TRACE
        if (traced) {
            cwTaskProfiler().stop(token);
        }
#endif
        if (logging) {
            cw::profile::write(*category,
                               QStringLiteral("%1 %2 us=%3%4")
                                   .arg(kind, name)
                                   .arg(cw::profile::elapsedUs(timer))
                                   .arg(fields));
        }
    }
};

cwProfilePhase::cwProfilePhase(const QLoggingCategory& category, const QString& kind, const QString& name)
{
    const bool logging = category.isDebugEnabled();
#if CAVEWHERE_TASK_TRACE
    const bool traced = cwTaskProfiler().enabled();
#else
    const bool traced = false;
#endif
    if (!logging && !traced) {
        return;
    }

    m_state = std::make_shared<State>();
    m_state->category = &category;
    m_state->kind = kind;
    m_state->name = name;
    m_state->logging = logging;
    if (logging) {
        m_state->timer.start();
    }
#if CAVEWHERE_TASK_TRACE
    m_state->traced = traced;
    if (traced) {
        m_state->token = cwTaskProfiler().start(QStringLiteral("%1 %2").arg(kind, name));
    }
#endif
}

bool cwProfilePhase::logging() const
{
    return m_state && m_state->logging && !m_state->finished;
}

void cwProfilePhase::addField(const QString& key, qint64 value)
{
    addField(key, QString::number(value));
}

void cwProfilePhase::addField(const QString& key, const QString& value)
{
    if (!logging()) {
        return;
    }
    m_state->fields += QStringLiteral(" %1=%2").arg(key, value);
}

void cwProfilePhase::finish()
{
    if (m_state) {
        m_state->end();
    }
}
