/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwLead.h"

class cwLeadData : public QSharedData
{
public:
    QPointF Position;
    QString Description;
    QSizeF Size;
    bool Completed = false;

};

cwLead::cwLead() : data(new cwLeadData)
{

}

cwLead::cwLead(const cwLead &rhs) : data(rhs.data)
{

}

cwLead &cwLead::operator=(const cwLead &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwLead::~cwLead()
{

}

/**
 * @brief cwLead::setPositionOnNote
 * @param point - The position of the lead on the scrap
 */
void cwLead::setPositionOnNote(QPointF point)
{
    data->Position = point;
}

/**
 * @brief cwLead::positionOnNote
 * @return The position of the lead on the scrap
 */
QPointF cwLead::positionOnNote() const
{
    return data->Position;
}

/**
 * @brief cwLead::setDescription
 * @param desciption - The text desciption of the lead
 */
void cwLead::setDescription(QString desciption)
{
    data->Description = desciption;
}

/**
 * @brief cwLead::desciption
 * @return The text desciption of the lead
 */
QString cwLead::desciption() const
{
    return data->Description;
}

/**
 * @brief cwLead::setSize
 * @param size - The size of the lead. This is a length like 2ft by 5ft or 3m by 2m
 *
 * There are no units for the cwLead. The unit's are stored in the cwTrip's calibration
 */
void cwLead::setSize(QSizeF size)
{
    data->Size = size;
}

/**
 * @brief cwLead::size
 * @return
 */
QSizeF cwLead::size() const
{
    return data->Size;
}

/**
 * @brief cwLead::setCompeleted
 * @param compeleted - Set to true if the lead has been complete, true if completed and false if
 * it is still a lead
 */
void cwLead::setCompleted(bool compeleted)
{
    data->Completed = compeleted;
}

/**
 * @brief cwLead::completed
 * @return return true if the lead is completed and false if the lead is still a lead
 */
bool cwLead::completed() const
{
    return data->Completed;
}

