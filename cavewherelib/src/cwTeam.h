/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTEAM_H
#define CWTEAM_H

//Qt includes
#include <QAbstractListModel>

//Our includes
#include "cwTeamMember.h"

class cwTeam : public QAbstractListModel
{
    Q_OBJECT

    Q_ENUMS(TeamModelRoles)

public:
    enum TeamModelRoles{
        NameRole,

        //In a sublist
        JobsRole
    };

    explicit cwTeam(QObject *parent = 0);
    cwTeam(const cwTeam& team);

    Q_INVOKABLE void addTeamMember();
    void addTeamMember(const cwTeamMember& teamMember);

    Q_INVOKABLE void removeTeamMember(int row);

    void setTeamMembers(QList<cwTeamMember> team);
    QList<cwTeamMember> teamMembers() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    virtual bool setData(const QModelIndex& index, const QVariant& data, int role);
    Q_INVOKABLE void setData(int index, cwTeam::TeamModelRoles role, const QVariant& data);

    virtual QHash<int, QByteArray> roleNames() const;

signals:

public slots:

private:
    QList<cwTeamMember> Team;
};

/**
  Gets all the team members
  */
inline QList<cwTeamMember> cwTeam::teamMembers() const {
    return Team;
}

/**
  \brief Adds a team member to the team
  */
inline void cwTeam::addTeamMember() {
    addTeamMember(cwTeamMember());
}



#endif // CWTEAM_H
