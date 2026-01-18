/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTableStaticColumn.h"

cwTableStaticColumn::cwTableStaticColumn(QObject* parent) :
    QObject(parent)
{
}

int cwTableStaticColumn::columnWidth() const
{
    return m_columnWidth;
}

void cwTableStaticColumn::setColumnWidth(int columnWidth)
{
    if (m_columnWidth == columnWidth) {
        return;
    }

    m_columnWidth = columnWidth;
    emit columnWidthChanged();
}

QString cwTableStaticColumn::text() const
{
    return m_text;
}

void cwTableStaticColumn::setText(const QString& text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    emit textChanged();
}

int cwTableStaticColumn::sortRole() const
{
    return m_sortRole;
}

void cwTableStaticColumn::setSortRole(int sortRole)
{
    if (m_sortRole == sortRole) {
        return;
    }

    m_sortRole = sortRole;
    emit sortRoleChanged();
}
