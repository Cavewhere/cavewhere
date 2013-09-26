/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwOpenFileEventHandler.h"

//Qt includes
#include <QApplication>
#include <QFileOpenEvent>
#include <QDebug>

cwOpenFileEventHandler::cwOpenFileEventHandler(QObject *parent) :
    QObject(parent)
{
}

void cwOpenFileEventHandler::setProject(cwProject *project)
{
    Project = project;
}

cwProject *cwOpenFileEventHandler::project() const
{
    return Project.data();
}

bool cwOpenFileEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if(event->type() == QEvent::FileOpen) {
        Q_ASSERT(dynamic_cast<QApplication*>(obj) != NULL);
        Q_ASSERT(dynamic_cast<QFileOpenEvent*>(event) != NULL);
        QFileOpenEvent* fileOpenEvent = static_cast<QFileOpenEvent*>(event);
        if(project() != NULL) {
            project()->loadFile(fileOpenEvent->file());\
            event->accept();
            return true;
        }
    }

    return false;
}
