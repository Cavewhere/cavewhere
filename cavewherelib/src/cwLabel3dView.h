/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLABEL3DVIEW_H
#define CWLABEL3DVIEW_H

//Qt includes
#include <QQuickItem>
#include <QQmlComponent>
#include <QMatrix4x4>
#include <QQmlEngine>

//Our includes
#include "cwLabel3dItem.h"
#include "cwCollisionRectKdTree.h"
class cwCamera;
class cwLabel3dGroup;

class cwLabel3dView : public QQuickItem
{
    friend class cwLabel3dGroup;

    Q_OBJECT
    QML_NAMED_ELEMENT(Label3dView)

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)

public:
    explicit cwLabel3dView(QQuickItem *parent = 0);
    ~cwLabel3dView();
    
    void addGroup(cwLabel3dGroup* group);
    void removeGroup(cwLabel3dGroup* group);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

signals:
    void cameraChanged();


public slots:
    
private:
    QSet<cwLabel3dGroup*> LabelGroups;

    //For rendering labels
    QQmlComponent* Component;
    QPointer<cwCamera> Camera; //!<
    cwCollisionRectKdTree LabelKdTree;

    void updateGroup(cwLabel3dGroup* group);
    void updateGroupPositions(cwLabel3dGroup* group);
private slots:
    void updatePositions();

};



#endif // CWLABEL3DVIEW_H
