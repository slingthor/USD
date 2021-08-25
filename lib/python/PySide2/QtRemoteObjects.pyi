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
PySide2.QtRemoteObjects, except for defaults which are replaced by "...".
"""

# Module PySide2.QtRemoteObjects
import PySide2
from PySide2.support.signature import typing
from PySide2.support.signature.mapping import (
    Virtual, Missing, Invalid, Default, Instance)

class Object(object): pass

import shiboken2 as Shiboken
Shiboken.Object = Object

import PySide2.QtCore
import PySide2.QtRemoteObjects


class QAbstractItemModelReplica(PySide2.QtCore.QAbstractItemModel):
    def availableRoles(self) -> List[int]: ...
    def columnCount(self, parent: PySide2.QtCore.QModelIndex=...) -> int: ...
    def data(self, index: PySide2.QtCore.QModelIndex, role: int=...) -> Any: ...
    def flags(self, index: PySide2.QtCore.QModelIndex) -> PySide2.QtCore.Qt.ItemFlags: ...
    def hasChildren(self, parent: PySide2.QtCore.QModelIndex=...) -> bool: ...
    def hasData(self, index: PySide2.QtCore.QModelIndex, role: int) -> bool: ...
    def headerData(self, section: int, orientation: PySide2.QtCore.Qt.Orientation, role: int) -> Any: ...
    def index(self, row: int, column: int, parent: PySide2.QtCore.QModelIndex=...) -> PySide2.QtCore.QModelIndex: ...
    def isInitialized(self) -> bool: ...
    @typing.overload
    def parent(self) -> PySide2.QtCore.QObject: ...
    @typing.overload
    def parent(self, index: PySide2.QtCore.QModelIndex) -> PySide2.QtCore.QModelIndex: ...
    def roleNames(self) -> Dict: ...
    def rowCount(self, parent: PySide2.QtCore.QModelIndex=...) -> int: ...
    def selectionModel(self) -> PySide2.QtCore.QItemSelectionModel: ...
    def setData(self, index: PySide2.QtCore.QModelIndex, value: Any, role: int=...) -> bool: ...


class QRemoteObjectAbstractPersistedStore(PySide2.QtCore.QObject):

    def __init__(self, parent: Optional[PySide2.QtCore.QObject]=...): ...

    def restoreProperties(self, repName: str, repSig: PySide2.QtCore.QByteArray) -> List[Any]: ...
    def saveProperties(self, repName: str, repSig: PySide2.QtCore.QByteArray, values: List[Any]): ...


class QRemoteObjectDynamicReplica(PySide2.QtRemoteObjects.QRemoteObjectReplica): ...


class QRemoteObjectHost(PySide2.QtRemoteObjects.QRemoteObjectHostBase):

    @typing.overload
    def __init__(self, address: PySide2.QtCore.QUrl, parent: PySide2.QtCore.QObject): ...
    @typing.overload
    def __init__(self, address: PySide2.QtCore.QUrl, registryAddress: PySide2.QtCore.QUrl=..., allowedSchemas: PySide2.QtRemoteObjects.QRemoteObjectHostBase.AllowedSchemas=..., parent: Optional[PySide2.QtCore.QObject]=...): ...
    @typing.overload
    def __init__(self, parent: Optional[PySide2.QtCore.QObject]=...): ...

    def hostUrl(self) -> PySide2.QtCore.QUrl: ...
    def setHostUrl(self, hostAddress: PySide2.QtCore.QUrl, allowedSchemas: PySide2.QtRemoteObjects.QRemoteObjectHostBase.AllowedSchemas=...) -> bool: ...


class QRemoteObjectHostBase(PySide2.QtRemoteObjects.QRemoteObjectNode):

    class AllowedSchemas(object): ...
    AllowExternalRegistration: AllowedSchemas = ... # 0x1
    BuiltInSchemasOnly       : AllowedSchemas = ... # 0x0
    def addHostSideConnection(self, ioDevice: PySide2.QtCore.QIODevice): ...
    def disableRemoting(self, remoteObject: PySide2.QtCore.QObject) -> bool: ...
    @typing.overload
    def enableRemoting(self, model: PySide2.QtCore.QAbstractItemModel, name: str, roles: List[int], selectionModel: Optional[PySide2.QtCore.QItemSelectionModel]=...) -> bool: ...
    @typing.overload
    def enableRemoting(self, object: PySide2.QtCore.QObject, name: str=...) -> bool: ...
    def hostUrl(self) -> PySide2.QtCore.QUrl: ...
    def proxy(self, registryUrl: PySide2.QtCore.QUrl, hostUrl: PySide2.QtCore.QUrl=...) -> bool: ...
    def reverseProxy(self) -> bool: ...
    def setHostUrl(self, hostAddress: PySide2.QtCore.QUrl, allowedSchemas: PySide2.QtRemoteObjects.QRemoteObjectHostBase.AllowedSchemas=...) -> bool: ...
    def setName(self, name: str): ...


class QRemoteObjectNode(PySide2.QtCore.QObject):

    class ErrorCode(object): ...
    RegistryAlreadyHosted    : ErrorCode = ... # 0x2
    MissingObjectName        : ErrorCode = ... # 0x8
    NodeIsNoServer           : ErrorCode = ... # 0x3
    OperationNotValidOnClientNode: ErrorCode = ... # 0x6
    ListenFailed             : ErrorCode = ... # 0xb
    ServerAlreadyCreated     : ErrorCode = ... # 0x4
    HostUrlInvalid           : ErrorCode = ... # 0x9
    NoError                  : ErrorCode = ... # 0x0
    UnintendedRegistryHosting: ErrorCode = ... # 0x5
    RegistryNotAcquired      : ErrorCode = ... # 0x1
    SourceNotRegistered      : ErrorCode = ... # 0x7
    ProtocolMismatch         : ErrorCode = ... # 0xa

    @typing.overload
    def __init__(self, parent: Optional[PySide2.QtCore.QObject]=...): ...
    @typing.overload
    def __init__(self, registryAddress: PySide2.QtCore.QUrl, parent: Optional[PySide2.QtCore.QObject]=...): ...

    def acquireDynamic(self, name: str) -> PySide2.QtRemoteObjects.QRemoteObjectDynamicReplica: ...
    def acquireModel(self, name: str) -> PySide2.QtRemoteObjects.QAbstractItemModelReplica: ...
    def addClientSideConnection(self, ioDevice: PySide2.QtCore.QIODevice): ...
    def connectToNode(self, address: PySide2.QtCore.QUrl) -> bool: ...
    def heartbeatInterval(self) -> int: ...
    def instances(self, typeName: str) -> List[str]: ...
    def lastError(self) -> PySide2.QtRemoteObjects.QRemoteObjectNode.ErrorCode: ...
    def persistedStore(self) -> PySide2.QtRemoteObjects.QRemoteObjectAbstractPersistedStore: ...
    def registry(self) -> PySide2.QtRemoteObjects.QRemoteObjectRegistry: ...
    def registryUrl(self) -> PySide2.QtCore.QUrl: ...
    def setHeartbeatInterval(self, interval: int): ...
    def setName(self, name: str): ...
    def setPersistedStore(self, persistedStore: PySide2.QtRemoteObjects.QRemoteObjectAbstractPersistedStore): ...
    def setRegistryUrl(self, registryAddress: PySide2.QtCore.QUrl) -> bool: ...
    def timerEvent(self, arg__1: PySide2.QtCore.QTimerEvent): ...
    def waitForRegistry(self, timeout: int=...) -> bool: ...


class QRemoteObjectPendingCall(Shiboken.Object):

    class Error(object): ...
    InvalidMessage           : Error = ... # 0x1
    NoError                  : Error = ... # 0x0

    @typing.overload
    def __init__(self): ...
    @typing.overload
    def __init__(self, other: PySide2.QtRemoteObjects.QRemoteObjectPendingCall): ...

    def error(self) -> PySide2.QtRemoteObjects.QRemoteObjectPendingCall.Error: ...
    @staticmethod
    def fromCompletedCall(returnValue: Any) -> PySide2.QtRemoteObjects.QRemoteObjectPendingCall: ...
    def isFinished(self) -> bool: ...
    def returnValue(self) -> Any: ...
    def waitForFinished(self, timeout: int=...) -> bool: ...


class QRemoteObjectPendingCallWatcher(PySide2.QtCore.QObject, PySide2.QtRemoteObjects.QRemoteObjectPendingCall):

    def __init__(self, call: PySide2.QtRemoteObjects.QRemoteObjectPendingCall, parent: Optional[PySide2.QtCore.QObject]=...): ...

    def isFinished(self) -> bool: ...
    @typing.overload
    def waitForFinished(self): ...
    @typing.overload
    def waitForFinished(self, timeout: int=...) -> bool: ...


class QRemoteObjectRegistry(PySide2.QtRemoteObjects.QRemoteObjectReplica):
    def initialize(self): ...
    def pushToRegistryIfNeeded(self): ...
    @staticmethod
    def registerMetatypes(): ...


class QRemoteObjectRegistryHost(PySide2.QtRemoteObjects.QRemoteObjectHostBase):

    def __init__(self, registryAddress: PySide2.QtCore.QUrl=..., parent: Optional[PySide2.QtCore.QObject]=...): ...

    def setRegistryUrl(self, registryUrl: PySide2.QtCore.QUrl) -> bool: ...


class QRemoteObjectReplica(PySide2.QtCore.QObject):

    class State(object): ...
    SignatureMismatch        : State = ... # 0x4
    Suspect                  : State = ... # 0x3
    Valid                    : State = ... # 0x2
    Uninitialized            : State = ... # 0x0
    Default                  : State = ... # 0x1

    def __init__(self): ...

    def initialize(self): ...
    def initializeNode(self, node: PySide2.QtRemoteObjects.QRemoteObjectNode, name: str=...): ...
    def isInitialized(self) -> bool: ...
    def isReplicaValid(self) -> bool: ...
    def node(self) -> PySide2.QtRemoteObjects.QRemoteObjectNode: ...
    def persistProperties(self, repName: str, repSig: PySide2.QtCore.QByteArray, props: List[Any]): ...
    def propAsVariant(self, i: int) -> Any: ...
    def retrieveProperties(self, repName: str, repSig: PySide2.QtCore.QByteArray) -> List[Any]: ...
    def send(self, call: PySide2.QtCore.QMetaObject.Call, index: int, args: List[Any]): ...
    def sendWithReply(self, call: PySide2.QtCore.QMetaObject.Call, index: int, args: List[Any]) -> PySide2.QtRemoteObjects.QRemoteObjectPendingCall: ...
    def setChild(self, i: int, arg__2: Any): ...
    def setNode(self, node: PySide2.QtRemoteObjects.QRemoteObjectNode): ...
    def setProperties(self, arg__1: List[Any]): ...
    def state(self) -> PySide2.QtRemoteObjects.QRemoteObjectReplica.State: ...
    def waitForSource(self, timeout: int=...) -> bool: ...


class QRemoteObjectSettingsStore(PySide2.QtRemoteObjects.QRemoteObjectAbstractPersistedStore):

    def __init__(self, parent: Optional[PySide2.QtCore.QObject]=...): ...

    def restoreProperties(self, repName: str, repSig: PySide2.QtCore.QByteArray) -> List[Any]: ...
    def saveProperties(self, repName: str, repSig: PySide2.QtCore.QByteArray, values: List[Any]): ...

# eof