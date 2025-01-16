/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWCAPTURELAYERMANIPUTALOR_H
#define CWCAPTURELAYERMANIPUTALOR_H

//Qt includes
#include <QQuickItem>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>

//Our includes
#include "cwCaptureManager.h"
#include "cwQuickSceneView.h"
class cwCaptureItem;

class cwCaptureItemManiputalor : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CaptureItemManiputalor)

    Q_PROPERTY(cwCaptureManager* manager READ manager WRITE setManager NOTIFY managerChanged)
    Q_PROPERTY(cwQuickSceneView* view READ view WRITE setView NOTIFY viewChanged FINAL)

public:
    explicit cwCaptureItemManiputalor(QQuickItem *parent = 0);

    cwCaptureManager* manager() const;
    void setManager(cwCaptureManager* manager);

    cwQuickSceneView *view() const;
    void setView(cwQuickSceneView *newView);

signals:
    void managerChanged();

    void viewChanged();

public slots:

private slots:
    void fullUpdate();
    void insertItems(const QModelIndex& parent, int start, int end);
    void removeItems(const QModelIndex& parent, int start, int end);
    void updateTransform();
    void managerDestroyed();

private:
    QPointer<cwCaptureManager> Manager; //!<
    QPointer<cwQuickSceneView> m_view;

    //The lookups to convert between catpures and items
    QHash<cwCaptureItem*, QQuickItem*> CaptureToQuickItem;
    QHash<QQuickItem*, cwCaptureItem*> QuickItemToCapture;

    QQmlComponent* InteractionComponent;

    double PaperToScreenScale; //Converts paper coordinates into screen coordinates
    QPointF SceneOffset;

    QQuickItem* createInteractionItem(const QVariantMap& requiredProperties);
    void clear();

    void updateItemTransform(QQuickItem* item);

    cwCaptureItem* captureItem(int i) const;

};

#endif // CWCAPTURELAYERMANIPUTALOR_H
