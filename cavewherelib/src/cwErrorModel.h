
/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWERRORMODEL_H
#define CWERRORMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "cwError.h"
#include "cwGlobals.h"
class cwErrorListModel;

class CAVEWHERE_LIB_EXPORT cwErrorModel : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ErrorModel)

    Q_PROPERTY(int fatalCount READ fatalCount NOTIFY fatalCountChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY warningCountChanged)
    Q_PROPERTY(int ignoredWarningCount READ ignoredWarningCount NOTIFY ignoredWarningCountChanged)
    Q_PROPERTY(cwErrorListModel* errors READ errors CONSTANT)

public:
    cwErrorModel(QObject* parent = nullptr);
    virtual ~cwErrorModel();

    int fatalCount() const;
    int warningCount() const;
    int ignoredWarningCount() const;

    void setParentModel(cwErrorModel* parent);
    cwErrorModel *parentModel() const;

    cwErrorListModel* errors() const;

    QList<cwErrorModel*> childModels() const;

    QStringList toStringList() const;

signals:
    void fatalCountChanged();
    void warningCountChanged();
    void ignoredWarningCountChanged();

    void childModelAdded(cwErrorModel* child);
    void childModelRemoved(cwErrorModel* child);

private:
    mutable int m_fatalCount; //!< Cached count of fatal errors
    mutable int m_warningCount; //!< Cached count of warnings
    mutable int m_ignoredWarningCount; //!< Cached count of ignored warnings

    mutable bool m_countsUpToDate;

    cwErrorListModel* m_errors;
    QList<cwErrorModel*> m_childModels;

    QPointer<cwErrorModel> m_parent;

    void addChildModel(cwErrorModel* model);
    void removeChildModel(cwErrorModel* model);

    void updateFatalAndWarningCount() const;
private slots:
    void makeCountDirty();
    void makeFatalDirty();
    void makeWarningDirty();
    void checkForCountChanged(QModelIndex topLeft, QModelIndex bottomRight, QVector<int> roles);

};







#endif // CWERRORMODEL_H
