/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwEventRecorderModel.h"

//

cwEventRecorderModel::cwEventRecorderModel(QObject *parent) :
    QAbstractItemModel(parent)
{
}

/**
* @brief cwEventRecorderModel::setRootEventObject
* @param rootEventObject
*/
void cwEventRecorderModel::setRootEventObject(QObject* rootEventObject) {
    if(RootEventObject != rootEventObject) {
        RootEventObject = rootEventObject;
        emit rootEventObjectChanged();
    }
}


/**
* @brief cwEventRecorderModel::rootEventObject
* @return
*/
inline QObject* cwEventRecorderModel::rootEventObject() const {
    return RecorderRootEventObject;
}
