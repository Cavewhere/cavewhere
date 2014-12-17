/* Copyright 2011 Stanislaw Adaszewski. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY STANISLAW ADASZEWSKI ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANISLAW ADASZEWSKI OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Stanislaw Adaszewski. */

#ifndef EVENTSERIALIZATION_H
#define EVENTSERIALIZATION_H

#include <QDataStream>
#include <QContextMenuEvent>

//Our includes
#include "cwMetaEvent.h"

struct EventClass
{
    enum {
		ContextMenuEvent = 1,
		KeyEvent,
		MouseEvent,
		TabletEvent,
		TouchEvent,
        WheelEvent,
        MetaCallEvent
	};
};

QDataStream& operator<<(QDataStream &ds, const QContextMenuEvent &ev);
QDataStream& operator<<(QDataStream &ds, const QKeyEvent &ev);
QDataStream& operator<<(QDataStream &ds, const QMouseEvent &ev);
//QDataStream& operator<<(QDataStream &ds, const QTabletEvent &ev);
//QDataStream& operator<<(QDataStream &ds, const QTouchEvent &ev);
QDataStream& operator<<(QDataStream &ds, const QWheelEvent &ev);
QDataStream& operator<<(QDataStream &ds, const QInputEvent *ev);
QDataStream& operator<<(QDataStream &ds, const cwMetaEvent &ev);
QDataStream& operator>>(QDataStream &ds, QContextMenuEvent* &ev);
QDataStream& operator>>(QDataStream &ds, QKeyEvent* &ev);
QDataStream& operator>>(QDataStream &ds, QMouseEvent* &ev);
//QDataStream& operator>>(QDataStream &ds, QTabletEvent* &ev);
//QDataStream& operator>>(QDataStream &ds, QTouchEvent* &ev);
QDataStream& operator>>(QDataStream &ds, QWheelEvent* &ev);
QDataStream& operator>>(QDataStream &ds, QInputEvent*& ev);

#endif // EVENTSERIALIZATION_H
