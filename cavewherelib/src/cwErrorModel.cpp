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
    m_fatalCount(0),
    m_warningCount(0),
    m_ignoredWarningCount(0),
    m_countsUpToDate(false),
    m_errors(new cwErrorListModel(this)),
    m_parent(nullptr)
{
    connect(m_errors, SIGNAL(countChanged()), this, SLOT(makeCountDirty()));
    connect(m_errors, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(checkForCountChanged(QModelIndex,QModelIndex,QVector<int>)));
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
    if(m_parent != parent) {
        if(!m_parent.isNull()) {
            m_parent->removeChildModel(this);
        }

        m_parent = parent;

        if(!m_parent.isNull()) {
            m_parent->addChildModel(this);
        }

        makeCountDirty();
    }
}

cwErrorModel *cwErrorModel::parentModel() const
{
    return m_parent;
}

/**
 * @brief cwErrorModel::addChildModel
 * @param model
 */
void cwErrorModel::addChildModel(cwErrorModel *model)
{
    Q_ASSERT(!m_childModels.contains(model));

    m_childModels.append(model);

    connect(model, SIGNAL(fatalCountChanged()), this, SLOT(makeFatalDirty()));
    connect(model, SIGNAL(warningCountChanged()), this, SLOT(makeWarningDirty()));

    makeCountDirty();
    emit childModelAdded(model);
}

void cwErrorModel::removeChildModel(cwErrorModel *model)
{
    Q_ASSERT(m_childModels.contains(model));

    m_childModels.removeOne(model);

    disconnect(model, SIGNAL(fatalCountChanged()), this, SLOT(makeFatalDirty()));
    disconnect(model, SIGNAL(warningCountChanged()), this, SLOT(makeWarningDirty()));

    makeCountDirty();
    emit childModelRemoved(model);
}

void cwErrorModel::makeCountDirty()
{
    makeFatalDirty();
    makeWarningDirty();

}

void cwErrorModel::makeFatalDirty()
{
    m_countsUpToDate = false;
    emit fatalCountChanged();
}

void cwErrorModel::makeWarningDirty()
{
    m_countsUpToDate = false;
    emit warningCountChanged();
    //Every change that can move the suppressed count also dirties the warnings
    emit ignoredWarningCountChanged();
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
        if(static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole) == role ||
            static_cast<int>(cwErrorListModel::ErrorRoles::ErrorTypeRole) == role)
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
    m_fatalCount = 0;
    m_warningCount = 0;
    m_ignoredWarningCount = 0;

    for(int i = 0; i < m_errors->count(); i++) {
        cwError error = m_errors->at(i);
        switch(error.type()) {
        case cwError::Fatal:
            m_fatalCount++;
            break;
        case cwError::Warning:
            if(error.suppressed()) {
                m_ignoredWarningCount++;
            } else {
                m_warningCount++;
            }
            break;
        default:
            break;
        }
    }

    foreach(cwErrorModel* child, m_childModels) {
        m_fatalCount += child->fatalCount();
        m_warningCount += child->warningCount();
        m_ignoredWarningCount += child->ignoredWarningCount();
    }

    m_countsUpToDate = true;
}

/**
* @brief cwErrorListModel::fatalCount
* @return
*/
int cwErrorModel::fatalCount() const {
    if(!m_countsUpToDate) {
        updateFatalAndWarningCount();
    }
    return m_fatalCount;
}


/**
* @brief cwErrorListModel::warningCount
* @return
*/
int cwErrorModel::warningCount() const {
    if(!m_countsUpToDate) {
        updateFatalAndWarningCount();
    }
    return m_warningCount;
}

int cwErrorModel::ignoredWarningCount() const
{
    if(!m_countsUpToDate) {
        updateFatalAndWarningCount();
    }
    return m_ignoredWarningCount;
}

/**
* @brief cwErrorListModel::errors
* @return
*/
cwErrorListModel* cwErrorModel::errors() const {
    return m_errors;
}

/**
 * @brief cwErrorModel::childModels
 * @return
 */
QList<cwErrorModel *> cwErrorModel::childModels() const
{
    return m_childModels;
}

/**
 * This is useful for debugging
 */
QStringList cwErrorModel::toStringList() const
{
    auto errors = this->errors()->toList();
    QStringList errorStringList;
    errorStringList.reserve(fatalCount() + warningCount());
    for(const auto& error : std::as_const(errors)) {
        // qDebug() << error.type() << error.message();
        errorStringList.append(QStringLiteral("%1:%2")
                                   .arg(error.type())
                                   .arg(error.message()));
    }

    for(auto child : m_childModels) {
        errorStringList.append(child->toStringList());
    }
    return errorStringList;
}
