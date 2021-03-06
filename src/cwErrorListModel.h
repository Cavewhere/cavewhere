/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWERRORLISTMODEL_H
#define CWERRORLISTMODEL_H

//Qt includes
#include <QQmlGadgetListModel.h>

//Our inculdes
#include "cwError.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwErrorListModel : public QQmlGadgetListModel<cwError>
{
    Q_OBJECT
public:
    cwErrorListModel(QObject* parent = nullptr);
};

#endif // CWERRORLISTMODEL_H
