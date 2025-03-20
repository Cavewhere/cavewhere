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
class cwLeadView : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LeadView)

    Q_PROPERTY(cwRegionTreeModel* regionModel READ regionModel WRITE setRegionModel NOTIFY regionModelChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)


public:
    cwLeadView(QQuickItem* parent = 0);
    ~cwLeadView();

    cwRegionTreeModel* regionModel() const;
    void setRegionModel(cwRegionTreeModel* regionModel);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

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
        int scrapLeadId;
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

#endif // CWLEADVIEW_H
