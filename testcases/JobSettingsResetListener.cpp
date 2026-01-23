/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include <catch2/reporters/catch_reporter_event_listener.hpp>

// Qt includes
#include <QThread>

// Project includes
#include "cwJobSettings.h"

class JobSettingsResetListener : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseStarting(Catch::TestCaseInfo const&) override
    {
        cwJobSettings::initialize();
        auto* settings = cwJobSettings::instance();
        if(settings == nullptr) {
            return;
        }

        settings->setAutomaticUpdate(true);
        settings->setThreadCount(QThread::idealThreadCount());
    }
};

CATCH_REGISTER_LISTENER(JobSettingsResetListener)
