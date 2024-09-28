/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

//Our includes
#include "cwSortFilterProxyModel.h"

//Qt includes
#include <QtDebug>
#include <QtQml>

cwSortFilterProxyModel::cwSortFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SIGNAL(countChanged()));
    connect(this, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SIGNAL(countChanged()));
}

int cwSortFilterProxyModel::count() const
{
    return rowCount();
}

QObject *cwSortFilterProxyModel::source() const
{
    return sourceModel();
}

void cwSortFilterProxyModel::setSource(QObject *source)
{
    setSourceModel(qobject_cast<QAbstractItemModel *>(source));
}

QByteArray cwSortFilterProxyModel::sortRole() const
{
    return roleNames().value(QSortFilterProxyModel::sortRole());
}

void cwSortFilterProxyModel::setSortRole(const QByteArray &role)
{
    QSortFilterProxyModel::setSortRole(roleKey(role));
}

void cwSortFilterProxyModel::setSortOrder(Qt::SortOrder order)
{
    QSortFilterProxyModel::sort(0, order);
}

QByteArray cwSortFilterProxyModel::filterRole() const
{
    return roleNames().value(QSortFilterProxyModel::filterRole());
}

void cwSortFilterProxyModel::setFilterRole(const QByteArray &role)
{
    QSortFilterProxyModel::setFilterRole(roleKey(role));
}

QString cwSortFilterProxyModel::filterString() const
{
    return filterRegularExpression().pattern();
}

void cwSortFilterProxyModel::setFilterString(const QString &filter)
{
    setFilterRegularExpression(QRegularExpression(filter, filterRegularExpression().patternOptions()));
}

// cwSortFilterProxyModel::FilterSyntax cwSortFilterProxyModel::filterSyntax() const
// {
//     return static_cast<FilterSyntax>(filterRegExp().patternSyntax());
// }

// void cwSortFilterProxyModel::setFilterSyntax(cwSortFilterProxyModel::FilterSyntax syntax)
// {
//     setFilterRegExp(QRegExp(filterString(), filterCaseSensitivity(), static_cast<QRegExp::PatternSyntax>(syntax)));
// }

QJSValue cwSortFilterProxyModel::get(int idx) const
{
    QJSEngine *engine = qmlEngine(this);
    QJSValue value = engine->newObject();
    if (idx >= 0 && idx < count()) {
        QHash<int, QByteArray> roles = roleNames();
        QHashIterator<int, QByteArray> it(roles);
        while (it.hasNext()) {
            it.next();
            value.setProperty(QString::fromUtf8(it.value()), data(index(idx, 0), it.key()).toString());
        }
    }
    return value;
}

QModelIndex cwSortFilterProxyModel::index(int row, int column, const QModelIndex &parent) const
{
    return QSortFilterProxyModel::index(row, column, parent);
}

QModelIndex cwSortFilterProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    return QSortFilterProxyModel::mapFromSource(sourceIndex);
}

QModelIndex cwSortFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    return QSortFilterProxyModel::mapToSource(proxyIndex);
}

QVariant cwSortFilterProxyModel::data(const QModelIndex &index, int role) const
{
    return QSortFilterProxyModel::data(index, role);
}

bool cwSortFilterProxyModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return QSortFilterProxyModel::setData(index, value, role);
}

int cwSortFilterProxyModel::roleKey(const QByteArray &role) const
{
    QHash<int, QByteArray> roles = roleNames();
    QHashIterator<int, QByteArray> it(roles);
    while (it.hasNext()) {
        it.next();
        if (it.value() == role)
            return it.key();
    }
    return -1;
}

QHash<int, QByteArray> cwSortFilterProxyModel::roleNames() const
{
    if (QAbstractItemModel *source = sourceModel())
        return source->roleNames();
    return QHash<int, QByteArray>();
}

bool cwSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    auto rx = filterRegularExpression();
    if (rx.pattern().isEmpty()) {
        return true;
    }
    QAbstractItemModel *model = sourceModel();
    if (filterRole().isEmpty()) {
        QHash<int, QByteArray> roles = roleNames();
        QHashIterator<int, QByteArray> it(roles);
        while (it.hasNext()) {
            it.next();
            QModelIndex sourceIndex = model->index(sourceRow, 0, sourceParent);
            QString key = model->data(sourceIndex, it.key()).toString();
            if (key.contains(rx))
                return true;
        }
        return false;
    }
    QModelIndex sourceIndex = model->index(sourceRow, 0, sourceParent);
    if (!sourceIndex.isValid())
        return true;
    QString key = model->data(sourceIndex, roleKey(filterRole())).toString();
    return key.contains(rx);
}
