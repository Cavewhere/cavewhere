//Our includes
#include "cwLeadCSVExporter.h"
#include "cwLeadModel.h"
#include "cwConcurrent.h"
#include "cwFuture.h"
#include "cwGlobals.h"
#include "cwUnits.h"

//Qt includes
#include <QAbstractProxyModel>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QLocale>
#include <QStringList>
#include <QSizeF>
#include <QStringConverter>
#include <QList>

//Std includes
#include <cmath>

namespace {

QString escapeCsv(const QString& value)
{
    QString escaped = value;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    if(escaped.contains(',') || escaped.contains('"') || escaped.contains('\n')) {
        escaped.prepend('"');
        escaped.append('"');
    }
    return escaped;
}

QString numberString(double value)
{
    if(!std::isfinite(value)) {
        return QString();
    }
    return QLocale::c().toString(value, 'g', 12);
}

} // namespace

cwLeadCSVExporter::cwLeadCSVExporter(QObject *parent) :
    QObject(parent)
{
}

QAbstractItemModel *cwLeadCSVExporter::model() const
{
    return mModel;
}

void cwLeadCSVExporter::setModel(QAbstractItemModel *model)
{
    if(mModel != model) {
        mModel = model;
        emit modelChanged();
    }
}

cwLeadModel *cwLeadCSVExporter::leadModel() const
{
    return mLeadModel;
}

void cwLeadCSVExporter::setLeadModel(cwLeadModel *leadModel)
{
    if(mLeadModel != leadModel) {
        mLeadModel = leadModel;
        emit leadModelChanged();
    }
}

cwFutureManagerToken cwLeadCSVExporter::futureManagerToken() const
{
    return mFutureToken;
}

void cwLeadCSVExporter::setFutureManagerToken(cwFutureManagerToken token)
{
    if(mFutureToken != token) {
        mFutureToken = token;
        emit futureManagerTokenChanged();
    }
}

cwLeadModel *cwLeadCSVExporter::resolvedLeadModel() const
{
    if(mLeadModel) {
        return mLeadModel;
    }

    if(auto leadModel = qobject_cast<cwLeadModel*>(mModel)) {
        return leadModel;
    }

    auto proxy = qobject_cast<QAbstractProxyModel*>(mModel);
    while(proxy) {
        auto source = proxy->sourceModel();
        if(auto leadModel = qobject_cast<cwLeadModel*>(source)) {
            return leadModel;
        }
        proxy = qobject_cast<QAbstractProxyModel*>(source);
    }

    return nullptr;
}

void cwLeadCSVExporter::exportToFile(const QUrl &fileUrl)
{
    if(mModel == nullptr) {
        return;
    }

    QString filename = cwGlobals::convertFromURL(fileUrl.toString());
    if(filename.isEmpty()) {
        return;
    }
    filename = cwGlobals::addExtension(filename, "csv");

    struct LeadRow {
        bool completed = false;
        QString nearestStation;
        QString tripName;
        QSizeF size;
        QString sizeUnits;
        double distanceToReference = 0.0;
        QString description;
    };

    QList<LeadRow> rows;
    rows.reserve(mModel->rowCount());
    for(int row = 0; row < mModel->rowCount(); ++row) {
        QModelIndex index = mModel->index(row, 0);
        if(!index.isValid()) {
            continue;
        }

        LeadRow rowData;
        rowData.completed = mModel->data(index, cwLeadModel::LeadCompleted).toBool();
        rowData.nearestStation = mModel->data(index, cwLeadModel::LeadNearestStation).toString();
        rowData.tripName = mModel->data(index, cwLeadModel::LeadTrip).toString();
        rowData.size = mModel->data(index, cwLeadModel::LeadSize).toSizeF();
        rowData.sizeUnits = cwUnits::unitName(static_cast<cwUnits::LengthUnit>(mModel->data(index, cwLeadModel::LeadUnits).toInt()));
        rowData.distanceToReference = mModel->data(index, cwLeadModel::LeadDistanceToReferanceStation).toDouble();
        rowData.description = mModel->data(index, cwLeadModel::LeadDesciption).toString();

        rows.append(rowData);
    }

    QString referenceStation;
    if(auto leadModel = resolvedLeadModel()) {
        referenceStation = leadModel->referanceStation();
    }

    QString jobName = QStringLiteral("Export Leads CSV");
    auto future = cwConcurrent::run([rows, filename, referenceStation]() {
        QFile file(filename);
        if(!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "Couldn't open lead export file:" << filename;
            return;
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream.setLocale(QLocale::c());

        QString distanceHeader = referenceStation.isEmpty()
                ? QStringLiteral("Distance to Reference Station (m)")
                : QStringLiteral("Distance to %1 (m)").arg(referenceStation);

        QStringList headers = {
            QStringLiteral("Completed"),
            QStringLiteral("Nearest Station"),
            QStringLiteral("Trip"),
            QStringLiteral("Size Width"),
            QStringLiteral("Size Height"),
            QStringLiteral("Size Units"),
            distanceHeader,
            QStringLiteral("Description")
        };

        for(QString& header : headers) {
            header = escapeCsv(header);
        }

        stream << headers.join(',') << "\n";

        for(const LeadRow& row : rows) {
            QStringList columns;
            columns << (row.completed ? QStringLiteral("Yes") : QStringLiteral("No"));
            columns << escapeCsv(row.nearestStation);
            columns << escapeCsv(row.tripName);
            columns << numberString(row.size.width());
            columns << numberString(row.size.height());
            columns << escapeCsv(row.sizeUnits);
            columns << numberString(row.distanceToReference);
            columns << escapeCsv(row.description);
            stream << columns.join(',') << "\n";
        }
    });

    if(mFutureToken.isValid()) {
        mFutureToken.addJob(cwFuture(future, jobName));
    }
}
