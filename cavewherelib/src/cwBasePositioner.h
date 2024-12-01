/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBasePositioner_H
#define CWBasePositioner_H

//Qt includes
#include <QQuickItem>
#include <QVector3D>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwBasePositioner : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BasePositioner)

    Q_PROPERTY(QQuickItem* transformTarget READ transformTarget WRITE setTransformTarget NOTIFY transformTargetChanged)

public:
    explicit cwBasePositioner(QQuickItem *parent = nullptr);

    QQuickItem* transformTarget() const;
    void setTransformTarget(QQuickItem* transformTarget);

signals:
    void position3DChanged();
    void transformTargetChanged();

private:
    QQuickItem* m_TransformTarget;
};

/**
 * Gets transformTarget
 */
inline QQuickItem* cwBasePositioner::transformTarget() const {
    return m_TransformTarget;
}

#endif // CWBasePositioner_H
