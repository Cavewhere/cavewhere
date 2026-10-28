/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLEADMARKERSOURCE_H
#define CWLEADMARKERSOURCE_H

//Qt includes
#include <QList>
#include <QModelIndex>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwMarkerSource.h"
class cwRegionTreeModel;
class cwScrap;

/**
 * @brief The lead markers: one marker per lead of every scrap under a note in
 * the region tree model, at the lead's cwScrap::LeadPosition.
 *
 * A scrap's leads are contiguous rows, in the order the scraps were added. Each
 * marker hands its LeadPoint delegate the scrap, the lead's pointIndex in the
 * scrap and the scrap's scrapId, and shares its scrap's keyword item (the
 * scrap's leadKeywordModel()).
 */
class CAVEWHERE_LIB_EXPORT cwLeadMarkerSource : public cwMarkerSource
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LeadMarkerSource)

    Q_PROPERTY(cwRegionTreeModel* regionModel READ regionModel WRITE setRegionModel NOTIFY regionModelChanged)

public:
    //! The atlas slots a lead MarkerView's sprites list declares, in this order.
    enum LeadSprite {
        OpenSprite = 0,
        CompletedSprite = 1
    };
    Q_ENUM(LeadSprite)

    explicit cwLeadMarkerSource(QObject* parent = nullptr);

    cwRegionTreeModel* regionModel() const;
    void setRegionModel(cwRegionTreeModel* regionModel);

    //! The row of @a scrap's lead @a leadIndex, or -1 when that lead has no marker.
    Q_INVOKABLE int row(cwScrap* scrap, int leadIndex) const;

    QPointF spriteAnchor() const override;
    float depthBias() const override;

signals:
    void regionModelChanged();

private:
    // A tracked scrap; its leads are the rows that follow the earlier scraps' leads.
    struct ScrapEntry {
        cwScrap* scrap = nullptr;
        int scrapLeadId = 0; //!< the LeadPoints' scrapId, 1 for the first scrap added
        int leadCount = 0;
    };

    QPointer<cwRegionTreeModel> m_regionModel;
    QList<ScrapEntry> m_scraps;
    int m_currentScrapId = 0;

    ScrapEntry trackScrap(cwScrap* scrap);
    void addScrap(cwScrap* scrap);
    void removeScrap(cwScrap* scrap);
    void connectScrap(cwScrap* scrap);

    int scrapIndex(const cwScrap* scrap) const;
    int firstRow(int scrapIndex) const;
    QList<cwMarker> leadMarkers(const ScrapEntry& entry, int first, int last) const;
    QList<cwScrap*> scrapsUnder(const QModelIndex& parent, int first, int last) const;

private slots:
    // A region-model reset (project load, git checkout) replaces every row
    // without per-row signals, so the scraps are cleared before it and added after.
    void clearAllScraps();
    void addAllScraps();
    void scrapsInserted(const QModelIndex& parent, int first, int last);
    void scrapsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void leadsInserted(int first, int last);
    void leadsRemoved(int first, int last);
    void leadsDataChanged(int first, int last, const QList<int>& roles);
    void leadsReset();
};

#endif // CWLEADMARKERSOURCE_H
