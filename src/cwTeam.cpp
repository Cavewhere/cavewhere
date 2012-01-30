//Our includes
#include "cwTeam.h"

//Qt includes
#include <QByteArray>

cwTeam::cwTeam(QObject *parent) :
    QAbstractListModel(parent)
{
    setupRoles();
}

/**
  Copy constructor for the team
  */
cwTeam::cwTeam(const cwTeam& team) :
    QAbstractListModel(NULL)
{
    setupRoles();

    //Copy the data
    Team = team.Team;
}


void cwTeam::addTeamMember(const cwTeamMember& teamMember) {
    beginInsertRows(QModelIndex(), Team.size(), Team.size());
    Team.append(teamMember);
    endInsertRows();
}

void cwTeam::setTeamMembers(QList<cwTeamMember> team) {
    beginResetModel();
    Team = team;
    endResetModel();
}

int cwTeam::rowCount(const QModelIndex &/*parent*/) const {
    return Team.size();
}

QVariant cwTeam::data(const QModelIndex &index, int role) const {
    const cwTeamMember& teamMember = Team.at(index.row());
    switch(role) {
    case NameRole:
        return teamMember.name();
    case JobsRole:
        return teamMember.jobs();
    }
    return QVariant();
}

void cwTeam::setData(int row, cwTeam::TeamModelRoles role, const QVariant &data) {
    QModelIndex modelIndex = index(row);
    setData(modelIndex, role, data);
}

bool cwTeam::setData(QModelIndex index, int role, const QVariant &data) {
    if(!index.isValid()) {
        return false;
    }

    switch(role) {
    case NameRole: {
        cwTeamMember& teamMember = Team[index.row()];
        teamMember.setName(data.toString());
        emit dataChanged(index, index);
        return true;
    }
    default:
        return false;
    }

}

/**
  Sets up the roles for this model, this is used for qml
  */
void cwTeam::setupRoles() {
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[JobsRole] = "jobs";
    setRoleNames(roles);
}

/**
  This removes the team member from the team
  */
void cwTeam::removeTeamMember(int row) {
    QModelIndex index = this->index(row);
    if(!index.isValid()) { return; }

    beginRemoveRows(QModelIndex(), row, row);
    Team.removeAt(row);
    endRemoveRows();
}
