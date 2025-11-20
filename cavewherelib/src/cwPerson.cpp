/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwPerson.h"

cwPerson::cwPerson(QObject *parent) :
    QObject(parent)
{
}

/**
  Sets the name of the person
  */
void cwPerson::setName(QString name) {
    if(Name != name) {
        Name = name;
        emit nameChanged(Name);
    }
}
