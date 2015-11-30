/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwErrorModel.h"
#include "cwErrorListModel.h"

cwErrorModel::cwErrorModel(QObject *parent) :
    QObject(parent),
    FatalCount(0),
    WarningCount(0),
    FatalWaringCountUptoDate(false),
    Errors(new cwErrorListModel(this)),
    Parent(nullptr)
{
    connect(Errors, SIGNAL(countChanged()), this, SLOT(makeCountDirty()));
    connect(Errors, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(checkForCountChanged(QModelIndex,QModelIndex,QVector<int>)));
}

/**
 * @brief cwErrorModel::~cwErrorModel
 */
cwErrorModel::~cwErrorModel()
{
    setParentModel(nullptr);
}

void cwErrorModel::setParentModel(cwErrorModel *parent)
{
    if(Parent != parent) {
        if(Parent != nullptr) {
            Parent->removeChildModel(this);
        }

        Parent = parent;

        if(Parent != nullptr) {
            Parent->addChildModel(this);
        }

        makeCountDirty();
    }
}

cwErrorModel *cwErrorModel::parentModel() const
{
    return Parent;
}

/**
 * @brief cwErrorModel::addChildModel
 * @param model
 */
void cwErrorModel::addChildModel(cwErrorModel *model)
{
    Q_ASSERT(!ChildModels.contains(model));

    ChildModels.append(model);

    connect(model, SIGNAL(fatalCountChanged()), this, SLOT(makeFatalDirty()));
    connect(model, SIGNAL(warningCountChanged()), this, SLOT(makeWarningDirty()));

    makeCountDirty();
}

void cwErrorModel::removeChildModel(cwErrorModel *model)
{
    Q_ASSERT(ChildModels.contains(model));

    ChildModels.removeOne(model);

    disconnect(model, SIGNAL(fatalCountChanged()), this, SLOT(makeFatalDirty()));
    disconnect(model, SIGNAL(warningCountChanged()), this, SLOT(makeWarningDirty()));

    makeCountDirty();
}

void cwErrorModel::makeCountDirty()
{
    makeFatalDirty();
    makeWarningDirty();

}

void cwErrorModel::makeFatalDirty()
{
    FatalWaringCountUptoDate = false;
    emit fatalCountChanged();
}

void cwErrorModel::makeWarningDirty()
{
    FatalWaringCountUptoDate = false;
    emit warningCountChanged();
}

/**
 * @brief cwErrorModel::checkForWarning
 * @param topLeft
 * @param bottomRight
 * @param roles
 */
void cwErrorModel::checkForCountChanged(QModelIndex topLeft, QModelIndex bottomRight, QVector<int> roles)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    foreach(int role, roles) {
        if(Errors->roleForName("suppressed") == role ||
                Errors->roleForName("type") == role)
        {

            makeCountDirty();
        }
    }
}

/**
 * @brief cwErrorModel::updateFatalAndWarningCount
 */
void cwErrorModel::updateFatalAndWarningCount() const
{
    FatalCount = 0;
    WarningCount = 0;

    for(int i = 0; i < Errors->count(); i++) {
        cwError error = Errors->at(i);
        switch(error.type()) {
        case cwError::Fatal:
            FatalCount++;
            break;
        case cwError::Warning:
            if(!error.suppressed()) {
                WarningCount++;
            }
            break;
        default:
            break;
        }
    }

    foreach(cwErrorModel* child, ChildModels) {
        FatalCount += child->fatalCount();
        WarningCount += child->warningCount();
    }

    FatalWaringCountUptoDate = true;
}

/**
* @brief cwErrorListModel::fatalCount
* @return
*/
int cwErrorModel::fatalCount() const {
    if(!FatalWaringCountUptoDate) {
        updateFatalAndWarningCount();
    }
    return FatalCount;
}


/**
* @brief cwErrorListModel::warningCount
* @return
*/
int cwErrorModel::warningCount() const {
    if(!FatalWaringCountUptoDate) {
        updateFatalAndWarningCount();
    }
    return WarningCount;
}

/**
* @brief cwErrorListModel::errors
* @return
*/
cwErrorListModel* cwErrorModel::errors() const {
    return Errors;
}

/**
 * @brief cwErrorModel::childModels
 * @return
 */
QList<cwErrorModel *> cwErrorModel::childModels() const
{
    return ChildModels;
}
