/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDEBUG_H
#define CWDEBUG_H

#define LOCATION __FILE__ << __LINE__

//Qt includes
#include <QQmlComponent>
#include <QDebug>

class cwDebug {
public:
    /**
     * @brief printErrors
     * @param component
     *
     * Afte the compontent has been create. This will print out any error to the commandline
     * of the component. If the component has no errors, this does nothing.
     */
    static void printErrors(QQmlComponent* component) {
        if(!component->errors().isEmpty()) {
            qDebug() << component->errorString();
        }
    }
};

#endif // CWDEBUG_H
