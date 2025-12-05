#ifndef CWLEADCSVEXPORTER_H
#define CWLEADCSVEXPORTER_H

//Qt includes
#include <QObject>
#include <QAbstractItemModel>
#include <QUrl>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwFutureManagerToken.h"
#include "cwLeadModel.h"

class cwLeadModel;

class CAVEWHERE_LIB_EXPORT cwLeadCSVExporter : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LeadCSVExporter)

    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(cwLeadModel* leadModel READ leadModel WRITE setLeadModel NOTIFY leadModelChanged)
    Q_PROPERTY(cwFutureManagerToken futureManagerToken READ futureManagerToken WRITE setFutureManagerToken NOTIFY futureManagerTokenChanged)

public:
    explicit cwLeadCSVExporter(QObject* parent = nullptr);

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model);

    cwLeadModel* leadModel() const;
    void setLeadModel(cwLeadModel* leadModel);

    cwFutureManagerToken futureManagerToken() const;
    void setFutureManagerToken(cwFutureManagerToken token);

    Q_INVOKABLE void exportToFile(const QUrl& fileUrl);

signals:
    void modelChanged();
    void leadModelChanged();
    void futureManagerTokenChanged();

private:
    QAbstractItemModel* mModel = nullptr;
    cwLeadModel* mLeadModel = nullptr;
    cwFutureManagerToken mFutureToken;

    cwLeadModel* resolvedLeadModel() const;
};

#endif // CWLEADCSVEXPORTER_H
