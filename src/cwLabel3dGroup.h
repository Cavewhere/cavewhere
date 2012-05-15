#ifndef CWLABEL3DGROUP_H
#define CWLABEL3DGROUP_H

//Qt includes
#include <QObject>
class QQuickItem;

//Our includes
#include "cwLabel3dItem.h"
class cwLabel3dView;

class cwLabel3dGroup : public QObject
{
    friend class cwLabel3dView;

    Q_OBJECT
public:
    explicit cwLabel3dGroup(cwLabel3dView *parent = 0);
    ~cwLabel3dGroup();

    void setParentView(cwLabel3dView* parent);

    void setLabels(QList<cwLabel3dItem> labels);
    QList<cwLabel3dItem> labels() const;

    void clear();

signals:
    
public slots:

private:
    cwLabel3dView* ParentView;
    QList<cwLabel3dItem> Labels;
    QList<QQuickItem*> LabelItems;
    
};

#endif // CWLABEL3DGROUP_H
