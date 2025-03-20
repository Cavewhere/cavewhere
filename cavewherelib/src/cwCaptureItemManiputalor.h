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
#include "cwSelectionManager.h"
class cwCaptureItem;

class cwCaptureItemManiputalor : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CaptureItemManiputalor)

    Q_PROPERTY(cwCaptureManager* manager READ manager WRITE setManager NOTIFY managerChanged)
    Q_PROPERTY(cwQuickSceneView* view READ view WRITE setView NOTIFY viewChanged FINAL)
    Q_PROPERTY(QQuickItem* selectedItem READ selectedItem WRITE setSelectedItem NOTIFY selectedItemChanged FINAL)

    // Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager NOTIFY selectionManagerChanged FINAL)

public:
    explicit cwCaptureItemManiputalor(QQuickItem *parent = 0);

    cwCaptureManager* manager() const;
    void setManager(cwCaptureManager* manager);

    cwQuickSceneView *view() const;
    void setView(cwQuickSceneView *newView);

    Q_INVOKABLE void select(cwCaptureItem* capture);

    // cwSelectionManager *selectionManager() const { return m_selectionManager; }

    // Q_INVOKABLE QQuickItem* visualItem(cwCaptureItem* capture) const { return CaptureToQuickItem.value(capture, nullptr); }

    QQuickItem* selectedItem() const;
    void setSelectedItem(QQuickItem *newSelectedItem);

signals:
    void managerChanged();
    void viewChanged();
    void selectionManagerChanged();

    void selectedItemChanged();

public slots:

private slots:
    void fullUpdate();
    void insertItems(const QModelIndex& parent, int start, int end);
    void removeItems(const QModelIndex& parent, int start, int end);
    void managerDestroyed();

private:
    cwSelectionManager* m_selectionManager;

    QPointer<cwCaptureManager> Manager; //!<
    QPointer<cwQuickSceneView> m_view;

    //The lookups to convert between catpures and items
    QHash<cwCaptureItem*, QQuickItem*> CaptureToQuickItem;
    QHash<QQuickItem*, cwCaptureItem*> QuickItemToCapture;

    QQmlComponent* InteractionComponent;

    double PaperToScreenScale = 1.0; //Converts paper coordinates into screen coordinates
    QPointF SceneOffset;

    QQuickItem* createInteractionItem(const QVariantMap& requiredProperties);
    void clear();

    cwCaptureItem* captureItem(int i) const;
};

#endif // CWCAPTURELAYERMANIPUTALOR_H
