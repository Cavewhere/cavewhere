#ifndef CWMENUWINDOW_H
#define CWMENUWINDOW_H

#include <QQuickCanvas>

class cwMenuWindow : public QQuickCanvas
{
    Q_OBJECT

//    Q_PROPERTY(bool active READ isActive() NOTIFY activeChanged)

public:
    cwMenuWindow(QWindow* parent = NULL);

protected:
    virtual bool event(QEvent * event);

signals:
//    void activatedChanged();
};

#endif // CWMENUWINDOW_H
