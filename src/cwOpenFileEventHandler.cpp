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
    Q_UNUSED(obj);
    if(event->type() == QEvent::FileOpen) {
        Q_ASSERT(dynamic_cast<QApplication*>(obj) != nullptr);
        Q_ASSERT(dynamic_cast<QFileOpenEvent*>(event) != nullptr);
        QFileOpenEvent* fileOpenEvent = static_cast<QFileOpenEvent*>(event);
        if(project() != nullptr) {
            project()->loadFile(fileOpenEvent->file());\
            event->accept();
            return true;
        }
    }

    return false;
}
