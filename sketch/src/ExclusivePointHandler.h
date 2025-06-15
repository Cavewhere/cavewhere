#ifndef EXCLUSIVEPOINTHANDLER_H
#define EXCLUSIVEPOINTHANDLER_H

//Qt includse
#include <QQmlEngine>
#include <QtQuick/private/qquickpointhandler_p.h>

class ExclusivePointHandler : public QQuickPointHandler
{
    Q_OBJECT
    QML_ELEMENT

public:
    ExclusivePointHandler(QQuickItem *parent = nullptr);

protected:
    bool wantsEventPoint(const QPointerEvent *event, const QEventPoint &point) override;
    void handleEventPoint(QPointerEvent *event, QEventPoint &point) override;
};

#endif // EXCLUSIVEPOINTHANDLER_H
