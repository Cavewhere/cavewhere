/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPositionItem.h"

cwPositionItem::cwPositionItem(QQuickItem *parent) :
    QQuickItem(parent)
{

}

void cwPositionItem::setPosition3D(const QVector3D &position3D) {
    if(m_position3D == position3D) {
        return;
    }
    m_position3D = position3D;
    emit position3DChanged();
}

void cwPositionItem::setInFrustum(bool inFrustum) {
    if(m_inFrustum == inFrustum) {
        return;
    }
    m_inFrustum = inFrustum;
    emit inFrustumChanged();
}
