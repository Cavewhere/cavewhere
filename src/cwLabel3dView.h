#ifndef CWLABEL3DVIEW_H
#define CWLABEL3DVIEW_H

//Qt includes
#include <QQuickItem>
#include <QQmlComponent>
#include <QMatrix4x4>

//Our includes
#include "cwLabel3dItem.h"
#include "cwCollisionRectKdTree.h"
class cwCamera;
class cwLabel3dGroup;

class cwLabel3dView : public QQuickItem
{
    friend class cwLabel3dGroup;

    Q_OBJECT

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
    /**
      \brief This is used to render text labels correctly

      This will transform the points in a multi thread manor.
      */
    class TransformPoint {
    public:
        typedef QVector3D result_type;

        TransformPoint(QMatrix4x4 modelViewProjection, QRect viewport) {
            ModelViewProjection = modelViewProjection;
            Viewport = viewport;
        }

        /**
          \brief Transforms the point
          */
        void operator()(cwLabel3dItem& label);

    private:
        QMatrix4x4 ModelViewProjection;
        QRect Viewport;
    };

    QSet<cwLabel3dGroup*> LabelGroups;

    //For rendering labels
    QQmlComponent* Component;
    cwCamera* Camera; //!<
    cwCollisionRectKdTree LabelKdTree;

    void updateGroup(cwLabel3dGroup* group);
    void updateGroupPositions(cwLabel3dGroup* group);
private slots:
    void updatePositions();

};


/**
Gets camera
*/
inline cwCamera* cwLabel3dView::camera() const {
    return Camera;
}
#endif // CWLABEL3DVIEW_H
