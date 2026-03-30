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
#include <QUrl>

//Our includes
#include "cwProject.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwOpenFileEventHandler : public QObject
{
    Q_OBJECT
public:
    explicit cwOpenFileEventHandler(QObject *parent = 0);

    void setProject(cwProject* project);
    cwProject* project() const;

signals:
    void deepLinkReceived(QUrl url);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public slots:

private:
    QPointer<cwProject> Project;

};

#endif // CWOPENFILEEVENTHANDLER_H
