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

#include "qinputeventrecorder.h"

#include <QContextMenuEvent>
#include <QFile>
#include <QDataStream>
#include <QTimer>
#include <QWidget>
#include <QApplication>

#include "eventserialization.h"

//
// Helper routines
//

static QString objectPath(QObject *obj)
{
	QString res;
	for(; obj; obj = obj->parent())
	{
		if (!res.isEmpty())
			res.prepend("/");
		res.prepend(obj->objectName());
	}
	return res;
}

static bool isChild(QObject *obj, QObject *parent)
{
	while ((obj = obj->parent()))
	{
		if (obj == parent)
			return true;
	}

	return false;
}

//
// QInputEventRecorder::EventDelivery
//

QInputEventRecorder::EventDelivery::EventDelivery(int timeOffset, QObject *obj, QEvent *ev):
	m_TimeOffset(timeOffset),
	m_ClsName(obj->metaObject()->className()),
	m_ObjName(objectPath(obj)),
	m_Ev(ev)
{
}

//
// QInputEventRecorder
//

QEvent* QInputEventRecorder::cloneEvent(QEvent *ev)
{
	if (dynamic_cast<QContextMenuEvent*>(ev))
		return new QContextMenuEvent(*static_cast<QContextMenuEvent*>(ev));
	else if (dynamic_cast<QKeyEvent*>(ev))
		return new QKeyEvent(*static_cast<QKeyEvent*>(ev));
	else if (dynamic_cast<QMouseEvent*>(ev))
		return new QMouseEvent(*static_cast<QMouseEvent*>(ev));
	else if (dynamic_cast<QTabletEvent*>(ev))
		return new QTabletEvent(*static_cast<QTabletEvent*>(ev));
	else if (dynamic_cast<QTouchEvent*>(ev))
		return new QTouchEvent(*static_cast<QTouchEvent*>(ev));
	else if (dynamic_cast<QWheelEvent*>(ev))
		return new QWheelEvent(*static_cast<QWheelEvent*>(ev));

	return 0;
}

QInputEventRecorder::QInputEventRecorder(QObject *obj):
	m_Obj(obj),
	m_Timer(new QTimer)
{
	m_Timer->setSingleShot(true);
	QObject::connect(m_Timer, SIGNAL(timeout()), this, SLOT(replay()));
}

QInputEventRecorder::~QInputEventRecorder()
{
	delete m_Timer;
}

bool QInputEventRecorder::eventFilter(QObject *obj, QEvent *ev)
{
    qDebug() << "Event:" << obj << ev;

	if (!isChild(obj, m_Obj))
		return false;

	QEvent *clonedEv = cloneEvent(ev);

	if (clonedEv)
	{
		int timeOffset;
		QDateTime curDt(QDateTime::currentDateTime());
		timeOffset = m_RecordingStartTime.daysTo(curDt) * 24 * 3600 * 1000 + m_RecordingStartTime.time().msecsTo(curDt.time());
		m_Recording.push_back(EventDelivery(timeOffset, obj, clonedEv));
	}

	return false;
}

void QInputEventRecorder::save(const QString &fileName)
{
	QFile f(fileName);
	if (!f.open(QFile::WriteOnly))
		return;
	QDataStream ds(&f);
	foreach(EventDelivery ed, m_Recording)
	{
		ds << (qint32) ed.timeOffset() << ed.clsName() << ed.objName();
		QEvent *ev(ed.event());
		ds << static_cast<QInputEvent*>(ev);
	}
}

void QInputEventRecorder::load(const QString &fileName)
{
	QFile f(fileName);
	if (!f.open(QFile::ReadOnly))
		return;

	m_Recording.clear();

	QDataStream ds(&f);
	while (!ds.atEnd())
	{
		qint32 timeOffset;
		QString clsName, objName;
		ds >> timeOffset >> clsName >> objName;
		QInputEvent *ev;
		ds >> ev;
		m_Recording.push_back(EventDelivery(timeOffset, clsName, objName, ev));
	}
}

void QInputEventRecorder::nameAllWidgets(QWidget *w)
{
	static int uniqueId = 0;

	QObjectList children = w->children();
	foreach(QObject *o, children)
	{
		if (dynamic_cast<QWidget*>(o))
		{
			if (o->objectName().isEmpty())
				o->setObjectName(QString("unique_%1").arg(uniqueId++));
			nameAllWidgets(static_cast<QWidget*>(o));
		}
	}
}

void QInputEventRecorder::record()
{
	qApp->installEventFilter(this);
	m_RecordingStartTime = QDateTime::currentDateTime();
}

void QInputEventRecorder::stop()
{
	qApp->removeEventFilter(this);
	m_Timer->stop();
}

void QInputEventRecorder::replay(float speedFactor)
{
	if (m_Recording.size() == 0)
	{
		emit replayDone();
		return;
	}

	m_ReplayPos = 0;
	m_ReplaySpeedFactor = speedFactor;
	m_Timer->setInterval(0);
	m_Timer->start();
}

void QInputEventRecorder::replay()
{
	EventDelivery& rec(m_Recording[m_ReplayPos++]);

	QStringList path = rec.objName().split("/", QString::KeepEmptyParts);
	if (path.size() > 0)
	{
		QList<QObject*> objects = m_Obj->findChildren<QObject*>(path.last());
		foreach(QObject *obj, objects)
		{
			if (obj->metaObject()->className() == rec.clsName() && objectPath(obj) == rec.objName())
			{
				qApp->postEvent(obj, cloneEvent(rec.event()));
				break;
			}
		}
	}

	if (m_ReplayPos >= m_Recording.size())
	{
		emit replayDone();
		return;
	}

	int delta = m_Recording[m_ReplayPos].timeOffset() - m_Recording[m_ReplayPos - 1].timeOffset();
	m_Timer->setInterval(delta > 0 ? delta * m_ReplaySpeedFactor : 0);
	m_Timer->start();
}
