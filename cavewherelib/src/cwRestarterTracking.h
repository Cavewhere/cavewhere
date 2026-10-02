#ifndef CWRESTARTERTRACKING_H
#define CWRESTARTERTRACKING_H

//Std includes
#include <type_traits>

//Our includes
#include "cwFuture.h"
#include "cwFutureManagerToken.h"

//Async includes
#include <asyncfuture.h>

// Registers every run started by `restarter` as a job with `token`'s future
// manager. `jobName` is either a QString or a callable returning one (for names
// computed per run). The token is read at fire time, so it may still be invalid
// when cwTrackRestarter() is called and become valid later via a setter.
// Composes with Restarter::onResult(), which is a separate hook.
//
// This is a free function rather than a cwFutureManagerToken member so the
// widely-included token header stays free of any asyncfuture dependency; the
// coupling is confined to the handful of call sites that already include
// asyncfuture.h.
template <typename T, typename Name>
void cwTrackRestarter(cwFutureManagerToken& token,
                      AsyncFuture::Restarter<T>& restarter,
                      Name jobName)
{
    cwFutureManagerToken* tokenPtr = &token;
    restarter.onFutureChanged([tokenPtr, &restarter, jobName]() {
        if (!tokenPtr->isValid()) {
            return;
        }
        QString name;
        if constexpr (std::is_invocable_v<Name>) {
            name = jobName();
        } else {
            name = jobName;
        }
        tokenPtr->addJob(cwFuture(QFuture<void>(restarter.future()), name));
    });
}

#endif // CWRESTARTERTRACKING_H
