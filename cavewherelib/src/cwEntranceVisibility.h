/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWENTRANCEVISIBILITY_H
#define CWENTRANCEVISIBILITY_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QUuid>

//Our includes
#include "cwGlobals.h"
#include "cwVisibilityProxy.h"
class cwEntranceView;

/**
 * \brief The setVisible() target for one fix's entrance keyword item.
 *
 * cwKeywordVisibility calls setVisible() on this object when keyword filters
 * change. It forwards to cwEntranceView::setFixKeywordVisible(), which shows or
 * hides that fix's pin, text and tap target. It holds the fix id, the key the
 * view tracks its entrances by, so it stays valid while the view rebuilds.
 */
class CAVEWHERE_LIB_EXPORT cwEntranceVisibility : public cwVisibilityProxy
{
    Q_OBJECT

public:
    cwEntranceVisibility(cwEntranceView* view, const QUuid& fixId, QObject* parent = nullptr);

protected:
    void applyVisible(bool visible) override;

private:
    QPointer<cwEntranceView> m_view;
    QUuid m_fixId;
};

#endif // CWENTRANCEVISIBILITY_H
