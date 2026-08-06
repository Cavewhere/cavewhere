/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLEADVISIBILITY_H
#define CWLEADVISIBILITY_H

//Qt includes
#include <QObject>
#include <QPointer>

//Our includes
#include "cwGlobals.h"
#include "cwVisibilityProxy.h"
class cwLeadView;
class cwScrap;

/**
 * \brief The setVisible() target for a scrap's lead keyword item.
 *
 * cwKeywordVisibility calls setVisible() on this object when keyword filters
 * change. It forwards to cwLeadView::setScrapKeywordVisible(), which toggles
 * every LeadPoint belonging to the scrap — the per-scrap analogue of
 * cwLinePlotTripVisibility. It holds the cwScrap (not the LeadPoint list, which
 * is rebuilt as leads come and go) so it stays valid across those rebuilds.
 */
class CAVEWHERE_LIB_EXPORT cwLeadVisibility : public cwVisibilityProxy
{
    Q_OBJECT

public:
    cwLeadVisibility(cwLeadView* view, cwScrap* scrap, QObject* parent = nullptr);

    cwScrap* scrap() const { return m_scrap; }

protected:
    void applyVisible(bool visible) override;

private:
    QPointer<cwLeadView> m_view;
    QPointer<cwScrap> m_scrap;
};

#endif // CWLEADVISIBILITY_H
