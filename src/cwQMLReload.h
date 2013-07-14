/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWQMLRELOAD_H
#define CWQMLRELOAD_H

//Qt includes
#include <QObject>
class QQuickView;

class cwQMLReload : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQuickView* quickView READ quickView WRITE setQuickView NOTIFY quickViewChanged)

public:
    explicit cwQMLReload(QObject *parent = 0);

    QQuickView* quickView() const;
    void setQuickView(QQuickView* quickView);

    Q_INVOKABLE void reload();

signals:
    void quickViewChanged();

public slots:

private:
    QQuickView* QuickView; //!<
    
};

/**
Gets quickView
*/
inline QQuickView* cwQMLReload::quickView() const {
    return QuickView;
}
#endif // CWQMLRELOAD_H
