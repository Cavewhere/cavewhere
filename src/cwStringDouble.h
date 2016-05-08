/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTRINGDOUBLE_H
#define CWSTRINGDOUBLE_H

//Qt includes
#include <QString>
#include <QMetaType>

class cwStringDouble : public QString
{

public:
    cwStringDouble();
    cwStringDouble(const cwStringDouble& str);
    cwStringDouble(const QString str);
    cwStringDouble(double value);

    cwStringDouble& operator=(const cwStringDouble& other);
    cwStringDouble& operator=(const QString& other);
    cwStringDouble& operator=(double other);

    bool isDouble() const {
        bool okay;
        toDouble(&okay);
        return okay;
    }

    /**
     * @brief operator double
     *
     * Allows the string to automatically be case to a double
     */
    operator double () const {
        bool okay;
        double value = toDouble(&okay);
        if(!okay) {
            throw std::bad_cast();
        }
        return value;
    }

};

Q_DECLARE_METATYPE(cwStringDouble)

#endif // CWSTRINGDOUBLE_H
