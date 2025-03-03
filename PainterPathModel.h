#ifndef PAINTERPATHMODEL_H
#define PAINTERPATHMODEL_H

#include <QAbstractListModel>
#include <QPainterPath>
#include <QPointer>
#include <QObjectBindableProperty>

//Our includes
#include "PenLineModel.h" // Ensure this header is available


/**
 * @brief The PainterPathModel class
 */
class PainterPathModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(PenLineModel* penLineModel READ penLineModel WRITE setPenLineModel NOTIFY penLineModelChanged)
    Q_PROPERTY(int activeLineIndex READ activeLineIndex WRITE setActiveLineIndex NOTIFY activeLineIndexChanged BINDABLE bindableActiveLineIndex)
    // Q_PROPERTY(QPainterPath activePath READ activePath WRITE setActivePath NOTIFY activePathChanged FINAL)
    // Q_PROPERTY(QPainterPath finalPath READ finalPath WRITE setFinalPath NOTIFY finalPathChanged FINAL)


public:
    explicit PainterPathModel(QObject *parent = nullptr);

    enum Roles {
        PainterPathRole = Qt::UserRole + 1,
    };

    enum RowType {
        FinishedPath = 0,
        ActivePath
    };

    //There's the active which is 0, and
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    PenLineModel* penLineModel() const;
    void setPenLineModel(PenLineModel* penLineModel);

    int activeLineIndex() const { return m_activeLineIndex.value(); }
    void setActiveLineIndex(const int& activeLineIndex) { m_activeLineIndex = activeLineIndex; }
    QBindable<int> bindableActiveLineIndex() { return &m_activeLineIndex; }

    Q_INVOKABLE QPainterPath debugPainterPath() const {
        QPainterPath debugPath;
        debugPath.addRect(10, 10, 50, 50);
        return debugPath;
    }



signals:
    void penLineModelChanged();
    void activeLineIndexChanged();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(PainterPathModel, int, m_activeLineIndex, -1, &PainterPathModel::activeLineIndexChanged);

    QPainterPath m_finishedPath; //At index 0
    QPainterPath m_activePath; //At index 1
    QPointer<PenLineModel> m_penLineModel;
    int m_previousActivePath = -1;

    void addLinePolygon(QPainterPath& path, int modelRow);

    QLineF perpendicularLineAt(const QVector<PenPoint> &points, int index) const;

    void updateActivePath();
    void rebuildFinalPath();

};

#endif // PAINTERPATHMODEL_H
