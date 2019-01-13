#include "cwColumnName.h"

class cwColumnNameData : public QSharedData
{
public:
    cwColumnNameData() {}
    cwColumnNameData(const QString& name, int id) :
        Name(name),
        Id(id)
    {}

    QString Name;
    int Id = -1;
};

cwColumnName::cwColumnName() : data(new cwColumnNameData)
{

}

cwColumnName::cwColumnName(const QString &name, int id) :
    data(new cwColumnNameData(name, id))
{

}

cwColumnName::cwColumnName(const cwColumnName &rhs) : data(rhs.data)
{

}

cwColumnName &cwColumnName::operator=(const cwColumnName &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwColumnName::~cwColumnName()
{

}

/**
 * Returns true if other name and id are equal to this object's name and id
 */
bool cwColumnName::operator==(const cwColumnName &other) const
{
    return data->Name == other.data->Name && data->Id == other.data->Id;
}

/**
* Returns the name of the column
*/
QString cwColumnName::name() const {
    return data->Name;
}

/**
* Sets the name for the column
*/
void cwColumnName::setName(QString name) {
    data->Name = name;
}

/**
* Returns the id of the column
*/
int cwColumnName::columnId() const {
    return data->Id;
}

/**
* Sets the id for the column
*/
void cwColumnName::setColumnId(int id) {
    data->Id = id;
}
