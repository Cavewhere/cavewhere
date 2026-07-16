/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwEDLSettings.h"

cwEDLSettings::cwEDLSettings(QObject* parent) :
    QObject(parent)
{
}

void cwEDLSettings::setStrength(float strength)
{
    if (m_data.strength == strength) {
        return;
    }
    m_data.strength = strength;
    emit strengthChanged();
    emit changed();
}

void cwEDLSettings::setMaxDarken(float maxDarken)
{
    if (m_data.maxDarken == maxDarken) {
        return;
    }
    m_data.maxDarken = maxDarken;
    emit maxDarkenChanged();
    emit changed();
}

void cwEDLSettings::setRadius(float radius)
{
    if (m_data.radiusPx == radius) {
        return;
    }
    m_data.radiusPx = radius;
    emit radiusChanged();
    emit changed();
}
