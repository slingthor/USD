/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt for Python.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#ifndef SBK_QTMULTIMEDIA_PYTHON_H
#define SBK_QTMULTIMEDIA_PYTHON_H

//workaround to access protected functions
#define protected public

#include <sbkpython.h>
#include <sbkconverter.h>
// Module Includes
#include <pyside2_qtcore_python.h>
#include <pyside2_qtgui_python.h>
#include <pyside2_qtnetwork_python.h>

// Binded library includes
#include <qmedianetworkaccesscontrol.h>
#include <qaudiosystem.h>
#include <qcameraimagecapturecontrol.h>
#include <qaudio.h>
#include <qabstractvideofilter.h>
#include <qcamerafocus.h>
#include <qmediacontainercontrol.h>
#include <qvideorenderercontrol.h>
#include <qmediacontent.h>
#include <qcamera.h>
#include <qaudiodecoder.h>
#include <qmediaresource.h>
#include <qaudiooutputselectorcontrol.h>
#include <qvideodeviceselectorcontrol.h>
#include <qvideoencodersettingscontrol.h>
#include <qcameraviewfindersettings.h>
#include <qaudioinputselectorcontrol.h>
#include <qcustomaudiorolecontrol.h>
#include <qmediarecorder.h>
#include <qradiotunercontrol.h>
#include <qmediagaplessplaybackcontrol.h>
#include <qmediabindableinterface.h>
#include <qsoundeffect.h>
#include <qcameracapturedestinationcontrol.h>
#include <qvideowindowcontrol.h>
#include <qcameralockscontrol.h>
#include <qcameracapturebufferformatcontrol.h>
#include <qmediarecordercontrol.h>
#include <qmultimedia.h>
#include <qaudiorolecontrol.h>
#include <qabstractvideosurface.h>
#include <qmediaavailabilitycontrol.h>
#include <qaudiobuffer.h>
#include <qradiodatacontrol.h>
#include <qcameraimagecapture.h>
#include <qcameracontrol.h>
#include <qcameraexposurecontrol.h>
#include <qaudioencodersettingscontrol.h>
#include <qmediaobject.h>
#include <qaudiorecorder.h>
#include <qmediaplayer.h>
#include <qvideosurfaceformat.h>
#include <qimageencodercontrol.h>
#include <qcamerainfocontrol.h>
#include <qmediaaudioprobecontrol.h>
#include <qcamerazoomcontrol.h>
#include <qmediaplayercontrol.h>
#include <qmediatimerange.h>
#include <qcameraimageprocessingcontrol.h>
#include <qradiodata.h>
#include <qvideoframe.h>
#include <qmetadatawritercontrol.h>
#include <qcamerafeedbackcontrol.h>
#include <qmediaserviceproviderplugin.h>
#include <qradiotuner.h>
#include <qaudiodeviceinfo.h>
#include <qmediastreamscontrol.h>
#include <qabstractvideobuffer.h>
#include <qcamerainfo.h>
#include <qmetadatareadercontrol.h>
#include <qaudioformat.h>
#include <qaudiodecodercontrol.h>
#include <qmediaencodersettings.h>
#include <qmediaplaylist.h>
#include <qvideoprobe.h>
#include <qsound.h>
#include <qmediacontrol.h>
#include <qaudioprobe.h>
#include <qcameraviewfindersettingscontrol.h>
#include <qmediavideoprobecontrol.h>
#include <qmediaservice.h>
#include <qaudiooutput.h>
#include <qaudioinput.h>
// Conversion Includes - Primitive Types
#include <qabstractitemmodel.h>
#include <QString>
#include <QStringList>
#include <signalmanager.h>

// Conversion Includes - Container Types
#include <pysideqflags.h>
#include <QLinkedList>
#include <QList>
#include <QMap>
#include <QMultiMap>
#include <QPair>
#include <QQueue>
#include <QSet>
#include <QStack>
#include <QVector>

// Type indices
enum : int {
    SBK_QABSTRACTAUDIODEVICEINFO_IDX                         = 0,
    SBK_QABSTRACTAUDIOINPUT_IDX                              = 1,
    SBK_QABSTRACTAUDIOOUTPUT_IDX                             = 2,
    SBK_QABSTRACTVIDEOBUFFER_IDX                             = 3,
    SBK_QABSTRACTVIDEOBUFFER_HANDLETYPE_IDX                  = 4,
    SBK_QABSTRACTVIDEOBUFFER_MAPMODE_IDX                     = 5,
    SBK_QABSTRACTVIDEOFILTER_IDX                             = 6,
    SBK_QABSTRACTVIDEOSURFACE_IDX                            = 7,
    SBK_QABSTRACTVIDEOSURFACE_ERROR_IDX                      = 8,
    SBK_QtMultimediaQAUDIO_IDX                               = 9,
    SBK_QAUDIO_ERROR_IDX                                     = 10,
    SBK_QAUDIO_STATE_IDX                                     = 13,
    SBK_QAUDIO_MODE_IDX                                      = 11,
    SBK_QAUDIO_ROLE_IDX                                      = 12,
    SBK_QAUDIO_VOLUMESCALE_IDX                               = 14,
    SBK_QAUDIOBUFFER_IDX                                     = 15,
    SBK_QAUDIODECODER_IDX                                    = 16,
    SBK_QAUDIODECODER_STATE_IDX                              = 18,
    SBK_QAUDIODECODER_ERROR_IDX                              = 17,
    SBK_QAUDIODECODERCONTROL_IDX                             = 19,
    SBK_QAUDIODEVICEINFO_IDX                                 = 20,
    SBK_QAUDIOENCODERSETTINGS_IDX                            = 21,
    SBK_QAUDIOENCODERSETTINGSCONTROL_IDX                     = 22,
    SBK_QAUDIOFORMAT_IDX                                     = 23,
    SBK_QAUDIOFORMAT_SAMPLETYPE_IDX                          = 25,
    SBK_QAUDIOFORMAT_ENDIAN_IDX                              = 24,
    SBK_QAUDIOINPUT_IDX                                      = 26,
    SBK_QAUDIOINPUTSELECTORCONTROL_IDX                       = 27,
    SBK_QAUDIOOUTPUT_IDX                                     = 28,
    SBK_QAUDIOOUTPUTSELECTORCONTROL_IDX                      = 29,
    SBK_QAUDIOPROBE_IDX                                      = 30,
    SBK_QAUDIORECORDER_IDX                                   = 31,
    SBK_QAUDIOROLECONTROL_IDX                                = 32,
    SBK_QCAMERA_IDX                                          = 33,
    SBK_QCAMERA_STATUS_IDX                                   = 42,
    SBK_QCAMERA_STATE_IDX                                    = 41,
    SBK_QCAMERA_CAPTUREMODE_IDX                              = 34,
    SBK_QFLAGS_QCAMERA_CAPTUREMODE_IDX                       = 69,
    SBK_QCAMERA_ERROR_IDX                                    = 35,
    SBK_QCAMERA_LOCKSTATUS_IDX                               = 38,
    SBK_QCAMERA_LOCKCHANGEREASON_IDX                         = 37,
    SBK_QCAMERA_LOCKTYPE_IDX                                 = 39,
    SBK_QFLAGS_QCAMERA_LOCKTYPE_IDX                          = 70,
    SBK_QCAMERA_POSITION_IDX                                 = 40,
    SBK_QCAMERA_FRAMERATERANGE_IDX                           = 36,
    SBK_QCAMERACAPTUREBUFFERFORMATCONTROL_IDX                = 43,
    SBK_QCAMERACAPTUREDESTINATIONCONTROL_IDX                 = 44,
    SBK_QCAMERACONTROL_IDX                                   = 45,
    SBK_QCAMERACONTROL_PROPERTYCHANGETYPE_IDX                = 46,
    SBK_QCAMERAEXPOSURECONTROL_IDX                           = 47,
    SBK_QCAMERAEXPOSURECONTROL_EXPOSUREPARAMETER_IDX         = 48,
    SBK_QCAMERAFEEDBACKCONTROL_IDX                           = 49,
    SBK_QCAMERAFEEDBACKCONTROL_EVENTTYPE_IDX                 = 50,
    SBK_QCAMERAFOCUSZONE_IDX                                 = 51,
    SBK_QCAMERAFOCUSZONE_FOCUSZONESTATUS_IDX                 = 52,
    SBK_QCAMERAIMAGECAPTURE_IDX                              = 53,
    SBK_QCAMERAIMAGECAPTURE_ERROR_IDX                        = 56,
    SBK_QCAMERAIMAGECAPTURE_DRIVEMODE_IDX                    = 55,
    SBK_QCAMERAIMAGECAPTURE_CAPTUREDESTINATION_IDX           = 54,
    SBK_QFLAGS_QCAMERAIMAGECAPTURE_CAPTUREDESTINATION_IDX    = 71,
    SBK_QCAMERAIMAGECAPTURECONTROL_IDX                       = 57,
    SBK_QCAMERAIMAGEPROCESSINGCONTROL_IDX                    = 58,
    SBK_QCAMERAIMAGEPROCESSINGCONTROL_PROCESSINGPARAMETER_IDX = 59,
    SBK_QCAMERAINFO_IDX                                      = 60,
    SBK_QCAMERAINFOCONTROL_IDX                               = 61,
    SBK_QCAMERALOCKSCONTROL_IDX                              = 62,
    SBK_QCAMERAVIEWFINDERSETTINGS_IDX                        = 63,
    SBK_QCAMERAVIEWFINDERSETTINGSCONTROL_IDX                 = 64,
    SBK_QCAMERAVIEWFINDERSETTINGSCONTROL_VIEWFINDERPARAMETER_IDX = 66,
    SBK_QCAMERAVIEWFINDERSETTINGSCONTROL2_IDX                = 65,
    SBK_QCAMERAZOOMCONTROL_IDX                               = 67,
    SBK_QCUSTOMAUDIOROLECONTROL_IDX                          = 68,
    SBK_QIMAGEENCODERCONTROL_IDX                             = 75,
    SBK_QIMAGEENCODERSETTINGS_IDX                            = 76,
    SBK_QMEDIAAUDIOPROBECONTROL_IDX                          = 77,
    SBK_QMEDIAAVAILABILITYCONTROL_IDX                        = 78,
    SBK_QMEDIABINDABLEINTERFACE_IDX                          = 79,
    SBK_QMEDIACONTAINERCONTROL_IDX                           = 80,
    SBK_QMEDIACONTENT_IDX                                    = 81,
    SBK_QMEDIACONTROL_IDX                                    = 82,
    SBK_QMEDIAGAPLESSPLAYBACKCONTROL_IDX                     = 83,
    SBK_QMEDIANETWORKACCESSCONTROL_IDX                       = 84,
    SBK_QMEDIAOBJECT_IDX                                     = 85,
    SBK_QMEDIAPLAYER_IDX                                     = 86,
    SBK_QMEDIAPLAYER_STATE_IDX                               = 90,
    SBK_QMEDIAPLAYER_MEDIASTATUS_IDX                         = 89,
    SBK_QMEDIAPLAYER_FLAG_IDX                                = 88,
    SBK_QFLAGS_QMEDIAPLAYER_FLAG_IDX                         = 72,
    SBK_QMEDIAPLAYER_ERROR_IDX                               = 87,
    SBK_QMEDIAPLAYERCONTROL_IDX                              = 91,
    SBK_QMEDIAPLAYLIST_IDX                                   = 92,
    SBK_QMEDIAPLAYLIST_PLAYBACKMODE_IDX                      = 94,
    SBK_QMEDIAPLAYLIST_ERROR_IDX                             = 93,
    SBK_QMEDIARECORDER_IDX                                   = 95,
    SBK_QMEDIARECORDER_STATE_IDX                             = 97,
    SBK_QMEDIARECORDER_STATUS_IDX                            = 98,
    SBK_QMEDIARECORDER_ERROR_IDX                             = 96,
    SBK_QMEDIARECORDERCONTROL_IDX                            = 99,
    SBK_QMEDIARESOURCE_IDX                                   = 100,
    SBK_QMEDIASERVICE_IDX                                    = 101,
    SBK_QMEDIASERVICECAMERAINFOINTERFACE_IDX                 = 102,
    SBK_QMEDIASERVICEDEFAULTDEVICEINTERFACE_IDX              = 103,
    SBK_QMEDIASERVICEFEATURESINTERFACE_IDX                   = 104,
    SBK_QMEDIASERVICEPROVIDERHINT_IDX                        = 105,
    SBK_QMEDIASERVICEPROVIDERHINT_TYPE_IDX                   = 107,
    SBK_QMEDIASERVICEPROVIDERHINT_FEATURE_IDX                = 106,
    SBK_QFLAGS_QMEDIASERVICEPROVIDERHINT_FEATURE_IDX         = 73,
    SBK_QMEDIASERVICESUPPORTEDDEVICESINTERFACE_IDX           = 108,
    SBK_QMEDIASERVICESUPPORTEDFORMATSINTERFACE_IDX           = 109,
    SBK_QMEDIASTREAMSCONTROL_IDX                             = 110,
    SBK_QMEDIASTREAMSCONTROL_STREAMTYPE_IDX                  = 111,
    SBK_QMEDIATIMEINTERVAL_IDX                               = 112,
    SBK_QMEDIATIMERANGE_IDX                                  = 113,
    SBK_QMEDIAVIDEOPROBECONTROL_IDX                          = 114,
    SBK_QMETADATAREADERCONTROL_IDX                           = 115,
    SBK_QMETADATAWRITERCONTROL_IDX                           = 116,
    SBK_QtMultimediaQMULTIMEDIA_IDX                          = 117,
    SBK_QMULTIMEDIA_SUPPORTESTIMATE_IDX                      = 121,
    SBK_QMULTIMEDIA_ENCODINGQUALITY_IDX                      = 120,
    SBK_QMULTIMEDIA_ENCODINGMODE_IDX                         = 119,
    SBK_QMULTIMEDIA_AVAILABILITYSTATUS_IDX                   = 118,
    SBK_QRADIODATA_IDX                                       = 122,
    SBK_QRADIODATA_ERROR_IDX                                 = 123,
    SBK_QRADIODATA_PROGRAMTYPE_IDX                           = 124,
    SBK_QRADIODATACONTROL_IDX                                = 125,
    SBK_QRADIOTUNER_IDX                                      = 126,
    SBK_QRADIOTUNER_STATE_IDX                                = 130,
    SBK_QRADIOTUNER_BAND_IDX                                 = 127,
    SBK_QRADIOTUNER_ERROR_IDX                                = 128,
    SBK_QRADIOTUNER_STEREOMODE_IDX                           = 131,
    SBK_QRADIOTUNER_SEARCHMODE_IDX                           = 129,
    SBK_QRADIOTUNERCONTROL_IDX                               = 132,
    SBK_QSOUND_IDX                                           = 133,
    SBK_QSOUND_LOOP_IDX                                      = 134,
    SBK_QSOUNDEFFECT_IDX                                     = 135,
    SBK_QSOUNDEFFECT_LOOP_IDX                                = 136,
    SBK_QSOUNDEFFECT_STATUS_IDX                              = 137,
    SBK_QVIDEODEVICESELECTORCONTROL_IDX                      = 138,
    SBK_QVIDEOENCODERSETTINGS_IDX                            = 139,
    SBK_QVIDEOENCODERSETTINGSCONTROL_IDX                     = 140,
    SBK_QVIDEOFILTERRUNNABLE_IDX                             = 141,
    SBK_QVIDEOFILTERRUNNABLE_RUNFLAG_IDX                     = 142,
    SBK_QFLAGS_QVIDEOFILTERRUNNABLE_RUNFLAG_IDX              = 74,
    SBK_QVIDEOFRAME_IDX                                      = 143,
    SBK_QVIDEOFRAME_FIELDTYPE_IDX                            = 144,
    SBK_QVIDEOFRAME_PIXELFORMAT_IDX                          = 145,
    SBK_QVIDEOPROBE_IDX                                      = 146,
    SBK_QVIDEORENDERERCONTROL_IDX                            = 147,
    SBK_QVIDEOSURFACEFORMAT_IDX                              = 148,
    SBK_QVIDEOSURFACEFORMAT_DIRECTION_IDX                    = 149,
    SBK_QVIDEOSURFACEFORMAT_YCBCRCOLORSPACE_IDX              = 150,
    SBK_QVIDEOWINDOWCONTROL_IDX                              = 151,
    SBK_QtMultimedia_IDX_COUNT                               = 152
};
// This variable stores all Python types exported by this module.
extern PyTypeObject **SbkPySide2_QtMultimediaTypes;

// This variable stores the Python module object exported by this module.
extern PyObject *SbkPySide2_QtMultimediaModuleObject;

// This variable stores all type converters exported by this module.
extern SbkConverter **SbkPySide2_QtMultimediaTypeConverters;

// Converter indices
enum : int {
    SBK_QTMULTIMEDIA_QLIST_QOBJECTPTR_IDX                    = 0, // const QList<QObject* > &
    SBK_QTMULTIMEDIA_QLIST_QBYTEARRAY_IDX                    = 1, // QList<QByteArray >
    SBK_QTMULTIMEDIA_QLIST_QAUDIOFORMAT_ENDIAN_IDX           = 2, // QList<QAudioFormat::Endian >
    SBK_QTMULTIMEDIA_QLIST_INT_IDX                           = 3, // QList<int >
    SBK_QTMULTIMEDIA_QLIST_QAUDIOFORMAT_SAMPLETYPE_IDX       = 4, // QList<QAudioFormat::SampleType >
    SBK_QTMULTIMEDIA_QLIST_QVIDEOFRAME_PIXELFORMAT_IDX       = 5, // QList<QVideoFrame::PixelFormat >
    SBK_QTMULTIMEDIA_QLIST_QAUDIODEVICEINFO_IDX              = 6, // QList<QAudioDeviceInfo >
    SBK_QTMULTIMEDIA_QMAP_QSTRING_QVARIANT_IDX               = 7, // QMap<QString,QVariant >
    SBK_QTMULTIMEDIA_QLIST_QSTRING_IDX                       = 8, // QList<QString >
    SBK_QTMULTIMEDIA_QLIST_QAUDIO_ROLE_IDX                   = 9, // QList<QAudio::Role >
    SBK_QTMULTIMEDIA_QLIST_QCAMERA_FRAMERATERANGE_IDX        = 10, // QList<QCamera::FrameRateRange >
    SBK_QTMULTIMEDIA_QLIST_QSIZE_IDX                         = 11, // QList<QSize >
    SBK_QTMULTIMEDIA_QLIST_QCAMERAVIEWFINDERSETTINGS_IDX     = 12, // QList<QCameraViewfinderSettings >
    SBK_QTMULTIMEDIA_QLIST_QVARIANT_IDX                      = 13, // QList<QVariant >
    SBK_QTMULTIMEDIA_QLIST_QCAMERAINFO_IDX                   = 14, // QList<QCameraInfo >
    SBK_QTMULTIMEDIA_QLIST_QMEDIARESOURCE_IDX                = 15, // const QList<QMediaResource > &
    SBK_QTMULTIMEDIA_QLIST_QNETWORKCONFIGURATION_IDX         = 16, // const QList<QNetworkConfiguration > &
    SBK_QTMULTIMEDIA_QLIST_QMEDIACONTENT_IDX                 = 17, // const QList<QMediaContent > &
    SBK_QTMULTIMEDIA_QLIST_QREAL_IDX                         = 18, // QList<qreal >
    SBK_QTMULTIMEDIA_QLIST_QMEDIATIMEINTERVAL_IDX            = 19, // QList<QMediaTimeInterval >
    SBK_QTMULTIMEDIA_QPAIR_INT_INT_IDX                       = 20, // QPair<int,int >
    SBK_QtMultimedia_CONVERTERS_IDX_COUNT                    = 21
};
// Macros for type check

namespace Shiboken
{

// PyType functions, to get the PyObjectType for a type T
template<> inline PyTypeObject *SbkType< ::QAbstractAudioDeviceInfo >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTAUDIODEVICEINFO_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractAudioInput >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTAUDIOINPUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractAudioOutput >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTAUDIOOUTPUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractVideoBuffer::HandleType >() { return SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTVIDEOBUFFER_HANDLETYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractVideoBuffer::MapMode >() { return SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTVIDEOBUFFER_MAPMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractVideoBuffer >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTVIDEOBUFFER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractVideoFilter >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTVIDEOFILTER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractVideoSurface::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTVIDEOSURFACE_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractVideoSurface >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QABSTRACTVIDEOSURFACE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudio::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIO_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudio::State >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIO_STATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudio::Mode >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIO_MODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudio::Role >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIO_ROLE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudio::VolumeScale >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIO_VOLUMESCALE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudioBuffer >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOBUFFER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioDecoder::State >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIODECODER_STATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudioDecoder::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIODECODER_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudioDecoder >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIODECODER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioDecoderControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIODECODERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioDeviceInfo >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIODEVICEINFO_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioEncoderSettings >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOENCODERSETTINGS_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioEncoderSettingsControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOENCODERSETTINGSCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioFormat::SampleType >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIOFORMAT_SAMPLETYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudioFormat::Endian >() { return SbkPySide2_QtMultimediaTypes[SBK_QAUDIOFORMAT_ENDIAN_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAudioFormat >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOFORMAT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioInput >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOINPUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioInputSelectorControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOINPUTSELECTORCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioOutput >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOOUTPUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioOutputSelectorControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOOUTPUTSELECTORCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioProbe >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOPROBE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioRecorder >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIORECORDER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAudioRoleControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QAUDIOROLECONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCamera::Status >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_STATUS_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCamera::State >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_STATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCamera::CaptureMode >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_CAPTUREMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QCamera::CaptureMode> >() { return SbkPySide2_QtMultimediaTypes[SBK_QFLAGS_QCAMERA_CAPTUREMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCamera::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCamera::LockStatus >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_LOCKSTATUS_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCamera::LockChangeReason >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_LOCKCHANGEREASON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCamera::LockType >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_LOCKTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QCamera::LockType> >() { return SbkPySide2_QtMultimediaTypes[SBK_QFLAGS_QCAMERA_LOCKTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCamera::Position >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_POSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCamera >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCamera::FrameRateRange >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERA_FRAMERATERANGE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraCaptureBufferFormatControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERACAPTUREBUFFERFORMATCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraCaptureDestinationControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERACAPTUREDESTINATIONCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraControl::PropertyChangeType >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERACONTROL_PROPERTYCHANGETYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERACONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraExposureControl::ExposureParameter >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERAEXPOSURECONTROL_EXPOSUREPARAMETER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraExposureControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAEXPOSURECONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraFeedbackControl::EventType >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERAFEEDBACKCONTROL_EVENTTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraFeedbackControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAFEEDBACKCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraFocusZone::FocusZoneStatus >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERAFOCUSZONE_FOCUSZONESTATUS_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraFocusZone >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAFOCUSZONE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraImageCapture::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERAIMAGECAPTURE_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraImageCapture::DriveMode >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERAIMAGECAPTURE_DRIVEMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraImageCapture::CaptureDestination >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERAIMAGECAPTURE_CAPTUREDESTINATION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QCameraImageCapture::CaptureDestination> >() { return SbkPySide2_QtMultimediaTypes[SBK_QFLAGS_QCAMERAIMAGECAPTURE_CAPTUREDESTINATION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraImageCapture >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAIMAGECAPTURE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraImageCaptureControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAIMAGECAPTURECONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraImageProcessingControl::ProcessingParameter >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERAIMAGEPROCESSINGCONTROL_PROCESSINGPARAMETER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraImageProcessingControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAIMAGEPROCESSINGCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraInfo >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAINFO_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraInfoControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAINFOCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraLocksControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERALOCKSCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraViewfinderSettings >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAVIEWFINDERSETTINGS_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraViewfinderSettingsControl::ViewfinderParameter >() { return SbkPySide2_QtMultimediaTypes[SBK_QCAMERAVIEWFINDERSETTINGSCONTROL_VIEWFINDERPARAMETER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCameraViewfinderSettingsControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAVIEWFINDERSETTINGSCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraViewfinderSettingsControl2 >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAVIEWFINDERSETTINGSCONTROL2_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCameraZoomControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCAMERAZOOMCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCustomAudioRoleControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QCUSTOMAUDIOROLECONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QImageEncoderControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QIMAGEENCODERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QImageEncoderSettings >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QIMAGEENCODERSETTINGS_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaAudioProbeControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIAAUDIOPROBECONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaAvailabilityControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIAAVAILABILITYCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaBindableInterface >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIABINDABLEINTERFACE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaContainerControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIACONTAINERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaContent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIACONTENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIACONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaGaplessPlaybackControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIAGAPLESSPLAYBACKCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaNetworkAccessControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIANETWORKACCESSCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaObject >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIAOBJECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaPlayer::State >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYER_STATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaPlayer::MediaStatus >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYER_MEDIASTATUS_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaPlayer::Flag >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYER_FLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QMediaPlayer::Flag> >() { return SbkPySide2_QtMultimediaTypes[SBK_QFLAGS_QMEDIAPLAYER_FLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaPlayer::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYER_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaPlayer >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaPlayerControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaPlaylist::PlaybackMode >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYLIST_PLAYBACKMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaPlaylist::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYLIST_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaPlaylist >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIAPLAYLIST_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaRecorder::State >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIARECORDER_STATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaRecorder::Status >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIARECORDER_STATUS_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaRecorder::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIARECORDER_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaRecorder >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIARECORDER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaRecorderControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIARECORDERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaResource >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIARESOURCE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaService >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaServiceCameraInfoInterface >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICECAMERAINFOINTERFACE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaServiceDefaultDeviceInterface >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICEDEFAULTDEVICEINTERFACE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaServiceFeaturesInterface >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICEFEATURESINTERFACE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaServiceProviderHint::Type >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICEPROVIDERHINT_TYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaServiceProviderHint::Feature >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICEPROVIDERHINT_FEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QMediaServiceProviderHint::Feature> >() { return SbkPySide2_QtMultimediaTypes[SBK_QFLAGS_QMEDIASERVICEPROVIDERHINT_FEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaServiceProviderHint >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICEPROVIDERHINT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaServiceSupportedDevicesInterface >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICESUPPORTEDDEVICESINTERFACE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaServiceSupportedFormatsInterface >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIASERVICESUPPORTEDFORMATSINTERFACE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaStreamsControl::StreamType >() { return SbkPySide2_QtMultimediaTypes[SBK_QMEDIASTREAMSCONTROL_STREAMTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMediaStreamsControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIASTREAMSCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaTimeInterval >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIATIMEINTERVAL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaTimeRange >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIATIMERANGE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMediaVideoProbeControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMEDIAVIDEOPROBECONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMetaDataReaderControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMETADATAREADERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMetaDataWriterControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QMETADATAWRITERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMultimedia::SupportEstimate >() { return SbkPySide2_QtMultimediaTypes[SBK_QMULTIMEDIA_SUPPORTESTIMATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMultimedia::EncodingQuality >() { return SbkPySide2_QtMultimediaTypes[SBK_QMULTIMEDIA_ENCODINGQUALITY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMultimedia::EncodingMode >() { return SbkPySide2_QtMultimediaTypes[SBK_QMULTIMEDIA_ENCODINGMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMultimedia::AvailabilityStatus >() { return SbkPySide2_QtMultimediaTypes[SBK_QMULTIMEDIA_AVAILABILITYSTATUS_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRadioData::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QRADIODATA_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRadioData::ProgramType >() { return SbkPySide2_QtMultimediaTypes[SBK_QRADIODATA_PROGRAMTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRadioData >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QRADIODATA_IDX]); }
template<> inline PyTypeObject *SbkType< ::QRadioDataControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QRADIODATACONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QRadioTuner::State >() { return SbkPySide2_QtMultimediaTypes[SBK_QRADIOTUNER_STATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRadioTuner::Band >() { return SbkPySide2_QtMultimediaTypes[SBK_QRADIOTUNER_BAND_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRadioTuner::Error >() { return SbkPySide2_QtMultimediaTypes[SBK_QRADIOTUNER_ERROR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRadioTuner::StereoMode >() { return SbkPySide2_QtMultimediaTypes[SBK_QRADIOTUNER_STEREOMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRadioTuner::SearchMode >() { return SbkPySide2_QtMultimediaTypes[SBK_QRADIOTUNER_SEARCHMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRadioTuner >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QRADIOTUNER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QRadioTunerControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QRADIOTUNERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSound::Loop >() { return SbkPySide2_QtMultimediaTypes[SBK_QSOUND_LOOP_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSound >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QSOUND_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSoundEffect::Loop >() { return SbkPySide2_QtMultimediaTypes[SBK_QSOUNDEFFECT_LOOP_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSoundEffect::Status >() { return SbkPySide2_QtMultimediaTypes[SBK_QSOUNDEFFECT_STATUS_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSoundEffect >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QSOUNDEFFECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoDeviceSelectorControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEODEVICESELECTORCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoEncoderSettings >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEOENCODERSETTINGS_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoEncoderSettingsControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEOENCODERSETTINGSCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoFilterRunnable::RunFlag >() { return SbkPySide2_QtMultimediaTypes[SBK_QVIDEOFILTERRUNNABLE_RUNFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QVideoFilterRunnable::RunFlag> >() { return SbkPySide2_QtMultimediaTypes[SBK_QFLAGS_QVIDEOFILTERRUNNABLE_RUNFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QVideoFilterRunnable >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEOFILTERRUNNABLE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoFrame::FieldType >() { return SbkPySide2_QtMultimediaTypes[SBK_QVIDEOFRAME_FIELDTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QVideoFrame::PixelFormat >() { return SbkPySide2_QtMultimediaTypes[SBK_QVIDEOFRAME_PIXELFORMAT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QVideoFrame >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEOFRAME_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoProbe >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEOPROBE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoRendererControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEORENDERERCONTROL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoSurfaceFormat::Direction >() { return SbkPySide2_QtMultimediaTypes[SBK_QVIDEOSURFACEFORMAT_DIRECTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QVideoSurfaceFormat::YCbCrColorSpace >() { return SbkPySide2_QtMultimediaTypes[SBK_QVIDEOSURFACEFORMAT_YCBCRCOLORSPACE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QVideoSurfaceFormat >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEOSURFACEFORMAT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVideoWindowControl >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtMultimediaTypes[SBK_QVIDEOWINDOWCONTROL_IDX]); }

} // namespace Shiboken

#endif // SBK_QTMULTIMEDIA_PYTHON_H

