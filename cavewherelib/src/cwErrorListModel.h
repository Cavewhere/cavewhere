/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWERRORLISTMODEL_H
#define CWERRORLISTMODEL_H

//Qt includes
#include <QQmlGadgetListModel.h>
#include <QQmlEngine>

//Our inculdes
#include "cwError.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwErrorListModel : public QAbstractListModel //QQmlGadgetListModel<cwError>
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ErrorListModel)

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

    Q_ENUMS(ErrorRoles)

public:
    enum class ErrorRoles : int {
        SuppressedRole = Qt::UserRole,
        ErrorTypeIdRole,
        MessageRole,
        ErrorTypeRole,
    };

    cwErrorListModel(QObject* parent = nullptr);

    int roleForName(const QByteArray& roleName) const;
    int count() const;
    cwError at(int index) const;
    int indexOf(const cwError& error) const;
    QList<cwError> toList() const { return m_errors; }
    void clear();
    bool isEmpty() const { return m_errors.isEmpty(); }
    int size() const { return m_errors.size(); }
    bool contains(const cwError& error) const { return m_errors.contains(error); }
    bool isEmpty() { return m_errors.isEmpty(); }
    void remove(int index);
    void remove(const cwError& error);
    void insert(int index, const cwError& error);
    void insert(int index, const QList<cwError>& errors);
    cwError first() const { return m_errors.first(); }
    cwError last() const { return m_errors.last(); }

    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;

    // void prepend(const cwError& error);
    // void prepend(const QList<cwError>& errors);
    void append(const cwError& error);
    void append(const QList<cwError>& errors);

    QHash<int, QByteArray> roleNames() const;

signals:
    void countChanged();

private:
    QList<cwError> m_errors;


};

#endif // CWERRORLISTMODEL_H
