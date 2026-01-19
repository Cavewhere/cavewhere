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
#include <QQmlEngine>

//Our includes
#include "cwTeamMember.h"
#include "cwTeamData.h"
#include "CaveWhereLibExport.h"
class cwKeywordModel;

class CAVEWHERE_LIB_EXPORT cwTeam : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Team)

public:
    enum TeamModelRoles{
        NameRole,

        //In a sublist
        JobsRole
    };
    Q_ENUM(TeamModelRoles)

    explicit cwTeam(QObject *parent = 0);

    // [[deprecated]]
    // cwTeam(const cwTeam& team);

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
    cwKeywordModel* keywordModel() const;

    void setData(const cwTeamData &data);
    cwTeamData data() const { return {Team}; };

signals:

public slots:

private:
    QList<cwTeamMember> Team;
    cwKeywordModel* m_keywordModel = nullptr;

    void updateKeywords();
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
