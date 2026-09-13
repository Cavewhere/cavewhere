/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPROFILELOG_H
#define CWPROFILELOG_H

//Qt includes
#include <QElapsedTimer>
#include <QLoggingCategory>

// The point cloud profiling harness. All three are off by default, so a build
// with them costs one branch per frame and per query. Enable with:
//   QT_LOGGING_RULES="cw.profile.render.debug=true"
// or through CaveWhere --profile-log, which turns all three on.
Q_DECLARE_LOGGING_CATEGORY(lcProfileRender)
Q_DECLARE_LOGGING_CATEGORY(lcProfilePick)
Q_DECLARE_LOGGING_CATEGORY(lcProfileLoad)

namespace cw::profile {
    //! Frames one cw.profile.render line summarizes.
    constexpr int kProfileBlockFrames = 120;

    //! Microseconds on @a timer, the unit every cw.profile line reports.
    inline qint64 elapsedUs(const QElapsedTimer& timer)
    {
        constexpr qint64 kNanosecondsPerMicrosecond = 1000;
        return timer.nsecsElapsed() / kNanosecondsPerMicrosecond;
    }
}

#endif // CWPROFILELOG_H
