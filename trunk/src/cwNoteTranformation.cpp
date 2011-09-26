#include "cwNoteTranformation.h"

cwNoteTranformation::cwNoteTranformation(QObject* parent) :
    QObject(parent),
    Data(new cwNoteTranformation::PrivateData())
{
}


cwNoteTranformation::cwNoteTranformation(const cwNoteTranformation& other) :
    QObject(NULL),
    Data(other.Data)
{

}

const cwNoteTranformation& cwNoteTranformation::operator =(const cwNoteTranformation& other) {
    if(this != &other) {
        Data = other.Data;
    }
    return *this;
}
