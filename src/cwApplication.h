/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWAPPLICATION_H
#define CWAPPLICATION_H

#include <QApplication>
#include <QEvent>
#include <QMetaEnum>
#include <QDebug>

class cwApplication : public QApplication
{
    Q_OBJECT
public:
    explicit cwApplication(int & argc, char ** argv);

    bool notify(QObject *receiver, QEvent *event);

signals:

public slots:

private:
    QEvent *cloneEvent(QEvent *event) const;

};



/// Gives human-readable event type information.
QDebug operator<<(QDebug str, const QEvent * ev);

#endif // CWAPPLICATION_H