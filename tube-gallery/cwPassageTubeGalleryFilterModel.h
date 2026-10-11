/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEGALLERYFILTERMODEL_H
#define CWPASSAGETUBEGALLERYFILTERMODEL_H

//Our includes
#include "cwPassageTubeGalleryModel.h"

//Qt includes
#include <QQmlEngine>
#include <QSortFilterProxyModel>

// The gallery list as shown: rows whose name or tags contain filterText, in
// catalog order, with the failing ones first when failingFirst is set.
class cwPassageTubeGalleryFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PassageTubeGalleryFilterModel)

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool failingFirst READ failingFirst WRITE setFailingFirst NOTIFY failingFirstChanged)

public:
    explicit cwPassageTubeGalleryFilterModel(QObject* parent = nullptr);

    QString filterText() const;
    void setFilterText(const QString& filterText);

    bool failingFirst() const;
    void setFailingFirst(bool failingFirst);

    Q_INVOKABLE int sourceRow(int proxyRow) const;

signals:
    void filterTextChanged();
    void failingFirstChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_filterText;
    bool m_failingFirst = false;
};

#endif // CWPASSAGETUBEGALLERYFILTERMODEL_H
