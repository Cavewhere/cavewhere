#ifndef CWCOLUMNNAME_H
#define CWCOLUMNNAME_H

//Qt includes
#include <QSharedDataPointer>
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

class cwColumnNameData;

class CAVEWHERE_LIB_EXPORT cwColumnName
{
    Q_GADGET
    QML_VALUE_TYPE(cwColumnName)

    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(int columnId READ columnId WRITE setColumnId)

public:
    cwColumnName();
    cwColumnName(const QString& name, int columnId);
    cwColumnName(const cwColumnName &);
    cwColumnName &operator=(const cwColumnName &);
    ~cwColumnName();

    bool operator==(const cwColumnName& other) const;
    bool operator!=(const cwColumnName& other) const;

    QString name() const;
    void setName(QString name);

    int columnId() const;
    void setColumnId(int columnId);

private:
    QSharedDataPointer<cwColumnNameData> data;
};

/**
 * Returns false if the name or id are the same as other's name and id
 */
inline bool cwColumnName::operator!=(const cwColumnName &other) const
{
    return !operator==(other);
}

#endif // CWCOLUMNNAME_H
