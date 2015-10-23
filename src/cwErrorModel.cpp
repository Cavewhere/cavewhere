/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwErrorModel.h"

//Qt includes
#include <QDebug>

cwErrorModel::cwErrorModel(QObject *parent) : QObject(parent)
{

}

bool errorLessThan(const cwError &left, const cwError &right) {
    if(left.index() == right.index()) {
        if(left.role() == right.role()) {
            if(left.errorTypeId() == right.errorTypeId()) {
                return left.type() < right.type();
            }
            return left.errorTypeId() < right.errorTypeId();
        }
        return left.role() < right.role();
    }
    return left.index() < right.index();
}

/**
 * @brief errorLessThan
 * @param left
 * @param right
 * @return
 */
bool errorLessThanIndexRole(const cwError& left, const cwError& right) {
    if(left.index() == right.index()) {
        return left.role() < right.role();
    }
    return left.index() < right.index();
}

/**
 * @brief errorLessThan
 * @param left
 * @param right
 * @return
 */
bool errorLessThanIndex(const cwError &left, const cwError &right) {
    return left.index() < right.index();
}

/**
 * @brief cwErrorManager::addError
 * @param error - Adds the error to the model
 */
void cwErrorModel::addError(const cwError &error)
{
    Q_ASSERT(error.parent() != nullptr); //You need to set the parent for the error

    //Parent object exists
    if(Database.contains(error.parent())) {
        //Just replace the error with the new one
        QList<cwError>& errorList = Database[error.parent()];
        auto iter = std::lower_bound(errorList.begin(), errorList.end(), error, &errorLessThan);        
        if(iter == errorList.end() || *iter != error) {
            //Insert into the sorted list
            errorList.insert(iter, error);
        }
    } else {
        QList<cwError> errors;
        errors.append(error);
        Database.insert(error.parent(), errors);
        Parents.insert(error.parent());
    }

    emit errorsChanged(error.parent(), error.index(), error.role());
    emitErrorChanged(error.parent());
}

/**
 * @brief cwErrorModel::removeError
 * @param error - Removes an error from the model
 */
void cwErrorModel::removeError(const cwError &error)
{
    Q_ASSERT(error.parent() != nullptr);

    if(Database.contains(error.parent())) {
        //Just replace the error with the new one
        QList<cwError>& errorList = Database[error.parent()];
        auto iter = std::lower_bound(errorList.begin(), errorList.end(), error, &errorLessThan);
        if(*iter == error) {
            //Found existing error, update it in the list
            errorList.erase(iter);
        }

        if(errorList.isEmpty()) {
            Database.remove(error.parent());
        }

        emitErrorChanged(error.parent());
    }
}

/**
 * @brief cwErrorModel::removeErrorsFor
 * @param parent
 */
void cwErrorModel::removeErrorsFor(QObject *parent)
{
    Database.remove(parent);
}

/**
 * @brief cwErrorModel::addParent
 * @param parent
 */
void cwErrorModel::addParent(const QObject *parent)
{
    Parents.insert(parent);
}

/**
 * @brief cwErrorModel::errors
 * @param parent
 * @return Returns all the errors for the parent and it's children
 */
QVariantList cwErrorModel::errors(const QObject *parent) const
{
    QVariantList errorListToReturn;

    if(Database.contains(parent)) {
        //Just replace the error with the new one
        const QList<cwError>& errorList = Database[parent];

        foreach(cwError error, errorList) {
            errorListToReturn.append(QVariant::fromValue(error));
        }

        foreach(QObject* children, parent->children()) {
            errorListToReturn.append(errors(children));
        }
    }

    return errorListToReturn;
}

/**
 * @brief cwErrorModel::errors
 * @param parent
 * @param index
 * @return
 */
QVariantList cwErrorModel::errors(const QObject *parent, int index) const
{
    return errors(parent, index, -1);
}

/**
 * @brief cwErrorModel::errors
 * @param parent
 * @param index
 * @param role
 * @return
 */
QVariantList cwErrorModel::errors(const QObject *parent, int index, int role) const
{
    if(Database.contains(parent)) {
        //Just replace the error with the new one
        const QList<cwError>& errorList = Database[parent];

        QVariantList errorsToReturn;

        if(errorList.isEmpty()) {
            return errorsToReturn;
        }

        cwError findError;
        findError.setIndex(index);
        findError.setRole(role);

        //Pick a less than function for the sorted list search, based if role is valid or not
        auto lessThan = role != -1 ? &errorLessThanIndexRole : &errorLessThanIndex;

        auto upperIter = std::upper_bound(errorList.begin(), errorList.end(), findError, lessThan);
        auto lowerIter = std::lower_bound(errorList.begin(), errorList.end(), findError, lessThan);

        while(lowerIter != upperIter && lowerIter != errorList.end()) {
            cwError currentError = *lowerIter;
            errorsToReturn.append(QVariant::fromValue(currentError));
            lowerIter++;
        }

        return errorsToReturn;
    }
    return QVariantList();
}

/**
 * @brief cwErrorModel::emitErrorChanged
 * @param parent
 */
void cwErrorModel::emitErrorChanged(QObject *parent)
{
    QObject* nextParent = parent;
    while(nextParent != nullptr) {
        if(Parents.contains(nextParent)) {
            emit parentErrorsChanged(nextParent);
        }
        nextParent = nextParent->parent();
    }
}

