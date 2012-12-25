/****************************************************************************
** Meta object code from reading C++ file 'cwAbstractProjection.h'
**
** Created: Tue Dec 25 10:46:11 2012
**      by: The Qt Meta Object Compiler version 67 (Qt 5.0.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../src/cwAbstractProjection.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'cwAbstractProjection.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.0.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_cwAbstractProjection_t {
    QByteArrayData data[14];
    char stringdata[175];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    offsetof(qt_meta_stringdata_cwAbstractProjection_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData) \
    )
static const qt_meta_stringdata_cwAbstractProjection_t qt_meta_stringdata_cwAbstractProjection = {
    {
QT_MOC_LITERAL(0, 0, 20),
QT_MOC_LITERAL(1, 21, 13),
QT_MOC_LITERAL(2, 35, 0),
QT_MOC_LITERAL(3, 36, 16),
QT_MOC_LITERAL(4, 53, 15),
QT_MOC_LITERAL(5, 69, 14),
QT_MOC_LITERAL(6, 84, 13),
QT_MOC_LITERAL(7, 98, 16),
QT_MOC_LITERAL(8, 115, 6),
QT_MOC_LITERAL(9, 122, 17),
QT_MOC_LITERAL(10, 140, 9),
QT_MOC_LITERAL(11, 150, 8),
QT_MOC_LITERAL(12, 159, 7),
QT_MOC_LITERAL(13, 167, 6)
    },
    "cwAbstractProjection\0viewerChanged\0\0"
    "nearPlaneChanged\0farPlaneChanged\0"
    "enabledChanged\0matrixChanged\0"
    "updateProjection\0viewer\0cw3dRegionViewer*\0"
    "nearPlane\0farPlane\0enabled\0matrix\0"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_cwAbstractProjection[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       5,   50, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   44,    2, 0x05,
       3,    0,   45,    2, 0x05,
       4,    0,   46,    2, 0x05,
       5,    0,   47,    2, 0x05,
       6,    0,   48,    2, 0x05,

 // slots: name, argc, parameters, tag, flags
       7,    0,   49,    2, 0x09,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,

 // properties: name, type, flags
       8, 0x80000000 | 9, 0x0049510b,
      10, QMetaType::Double, 0x00495103,
      11, QMetaType::Double, 0x00495103,
      12, QMetaType::Bool, 0x00495103,
      13, QMetaType::QMatrix4x4, 0x00495001,

 // properties: notify_signal_id
       0,
       1,
       2,
       3,
       4,

       0        // eod
};

void cwAbstractProjection::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        cwAbstractProjection *_t = static_cast<cwAbstractProjection *>(_o);
        switch (_id) {
        case 0: _t->viewerChanged(); break;
        case 1: _t->nearPlaneChanged(); break;
        case 2: _t->farPlaneChanged(); break;
        case 3: _t->enabledChanged(); break;
        case 4: _t->matrixChanged(); break;
        case 5: _t->updateProjection(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (cwAbstractProjection::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&cwAbstractProjection::viewerChanged)) {
                *result = 0;
            }
        }
        {
            typedef void (cwAbstractProjection::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&cwAbstractProjection::nearPlaneChanged)) {
                *result = 1;
            }
        }
        {
            typedef void (cwAbstractProjection::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&cwAbstractProjection::farPlaneChanged)) {
                *result = 2;
            }
        }
        {
            typedef void (cwAbstractProjection::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&cwAbstractProjection::enabledChanged)) {
                *result = 3;
            }
        }
        {
            typedef void (cwAbstractProjection::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&cwAbstractProjection::matrixChanged)) {
                *result = 4;
            }
        }
    }
    Q_UNUSED(_a);
}

const QMetaObject cwAbstractProjection::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_cwAbstractProjection.data,
      qt_meta_data_cwAbstractProjection,  qt_static_metacall, 0, 0}
};


const QMetaObject *cwAbstractProjection::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *cwAbstractProjection::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_cwAbstractProjection.stringdata))
        return static_cast<void*>(const_cast< cwAbstractProjection*>(this));
    return QObject::qt_metacast(_clname);
}

int cwAbstractProjection::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 6;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< cw3dRegionViewer**>(_v) = viewer(); break;
        case 1: *reinterpret_cast< double*>(_v) = nearPlane(); break;
        case 2: *reinterpret_cast< double*>(_v) = farPlane(); break;
        case 3: *reinterpret_cast< bool*>(_v) = enabled(); break;
        case 4: *reinterpret_cast< QMatrix4x4*>(_v) = matrix(); break;
        }
        _id -= 5;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setViewer(*reinterpret_cast< cw3dRegionViewer**>(_v)); break;
        case 1: setNearPlane(*reinterpret_cast< double*>(_v)); break;
        case 2: setFarPlane(*reinterpret_cast< double*>(_v)); break;
        case 3: setEnabled(*reinterpret_cast< bool*>(_v)); break;
        }
        _id -= 5;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 5;
    } else if (_c == QMetaObject::RegisterPropertyMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void cwAbstractProjection::viewerChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}

// SIGNAL 1
void cwAbstractProjection::nearPlaneChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, 0);
}

// SIGNAL 2
void cwAbstractProjection::farPlaneChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, 0);
}

// SIGNAL 3
void cwAbstractProjection::enabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, 0);
}

// SIGNAL 4
void cwAbstractProjection::matrixChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, 0);
}
QT_END_MOC_NAMESPACE
