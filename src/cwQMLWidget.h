#ifndef CWQMLWIDGET_H
#define CWQMLWIDGET_H

//Qt includes
#include <QDeclarativeItem>
class QGraphicsProxyWidget;
class QWidget;


class cwQMLWidget : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(QWidget* widget READ widget WRITE setWidget NOTIFY widgetChanged);

public:
    explicit cwQMLWidget(QDeclarativeItem *parent = 0);

    void setWidget(QWidget* widget);
    QWidget* widget() const;

signals:
    void widgetChanged();

public slots:

private:
    QGraphicsProxyWidget* ProxyWidget;

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);


};

#endif // CWQMLWIDGET_H
