//Our includes
#include "cwFutureManagerToken.h"
#include "cwFutureManagerModel.h"
#include "cwDebug.h"

//Qt includes
#include <QMetaObject>

cwFutureManagerToken::cwFutureManagerToken(cwFutureManagerModel *model) :
    Model(model)
{

}

void cwFutureManagerToken::addJob(const cwFuture &job)
{
    auto model = Model;

    if(model.isNull()) {
        qWarning() << "Can't add job because model is null" << LOCATION;
    }

    QMetaObject::invokeMethod(model, [model, job]()
    {
        if(model) {
            model->addJob(job);
        }
    });
}

bool cwFutureManagerToken::operator==(const cwFutureManagerToken &token) const
{
    return token.Model == Model;
}
