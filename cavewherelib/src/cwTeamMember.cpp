/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTeamMember.h"

cwTeamMember::cwTeamMember()
    : m_id(QUuid::createUuid())
{
}

cwTeamMember::cwTeamMember(QString name, QStringList roles)
    : m_id(QUuid::createUuid())
{
    Name = name;
    Jobs = roles;
}

/**
  Sets the name of a team member of a survey trip
  */
void cwTeamMember::setName(QString name) {
    Name = name;
}

/**
  Sets the roles of a team member
  */
void cwTeamMember::setJobs(QStringList roles)  {
    Jobs = roles;
}
