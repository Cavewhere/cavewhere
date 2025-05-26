#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QPointF>
#include <QFont>
#include <QColor>

namespace cwSketch {

class TextModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct TextRow
    {
        QString text;
        QPointF position;
        QFont font;
        QColor fillColor;
        QColor strokeColor;
    };

    enum TextRoles
    {
        TextRole = Qt::UserRole + 1,
        PositionRole,
        FontRole,
        FillColorRole,
        StrokeColorRole
    };

    explicit TextModel(QObject* parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addText(const QModelIndex& index, const TextRow& row);
    Q_INVOKABLE void removeText(const QModelIndex& index);
    Q_INVOKABLE void setRows(const QVector<TextRow>& rows);
    Q_INVOKABLE void clear();

    QVector<TextRow> rows() const { return m_rows; }

private:
    QVector<TextRow> m_rows;
};

};
