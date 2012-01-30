#ifndef CWTREEVIEW_H
#define CWTREEVIEW_H

//Our includes
#include <cwRegionTreeModel.h>

//Qt includes
#include <QDeclarativeItem>
class QTreeView;
class QGraphicsProxyWidget;
class QAbstractItemModel;

class cwTreeView : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwRegionTreeModel* model READ model WRITE setModel NOTIFY modelChanged);

public:
    explicit cwTreeView(QDeclarativeItem *parent = 0);
    ~cwTreeView();

    void setModel(cwRegionTreeModel* model);
    cwRegionTreeModel* model() const;

    QTreeView* view() const;

signals:
    void modelChanged();

public slots:

private:
    QGraphicsProxyWidget* ProxyWidget;
    QTreeView* TreeView;

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);

};

/**
  \brief The view that this qml tree view shows
  */
inline QTreeView* cwTreeView::view() const {
    return TreeView;
}



#endif // CWTREEVIEW_H
