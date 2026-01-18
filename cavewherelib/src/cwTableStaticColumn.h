/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTABLESTATICCOLUMN_H
#define CWTABLESTATICCOLUMN_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

class cwTableStaticColumn : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TableStaticColumn)

    Q_PROPERTY(int columnWidth READ columnWidth WRITE setColumnWidth NOTIFY columnWidthChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(int sortRole READ sortRole WRITE setSortRole NOTIFY sortRoleChanged)

public:
    explicit cwTableStaticColumn(QObject* parent = nullptr);

    int columnWidth() const;
    void setColumnWidth(int columnWidth);

    QString text() const;
    void setText(const QString& text);

    int sortRole() const;
    void setSortRole(int sortRole);

signals:
    void columnWidthChanged();
    void textChanged();
    void sortRoleChanged();

private:
    int m_columnWidth = 0;
    QString m_text;
    int m_sortRole = -1;
};

#endif // CWTABLESTATICCOLUMN_H
