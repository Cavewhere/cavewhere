#ifndef SKETCHSHAPEPATH_H
#define SKETCHSHAPEPATH_H

//Qt includes
#include <QQmlEngine>
#include <QtQuickShapes/private/qquickshape_p.h>


class SketchShapePath : public QQuickShapePath
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QPainterPath painterPath READ path WRITE setPath REQUIRED)

public:
    SketchShapePath(QQuickItem* parent = nullptr);


    // void setPainterPath(const QPainterPath& path) {
    //     qDebug() << "Setting painter path:" << this;
    //     // setPath(path);

    //     QPainterPath debugPath;
    //     debugPath.addRect(10, 10, 50, 50);

    //     setPath(debugPath);
    // }

};

#endif // SKETCHSHAPEPATH_H
