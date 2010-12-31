//Our includes
#include "cwTreeView.h"
#include "cwRegionTreeModel.h"

//Qt includes
#include <QGraphicsProxyWidget>
#include <QTreeView>
#include <QPushButton>
#include <QAbstractItemModel>

cwTreeView::cwTreeView(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{
    TreeView = new QTreeView();
    ProxyWidget = new QGraphicsProxyWidget(this);
    ProxyWidget->setWidget(TreeView);
}

cwTreeView::~cwTreeView() {
    TreeView->deleteLater();
}

/**
  Update geometry of the proxy widget
  */
void cwTreeView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) {
   qDebug() << "Geometry Changed" << newGeometry;
    QDeclarativeItem::geometryChanged(newGeometry, oldGeometry);
    ProxyWidget->resize(newGeometry.size());
}

/**
  \brief Sets the model then the views
  */
void cwTreeView::setModel(cwRegionTreeModel* newModel) {
    qDebug() << "Sets the model of the view:" << newModel;
    if(model() != newModel) {
        TreeView->setModel(newModel);
        emit modelChanged();
    }
}

/**
  \brief Gets the model of the view
  */
cwRegionTreeModel* cwTreeView::model() const {
   return static_cast<cwRegionTreeModel*>(TreeView->model());
}
