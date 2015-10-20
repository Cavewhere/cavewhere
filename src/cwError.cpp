/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwError.h"

//Qt includes
#include <QPointer>

class cwErrorData : public QSharedData
{
public:
    cwErrorData() :
        Type(cwError::NoError),
        ErrorId(0),
        Suppressed(false)
    {

    }

    cwError::ErrorType Type;
    int ErrorId; //!<
    int Index; //!<
    bool Suppressed;
    QString Message;
    QPointer<QObject> Parent; //!<
};

cwError::cwError() : data(new cwErrorData)
{

}

cwError::cwError(const cwError &rhs) : data(rhs.data)
{

}

cwError &cwError::operator=(const cwError &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwError::~cwError()
{

}

cwError::ErrorType cwError::type() const
{
    return data->Type;
}


void cwError::setType(cwError::ErrorType type)
{
    data->Type = type;
}

/**
* @brief cwError::suppressed
* @return
*/
bool cwError::suppressed() const {
    return data->Suppressed;
}

/**
* @brief cwError::setSupressed
* @param suppressed
*/
void cwError::setSupressed(bool suppressed) {
    data->Suppressed = suppressed;
}

/**
* @brief cwError::message
* @return
*/
QString cwError::message() const {
    return data->Message;
}

/**
* @brief cwError::setMessage
* @param message
*/
void cwError::setMessage(QString message) {
    data->Message = message;
}

/**
 * @brief cwError::operator ==
 * @param error
 * @return
 */
bool cwError::operator==(const cwError &other) const
{
    return data->ErrorId == other.data->ErrorId &&
            data->Type == other.data->Type &&
            data->Parent == other.data->Parent;
}

/**
* @brief cwError::errorId
* @return The error id. Is to id the error in a particular context that it was created in.
*/
int cwError::errorTypeId() const {
    return data->ErrorId;
}

/**
* @brief cwError::setErrorId
* @param errorId - Set's the error id. This is user defined by the caller, and is useful to
* identify the error. This is handy when serilizing the error when the error is suppressed. The
* error's message may change between cavewhere versions, but the id should stay the same.
*/
void cwError::setErrorTypeId(int errorId) {
    data->ErrorId = errorId;
}

/**
* @brief cwError::parent
* @return The parent that this error is referancing
*/
QObject* cwError::parent() const {
    return data->Parent;
}

/**
* @brief cwError::setParent
* @param parent
*/
void cwError::setParent(QObject* parent) {
    data->Parent = parent;
}

/**
* @brief cwError::index
* @return
*/
int cwError::index() const {
    return data->Index;
}

/**
* @brief cwError::setIndex
* @param index
*/
void cwError::setIndex(int index) {
    data->Index = index;
}
