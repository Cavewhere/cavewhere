/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLEADVIEW_H
#define CWLEADVIEW_H

//Qt includes
#include <QPointer>
#include <QModelIndex>
#include <QQmlEngine>

//Our includes
#include "cwAbstractPointManager.h"
#include "cwTransformUpdater.h"
class cwRegionTreeModel;
class cwScrap;
class cwScrapLeadView;
class cwCamera;

/**
 * @brief The cwLeadView class
 *
 * This shows all the leads from all the caves and all scraps. This class creates a cwScrapLeadView
 * for each scrap, and pass around the same transformation to all of them. It also sets the positionRole()
 * for each of the cwScrapLeadView to cwScrap::LeadPosition so it uses the global position of the
 * lead. This global position is calculated by the carpeting algorithm.
 */
//TODO: convert this class to us cwItem3DRepeater
class cwLeadView : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LeadView)

    Q_PROPERTY(cwRegionTreeModel* regionModel READ regionModel WRITE setRegionModel NOTIFY regionModelChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager CONSTANT)


public:
    cwLeadView(QQuickItem* parent = 0);
    ~cwLeadView();

    cwRegionTreeModel* regionModel() const;
    void setRegionModel(cwRegionTreeModel* regionModel);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    cwSelectionManager* selectionManager() const;

    Q_INVOKABLE void select(cwScrap* scarp, int index);

signals:
    void regionModelChanged();
    void cameraChanged();

private:
    QPointer<cwRegionTreeModel> RegionModel; //!<
    QPointer<cwCamera> Camera; //!<
    cwTransformUpdater* TransformUpdater;
    cwSelectionManager* SelectionMananger;

    struct ScrapEntry {
        int scrapLeadId = -1;
        QVector<QQuickItem*> items;
    };

    QHash<cwScrap*, ScrapEntry> m_leadItems;
    QQmlComponent* m_itemComponent = nullptr;
    int m_currentScrapId = 0;

    void addScrap(cwScrap* scrap);
    void removeScrap(cwScrap* scrap);

    QQuickItem *createItem();
    void createComponent();

    QString qmlSource() const;

private slots:
    void scrapsAdded(QModelIndex parent, int begin, int end);
    void scrapsRemoved(QModelIndex parent, int begin, int end);

};

/**
 * @brief cwLeadView::selectionManager
 * @return The selection manager that tracks the single selected lead, so lead
 * taps route through it (single selection) and a tap on empty space can clear it.
 */
inline cwSelectionManager* cwLeadView::selectionManager() const {
    return SelectionMananger;
}

#endif // CWLEADVIEW_H
