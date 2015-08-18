/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTeamMember.h"

cwTeamMember::cwTeamMember() {
}

cwTeamMember::cwTeamMember(QString name, QStringList roles) {
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

/**
 * @brief cwTeamMember::operator ==
 * @param other
 * @return True if other has the same name and jobs as this team member. False
 * if they aren't the same
 */
bool cwTeamMember::operator==(const cwTeamMember &other) const
{
    return Name == other.Name &&
            Jobs == other.Jobs;
}

/**
 * @brief cwTeamMember::operator !=
 * @param other
 * @return False if other has the same name and jobs as this team member. True
 * if they aren't the same
 */
bool cwTeamMember::operator!=(const cwTeamMember &other) const
{
    return !operator==(other);
}
