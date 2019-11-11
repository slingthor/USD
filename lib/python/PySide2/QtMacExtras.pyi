# This Python file uses the following encoding: utf-8
#############################################################################
##
## Copyright (C) 2019 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of Qt for Python.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU Lesser General Public License version 3 requirements
## will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 2.0 or (at your option) the GNU General
## Public license version 3 or any later version approved by the KDE Free
## Qt Foundation. The licenses are as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-2.0.html and
## https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

"""
This file contains the exact signatures for all functions in module
PySide2.QtMacExtras, except for defaults which are replaced by "...".
"""

# Module PySide2.QtMacExtras
import PySide2
from PySide2.support.signature import typing
from PySide2.support.signature.mapping import (
    Virtual, Missing, Invalid, Default, Instance)

class Object(object): pass

import shiboken2 as Shiboken
Shiboken.Object = Object

import PySide2.QtCore
import PySide2.QtGui
import PySide2.QtMacExtras


class QMacPasteboardMime(Shiboken.Object):

    class QMacPasteboardMimeType(object): ...
    MIME_CLIP                : QMacPasteboardMimeType = ... # 0x2
    MIME_QT3_CONVERTOR       : QMacPasteboardMimeType = ... # 0x8
    MIME_ALL                 : QMacPasteboardMimeType = ... # 0x3
    MIME_QT_CONVERTOR        : QMacPasteboardMimeType = ... # 0x4
    MIME_DND                 : QMacPasteboardMimeType = ... # 0x1

    def __init__(self, arg__1: Char): ...

    def canConvert(self, mime: str, flav: str) -> bool: ...
    def convertFromMime(self, mime: str, data: Any, flav: str) -> List[PySide2.QtCore.QByteArray]: ...
    def convertToMime(self, mime: str, data: List[PySide2.QtCore.QByteArray], flav: str) -> Any: ...
    def convertorName(self) -> str: ...
    def count(self, mimeData: PySide2.QtCore.QMimeData) -> int: ...
    def flavorFor(self, mime: str) -> str: ...
    def mimeFor(self, flav: str) -> str: ...


class QMacToolBar(PySide2.QtCore.QObject):

    @typing.overload
    def __init__(self, identifier: str, parent: Optional[PySide2.QtCore.QObject]=...): ...
    @typing.overload
    def __init__(self, parent: Optional[PySide2.QtCore.QObject]=...): ...

    def addAllowedItem(self, icon: PySide2.QtGui.QIcon, text: str) -> PySide2.QtMacExtras.QMacToolBarItem: ...
    def addAllowedStandardItem(self, standardItem: PySide2.QtMacExtras.QMacToolBarItem.StandardItem) -> PySide2.QtMacExtras.QMacToolBarItem: ...
    def addItem(self, icon: PySide2.QtGui.QIcon, text: str) -> PySide2.QtMacExtras.QMacToolBarItem: ...
    def addSeparator(self): ...
    def addStandardItem(self, standardItem: PySide2.QtMacExtras.QMacToolBarItem.StandardItem) -> PySide2.QtMacExtras.QMacToolBarItem: ...
    def allowedItems(self) -> List[PySide2.QtMacExtras.QMacToolBarItem]: ...
    def attachToWindow(self, window: PySide2.QtGui.QWindow): ...
    def detachFromWindow(self): ...
    def items(self) -> List[PySide2.QtMacExtras.QMacToolBarItem]: ...
    def setAllowedItems(self, allowedItems: List[PySide2.QtMacExtras.QMacToolBarItem]): ...
    def setItems(self, items: List[PySide2.QtMacExtras.QMacToolBarItem]): ...


class QMacToolBarItem(PySide2.QtCore.QObject):

    class StandardItem(object): ...
    Space                    : StandardItem = ... # 0x1
    NoStandardItem           : StandardItem = ... # 0x0
    FlexibleSpace            : StandardItem = ... # 0x2

    def __init__(self, parent: Optional[PySide2.QtCore.QObject]=...): ...

    def icon(self) -> PySide2.QtGui.QIcon: ...
    def selectable(self) -> bool: ...
    def setIcon(self, icon: PySide2.QtGui.QIcon): ...
    def setSelectable(self, selectable: bool): ...
    def setStandardItem(self, standardItem: PySide2.QtMacExtras.QMacToolBarItem.StandardItem): ...
    def setText(self, text: str): ...
    def standardItem(self) -> PySide2.QtMacExtras.QMacToolBarItem.StandardItem: ...
    def text(self) -> str: ...
def qRegisterDraggedTypes(types: List[str]): ...

# eof
