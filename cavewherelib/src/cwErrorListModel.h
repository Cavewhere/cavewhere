/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWERRORLISTMODEL_H
#define CWERRORLISTMODEL_H

//Qt includes
#include <QQmlEngine>
#include <QAbstractListModel>
#include <QStringList>

//Our inculdes
#include "cwError.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwErrorListModel : public QAbstractListModel {

    Q_OBJECT
    QML_NAMED_ELEMENT(ErrorListModel)

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(QStringList warningMessages READ warningMessages NOTIFY warningMessagesChanged FINAL)

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
    QStringList warningMessages() const;

    //! The non-suppressed Warning messages whose errorTypeId is one of `ids` —
    //! a scoped slice of warningMessages() for surfacing one feature's errors on
    //! their own (e.g. the fix-station badge). Generic: the caller supplies the
    //! id set, so this model stays feature-agnostic. Reactive off the same
    //! warningMessagesChanged() as warningMessages(), which fires on every add,
    //! remove, suppression toggle, and message edit — so a QML binding that also
    //! reads warningMessages (or its length) tracks this live.
    Q_INVOKABLE QStringList warningMessagesForTypeIds(const QList<int>& ids) const;
    cwError at(int index) const;
    int indexOf(const cwError& error) const;
    QList<cwError> toList() const { return m_errors; }
    bool isEmpty() const { return m_errors.isEmpty(); }
    int size() const { return m_errors.size(); }
    bool contains(const cwError& error) const { return m_errors.contains(error); }
    bool isEmpty() { return m_errors.isEmpty(); }
    void remove(int index);
    void remove(const cwError& error);
    void insert(int index, const cwError& error);
    void insert(int index, const QList<cwError>& errors);
    Q_INVOKABLE cwError last() const { return m_errors.last(); }
    Q_INVOKABLE QString allMessagesAsText() const;

    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    void append(const cwError& error);
    void append(const QList<cwError>& errors);

    QHash<int, QByteArray> roleNames() const;

public slots:
    cwError first() const { return m_errors.first(); }
    void clear();

signals:
    void countChanged();
    void warningMessagesChanged();


private:
    QList<cwError> m_errors;

};

#endif // CWERRORLISTMODEL_H
