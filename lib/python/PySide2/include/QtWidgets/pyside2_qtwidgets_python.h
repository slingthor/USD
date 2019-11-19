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


#ifndef SBK_QTWIDGETS_PYTHON_H
#define SBK_QTWIDGETS_PYTHON_H

//workaround to access protected functions
#define protected public

#include <sbkpython.h>
#include <sbkconverter.h>
// Module Includes
#include <pyside2_qtgui_python.h>
#include <pyside2_qtcore_python.h>

// Binded library includes
#include <qrubberband.h>
#include <qmessagebox.h>
#include <qtoolbar.h>
#include <qgraphicseffect.h>
#include <qaccessiblewidget.h>
#include <qsizepolicy.h>
#include <qsystemtrayicon.h>
#include <qabstractitemdelegate.h>
#include <qfilesystemmodel.h>
#include <qfileiconprovider.h>
#include <qgesture.h>
#include <qdialogbuttonbox.h>
#include <qdesktopwidget.h>
#include <qfiledialog.h>
#include <qheaderview.h>
#include <qgraphicswidget.h>
#include <qsplitter.h>
#include <qactiongroup.h>
#include <qlabel.h>
#include <qgraphicsitem.h>
#include <qfontcombobox.h>
#include <qgraphicsanchorlayout.h>
#include <qkeyeventtransition.h>
#include <qmenubar.h>
#include <qradiobutton.h>
#include <qcombobox.h>
#include <qprogressbar.h>
#include <qapplication.h>
#include <qmouseeventtransition.h>
#include <qstyle.h>
#include <qgridlayout.h>
#include <qgraphicsitemanimation.h>
#include <qinputdialog.h>
#include <qcommandlinkbutton.h>
#include <qcalendarwidget.h>
#include <qwidget.h>
#include <qtreewidgetitemiterator.h>
#include <qbuttongroup.h>
#include <qdial.h>
#include <qtoolbutton.h>
#include <qdatetimeedit.h>
#include <qgraphicsproxywidget.h>
#include <qabstractspinbox.h>
#include <qstylepainter.h>
#include <qtreewidget.h>
#include <qpushbutton.h>
#include <qtabbar.h>
#include <qlcdnumber.h>
#include <qsizegrip.h>
#include <QTextEdit>
#include <qgraphicssceneevent.h>
#include <qkeysequenceedit.h>
#include <qlineedit.h>
#include <qlistwidget.h>
#include <qaction.h>
#include <qdirmodel.h>
#include <qstatusbar.h>
#include <qwhatsthis.h>
#include <qcolordialog.h>
#include <qcolumnview.h>
#include <qundostack.h>
#include <qspinbox.h>
#include <qscroller.h>
#include <qdrawutil.h>
#include <qstackedwidget.h>
#include <qtreeview.h>
#include <qabstractitemview.h>
#include <qabstractscrollarea.h>
#include <qopenglwidget.h>
#include <qgraphicslayout.h>
#include <qitemdelegate.h>
#include <qabstractbutton.h>
#include <qlayoutitem.h>
#include <qgesturerecognizer.h>
#include <qdatawidgetmapper.h>
#include <qgraphicsgridlayout.h>
#include <qcheckbox.h>
#include <qstyleditemdelegate.h>
#include <qplaintextedit.h>
#include <qdialog.h>
#include <qmdiarea.h>
#include <qtooltip.h>
#include <qwizard.h>
#include <qtextedit.h>
#include <qprogressdialog.h>
#include <qtablewidget.h>
#include <qtextbrowser.h>
#include <qfontdialog.h>
#include <qabstractslider.h>
#include <qstyleoption.h>
#include <qtableview.h>
#include <qwidgetaction.h>
#include <qscrollarea.h>
#include <qcompleter.h>
#include <qgraphicsview.h>
#include <qfocusframe.h>
#include <qcolormap.h>
#include <qlayout.h>
#include <qscrollbar.h>
#include <qsplashscreen.h>
#include <qgroupbox.h>
#include <qcommonstyle.h>
#include <qtoolbox.h>
#include <qshortcut.h>
#include <qtabwidget.h>
#include <qmenu.h>
#include <qmdisubwindow.h>
#include <qformlayout.h>
#include <qerrormessage.h>
#include <qmainwindow.h>
#include <qgraphicsscene.h>
#include <qstylefactory.h>
#include <qgraphicstransform.h>
#include <qboxlayout.h>
#include <qproxystyle.h>
#include <qundogroup.h>
#include <qslider.h>
#include <qundoview.h>
#include <qgraphicslinearlayout.h>
#include <qitemeditorfactory.h>
#include <qgraphicslayoutitem.h>
#include <qstackedlayout.h>
#include <qlistview.h>
#include <qscrollerproperties.h>
#include <qdockwidget.h>
#include <qframe.h>
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
    SBK_QABSTRACTBUTTON_IDX                                  = 0,
    SBK_QABSTRACTGRAPHICSSHAPEITEM_IDX                       = 1,
    SBK_QABSTRACTITEMDELEGATE_IDX                            = 2,
    SBK_QABSTRACTITEMDELEGATE_ENDEDITHINT_IDX                = 3,
    SBK_QABSTRACTITEMVIEW_IDX                                = 4,
    SBK_QABSTRACTITEMVIEW_SELECTIONMODE_IDX                  = 12,
    SBK_QABSTRACTITEMVIEW_SELECTIONBEHAVIOR_IDX              = 11,
    SBK_QABSTRACTITEMVIEW_SCROLLHINT_IDX                     = 9,
    SBK_QABSTRACTITEMVIEW_EDITTRIGGER_IDX                    = 8,
    SBK_QFLAGS_QABSTRACTITEMVIEW_EDITTRIGGER_IDX             = 83,
    SBK_QABSTRACTITEMVIEW_SCROLLMODE_IDX                     = 10,
    SBK_QABSTRACTITEMVIEW_DRAGDROPMODE_IDX                   = 6,
    SBK_QABSTRACTITEMVIEW_CURSORACTION_IDX                   = 5,
    SBK_QABSTRACTITEMVIEW_STATE_IDX                          = 13,
    SBK_QABSTRACTITEMVIEW_DROPINDICATORPOSITION_IDX          = 7,
    SBK_QABSTRACTSCROLLAREA_IDX                              = 14,
    SBK_QABSTRACTSCROLLAREA_SIZEADJUSTPOLICY_IDX             = 15,
    SBK_QABSTRACTSLIDER_IDX                                  = 16,
    SBK_QABSTRACTSLIDER_SLIDERACTION_IDX                     = 17,
    SBK_QABSTRACTSLIDER_SLIDERCHANGE_IDX                     = 18,
    SBK_QABSTRACTSPINBOX_IDX                                 = 19,
    SBK_QABSTRACTSPINBOX_STEPENABLEDFLAG_IDX                 = 22,
    SBK_QFLAGS_QABSTRACTSPINBOX_STEPENABLEDFLAG_IDX          = 84,
    SBK_QABSTRACTSPINBOX_BUTTONSYMBOLS_IDX                   = 20,
    SBK_QABSTRACTSPINBOX_CORRECTIONMODE_IDX                  = 21,
    SBK_QABSTRACTSPINBOX_STEPTYPE_IDX                        = 23,
    SBK_QACCESSIBLEWIDGET_IDX                                = 24,
    SBK_QACTION_IDX                                          = 25,
    SBK_QACTION_MENUROLE_IDX                                 = 27,
    SBK_QACTION_PRIORITY_IDX                                 = 28,
    SBK_QACTION_ACTIONEVENT_IDX                              = 26,
    SBK_QACTIONGROUP_IDX                                     = 29,
    SBK_QAPPLICATION_IDX                                     = 30,
    SBK_QAPPLICATION_COLORSPEC_IDX                           = 31,
    SBK_QBOXLAYOUT_IDX                                       = 32,
    SBK_QBOXLAYOUT_DIRECTION_IDX                             = 33,
    SBK_QBUTTONGROUP_IDX                                     = 34,
    SBK_QCALENDARWIDGET_IDX                                  = 35,
    SBK_QCALENDARWIDGET_HORIZONTALHEADERFORMAT_IDX           = 36,
    SBK_QCALENDARWIDGET_VERTICALHEADERFORMAT_IDX             = 38,
    SBK_QCALENDARWIDGET_SELECTIONMODE_IDX                    = 37,
    SBK_QCHECKBOX_IDX                                        = 39,
    SBK_QCOLORDIALOG_IDX                                     = 40,
    SBK_QCOLORDIALOG_COLORDIALOGOPTION_IDX                   = 41,
    SBK_QFLAGS_QCOLORDIALOG_COLORDIALOGOPTION_IDX            = 85,
    SBK_QCOLORMAP_IDX                                        = 42,
    SBK_QCOLORMAP_MODE_IDX                                   = 43,
    SBK_QCOLUMNVIEW_IDX                                      = 44,
    SBK_QCOMBOBOX_IDX                                        = 45,
    SBK_QCOMBOBOX_INSERTPOLICY_IDX                           = 46,
    SBK_QCOMBOBOX_SIZEADJUSTPOLICY_IDX                       = 47,
    SBK_QCOMMANDLINKBUTTON_IDX                               = 48,
    SBK_QCOMMONSTYLE_IDX                                     = 49,
    SBK_QCOMPLETER_IDX                                       = 50,
    SBK_QCOMPLETER_COMPLETIONMODE_IDX                        = 51,
    SBK_QCOMPLETER_MODELSORTING_IDX                          = 52,
    SBK_QDATAWIDGETMAPPER_IDX                                = 53,
    SBK_QDATAWIDGETMAPPER_SUBMITPOLICY_IDX                   = 54,
    SBK_QDATEEDIT_IDX                                        = 55,
    SBK_QDATETIMEEDIT_IDX                                    = 56,
    SBK_QDATETIMEEDIT_SECTION_IDX                            = 57,
    SBK_QFLAGS_QDATETIMEEDIT_SECTION_IDX                     = 86,
    SBK_QDESKTOPWIDGET_IDX                                   = 58,
    SBK_QDIAL_IDX                                            = 59,
    SBK_QDIALOG_IDX                                          = 60,
    SBK_QDIALOG_DIALOGCODE_IDX                               = 61,
    SBK_QDIALOGBUTTONBOX_IDX                                 = 62,
    SBK_QDIALOGBUTTONBOX_BUTTONROLE_IDX                      = 64,
    SBK_QDIALOGBUTTONBOX_STANDARDBUTTON_IDX                  = 65,
    SBK_QFLAGS_QDIALOGBUTTONBOX_STANDARDBUTTON_IDX           = 87,
    SBK_QDIALOGBUTTONBOX_BUTTONLAYOUT_IDX                    = 63,
    SBK_QDIRMODEL_IDX                                        = 66,
    SBK_QDIRMODEL_ROLES_IDX                                  = 67,
    SBK_QDOCKWIDGET_IDX                                      = 68,
    SBK_QDOCKWIDGET_DOCKWIDGETFEATURE_IDX                    = 69,
    SBK_QFLAGS_QDOCKWIDGET_DOCKWIDGETFEATURE_IDX             = 88,
    SBK_QDOUBLESPINBOX_IDX                                   = 70,
    SBK_QERRORMESSAGE_IDX                                    = 71,
    SBK_QFILEDIALOG_IDX                                      = 72,
    SBK_QFILEDIALOG_VIEWMODE_IDX                             = 77,
    SBK_QFILEDIALOG_FILEMODE_IDX                             = 75,
    SBK_QFILEDIALOG_ACCEPTMODE_IDX                           = 73,
    SBK_QFILEDIALOG_DIALOGLABEL_IDX                          = 74,
    SBK_QFILEDIALOG_OPTION_IDX                               = 76,
    SBK_QFLAGS_QFILEDIALOG_OPTION_IDX                        = 89,
    SBK_QFILEICONPROVIDER_IDX                                = 78,
    SBK_QFILEICONPROVIDER_ICONTYPE_IDX                       = 79,
    SBK_QFILEICONPROVIDER_OPTION_IDX                         = 80,
    SBK_QFLAGS_QFILEICONPROVIDER_OPTION_IDX                  = 90,
    SBK_QFILESYSTEMMODEL_IDX                                 = 81,
    SBK_QFILESYSTEMMODEL_ROLES_IDX                           = 82,
    SBK_QFOCUSFRAME_IDX                                      = 119,
    SBK_QFONTCOMBOBOX_IDX                                    = 120,
    SBK_QFONTCOMBOBOX_FONTFILTER_IDX                         = 121,
    SBK_QFLAGS_QFONTCOMBOBOX_FONTFILTER_IDX                  = 91,
    SBK_QFONTDIALOG_IDX                                      = 122,
    SBK_QFONTDIALOG_FONTDIALOGOPTION_IDX                     = 123,
    SBK_QFLAGS_QFONTDIALOG_FONTDIALOGOPTION_IDX              = 92,
    SBK_QFORMLAYOUT_IDX                                      = 124,
    SBK_QFORMLAYOUT_FIELDGROWTHPOLICY_IDX                    = 125,
    SBK_QFORMLAYOUT_ROWWRAPPOLICY_IDX                        = 127,
    SBK_QFORMLAYOUT_ITEMROLE_IDX                             = 126,
    SBK_QFRAME_IDX                                           = 128,
    SBK_QFRAME_SHAPE_IDX                                     = 130,
    SBK_QFRAME_SHADOW_IDX                                    = 129,
    SBK_QFRAME_STYLEMASK_IDX                                 = 131,
    SBK_QGESTURE_IDX                                         = 132,
    SBK_QGESTURE_GESTURECANCELPOLICY_IDX                     = 133,
    SBK_QGESTUREEVENT_IDX                                    = 134,
    SBK_QGESTURERECOGNIZER_IDX                               = 135,
    SBK_QGESTURERECOGNIZER_RESULTFLAG_IDX                    = 136,
    SBK_QFLAGS_QGESTURERECOGNIZER_RESULTFLAG_IDX             = 93,
    SBK_QGRAPHICSANCHOR_IDX                                  = 137,
    SBK_QGRAPHICSANCHORLAYOUT_IDX                            = 138,
    SBK_QGRAPHICSBLUREFFECT_IDX                              = 139,
    SBK_QGRAPHICSBLUREFFECT_BLURHINT_IDX                     = 140,
    SBK_QFLAGS_QGRAPHICSBLUREFFECT_BLURHINT_IDX              = 94,
    SBK_QGRAPHICSCOLORIZEEFFECT_IDX                          = 141,
    SBK_QGRAPHICSDROPSHADOWEFFECT_IDX                        = 142,
    SBK_QGRAPHICSEFFECT_IDX                                  = 143,
    SBK_QGRAPHICSEFFECT_CHANGEFLAG_IDX                       = 144,
    SBK_QFLAGS_QGRAPHICSEFFECT_CHANGEFLAG_IDX                = 95,
    SBK_QGRAPHICSEFFECT_PIXMAPPADMODE_IDX                    = 145,
    SBK_QGRAPHICSELLIPSEITEM_IDX                             = 146,
    SBK_QGRAPHICSGRIDLAYOUT_IDX                              = 147,
    SBK_QGRAPHICSITEM_IDX                                    = 148,
    SBK_QGRAPHICSITEM_GRAPHICSITEMFLAG_IDX                   = 152,
    SBK_QFLAGS_QGRAPHICSITEM_GRAPHICSITEMFLAG_IDX            = 96,
    SBK_QGRAPHICSITEM_GRAPHICSITEMCHANGE_IDX                 = 151,
    SBK_QGRAPHICSITEM_CACHEMODE_IDX                          = 149,
    SBK_QGRAPHICSITEM_PANELMODALITY_IDX                      = 153,
    SBK_QGRAPHICSITEM_EXTENSION_IDX                          = 150,
    SBK_QGRAPHICSITEMANIMATION_IDX                           = 154,
    SBK_QGRAPHICSITEMGROUP_IDX                               = 155,
    SBK_QGRAPHICSLAYOUT_IDX                                  = 156,
    SBK_QGRAPHICSLAYOUTITEM_IDX                              = 157,
    SBK_QGRAPHICSLINEITEM_IDX                                = 158,
    SBK_QGRAPHICSLINEARLAYOUT_IDX                            = 159,
    SBK_QGRAPHICSOBJECT_IDX                                  = 160,
    SBK_QGRAPHICSOPACITYEFFECT_IDX                           = 161,
    SBK_QGRAPHICSPATHITEM_IDX                                = 162,
    SBK_QGRAPHICSPIXMAPITEM_IDX                              = 163,
    SBK_QGRAPHICSPIXMAPITEM_SHAPEMODE_IDX                    = 164,
    SBK_QGRAPHICSPOLYGONITEM_IDX                             = 165,
    SBK_QGRAPHICSPROXYWIDGET_IDX                             = 166,
    SBK_QGRAPHICSRECTITEM_IDX                                = 167,
    SBK_QGRAPHICSROTATION_IDX                                = 168,
    SBK_QGRAPHICSSCALE_IDX                                   = 169,
    SBK_QGRAPHICSSCENE_IDX                                   = 170,
    SBK_QGRAPHICSSCENE_ITEMINDEXMETHOD_IDX                   = 171,
    SBK_QGRAPHICSSCENE_SCENELAYER_IDX                        = 172,
    SBK_QFLAGS_QGRAPHICSSCENE_SCENELAYER_IDX                 = 97,
    SBK_QGRAPHICSSCENECONTEXTMENUEVENT_IDX                   = 173,
    SBK_QGRAPHICSSCENECONTEXTMENUEVENT_REASON_IDX            = 174,
    SBK_QGRAPHICSSCENEDRAGDROPEVENT_IDX                      = 175,
    SBK_QGRAPHICSSCENEEVENT_IDX                              = 176,
    SBK_QGRAPHICSSCENEHELPEVENT_IDX                          = 177,
    SBK_QGRAPHICSSCENEHOVEREVENT_IDX                         = 178,
    SBK_QGRAPHICSSCENEMOUSEEVENT_IDX                         = 179,
    SBK_QGRAPHICSSCENEMOVEEVENT_IDX                          = 180,
    SBK_QGRAPHICSSCENERESIZEEVENT_IDX                        = 181,
    SBK_QGRAPHICSSCENEWHEELEVENT_IDX                         = 182,
    SBK_QGRAPHICSSIMPLETEXTITEM_IDX                          = 183,
    SBK_QGRAPHICSTEXTITEM_IDX                                = 184,
    SBK_QGRAPHICSTRANSFORM_IDX                               = 185,
    SBK_QGRAPHICSVIEW_IDX                                    = 186,
    SBK_QGRAPHICSVIEW_VIEWPORTANCHOR_IDX                     = 190,
    SBK_QGRAPHICSVIEW_CACHEMODEFLAG_IDX                      = 187,
    SBK_QFLAGS_QGRAPHICSVIEW_CACHEMODEFLAG_IDX               = 98,
    SBK_QGRAPHICSVIEW_DRAGMODE_IDX                           = 188,
    SBK_QGRAPHICSVIEW_VIEWPORTUPDATEMODE_IDX                 = 191,
    SBK_QGRAPHICSVIEW_OPTIMIZATIONFLAG_IDX                   = 189,
    SBK_QFLAGS_QGRAPHICSVIEW_OPTIMIZATIONFLAG_IDX            = 99,
    SBK_QGRAPHICSWIDGET_IDX                                  = 192,
    SBK_QGRIDLAYOUT_IDX                                      = 193,
    SBK_QGROUPBOX_IDX                                        = 194,
    SBK_QHBOXLAYOUT_IDX                                      = 195,
    SBK_QHEADERVIEW_IDX                                      = 196,
    SBK_QHEADERVIEW_RESIZEMODE_IDX                           = 197,
    SBK_QINPUTDIALOG_IDX                                     = 198,
    SBK_QINPUTDIALOG_INPUTDIALOGOPTION_IDX                   = 199,
    SBK_QINPUTDIALOG_INPUTMODE_IDX                           = 200,
    SBK_QITEMDELEGATE_IDX                                    = 201,
    SBK_QITEMEDITORCREATORBASE_IDX                           = 202,
    SBK_QITEMEDITORFACTORY_IDX                               = 203,
    SBK_QKEYEVENTTRANSITION_IDX                              = 204,
    SBK_QKEYSEQUENCEEDIT_IDX                                 = 205,
    SBK_QLCDNUMBER_IDX                                       = 206,
    SBK_QLCDNUMBER_MODE_IDX                                  = 207,
    SBK_QLCDNUMBER_SEGMENTSTYLE_IDX                          = 208,
    SBK_QLABEL_IDX                                           = 209,
    SBK_QLAYOUT_IDX                                          = 210,
    SBK_QLAYOUT_SIZECONSTRAINT_IDX                           = 211,
    SBK_QLAYOUTITEM_IDX                                      = 212,
    SBK_QLINEEDIT_IDX                                        = 213,
    SBK_QLINEEDIT_ACTIONPOSITION_IDX                         = 214,
    SBK_QLINEEDIT_ECHOMODE_IDX                               = 215,
    SBK_QLISTVIEW_IDX                                        = 216,
    SBK_QLISTVIEW_MOVEMENT_IDX                               = 219,
    SBK_QLISTVIEW_FLOW_IDX                                   = 217,
    SBK_QLISTVIEW_RESIZEMODE_IDX                             = 220,
    SBK_QLISTVIEW_LAYOUTMODE_IDX                             = 218,
    SBK_QLISTVIEW_VIEWMODE_IDX                               = 221,
    SBK_QLISTWIDGET_IDX                                      = 222,
    SBK_QLISTWIDGETITEM_IDX                                  = 223,
    SBK_QLISTWIDGETITEM_ITEMTYPE_IDX                         = 224,
    SBK_QMAINWINDOW_IDX                                      = 225,
    SBK_QMAINWINDOW_DOCKOPTION_IDX                           = 226,
    SBK_QFLAGS_QMAINWINDOW_DOCKOPTION_IDX                    = 100,
    SBK_QMDIAREA_IDX                                         = 227,
    SBK_QMDIAREA_AREAOPTION_IDX                              = 228,
    SBK_QFLAGS_QMDIAREA_AREAOPTION_IDX                       = 101,
    SBK_QMDIAREA_WINDOWORDER_IDX                             = 230,
    SBK_QMDIAREA_VIEWMODE_IDX                                = 229,
    SBK_QMDISUBWINDOW_IDX                                    = 231,
    SBK_QMDISUBWINDOW_SUBWINDOWOPTION_IDX                    = 232,
    SBK_QFLAGS_QMDISUBWINDOW_SUBWINDOWOPTION_IDX             = 102,
    SBK_QMENU_IDX                                            = 233,
    SBK_QMENUBAR_IDX                                         = 234,
    SBK_QMESSAGEBOX_IDX                                      = 235,
    SBK_QMESSAGEBOX_ICON_IDX                                 = 237,
    SBK_QMESSAGEBOX_BUTTONROLE_IDX                           = 236,
    SBK_QMESSAGEBOX_STANDARDBUTTON_IDX                       = 238,
    SBK_QFLAGS_QMESSAGEBOX_STANDARDBUTTON_IDX                = 103,
    SBK_QMOUSEEVENTTRANSITION_IDX                            = 239,
    SBK_QOPENGLWIDGET_IDX                                    = 240,
    SBK_QOPENGLWIDGET_UPDATEBEHAVIOR_IDX                     = 241,
    SBK_QPANGESTURE_IDX                                      = 242,
    SBK_QPINCHGESTURE_IDX                                    = 243,
    SBK_QPINCHGESTURE_CHANGEFLAG_IDX                         = 244,
    SBK_QFLAGS_QPINCHGESTURE_CHANGEFLAG_IDX                  = 104,
    SBK_QPLAINTEXTDOCUMENTLAYOUT_IDX                         = 245,
    SBK_QPLAINTEXTEDIT_IDX                                   = 246,
    SBK_QPLAINTEXTEDIT_LINEWRAPMODE_IDX                      = 247,
    SBK_QPROGRESSBAR_IDX                                     = 248,
    SBK_QPROGRESSBAR_DIRECTION_IDX                           = 249,
    SBK_QPROGRESSDIALOG_IDX                                  = 250,
    SBK_QPROXYSTYLE_IDX                                      = 251,
    SBK_QPUSHBUTTON_IDX                                      = 252,
    SBK_QRADIOBUTTON_IDX                                     = 253,
    SBK_QRUBBERBAND_IDX                                      = 254,
    SBK_QRUBBERBAND_SHAPE_IDX                                = 255,
    SBK_QSCROLLAREA_IDX                                      = 256,
    SBK_QSCROLLBAR_IDX                                       = 257,
    SBK_QSCROLLER_IDX                                        = 258,
    SBK_QSCROLLER_STATE_IDX                                  = 261,
    SBK_QSCROLLER_SCROLLERGESTURETYPE_IDX                    = 260,
    SBK_QSCROLLER_INPUT_IDX                                  = 259,
    SBK_QSCROLLERPROPERTIES_IDX                              = 262,
    SBK_QSCROLLERPROPERTIES_OVERSHOOTPOLICY_IDX              = 264,
    SBK_QSCROLLERPROPERTIES_FRAMERATES_IDX                   = 263,
    SBK_QSCROLLERPROPERTIES_SCROLLMETRIC_IDX                 = 265,
    SBK_QSHORTCUT_IDX                                        = 266,
    SBK_QSIZEGRIP_IDX                                        = 267,
    SBK_QSIZEPOLICY_IDX                                      = 268,
    SBK_QSIZEPOLICY_POLICYFLAG_IDX                           = 271,
    SBK_QSIZEPOLICY_POLICY_IDX                               = 270,
    SBK_QSIZEPOLICY_CONTROLTYPE_IDX                          = 269,
    SBK_QFLAGS_QSIZEPOLICY_CONTROLTYPE_IDX                   = 105,
    SBK_QSLIDER_IDX                                          = 272,
    SBK_QSLIDER_TICKPOSITION_IDX                             = 273,
    SBK_QSPACERITEM_IDX                                      = 274,
    SBK_QSPINBOX_IDX                                         = 275,
    SBK_QSPLASHSCREEN_IDX                                    = 276,
    SBK_QSPLITTER_IDX                                        = 277,
    SBK_QSPLITTERHANDLE_IDX                                  = 278,
    SBK_QSTACKEDLAYOUT_IDX                                   = 279,
    SBK_QSTACKEDLAYOUT_STACKINGMODE_IDX                      = 280,
    SBK_QSTACKEDWIDGET_IDX                                   = 281,
    SBK_QSTATUSBAR_IDX                                       = 282,
    SBK_QSTYLE_IDX                                           = 283,
    SBK_QSTYLE_STATEFLAG_IDX                                 = 291,
    SBK_QFLAGS_QSTYLE_STATEFLAG_IDX                          = 106,
    SBK_QSTYLE_PRIMITIVEELEMENT_IDX                          = 288,
    SBK_QSTYLE_CONTROLELEMENT_IDX                            = 286,
    SBK_QSTYLE_SUBELEMENT_IDX                                = 294,
    SBK_QSTYLE_COMPLEXCONTROL_IDX                            = 284,
    SBK_QSTYLE_SUBCONTROL_IDX                                = 293,
    SBK_QFLAGS_QSTYLE_SUBCONTROL_IDX                         = 107,
    SBK_QSTYLE_PIXELMETRIC_IDX                               = 287,
    SBK_QSTYLE_CONTENTSTYPE_IDX                              = 285,
    SBK_QSTYLE_REQUESTSOFTWAREINPUTPANEL_IDX                 = 289,
    SBK_QSTYLE_STYLEHINT_IDX                                 = 292,
    SBK_QSTYLE_STANDARDPIXMAP_IDX                            = 290,
    SBK_QSTYLEFACTORY_IDX                                    = 295,
    SBK_QSTYLEHINTRETURN_IDX                                 = 296,
    SBK_QSTYLEHINTRETURN_HINTRETURNTYPE_IDX                  = 297,
    SBK_QSTYLEHINTRETURN_STYLEOPTIONTYPE_IDX                 = 298,
    SBK_QSTYLEHINTRETURN_STYLEOPTIONVERSION_IDX              = 299,
    SBK_QSTYLEHINTRETURNMASK_IDX                             = 300,
    SBK_QSTYLEHINTRETURNMASK_STYLEOPTIONTYPE_IDX             = 301,
    SBK_QSTYLEHINTRETURNMASK_STYLEOPTIONVERSION_IDX          = 302,
    SBK_QSTYLEHINTRETURNVARIANT_IDX                          = 303,
    SBK_QSTYLEHINTRETURNVARIANT_STYLEOPTIONTYPE_IDX          = 304,
    SBK_QSTYLEHINTRETURNVARIANT_STYLEOPTIONVERSION_IDX       = 305,
    SBK_QSTYLEOPTION_IDX                                     = 306,
    SBK_QSTYLEOPTION_OPTIONTYPE_IDX                          = 307,
    SBK_QSTYLEOPTION_STYLEOPTIONTYPE_IDX                     = 308,
    SBK_QSTYLEOPTION_STYLEOPTIONVERSION_IDX                  = 309,
    SBK_QSTYLEOPTIONBUTTON_IDX                               = 310,
    SBK_QSTYLEOPTIONBUTTON_STYLEOPTIONTYPE_IDX               = 312,
    SBK_QSTYLEOPTIONBUTTON_STYLEOPTIONVERSION_IDX            = 313,
    SBK_QSTYLEOPTIONBUTTON_BUTTONFEATURE_IDX                 = 311,
    SBK_QFLAGS_QSTYLEOPTIONBUTTON_BUTTONFEATURE_IDX          = 108,
    SBK_QSTYLEOPTIONCOMBOBOX_IDX                             = 314,
    SBK_QSTYLEOPTIONCOMBOBOX_STYLEOPTIONTYPE_IDX             = 315,
    SBK_QSTYLEOPTIONCOMBOBOX_STYLEOPTIONVERSION_IDX          = 316,
    SBK_QSTYLEOPTIONCOMPLEX_IDX                              = 317,
    SBK_QSTYLEOPTIONCOMPLEX_STYLEOPTIONTYPE_IDX              = 318,
    SBK_QSTYLEOPTIONCOMPLEX_STYLEOPTIONVERSION_IDX           = 319,
    SBK_QSTYLEOPTIONDOCKWIDGET_IDX                           = 320,
    SBK_QSTYLEOPTIONDOCKWIDGET_STYLEOPTIONTYPE_IDX           = 321,
    SBK_QSTYLEOPTIONDOCKWIDGET_STYLEOPTIONVERSION_IDX        = 322,
    SBK_QSTYLEOPTIONFOCUSRECT_IDX                            = 323,
    SBK_QSTYLEOPTIONFOCUSRECT_STYLEOPTIONTYPE_IDX            = 324,
    SBK_QSTYLEOPTIONFOCUSRECT_STYLEOPTIONVERSION_IDX         = 325,
    SBK_QSTYLEOPTIONFRAME_IDX                                = 326,
    SBK_QSTYLEOPTIONFRAME_STYLEOPTIONTYPE_IDX                = 328,
    SBK_QSTYLEOPTIONFRAME_STYLEOPTIONVERSION_IDX             = 329,
    SBK_QSTYLEOPTIONFRAME_FRAMEFEATURE_IDX                   = 327,
    SBK_QFLAGS_QSTYLEOPTIONFRAME_FRAMEFEATURE_IDX            = 109,
    SBK_QSTYLEOPTIONGRAPHICSITEM_IDX                         = 330,
    SBK_QSTYLEOPTIONGRAPHICSITEM_STYLEOPTIONTYPE_IDX         = 331,
    SBK_QSTYLEOPTIONGRAPHICSITEM_STYLEOPTIONVERSION_IDX      = 332,
    SBK_QSTYLEOPTIONGROUPBOX_IDX                             = 333,
    SBK_QSTYLEOPTIONGROUPBOX_STYLEOPTIONTYPE_IDX             = 334,
    SBK_QSTYLEOPTIONGROUPBOX_STYLEOPTIONVERSION_IDX          = 335,
    SBK_QSTYLEOPTIONHEADER_IDX                               = 336,
    SBK_QSTYLEOPTIONHEADER_STYLEOPTIONTYPE_IDX               = 340,
    SBK_QSTYLEOPTIONHEADER_STYLEOPTIONVERSION_IDX            = 341,
    SBK_QSTYLEOPTIONHEADER_SECTIONPOSITION_IDX               = 337,
    SBK_QSTYLEOPTIONHEADER_SELECTEDPOSITION_IDX              = 338,
    SBK_QSTYLEOPTIONHEADER_SORTINDICATOR_IDX                 = 339,
    SBK_QSTYLEOPTIONMENUITEM_IDX                             = 342,
    SBK_QSTYLEOPTIONMENUITEM_STYLEOPTIONTYPE_IDX             = 345,
    SBK_QSTYLEOPTIONMENUITEM_STYLEOPTIONVERSION_IDX          = 346,
    SBK_QSTYLEOPTIONMENUITEM_MENUITEMTYPE_IDX                = 344,
    SBK_QSTYLEOPTIONMENUITEM_CHECKTYPE_IDX                   = 343,
    SBK_QSTYLEOPTIONPROGRESSBAR_IDX                          = 347,
    SBK_QSTYLEOPTIONPROGRESSBAR_STYLEOPTIONTYPE_IDX          = 348,
    SBK_QSTYLEOPTIONPROGRESSBAR_STYLEOPTIONVERSION_IDX       = 349,
    SBK_QSTYLEOPTIONRUBBERBAND_IDX                           = 350,
    SBK_QSTYLEOPTIONRUBBERBAND_STYLEOPTIONTYPE_IDX           = 351,
    SBK_QSTYLEOPTIONRUBBERBAND_STYLEOPTIONVERSION_IDX        = 352,
    SBK_QSTYLEOPTIONSIZEGRIP_IDX                             = 353,
    SBK_QSTYLEOPTIONSIZEGRIP_STYLEOPTIONTYPE_IDX             = 354,
    SBK_QSTYLEOPTIONSIZEGRIP_STYLEOPTIONVERSION_IDX          = 355,
    SBK_QSTYLEOPTIONSLIDER_IDX                               = 356,
    SBK_QSTYLEOPTIONSLIDER_STYLEOPTIONTYPE_IDX               = 357,
    SBK_QSTYLEOPTIONSLIDER_STYLEOPTIONVERSION_IDX            = 358,
    SBK_QSTYLEOPTIONSPINBOX_IDX                              = 359,
    SBK_QSTYLEOPTIONSPINBOX_STYLEOPTIONTYPE_IDX              = 360,
    SBK_QSTYLEOPTIONSPINBOX_STYLEOPTIONVERSION_IDX           = 361,
    SBK_QSTYLEOPTIONTAB_IDX                                  = 362,
    SBK_QSTYLEOPTIONTAB_STYLEOPTIONTYPE_IDX                  = 365,
    SBK_QSTYLEOPTIONTAB_STYLEOPTIONVERSION_IDX               = 366,
    SBK_QSTYLEOPTIONTAB_TABPOSITION_IDX                      = 368,
    SBK_QSTYLEOPTIONTAB_SELECTEDPOSITION_IDX                 = 364,
    SBK_QSTYLEOPTIONTAB_CORNERWIDGET_IDX                     = 363,
    SBK_QFLAGS_QSTYLEOPTIONTAB_CORNERWIDGET_IDX              = 110,
    SBK_QSTYLEOPTIONTAB_TABFEATURE_IDX                       = 367,
    SBK_QFLAGS_QSTYLEOPTIONTAB_TABFEATURE_IDX                = 111,
    SBK_QSTYLEOPTIONTABBARBASE_IDX                           = 369,
    SBK_QSTYLEOPTIONTABBARBASE_STYLEOPTIONTYPE_IDX           = 370,
    SBK_QSTYLEOPTIONTABBARBASE_STYLEOPTIONVERSION_IDX        = 371,
    SBK_QSTYLEOPTIONTABWIDGETFRAME_IDX                       = 372,
    SBK_QSTYLEOPTIONTABWIDGETFRAME_STYLEOPTIONTYPE_IDX       = 373,
    SBK_QSTYLEOPTIONTABWIDGETFRAME_STYLEOPTIONVERSION_IDX    = 374,
    SBK_QSTYLEOPTIONTITLEBAR_IDX                             = 375,
    SBK_QSTYLEOPTIONTITLEBAR_STYLEOPTIONTYPE_IDX             = 376,
    SBK_QSTYLEOPTIONTITLEBAR_STYLEOPTIONVERSION_IDX          = 377,
    SBK_QSTYLEOPTIONTOOLBAR_IDX                              = 378,
    SBK_QSTYLEOPTIONTOOLBAR_STYLEOPTIONTYPE_IDX              = 379,
    SBK_QSTYLEOPTIONTOOLBAR_STYLEOPTIONVERSION_IDX           = 380,
    SBK_QSTYLEOPTIONTOOLBAR_TOOLBARPOSITION_IDX              = 382,
    SBK_QSTYLEOPTIONTOOLBAR_TOOLBARFEATURE_IDX               = 381,
    SBK_QFLAGS_QSTYLEOPTIONTOOLBAR_TOOLBARFEATURE_IDX        = 112,
    SBK_QSTYLEOPTIONTOOLBOX_IDX                              = 383,
    SBK_QSTYLEOPTIONTOOLBOX_STYLEOPTIONTYPE_IDX              = 385,
    SBK_QSTYLEOPTIONTOOLBOX_STYLEOPTIONVERSION_IDX           = 386,
    SBK_QSTYLEOPTIONTOOLBOX_TABPOSITION_IDX                  = 387,
    SBK_QSTYLEOPTIONTOOLBOX_SELECTEDPOSITION_IDX             = 384,
    SBK_QSTYLEOPTIONTOOLBUTTON_IDX                           = 388,
    SBK_QSTYLEOPTIONTOOLBUTTON_STYLEOPTIONTYPE_IDX           = 389,
    SBK_QSTYLEOPTIONTOOLBUTTON_STYLEOPTIONVERSION_IDX        = 390,
    SBK_QSTYLEOPTIONTOOLBUTTON_TOOLBUTTONFEATURE_IDX         = 391,
    SBK_QFLAGS_QSTYLEOPTIONTOOLBUTTON_TOOLBUTTONFEATURE_IDX  = 113,
    SBK_QSTYLEOPTIONVIEWITEM_IDX                             = 392,
    SBK_QSTYLEOPTIONVIEWITEM_STYLEOPTIONTYPE_IDX             = 394,
    SBK_QSTYLEOPTIONVIEWITEM_STYLEOPTIONVERSION_IDX          = 395,
    SBK_QSTYLEOPTIONVIEWITEM_POSITION_IDX                    = 393,
    SBK_QSTYLEOPTIONVIEWITEM_VIEWITEMFEATURE_IDX             = 396,
    SBK_QFLAGS_QSTYLEOPTIONVIEWITEM_VIEWITEMFEATURE_IDX      = 114,
    SBK_QSTYLEOPTIONVIEWITEM_VIEWITEMPOSITION_IDX            = 397,
    SBK_QSTYLEPAINTER_IDX                                    = 398,
    SBK_QSTYLEDITEMDELEGATE_IDX                              = 399,
    SBK_QSWIPEGESTURE_IDX                                    = 400,
    SBK_QSWIPEGESTURE_SWIPEDIRECTION_IDX                     = 401,
    SBK_QSYSTEMTRAYICON_IDX                                  = 402,
    SBK_QSYSTEMTRAYICON_ACTIVATIONREASON_IDX                 = 403,
    SBK_QSYSTEMTRAYICON_MESSAGEICON_IDX                      = 404,
    SBK_QTABBAR_IDX                                          = 405,
    SBK_QTABBAR_SHAPE_IDX                                    = 408,
    SBK_QTABBAR_BUTTONPOSITION_IDX                           = 406,
    SBK_QTABBAR_SELECTIONBEHAVIOR_IDX                        = 407,
    SBK_QTABWIDGET_IDX                                       = 409,
    SBK_QTABWIDGET_TABPOSITION_IDX                           = 410,
    SBK_QTABWIDGET_TABSHAPE_IDX                              = 411,
    SBK_QTABLEVIEW_IDX                                       = 412,
    SBK_QTABLEWIDGET_IDX                                     = 413,
    SBK_QTABLEWIDGETITEM_IDX                                 = 414,
    SBK_QTABLEWIDGETITEM_ITEMTYPE_IDX                        = 415,
    SBK_QTABLEWIDGETSELECTIONRANGE_IDX                       = 416,
    SBK_QTAPANDHOLDGESTURE_IDX                               = 417,
    SBK_QTAPGESTURE_IDX                                      = 418,
    SBK_QTEXTBROWSER_IDX                                     = 419,
    SBK_QTEXTEDIT_IDX                                        = 420,
    SBK_QTEXTEDIT_LINEWRAPMODE_IDX                           = 423,
    SBK_QTEXTEDIT_AUTOFORMATTINGFLAG_IDX                     = 421,
    SBK_QFLAGS_QTEXTEDIT_AUTOFORMATTINGFLAG_IDX              = 115,
    SBK_QTEXTEDIT_EXTRASELECTION_IDX                         = 422,
    SBK_QTILERULES_IDX                                       = 424,
    SBK_QTIMEEDIT_IDX                                        = 425,
    SBK_QTOOLBAR_IDX                                         = 426,
    SBK_QTOOLBOX_IDX                                         = 427,
    SBK_QTOOLBUTTON_IDX                                      = 428,
    SBK_QTOOLBUTTON_TOOLBUTTONPOPUPMODE_IDX                  = 429,
    SBK_QTOOLTIP_IDX                                         = 430,
    SBK_QTREEVIEW_IDX                                        = 431,
    SBK_QTREEWIDGET_IDX                                      = 432,
    SBK_QTREEWIDGETITEM_IDX                                  = 433,
    SBK_QTREEWIDGETITEM_ITEMTYPE_IDX                         = 435,
    SBK_QTREEWIDGETITEM_CHILDINDICATORPOLICY_IDX             = 434,
    SBK_QTREEWIDGETITEMITERATOR_IDX                          = 436,
    SBK_QTREEWIDGETITEMITERATOR_ITERATORFLAG_IDX             = 437,
    SBK_QFLAGS_QTREEWIDGETITEMITERATOR_ITERATORFLAG_IDX      = 116,
    SBK_QUNDOCOMMAND_IDX                                     = 438,
    SBK_QUNDOGROUP_IDX                                       = 439,
    SBK_QUNDOSTACK_IDX                                       = 440,
    SBK_QUNDOVIEW_IDX                                        = 441,
    SBK_QVBOXLAYOUT_IDX                                      = 442,
    SBK_QWHATSTHIS_IDX                                       = 443,
    SBK_QWIDGET_IDX                                          = 444,
    SBK_QWIDGET_RENDERFLAG_IDX                               = 445,
    SBK_QFLAGS_QWIDGET_RENDERFLAG_IDX                        = 117,
    SBK_QWIDGETACTION_IDX                                    = 446,
    SBK_QWIDGETITEM_IDX                                      = 447,
    SBK_QWIZARD_IDX                                          = 448,
    SBK_QWIZARD_WIZARDBUTTON_IDX                             = 449,
    SBK_QWIZARD_WIZARDPIXMAP_IDX                             = 451,
    SBK_QWIZARD_WIZARDSTYLE_IDX                              = 452,
    SBK_QWIZARD_WIZARDOPTION_IDX                             = 450,
    SBK_QFLAGS_QWIZARD_WIZARDOPTION_IDX                      = 118,
    SBK_QWIZARDPAGE_IDX                                      = 453,
    SBK_QtWidgets_IDX_COUNT                                  = 454
};
// This variable stores all Python types exported by this module.
extern PyTypeObject **SbkPySide2_QtWidgetsTypes;

// This variable stores the Python module object exported by this module.
extern PyObject *SbkPySide2_QtWidgetsModuleObject;

// This variable stores all type converters exported by this module.
extern SbkConverter **SbkPySide2_QtWidgetsTypeConverters;

// Converter indices
enum : int {
    SBK_QTWIDGETS_QLIST_QACTIONPTR_IDX                       = 0, // QList<QAction* >
    SBK_QTWIDGETS_QLIST_QGRAPHICSITEMPTR_IDX                 = 1, // QList<QGraphicsItem* >
    SBK_QTWIDGETS_QLIST_QGRAPHICSTRANSFORMPTR_IDX            = 2, // const QList<QGraphicsTransform* > &
    SBK_QTWIDGETS_QLIST_QOBJECTPTR_IDX                       = 3, // const QList<QObject* > &
    SBK_QTWIDGETS_QLIST_QBYTEARRAY_IDX                       = 4, // QList<QByteArray >
    SBK_QTWIDGETS_QVECTOR_INT_IDX                            = 5, // QVector<int >
    SBK_QTWIDGETS_QLIST_QWIDGETPTR_IDX                       = 6, // QList<QWidget* >
    SBK_QTWIDGETS_QPAIR_QACCESSIBLEINTERFACEPTR_QFLAGS_QACCESSIBLE_RELATIONFLAG_IDX = 7, // QPair<QAccessibleInterface*,QFlags<QAccessible::RelationFlag> >
    SBK_QTWIDGETS_QVECTOR_QPAIR_QACCESSIBLEINTERFACEPTR_QFLAGS_QACCESSIBLE_RELATIONFLAG_IDX = 8, // QVector<QPair< QAccessibleInterface*,QFlags<QAccessible::RelationFlag> > >
    SBK_QTWIDGETS_QLIST_QGRAPHICSWIDGETPTR_IDX               = 9, // QList<QGraphicsWidget* >
    SBK_QTWIDGETS_QLIST_QKEYSEQUENCE_IDX                     = 10, // const QList<QKeySequence > &
    SBK_QTWIDGETS_QLIST_QWINDOWPTR_IDX                       = 11, // QList<QWindow* >
    SBK_QTWIDGETS_QLIST_QSCREENPTR_IDX                       = 12, // QList<QScreen* >
    SBK_QTWIDGETS_QLIST_QABSTRACTBUTTONPTR_IDX               = 13, // QList<QAbstractButton* >
    SBK_QTWIDGETS_QMAP_QDATE_QTEXTCHARFORMAT_IDX             = 14, // QMap<QDate,QTextCharFormat >
    SBK_QTWIDGETS_QVECTOR_QCOLOR_IDX                         = 15, // const QVector<QColor >
    SBK_QTWIDGETS_QLIST_INT_IDX                              = 16, // QList<int >
    SBK_QTWIDGETS_QHASH_INT_QBYTEARRAY_IDX                   = 17, // const QHash<int,QByteArray > &
    SBK_QTWIDGETS_QMAP_INT_QVARIANT_IDX                      = 18, // QMap<int,QVariant >
    SBK_QTWIDGETS_QLIST_QPERSISTENTMODELINDEX_IDX            = 19, // const QList<QPersistentModelIndex > &
    SBK_QTWIDGETS_QLIST_QURL_IDX                             = 20, // QList<QUrl >
    SBK_QTWIDGETS_QLIST_QGESTUREPTR_IDX                      = 21, // const QList<QGesture* > &
    SBK_QTWIDGETS_QPAIR_QREAL_QPOINTF_IDX                    = 22, // QPair<qreal,QPointF >
    SBK_QTWIDGETS_QLIST_QPAIR_QREAL_QPOINTF_IDX              = 23, // QList<QPair< qreal,QPointF > >
    SBK_QTWIDGETS_QPAIR_QREAL_QREAL_IDX                      = 24, // QPair<qreal,qreal >
    SBK_QTWIDGETS_QLIST_QPAIR_QREAL_QREAL_IDX                = 25, // QList<QPair< qreal,qreal > >
    SBK_QTWIDGETS_QLIST_QRECTF_IDX                           = 26, // const QList<QRectF > &
    SBK_QTWIDGETS_QLIST_QGRAPHICSVIEWPTR_IDX                 = 27, // QList<QGraphicsView* >
    SBK_QTWIDGETS_QLIST_QABSTRACTANIMATIONPTR_IDX            = 28, // QList<QAbstractAnimation* >
    SBK_QTWIDGETS_QLIST_QABSTRACTSTATEPTR_IDX                = 29, // const QList<QAbstractState* > &
    SBK_QTWIDGETS_QLIST_QLISTWIDGETITEMPTR_IDX               = 30, // QList<QListWidgetItem* >
    SBK_QTWIDGETS_QLIST_QDOCKWIDGETPTR_IDX                   = 31, // const QList<QDockWidget* > &
    SBK_QTWIDGETS_QLIST_QMDISUBWINDOWPTR_IDX                 = 32, // QList<QMdiSubWindow* >
    SBK_QTWIDGETS_QLIST_QTEXTEDIT_EXTRASELECTION_IDX         = 33, // QList<QTextEdit::ExtraSelection >
    SBK_QTWIDGETS_QLIST_QSCROLLERPTR_IDX                     = 34, // QList<QScroller* >
    SBK_QTWIDGETS_QLIST_QREAL_IDX                            = 35, // const QList<qreal > &
    SBK_QTWIDGETS_QVECTOR_QLINE_IDX                          = 36, // const QVector<QLine > &
    SBK_QTWIDGETS_QVECTOR_QLINEF_IDX                         = 37, // const QVector<QLineF > &
    SBK_QTWIDGETS_QVECTOR_QPOINT_IDX                         = 38, // const QVector<QPoint > &
    SBK_QTWIDGETS_QVECTOR_QPOINTF_IDX                        = 39, // const QVector<QPointF > &
    SBK_QTWIDGETS_QVECTOR_QRECT_IDX                          = 40, // const QVector<QRect > &
    SBK_QTWIDGETS_QVECTOR_QRECTF_IDX                         = 41, // const QVector<QRectF > &
    SBK_QTWIDGETS_QLIST_QTABLEWIDGETITEMPTR_IDX              = 42, // QList<QTableWidgetItem* >
    SBK_QTWIDGETS_QLIST_QTABLEWIDGETSELECTIONRANGE_IDX       = 43, // QList<QTableWidgetSelectionRange >
    SBK_QTWIDGETS_QLIST_QTREEWIDGETITEMPTR_IDX               = 44, // const QList<QTreeWidgetItem* > &
    SBK_QTWIDGETS_QLIST_QUNDOSTACKPTR_IDX                    = 45, // QList<QUndoStack* >
    SBK_QTWIDGETS_QLIST_QWIZARD_WIZARDBUTTON_IDX             = 46, // const QList<QWizard::WizardButton > &
    SBK_QTWIDGETS_QLIST_QVARIANT_IDX                         = 47, // QList<QVariant >
    SBK_QTWIDGETS_QLIST_QSTRING_IDX                          = 48, // QList<QString >
    SBK_QTWIDGETS_QMAP_QSTRING_QVARIANT_IDX                  = 49, // QMap<QString,QVariant >
    SBK_QtWidgets_CONVERTERS_IDX_COUNT                       = 50
};
// Macros for type check

namespace Shiboken
{

// PyType functions, to get the PyObjectType for a type T
template<> inline PyTypeObject *SbkType< ::QAbstractButton >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTBUTTON_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractGraphicsShapeItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTGRAPHICSSHAPEITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractItemDelegate::EndEditHint >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMDELEGATE_ENDEDITHINT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemDelegate >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMDELEGATE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::SelectionMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_SELECTIONMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::SelectionBehavior >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_SELECTIONBEHAVIOR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::ScrollHint >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_SCROLLHINT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::EditTrigger >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_EDITTRIGGER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QAbstractItemView::EditTrigger> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QABSTRACTITEMVIEW_EDITTRIGGER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::ScrollMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_SCROLLMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::DragDropMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_DRAGDROPMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::CursorAction >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_CURSORACTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::State >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_STATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView::DropIndicatorPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_DROPINDICATORPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractItemView >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTITEMVIEW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractScrollArea::SizeAdjustPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSCROLLAREA_SIZEADJUSTPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractScrollArea >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSCROLLAREA_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractSlider::SliderAction >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSLIDER_SLIDERACTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractSlider::SliderChange >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSLIDER_SLIDERCHANGE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractSlider >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSLIDER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAbstractSpinBox::StepEnabledFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSPINBOX_STEPENABLEDFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QAbstractSpinBox::StepEnabledFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QABSTRACTSPINBOX_STEPENABLEDFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractSpinBox::ButtonSymbols >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSPINBOX_BUTTONSYMBOLS_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractSpinBox::CorrectionMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSPINBOX_CORRECTIONMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractSpinBox::StepType >() { return SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSPINBOX_STEPTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAbstractSpinBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QABSTRACTSPINBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAccessibleWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QACCESSIBLEWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QAction::MenuRole >() { return SbkPySide2_QtWidgetsTypes[SBK_QACTION_MENUROLE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAction::Priority >() { return SbkPySide2_QtWidgetsTypes[SBK_QACTION_PRIORITY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAction::ActionEvent >() { return SbkPySide2_QtWidgetsTypes[SBK_QACTION_ACTIONEVENT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QAction >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QACTION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QActionGroup >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QACTIONGROUP_IDX]); }
template<> inline PyTypeObject *SbkType< ::QApplication::ColorSpec >() { return SbkPySide2_QtWidgetsTypes[SBK_QAPPLICATION_COLORSPEC_IDX]; }
template<> inline PyTypeObject *SbkType< ::QApplication >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QAPPLICATION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QBoxLayout::Direction >() { return SbkPySide2_QtWidgetsTypes[SBK_QBOXLAYOUT_DIRECTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QBoxLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QBOXLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QButtonGroup >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QBUTTONGROUP_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCalendarWidget::HorizontalHeaderFormat >() { return SbkPySide2_QtWidgetsTypes[SBK_QCALENDARWIDGET_HORIZONTALHEADERFORMAT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCalendarWidget::VerticalHeaderFormat >() { return SbkPySide2_QtWidgetsTypes[SBK_QCALENDARWIDGET_VERTICALHEADERFORMAT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCalendarWidget::SelectionMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QCALENDARWIDGET_SELECTIONMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCalendarWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCALENDARWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCheckBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCHECKBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QColorDialog::ColorDialogOption >() { return SbkPySide2_QtWidgetsTypes[SBK_QCOLORDIALOG_COLORDIALOGOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QColorDialog::ColorDialogOption> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QCOLORDIALOG_COLORDIALOGOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QColorDialog >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCOLORDIALOG_IDX]); }
template<> inline PyTypeObject *SbkType< ::QColormap::Mode >() { return SbkPySide2_QtWidgetsTypes[SBK_QCOLORMAP_MODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QColormap >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCOLORMAP_IDX]); }
template<> inline PyTypeObject *SbkType< ::QColumnView >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCOLUMNVIEW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QComboBox::InsertPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QCOMBOBOX_INSERTPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QComboBox::SizeAdjustPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QCOMBOBOX_SIZEADJUSTPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QComboBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCOMBOBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCommandLinkButton >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCOMMANDLINKBUTTON_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCommonStyle >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCOMMONSTYLE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QCompleter::CompletionMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QCOMPLETER_COMPLETIONMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCompleter::ModelSorting >() { return SbkPySide2_QtWidgetsTypes[SBK_QCOMPLETER_MODELSORTING_IDX]; }
template<> inline PyTypeObject *SbkType< ::QCompleter >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QCOMPLETER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDataWidgetMapper::SubmitPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QDATAWIDGETMAPPER_SUBMITPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QDataWidgetMapper >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDATAWIDGETMAPPER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDateEdit >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDATEEDIT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDateTimeEdit::Section >() { return SbkPySide2_QtWidgetsTypes[SBK_QDATETIMEEDIT_SECTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QDateTimeEdit::Section> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QDATETIMEEDIT_SECTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QDateTimeEdit >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDATETIMEEDIT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDesktopWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDESKTOPWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDial >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDIAL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDialog::DialogCode >() { return SbkPySide2_QtWidgetsTypes[SBK_QDIALOG_DIALOGCODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QDialog >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDIALOG_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDialogButtonBox::ButtonRole >() { return SbkPySide2_QtWidgetsTypes[SBK_QDIALOGBUTTONBOX_BUTTONROLE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QDialogButtonBox::StandardButton >() { return SbkPySide2_QtWidgetsTypes[SBK_QDIALOGBUTTONBOX_STANDARDBUTTON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QDialogButtonBox::StandardButton> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QDIALOGBUTTONBOX_STANDARDBUTTON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QDialogButtonBox::ButtonLayout >() { return SbkPySide2_QtWidgetsTypes[SBK_QDIALOGBUTTONBOX_BUTTONLAYOUT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QDialogButtonBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDIALOGBUTTONBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDirModel::Roles >() { return SbkPySide2_QtWidgetsTypes[SBK_QDIRMODEL_ROLES_IDX]; }
template<> inline PyTypeObject *SbkType< ::QDirModel >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDIRMODEL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDockWidget::DockWidgetFeature >() { return SbkPySide2_QtWidgetsTypes[SBK_QDOCKWIDGET_DOCKWIDGETFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QDockWidget::DockWidgetFeature> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QDOCKWIDGET_DOCKWIDGETFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QDockWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDOCKWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QDoubleSpinBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QDOUBLESPINBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QErrorMessage >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QERRORMESSAGE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QFileDialog::ViewMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QFILEDIALOG_VIEWMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFileDialog::FileMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QFILEDIALOG_FILEMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFileDialog::AcceptMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QFILEDIALOG_ACCEPTMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFileDialog::DialogLabel >() { return SbkPySide2_QtWidgetsTypes[SBK_QFILEDIALOG_DIALOGLABEL_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFileDialog::Option >() { return SbkPySide2_QtWidgetsTypes[SBK_QFILEDIALOG_OPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QFileDialog::Option> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QFILEDIALOG_OPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFileDialog >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QFILEDIALOG_IDX]); }
template<> inline PyTypeObject *SbkType< ::QFileIconProvider::IconType >() { return SbkPySide2_QtWidgetsTypes[SBK_QFILEICONPROVIDER_ICONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFileIconProvider::Option >() { return SbkPySide2_QtWidgetsTypes[SBK_QFILEICONPROVIDER_OPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QFileIconProvider::Option> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QFILEICONPROVIDER_OPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFileIconProvider >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QFILEICONPROVIDER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QFileSystemModel::Roles >() { return SbkPySide2_QtWidgetsTypes[SBK_QFILESYSTEMMODEL_ROLES_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFileSystemModel >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QFILESYSTEMMODEL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QFocusFrame >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QFOCUSFRAME_IDX]); }
template<> inline PyTypeObject *SbkType< ::QFontComboBox::FontFilter >() { return SbkPySide2_QtWidgetsTypes[SBK_QFONTCOMBOBOX_FONTFILTER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QFontComboBox::FontFilter> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QFONTCOMBOBOX_FONTFILTER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFontComboBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QFONTCOMBOBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QFontDialog::FontDialogOption >() { return SbkPySide2_QtWidgetsTypes[SBK_QFONTDIALOG_FONTDIALOGOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QFontDialog::FontDialogOption> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QFONTDIALOG_FONTDIALOGOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFontDialog >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QFONTDIALOG_IDX]); }
template<> inline PyTypeObject *SbkType< ::QFormLayout::FieldGrowthPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QFORMLAYOUT_FIELDGROWTHPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFormLayout::RowWrapPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QFORMLAYOUT_ROWWRAPPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFormLayout::ItemRole >() { return SbkPySide2_QtWidgetsTypes[SBK_QFORMLAYOUT_ITEMROLE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFormLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QFORMLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QFrame::Shape >() { return SbkPySide2_QtWidgetsTypes[SBK_QFRAME_SHAPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFrame::Shadow >() { return SbkPySide2_QtWidgetsTypes[SBK_QFRAME_SHADOW_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFrame::StyleMask >() { return SbkPySide2_QtWidgetsTypes[SBK_QFRAME_STYLEMASK_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFrame >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QFRAME_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGesture::GestureCancelPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QGESTURE_GESTURECANCELPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGesture >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGESTURE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGestureEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGESTUREEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGestureRecognizer::ResultFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QGESTURERECOGNIZER_RESULTFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QGestureRecognizer::ResultFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QGESTURERECOGNIZER_RESULTFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGestureRecognizer >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGESTURERECOGNIZER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsAnchor >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSANCHOR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsAnchorLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSANCHORLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsBlurEffect::BlurHint >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSBLUREFFECT_BLURHINT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QGraphicsBlurEffect::BlurHint> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QGRAPHICSBLUREFFECT_BLURHINT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsBlurEffect >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSBLUREFFECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsColorizeEffect >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSCOLORIZEEFFECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsDropShadowEffect >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSDROPSHADOWEFFECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsEffect::ChangeFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSEFFECT_CHANGEFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QGraphicsEffect::ChangeFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QGRAPHICSEFFECT_CHANGEFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsEffect::PixmapPadMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSEFFECT_PIXMAPPADMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsEffect >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSEFFECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsEllipseItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSELLIPSEITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsGridLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSGRIDLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsItem::GraphicsItemFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSITEM_GRAPHICSITEMFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QGraphicsItem::GraphicsItemFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QGRAPHICSITEM_GRAPHICSITEMFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsItem::GraphicsItemChange >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSITEM_GRAPHICSITEMCHANGE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsItem::CacheMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSITEM_CACHEMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsItem::PanelModality >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSITEM_PANELMODALITY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsItem::Extension >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSITEM_EXTENSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsItemAnimation >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSITEMANIMATION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsItemGroup >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSITEMGROUP_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsLayoutItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSLAYOUTITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsLineItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSLINEITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsLinearLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSLINEARLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsObject >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSOBJECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsOpacityEffect >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSOPACITYEFFECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsPathItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSPATHITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsPixmapItem::ShapeMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSPIXMAPITEM_SHAPEMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsPixmapItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSPIXMAPITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsPolygonItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSPOLYGONITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsProxyWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSPROXYWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsRectItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSRECTITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsRotation >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSROTATION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsScale >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCALE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsScene::ItemIndexMethod >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENE_ITEMINDEXMETHOD_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsScene::SceneLayer >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENE_SCENELAYER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QGraphicsScene::SceneLayer> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QGRAPHICSSCENE_SCENELAYER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsScene >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneContextMenuEvent::Reason >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENECONTEXTMENUEVENT_REASON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneContextMenuEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENECONTEXTMENUEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneDragDropEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENEDRAGDROPEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENEEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneHelpEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENEHELPEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneHoverEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENEHOVEREVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneMouseEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENEMOUSEEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneMoveEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENEMOVEEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneResizeEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENERESIZEEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSceneWheelEvent >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSCENEWHEELEVENT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsSimpleTextItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSSIMPLETEXTITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsTextItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSTEXTITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsTransform >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSTRANSFORM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsView::ViewportAnchor >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSVIEW_VIEWPORTANCHOR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsView::CacheModeFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSVIEW_CACHEMODEFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QGraphicsView::CacheModeFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QGRAPHICSVIEW_CACHEMODEFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsView::DragMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSVIEW_DRAGMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsView::ViewportUpdateMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSVIEW_VIEWPORTUPDATEMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsView::OptimizationFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSVIEW_OPTIMIZATIONFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QGraphicsView::OptimizationFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QGRAPHICSVIEW_OPTIMIZATIONFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QGraphicsView >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSVIEW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGraphicsWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRAPHICSWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGridLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGRIDLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QGroupBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QGROUPBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QHBoxLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QHBOXLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QHeaderView::ResizeMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QHEADERVIEW_RESIZEMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QHeaderView >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QHEADERVIEW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QInputDialog::InputDialogOption >() { return SbkPySide2_QtWidgetsTypes[SBK_QINPUTDIALOG_INPUTDIALOGOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QInputDialog::InputMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QINPUTDIALOG_INPUTMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QInputDialog >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QINPUTDIALOG_IDX]); }
template<> inline PyTypeObject *SbkType< ::QItemDelegate >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QITEMDELEGATE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QItemEditorCreatorBase >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QITEMEDITORCREATORBASE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QItemEditorFactory >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QITEMEDITORFACTORY_IDX]); }
template<> inline PyTypeObject *SbkType< ::QKeyEventTransition >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QKEYEVENTTRANSITION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QKeySequenceEdit >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QKEYSEQUENCEEDIT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QLCDNumber::Mode >() { return SbkPySide2_QtWidgetsTypes[SBK_QLCDNUMBER_MODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QLCDNumber::SegmentStyle >() { return SbkPySide2_QtWidgetsTypes[SBK_QLCDNUMBER_SEGMENTSTYLE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QLCDNumber >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QLCDNUMBER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QLabel >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QLABEL_IDX]); }
template<> inline PyTypeObject *SbkType< ::QLayout::SizeConstraint >() { return SbkPySide2_QtWidgetsTypes[SBK_QLAYOUT_SIZECONSTRAINT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QLayoutItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QLAYOUTITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QLineEdit::ActionPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QLINEEDIT_ACTIONPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QLineEdit::EchoMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QLINEEDIT_ECHOMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QLineEdit >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QLINEEDIT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QListView::Movement >() { return SbkPySide2_QtWidgetsTypes[SBK_QLISTVIEW_MOVEMENT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QListView::Flow >() { return SbkPySide2_QtWidgetsTypes[SBK_QLISTVIEW_FLOW_IDX]; }
template<> inline PyTypeObject *SbkType< ::QListView::ResizeMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QLISTVIEW_RESIZEMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QListView::LayoutMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QLISTVIEW_LAYOUTMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QListView::ViewMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QLISTVIEW_VIEWMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QListView >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QLISTVIEW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QListWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QLISTWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QListWidgetItem::ItemType >() { return SbkPySide2_QtWidgetsTypes[SBK_QLISTWIDGETITEM_ITEMTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QListWidgetItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QLISTWIDGETITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMainWindow::DockOption >() { return SbkPySide2_QtWidgetsTypes[SBK_QMAINWINDOW_DOCKOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QMainWindow::DockOption> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QMAINWINDOW_DOCKOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMainWindow >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QMAINWINDOW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMdiArea::AreaOption >() { return SbkPySide2_QtWidgetsTypes[SBK_QMDIAREA_AREAOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QMdiArea::AreaOption> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QMDIAREA_AREAOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMdiArea::WindowOrder >() { return SbkPySide2_QtWidgetsTypes[SBK_QMDIAREA_WINDOWORDER_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMdiArea::ViewMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QMDIAREA_VIEWMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMdiArea >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QMDIAREA_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMdiSubWindow::SubWindowOption >() { return SbkPySide2_QtWidgetsTypes[SBK_QMDISUBWINDOW_SUBWINDOWOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QMdiSubWindow::SubWindowOption> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QMDISUBWINDOW_SUBWINDOWOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMdiSubWindow >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QMDISUBWINDOW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMenu >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QMENU_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMenuBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QMENUBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMessageBox::Icon >() { return SbkPySide2_QtWidgetsTypes[SBK_QMESSAGEBOX_ICON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMessageBox::ButtonRole >() { return SbkPySide2_QtWidgetsTypes[SBK_QMESSAGEBOX_BUTTONROLE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMessageBox::StandardButton >() { return SbkPySide2_QtWidgetsTypes[SBK_QMESSAGEBOX_STANDARDBUTTON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QMessageBox::StandardButton> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QMESSAGEBOX_STANDARDBUTTON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QMessageBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QMESSAGEBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QMouseEventTransition >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QMOUSEEVENTTRANSITION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QOpenGLWidget::UpdateBehavior >() { return SbkPySide2_QtWidgetsTypes[SBK_QOPENGLWIDGET_UPDATEBEHAVIOR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QOpenGLWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QOPENGLWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QPanGesture >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QPANGESTURE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QPinchGesture::ChangeFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QPINCHGESTURE_CHANGEFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QPinchGesture::ChangeFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QPINCHGESTURE_CHANGEFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QPinchGesture >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QPINCHGESTURE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QPlainTextDocumentLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QPLAINTEXTDOCUMENTLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QPlainTextEdit::LineWrapMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QPLAINTEXTEDIT_LINEWRAPMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QPlainTextEdit >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QPLAINTEXTEDIT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QProgressBar::Direction >() { return SbkPySide2_QtWidgetsTypes[SBK_QPROGRESSBAR_DIRECTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QProgressBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QPROGRESSBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QProgressDialog >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QPROGRESSDIALOG_IDX]); }
template<> inline PyTypeObject *SbkType< ::QProxyStyle >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QPROXYSTYLE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QPushButton >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QPUSHBUTTON_IDX]); }
template<> inline PyTypeObject *SbkType< ::QRadioButton >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QRADIOBUTTON_IDX]); }
template<> inline PyTypeObject *SbkType< ::QRubberBand::Shape >() { return SbkPySide2_QtWidgetsTypes[SBK_QRUBBERBAND_SHAPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QRubberBand >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QRUBBERBAND_IDX]); }
template<> inline PyTypeObject *SbkType< ::QScrollArea >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSCROLLAREA_IDX]); }
template<> inline PyTypeObject *SbkType< ::QScrollBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSCROLLBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QScroller::State >() { return SbkPySide2_QtWidgetsTypes[SBK_QSCROLLER_STATE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QScroller::ScrollerGestureType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSCROLLER_SCROLLERGESTURETYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QScroller::Input >() { return SbkPySide2_QtWidgetsTypes[SBK_QSCROLLER_INPUT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QScroller >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSCROLLER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QScrollerProperties::OvershootPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QSCROLLERPROPERTIES_OVERSHOOTPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QScrollerProperties::FrameRates >() { return SbkPySide2_QtWidgetsTypes[SBK_QSCROLLERPROPERTIES_FRAMERATES_IDX]; }
template<> inline PyTypeObject *SbkType< ::QScrollerProperties::ScrollMetric >() { return SbkPySide2_QtWidgetsTypes[SBK_QSCROLLERPROPERTIES_SCROLLMETRIC_IDX]; }
template<> inline PyTypeObject *SbkType< ::QScrollerProperties >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSCROLLERPROPERTIES_IDX]); }
template<> inline PyTypeObject *SbkType< ::QShortcut >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSHORTCUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSizeGrip >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSIZEGRIP_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSizePolicy::PolicyFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QSIZEPOLICY_POLICYFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSizePolicy::Policy >() { return SbkPySide2_QtWidgetsTypes[SBK_QSIZEPOLICY_POLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSizePolicy::ControlType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSIZEPOLICY_CONTROLTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QSizePolicy::ControlType> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSIZEPOLICY_CONTROLTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSizePolicy >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSIZEPOLICY_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSlider::TickPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSLIDER_TICKPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSlider >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSLIDER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSpacerItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSPACERITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSpinBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSPINBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSplashScreen >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSPLASHSCREEN_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSplitter >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSPLITTER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSplitterHandle >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSPLITTERHANDLE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStackedLayout::StackingMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTACKEDLAYOUT_STACKINGMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStackedLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTACKEDLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStackedWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTACKEDWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStatusBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTATUSBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyle::StateFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_STATEFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyle::StateFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLE_STATEFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::PrimitiveElement >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_PRIMITIVEELEMENT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::ControlElement >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_CONTROLELEMENT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::SubElement >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_SUBELEMENT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::ComplexControl >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_COMPLEXCONTROL_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::SubControl >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_SUBCONTROL_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyle::SubControl> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLE_SUBCONTROL_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::PixelMetric >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_PIXELMETRIC_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::ContentsType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_CONTENTSTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::RequestSoftwareInputPanel >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_REQUESTSOFTWAREINPUTPANEL_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::StyleHint >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_STYLEHINT_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle::StandardPixmap >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_STANDARDPIXMAP_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyle >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleFactory >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEFACTORY_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturn::HintReturnType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURN_HINTRETURNTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturn::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURN_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturn::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURN_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturn >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURN_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturnMask::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURNMASK_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturnMask::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURNMASK_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturnMask >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURNMASK_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturnVariant::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURNVARIANT_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturnVariant::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURNVARIANT_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleHintReturnVariant >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEHINTRETURNVARIANT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOption::OptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTION_OPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOption::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTION_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOption::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTION_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOption >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionButton::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONBUTTON_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionButton::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONBUTTON_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionButton::ButtonFeature >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONBUTTON_BUTTONFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyleOptionButton::ButtonFeature> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLEOPTIONBUTTON_BUTTONFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionButton >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONBUTTON_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionComboBox::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONCOMBOBOX_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionComboBox::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONCOMBOBOX_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionComboBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONCOMBOBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionComplex::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONCOMPLEX_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionComplex::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONCOMPLEX_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionComplex >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONCOMPLEX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionDockWidget::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONDOCKWIDGET_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionDockWidget::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONDOCKWIDGET_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionDockWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONDOCKWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionFocusRect::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONFOCUSRECT_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionFocusRect::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONFOCUSRECT_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionFocusRect >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONFOCUSRECT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionFrame::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONFRAME_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionFrame::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONFRAME_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionFrame::FrameFeature >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONFRAME_FRAMEFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyleOptionFrame::FrameFeature> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLEOPTIONFRAME_FRAMEFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionFrame >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONFRAME_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionGraphicsItem::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONGRAPHICSITEM_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionGraphicsItem::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONGRAPHICSITEM_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionGraphicsItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONGRAPHICSITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionGroupBox::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONGROUPBOX_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionGroupBox::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONGROUPBOX_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionGroupBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONGROUPBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionHeader::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONHEADER_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionHeader::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONHEADER_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionHeader::SectionPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONHEADER_SECTIONPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionHeader::SelectedPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONHEADER_SELECTEDPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionHeader::SortIndicator >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONHEADER_SORTINDICATOR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionHeader >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONHEADER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionMenuItem::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONMENUITEM_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionMenuItem::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONMENUITEM_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionMenuItem::MenuItemType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONMENUITEM_MENUITEMTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionMenuItem::CheckType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONMENUITEM_CHECKTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionMenuItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONMENUITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionProgressBar::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONPROGRESSBAR_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionProgressBar::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONPROGRESSBAR_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionProgressBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONPROGRESSBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionRubberBand::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONRUBBERBAND_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionRubberBand::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONRUBBERBAND_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionRubberBand >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONRUBBERBAND_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSizeGrip::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSIZEGRIP_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSizeGrip::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSIZEGRIP_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSizeGrip >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSIZEGRIP_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSlider::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSLIDER_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSlider::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSLIDER_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSlider >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSLIDER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSpinBox::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSPINBOX_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSpinBox::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSPINBOX_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionSpinBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONSPINBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTab::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTAB_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTab::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTAB_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTab::TabPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTAB_TABPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTab::SelectedPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTAB_SELECTEDPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTab::CornerWidget >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTAB_CORNERWIDGET_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyleOptionTab::CornerWidget> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLEOPTIONTAB_CORNERWIDGET_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTab::TabFeature >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTAB_TABFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyleOptionTab::TabFeature> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLEOPTIONTAB_TABFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTab >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTAB_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTabBarBase::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTABBARBASE_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTabBarBase::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTABBARBASE_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTabBarBase >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTABBARBASE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTabWidgetFrame::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTABWIDGETFRAME_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTabWidgetFrame::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTABWIDGETFRAME_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTabWidgetFrame >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTABWIDGETFRAME_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTitleBar::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTITLEBAR_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTitleBar::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTITLEBAR_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionTitleBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTITLEBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBar::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBAR_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBar::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBAR_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBar::ToolBarPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBAR_TOOLBARPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBar::ToolBarFeature >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBAR_TOOLBARFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyleOptionToolBar::ToolBarFeature> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLEOPTIONTOOLBAR_TOOLBARFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBox::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBOX_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBox::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBOX_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBox::TabPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBOX_TABPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBox::SelectedPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBOX_SELECTEDPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolButton::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBUTTON_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolButton::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBUTTON_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolButton::ToolButtonFeature >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBUTTON_TOOLBUTTONFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyleOptionToolButton::ToolButtonFeature> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLEOPTIONTOOLBUTTON_TOOLBUTTONFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionToolButton >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONTOOLBUTTON_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyleOptionViewItem::StyleOptionType >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONVIEWITEM_STYLEOPTIONTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionViewItem::StyleOptionVersion >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONVIEWITEM_STYLEOPTIONVERSION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionViewItem::Position >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONVIEWITEM_POSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionViewItem::ViewItemFeature >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONVIEWITEM_VIEWITEMFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QStyleOptionViewItem::ViewItemFeature> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QSTYLEOPTIONVIEWITEM_VIEWITEMFEATURE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionViewItem::ViewItemPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONVIEWITEM_VIEWITEMPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QStyleOptionViewItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEOPTIONVIEWITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStylePainter >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEPAINTER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QStyledItemDelegate >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSTYLEDITEMDELEGATE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSwipeGesture::SwipeDirection >() { return SbkPySide2_QtWidgetsTypes[SBK_QSWIPEGESTURE_SWIPEDIRECTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSwipeGesture >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSWIPEGESTURE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QSystemTrayIcon::ActivationReason >() { return SbkPySide2_QtWidgetsTypes[SBK_QSYSTEMTRAYICON_ACTIVATIONREASON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSystemTrayIcon::MessageIcon >() { return SbkPySide2_QtWidgetsTypes[SBK_QSYSTEMTRAYICON_MESSAGEICON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QSystemTrayIcon >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QSYSTEMTRAYICON_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTabBar::Shape >() { return SbkPySide2_QtWidgetsTypes[SBK_QTABBAR_SHAPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTabBar::ButtonPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QTABBAR_BUTTONPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTabBar::SelectionBehavior >() { return SbkPySide2_QtWidgetsTypes[SBK_QTABBAR_SELECTIONBEHAVIOR_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTabBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTABBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTabWidget::TabPosition >() { return SbkPySide2_QtWidgetsTypes[SBK_QTABWIDGET_TABPOSITION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTabWidget::TabShape >() { return SbkPySide2_QtWidgetsTypes[SBK_QTABWIDGET_TABSHAPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTabWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTABWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTableView >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTABLEVIEW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTableWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTABLEWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTableWidgetItem::ItemType >() { return SbkPySide2_QtWidgetsTypes[SBK_QTABLEWIDGETITEM_ITEMTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTableWidgetItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTABLEWIDGETITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTableWidgetSelectionRange >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTABLEWIDGETSELECTIONRANGE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTapAndHoldGesture >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTAPANDHOLDGESTURE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTapGesture >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTAPGESTURE_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTextBrowser >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTEXTBROWSER_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTextEdit::LineWrapMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QTEXTEDIT_LINEWRAPMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTextEdit::AutoFormattingFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QTEXTEDIT_AUTOFORMATTINGFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QTextEdit::AutoFormattingFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QTEXTEDIT_AUTOFORMATTINGFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTextEdit >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTEXTEDIT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTextEdit::ExtraSelection >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTEXTEDIT_EXTRASELECTION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTileRules >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTILERULES_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTimeEdit >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTIMEEDIT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QToolBar >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTOOLBAR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QToolBox >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTOOLBOX_IDX]); }
template<> inline PyTypeObject *SbkType< ::QToolButton::ToolButtonPopupMode >() { return SbkPySide2_QtWidgetsTypes[SBK_QTOOLBUTTON_TOOLBUTTONPOPUPMODE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QToolButton >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTOOLBUTTON_IDX]); }
template<> inline PyTypeObject *SbkType< ::QToolTip >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTOOLTIP_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTreeView >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTREEVIEW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTreeWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTREEWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTreeWidgetItem::ItemType >() { return SbkPySide2_QtWidgetsTypes[SBK_QTREEWIDGETITEM_ITEMTYPE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTreeWidgetItem::ChildIndicatorPolicy >() { return SbkPySide2_QtWidgetsTypes[SBK_QTREEWIDGETITEM_CHILDINDICATORPOLICY_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTreeWidgetItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTREEWIDGETITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QTreeWidgetItemIterator::IteratorFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QTREEWIDGETITEMITERATOR_ITERATORFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QTreeWidgetItemIterator::IteratorFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QTREEWIDGETITEMITERATOR_ITERATORFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QTreeWidgetItemIterator >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QTREEWIDGETITEMITERATOR_IDX]); }
template<> inline PyTypeObject *SbkType< ::QUndoCommand >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QUNDOCOMMAND_IDX]); }
template<> inline PyTypeObject *SbkType< ::QUndoGroup >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QUNDOGROUP_IDX]); }
template<> inline PyTypeObject *SbkType< ::QUndoStack >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QUNDOSTACK_IDX]); }
template<> inline PyTypeObject *SbkType< ::QUndoView >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QUNDOVIEW_IDX]); }
template<> inline PyTypeObject *SbkType< ::QVBoxLayout >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QVBOXLAYOUT_IDX]); }
template<> inline PyTypeObject *SbkType< ::QWhatsThis >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QWHATSTHIS_IDX]); }
template<> inline PyTypeObject *SbkType< ::QWidget::RenderFlag >() { return SbkPySide2_QtWidgetsTypes[SBK_QWIDGET_RENDERFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QWidget::RenderFlag> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QWIDGET_RENDERFLAG_IDX]; }
template<> inline PyTypeObject *SbkType< ::QWidget >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QWIDGET_IDX]); }
template<> inline PyTypeObject *SbkType< ::QWidgetAction >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QWIDGETACTION_IDX]); }
template<> inline PyTypeObject *SbkType< ::QWidgetItem >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QWIDGETITEM_IDX]); }
template<> inline PyTypeObject *SbkType< ::QWizard::WizardButton >() { return SbkPySide2_QtWidgetsTypes[SBK_QWIZARD_WIZARDBUTTON_IDX]; }
template<> inline PyTypeObject *SbkType< ::QWizard::WizardPixmap >() { return SbkPySide2_QtWidgetsTypes[SBK_QWIZARD_WIZARDPIXMAP_IDX]; }
template<> inline PyTypeObject *SbkType< ::QWizard::WizardStyle >() { return SbkPySide2_QtWidgetsTypes[SBK_QWIZARD_WIZARDSTYLE_IDX]; }
template<> inline PyTypeObject *SbkType< ::QWizard::WizardOption >() { return SbkPySide2_QtWidgetsTypes[SBK_QWIZARD_WIZARDOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QFlags<QWizard::WizardOption> >() { return SbkPySide2_QtWidgetsTypes[SBK_QFLAGS_QWIZARD_WIZARDOPTION_IDX]; }
template<> inline PyTypeObject *SbkType< ::QWizard >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QWIZARD_IDX]); }
template<> inline PyTypeObject *SbkType< ::QWizardPage >() { return reinterpret_cast<PyTypeObject *>(SbkPySide2_QtWidgetsTypes[SBK_QWIZARDPAGE_IDX]); }

} // namespace Shiboken

#endif // SBK_QTWIDGETS_PYTHON_H

