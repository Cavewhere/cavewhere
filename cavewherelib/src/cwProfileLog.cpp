/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProfileLog.h"

//Qt includes
#include <QDebug>

Q_LOGGING_CATEGORY(lcProfileRender, "cw.profile.render", QtWarningMsg)
Q_LOGGING_CATEGORY(lcProfilePick, "cw.profile.pick", QtWarningMsg)
Q_LOGGING_CATEGORY(lcProfileLoad, "cw.profile.load", QtWarningMsg)

void cw::profile::write(const QLoggingCategory& category, const QString& line)
{
    if (!category.isDebugEnabled()) {
        return;
    }

    QMessageLogger().debug(category).noquote() << line;
}
