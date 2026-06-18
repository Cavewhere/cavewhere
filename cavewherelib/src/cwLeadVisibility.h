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
class CAVEWHERE_LIB_EXPORT cwLeadVisibility : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

public:
    cwLeadVisibility(cwLeadView* view, cwScrap* scrap, QObject* parent = nullptr);

    bool isVisible() const { return m_visible; }
    cwScrap* scrap() const { return m_scrap; }

public slots:
    void setVisible(bool visible);

signals:
    void visibleChanged();

private:
    QPointer<cwLeadView> m_view;
    QPointer<cwScrap> m_scrap;
    bool m_visible = true;
};

#endif // CWLEADVISIBILITY_H
