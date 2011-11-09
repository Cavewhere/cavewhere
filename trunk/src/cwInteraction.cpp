#include "cwInteraction.h"

cwInteraction::cwInteraction(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{

}

void cwInteraction::activate()
{
    emit activated(this);
}

void cwInteraction::deactivate()
{
    emit deactivated(this);
}

