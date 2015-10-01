/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwSurveyChunkError.h"

class cwSurveyChunkErrorData : public QSharedData
{
public:
    cwSurveyChunkErrorData() :
        Type(cwSurveyChunkError::Unknown),
        Error(cwSurveyChunkError::NoError),
        Suppressed(false)
    {

    }

    cwSurveyChunkError::ErrorType Type;
    cwSurveyChunkError::Error Error;
    bool Suppressed;
    QString Message;
};

cwSurveyChunkError::cwSurveyChunkError() : data(new cwSurveyChunkErrorData)
{

}

cwSurveyChunkError::cwSurveyChunkError(const cwSurveyChunkError &rhs) : data(rhs.data)
{

}

cwSurveyChunkError &cwSurveyChunkError::operator=(const cwSurveyChunkError &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwSurveyChunkError::~cwSurveyChunkError()
{

}

cwSurveyChunkError::ErrorType cwSurveyChunkError::type() const
{
    return data->Type;
}


void cwSurveyChunkError::setType(cwSurveyChunkError::ErrorType type)
{
    data->Type = type;
}

/**
* @brief cwSurveyChunkError::suppressed
* @return
*/
bool cwSurveyChunkError::suppressed() const {
    return data->Suppressed;
}

/**
* @brief cwSurveyChunkError::setSupressed
* @param suppressed
*/
void cwSurveyChunkError::setSupressed(bool suppressed) {
    data->Suppressed = suppressed;
}

/**
* @brief cwSurveyChunkError::error
* @return
*/
cwSurveyChunkError::Error cwSurveyChunkError::error() const {
    return data->Error;
}

/**
* @brief cwSurveyChunkError::setError
* @param error
*/
void cwSurveyChunkError::setError(Error error) {
    data->Error = error;
}

/**
* @brief cwSurveyChunkError::message
* @return
*/
QString cwSurveyChunkError::message() const {
    return data->Message;
}

/**
* @brief cwSurveyChunkError::setMessage
* @param message
*/
void cwSurveyChunkError::setMessage(QString message) {
    data->Message = message;
}

/**
 * @brief cwSurveyChunkError::operator ==
 * @param error
 * @return
 */
bool cwSurveyChunkError::operator==(const cwSurveyChunkError &other) const
{
    return data->Error == other.data->Error &&
            data->Type == other.data->Type;
}
