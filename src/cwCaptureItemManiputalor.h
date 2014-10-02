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

//Our includes
class cwCaptureManager;
class cwCaptureItem;

class cwCaptureItemManiputalor : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(cwCaptureManager* manager READ manager WRITE setManager NOTIFY managerChanged)

public:
    explicit cwCaptureItemManiputalor(QQuickItem *parent = 0);

    cwCaptureManager* manager() const;
    void setManager(cwCaptureManager* manager);

signals:
    void managerChanged();

public slots:

private slots:
    void fullUpdate();
    void insertItems(const QModelIndex& parent, int start, int end);
    void removeItems(const QModelIndex& parent, int start, int end);
    void updateTransform();

private:
    QPointer<cwCaptureManager> Manager; //!<

    //The lookups to convert between catpures and items
    QHash<cwCaptureItem*, QQuickItem*> CaptureToQuickItem;
    QHash<QQuickItem*, cwCaptureItem*> QuickItemToCapture;

    QQmlComponent* InteractionComponent;

    double PaperToScreenScale; //Converts paper coordinates into screen coordinates
    QPointF SceneOffset;

    QQuickItem* createInteractionItem();
    void clear();

    void updateItemTransform(QQuickItem* item);

    cwCaptureItem* captureItem(int i) const;

};

#endif // CWCAPTURELAYERMANIPUTALOR_H
