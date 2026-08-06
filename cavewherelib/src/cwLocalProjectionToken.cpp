/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLocalProjectionToken.h"
#include "cwLocalProjectionManager.h"

//AsyncFuture
#include <asyncfuture.h>

cwLocalProjectionToken::cwLocalProjectionToken(cwLocalProjectionManager* manager) :
    m_manager(manager)
{
}

QFuture<QString> cwLocalProjectionToken::frameFuture() const
{
    if (m_manager.isNull()) {
        // No project deriving a frame, so there is nothing to wait for and no
        // frame to load into: the answer is final and empty.
        return AsyncFuture::completed(QString());
    }
    return m_manager->frameFuture();
}
