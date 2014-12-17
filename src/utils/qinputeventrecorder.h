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

#ifndef QINPUTEVENTRECORDER_H
#define QINPUTEVENTRECORDER_H

#include <QVector>
#include <QEvent>
#include <QDateTime>
#include <QPointer>

class QTimer;

class QInputEventRecorder: public QObject
{
	Q_OBJECT

public:
	class EventDelivery
	{
	public:
		EventDelivery(): m_Ev(0) {}

		EventDelivery(int timeOffset, QObject *obj, QEvent *ev);

		EventDelivery(int timeOffset, const QString &clsName, const QString &objName, QEvent *ev):
			m_TimeOffset(timeOffset),
			m_ClsName(clsName),
			m_ObjName(objName),
			m_Ev(ev)
		{
		}

		int timeOffset() const { return m_TimeOffset; }
		const QString& clsName() const { return m_ClsName; }
		const QString& objName() const { return m_ObjName; }
		QEvent* event() const { return m_Ev.data(); }

	private:
		int m_TimeOffset;
		QString m_ClsName;
		QString m_ObjName;
		QSharedPointer<QEvent> m_Ev;
	};

public:
	QInputEventRecorder(QObject *obj = 0);
	~QInputEventRecorder();

	bool eventFilter(QObject *obj, QEvent *ev);

    void setRootObject(QObject *obj) { m_Obj = obj; }
    QObject* rootObject() const { return m_Obj.data(); }

	void save(const QString &fileName);
	void load(const QString &fileName);

	static void nameAllWidgets(QWidget*);

public slots:
	void record();
	void replay(float speedFactor);
	void stop();

private slots:
	void replay();

signals:
	void replayDone();

private:
	QEvent *cloneEvent(QEvent*);

private:
    QPointer<QObject> m_Obj;
	QVector<EventDelivery> m_Recording;
	int m_ReplayPos;
	QTimer *m_Timer;
	QDateTime m_RecordingStartTime;
	float m_ReplaySpeedFactor;
};

#endif // QINPUTEVENTRECORDER_H
