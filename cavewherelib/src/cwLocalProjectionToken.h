/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLOCALPROJECTIONTOKEN_H
#define CWLOCALPROJECTIONTOKEN_H

//Qt includes
#include <QFuture>
#include <QPointer>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"

class cwLocalProjectionManager;

/**
 * Where a GIS layer gets the frame it loads into, and when that frame can be
 * trusted. Both answers arrive as one future: it carries the frame, and it
 * finishes once the frame has stopped moving.
 *
 * Every layer asks the same question at the same moment — just before it
 * decodes — and asks it through a token rather than through a pointer to the
 * manager, so the only thing a layer can do to the local projection is wait for
 * it. It has to be asked rather than pushed: a layer starts reading before the
 * model announces it as a row, which is before anything could have handed it
 * anything.
 *
 * Default-constructed it stands for a layer outside a project: there is no
 * frame to wait for, and the cloud loads in its own coordinates.
 */
class CAVEWHERE_LIB_EXPORT cwLocalProjectionToken
{
public:
    cwLocalProjectionToken() = default;
    explicit cwLocalProjectionToken(cwLocalProjectionManager* manager);

    //! The frame to load into, as a future that finishes once the frame has
    //! stopped moving. See cwLocalProjectionManager::frameFuture().
    QFuture<QString> frameFuture() const;

private:
    QPointer<cwLocalProjectionManager> m_manager;
};

#endif // CWLOCALPROJECTIONTOKEN_H
