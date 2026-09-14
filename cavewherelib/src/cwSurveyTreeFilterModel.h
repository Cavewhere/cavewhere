/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYTREEFILTERMODEL_H
#define CWSURVEYTREEFILTERMODEL_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QString>

/**
 * The name filter over cwSurveyTreeModel: a row survives when its own name, or
 * some descendant's name, holds filterText() as a case-insensitive substring.
 * Rows keep the order their node lists them in — sorting stays off.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyTreeFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyTreeFilterModel)

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit cwSurveyTreeFilterModel(QObject* parent = nullptr);

    QString filterText() const;
    void setFilterText(const QString& filterText);

signals:
    void filterTextChanged();

private:
    QString m_filterText;
};

#endif // CWSURVEYTREEFILTERMODEL_H
