/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWCAPTURESELECTIONMODEL_H
#define CWCAPTURESELECTIONMODEL_H

//Qt includes
#include <QObject>
#include <QList>
#include <QSet>

//Our includes
class cwCaptureItem;

class cwCaptureSelectionModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QList<cwCaptureItem*> selectedItems READ selectedItems NOTIFY selectedItemsChanged)


public:
    explicit cwCaptureSelectionModel(QObject *parent = 0);

    QList<cwCaptureItem*> selectedItems() const;

    Q_INVOKABLE void select(cwCaptureItem* item);
    Q_INVOKABLE void addToSelection(cwCaptureItem* item);
    Q_INVOKABLE void removeFromSelection(cwCaptureItem* item);
    Q_INVOKABLE void clear();

signals:
    void selectedItemsChanged();

public slots:

private:
    bool removeHelper(cwCaptureItem* item);

    QSet<cwCaptureItem*> SelectedItems; //!<

private slots:
    void removeDeleted(QObject* object);



};

/**
* @brief cwCaptureSelectionModel::selectedItems
* @return
*/
inline QList<cwCaptureItem*> cwCaptureSelectionModel::selectedItems() const {
    return SelectedItems.toList();
}

#endif // CWCAPTURESELECTIONMODEL_H
