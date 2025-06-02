#pragma once

#include <QAbstractListModel>
#include <QConcatenateTablesProxyModel>
#include <QVector>
#include <QPointF>
#include <QFont>
#include <QColor>
#include <QQmlEngine>

namespace cwSketch {

class TextModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    struct TextRow
    {
        QString text;
        QPointF position;
        QFont font;
        QColor fillColor;
        QColor strokeColor;

        bool operator==(const TextRow& other) const {
            return text == other.text &&
                   position == other.position &&
                   font == other.font &&
                   fillColor == other.fillColor &&
                   strokeColor == other.strokeColor;
        }

        bool operator!=(const TextRow& other) const {
            return !(*this == other);
        }
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
    static QHash<int, QByteArray> staticRoleNames();

    Q_INVOKABLE void addText(int rowIndex, const TextRow& row);
    Q_INVOKABLE void addText(int rowIndex, const QVector<TextRow>& rows);
    Q_INVOKABLE void removeText(const QModelIndex& index);
    Q_INVOKABLE void removeText(const QModelIndex& index, int count);
    Q_INVOKABLE void setRows(const QVector<TextRow>& rows);
    Q_INVOKABLE void replaceRows(const QVector<TextRow>& rows);
    Q_INVOKABLE void clear();

    QVector<TextRow> rows() const { return m_rows; }

private:
    QVector<TextRow> m_rows;
};



class TextModelConcatenate : public QConcatenateTablesProxyModel {
    Q_OBJECT
    QML_ELEMENT
public:
    TextModelConcatenate(QObject* parent) : QConcatenateTablesProxyModel(parent) {}

    QHash<int, QByteArray> roleNames() const {
        return TextModel::staticRoleNames();
    }
};
};
