/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwBasePositioner.h"

cwBasePositioner::cwBasePositioner(QQuickItem *parent) :
    QQuickItem(parent)
{

}


void cwBasePositioner::setTransformTarget(QQuickItem* transformTarget) {
    if (m_TransformTarget != transformTarget) {
        m_TransformTarget = transformTarget;
        emit transformTargetChanged();
    }
}
