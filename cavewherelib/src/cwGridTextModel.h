/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGRIDTEXTMODEL_H
#define CWGRIDTEXTMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QColor>
#include <QConcatenateTablesProxyModel>
#include <QFont>
#include <QPointF>
#include <QQmlEngine>
#include <QRectF>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwGridTextModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GridTextModel)

public:
    struct TextRow
    {
        QString text;
        // Top-left corner of the label's rectangle in *scene* coordinates (world
        // meters in the sketch pipeline). The painter maps `bounds` through the
        // world-to-screen transform and derives the baseline anchor in screen
        // space, which is the only way to handle Y-flipped mapMatrixes (the
        // model can't know the caller's Y orientation).
        QRectF  bounds;
        // Legacy mirror of `bounds.topLeft()` — kept so existing
        // PositionRole QML consumers keep working. Painters should use
        // `bounds` directly.
        QPointF position;
        QFont   font;
        QColor  fillColor;
        QColor  strokeColor;

        bool operator==(const TextRow &other) const {
            return text == other.text &&
                   bounds == other.bounds &&
                   position == other.position &&
                   font == other.font &&
                   fillColor == other.fillColor &&
                   strokeColor == other.strokeColor;
        }

        bool operator!=(const TextRow &other) const { return !(*this == other); }
    };

    enum TextRoles
    {
        TextRole = Qt::UserRole + 1,
        PositionRole,
        FontRole,
        FillColorRole,
        StrokeColorRole
    };

    explicit cwGridTextModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;
    static QHash<int, QByteArray> staticRoleNames();

    Q_INVOKABLE void addText(int rowIndex, const TextRow &row);
    Q_INVOKABLE void addText(int rowIndex, const QVector<TextRow> &rows);
    Q_INVOKABLE void removeText(const QModelIndex &index);
    Q_INVOKABLE void removeText(const QModelIndex &index, int count);
    Q_INVOKABLE void setRows(const QVector<TextRow> &rows);
    Q_INVOKABLE void replaceRows(const QVector<TextRow> &rows);
    Q_INVOKABLE void clear();

    QVector<TextRow> rows() const { return m_rows; }

private:
    QVector<TextRow> m_rows;
};

class CAVEWHERE_LIB_EXPORT cwGridTextModelConcatenate : public QConcatenateTablesProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GridTextModelConcatenate)

public:
    explicit cwGridTextModelConcatenate(QObject *parent = nullptr)
        : QConcatenateTablesProxyModel(parent) {}

    QHash<int, QByteArray> roleNames() const override {
        return cwGridTextModel::staticRoleNames();
    }
};

#endif // CWGRIDTEXTMODEL_H
