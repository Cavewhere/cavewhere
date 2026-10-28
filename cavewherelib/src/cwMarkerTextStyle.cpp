/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwMarkerTextStyle.h"

cwMarkerTextStyle::cwMarkerTextStyle(QObject* parent) :
    QObject(parent)
{
}

QString cwMarkerTextStyle::styleKey() const
{
    return m_styleKey;
}

void cwMarkerTextStyle::setStyleKey(const QString& styleKey)
{
    if(m_styleKey == styleKey) {
        return;
    }
    m_styleKey = styleKey;
    emit styleKeyChanged();
}

QString cwMarkerTextStyle::layerKey() const
{
    return m_layerKey;
}

void cwMarkerTextStyle::setLayerKey(const QString& layerKey)
{
    if(m_layerKey == layerKey) {
        return;
    }
    m_layerKey = layerKey;
    emit layerKeyChanged();
}

QFont cwMarkerTextStyle::font() const
{
    return m_font;
}

void cwMarkerTextStyle::setFont(const QFont& font)
{
    if(m_font == font) {
        return;
    }
    m_font = font;
    emit fontChanged();
}

QColor cwMarkerTextStyle::color() const
{
    return m_color;
}

void cwMarkerTextStyle::setColor(const QColor& color)
{
    if(m_color == color) {
        return;
    }
    m_color = color;
    emit colorChanged();
}

QColor cwMarkerTextStyle::outlineColor() const
{
    return m_outlineColor;
}

void cwMarkerTextStyle::setOutlineColor(const QColor& outlineColor)
{
    if(m_outlineColor == outlineColor) {
        return;
    }
    m_outlineColor = outlineColor;
    emit outlineColorChanged();
}
