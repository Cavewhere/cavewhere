/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPalette.h"

cwSymbologyPalette::cwSymbologyPalette(QObject *parent) :
    QObject(parent)
{
}

void cwSymbologyPalette::setData(const cwSymbologyPaletteData &data)
{
    if (m_data == data) { return; }
    m_data = data;
    emit dataChanged();
}

void cwSymbologyPalette::setWritable(bool writable)
{
    if (m_writable == writable) { return; }
    m_writable = writable;
    emit writableChanged();
}
