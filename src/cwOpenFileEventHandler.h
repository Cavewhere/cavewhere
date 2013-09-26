/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWOPENFILEEVENTHANDLER_H
#define CWOPENFILEEVENTHANDLER_H

//Qt includes
#include <QObject>
#include <QPointer>

//Our includes
#include "cwProject.h"

class cwOpenFileEventHandler : public QObject
{
    Q_OBJECT
public:
    explicit cwOpenFileEventHandler(QObject *parent = 0);

    void setProject(cwProject* project);
    cwProject* project() const;

signals:

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public slots:

private:
    QPointer<cwProject> Project;

};

#endif // CWOPENFILEEVENTHANDLER_H
