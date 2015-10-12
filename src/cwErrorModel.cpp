/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwErrorModel.h"

cwErrorModel::cwErrorModel(QObject *parent) : QObject(parent)
{

}

/**
 * @brief errorLessThan
 * @param left
 * @param right
 * @return
 */
bool errorLessThan(const cwError& left, const cwError& right) {
    if(left.index() == right.index()) {
        if(left.errorTypeId() == right.errorTypeId()) {
            return left.type() < right.type();
        }
        return left.errorTypeId() < right.errorTypeId();
    }
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
        if(*iter == error) {
            //Found existing error, update it in the list
            *iter = error;
        } else {
            //Insert into the sorted list
            errorList.insert(iter, error);
        }
    } else {
        QList<cwError> errors;
        errors.append(error);
        Database.insert(error.parent(), errors);
    }

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
 * @brief cwErrorModel::errors
 * @param parent
 * @return Returns all the errors for the parent and it's children
 */
QVariantList cwErrorModel::errors(const QObject *parent) const
{
    QVariantList errorListToReturn;

    if(Database.contains(error.parent())) {
        //Just replace the error with the new one
        const QList<cwError>& errorList = Database[error.parent()];

        foreach(cwError error, errorList) {
            errorListToReturn.append(error);
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
    if(Database.contains(error.parent())) {
        //Just replace the error with the new one
        const QList<cwError>& errorList = Database[error.parent()];

        cwError findError;

        std::upper_bound(errorList.begin(), errorList.end(), findError, &errorLessThan);


        foreach(cwError error, errorList) {
            errorListToReturn.append(error);
        }

        foreach(QObject* children, parent->children()) {
            errorListToReturn.append(errors(children));
        }
    }
}

/**
 * @brief cwErrorModel::emitErrorChanged
 * @param parent
 */
void cwErrorModel::emitErrorChanged(const QObject *parent)
{
    QObject* nextParent = parent;
    while(nextParent != nullptr) {
        emit errorsChanged(nextParent);
        nextParent = nextParent->parent();
    }
}

