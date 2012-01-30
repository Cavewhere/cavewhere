//Our includes
#include "cwQMLWidget.h"

//Qt includes
#include <QWidget>
#include <QGraphicsProxyWidget>

cwQMLWidget::cwQMLWidget(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{
    ProxyWidget = new QGraphicsProxyWidget(this);
}

/**
  \brief Sets the widget for qml
  */
void cwQMLWidget::setWidget(QWidget* newWidget) {
    if(newWidget != widget()) {
        ProxyWidget->setWidget(newWidget);
        emit widgetChanged();
    }
}

/**
  \brief Gets the widget
  */
QWidget* cwQMLWidget::widget() const {
    return ProxyWidget->widget();
}

/**
  \brief Listens to the current geometry changes of the declarative item
  */
void cwQMLWidget::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QDeclarativeItem::geometryChanged(newGeometry, oldGeometry);
    qDebug() << "Geometry: " << newGeometry;
//    QRectF geometry = newGeometry;
//    geometry.setWidth(newGeometry.width());
//    geometry.setHeight(newGeometry.height());
    ProxyWidget->resize(newGeometry.size());
    ProxyWidget->setPos(newGeometry.topLeft());
}
