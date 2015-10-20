/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSURVEYCHUNKERROR_H
#define CWSURVEYCHUNKERROR_H

#include <QSharedDataPointer>
#include <QObject>

class cwErrorData;

class cwError
{
    Q_GADGET
    Q_ENUMS(ErrorType)

    Q_PROPERTY(bool suppressed READ suppressed WRITE setSupressed)
    Q_PROPERTY(int index READ index WRITE setIndex)
    Q_PROPERTY(int errorTypeId READ errorTypeId WRITE setErrorTypeId)
    Q_PROPERTY(QString message READ message WRITE setMessage)
    Q_PROPERTY(QObject* parent READ parent WRITE setParent)

public:
    enum ErrorType {
        Warning, //Survex should still run
        Fatal, //Survex will not run
        NoError
    };

    cwError();
    cwError(const cwError &);
    cwError &operator=(const cwError &);
    ~cwError();

    ErrorType type() const;
    void setType(ErrorType type);

    int index() const;
    void setIndex(int index);

    int errorTypeId() const;
    void setErrorTypeId(int errorTypeId);

    bool suppressed() const;
    void setSupressed(bool suppressed);

    QString message() const;
    void setMessage(QString message);

    QObject* parent() const;
    void setParent(QObject* parent);

    bool operator==(const cwError& error) const;

private:
    QSharedDataPointer<cwErrorData> data;
};

Q_DECLARE_METATYPE(cwError)

#endif // CWSURVEYCHUNKERROR_H
