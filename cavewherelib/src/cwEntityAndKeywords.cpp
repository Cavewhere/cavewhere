#include "cwEntityAndKeywords.h"

QVector<QObject *> cwEntityAndKeywords::justEnitites(const QVector<cwEntityAndKeywords> &entitiesKeywords)
{
    QVector<QObject*> result;
    result.reserve(entitiesKeywords.size());
    for(const cwEntityAndKeywords& current : entitiesKeywords) {
        result.append(current.entity());
    }
    return result;
}
