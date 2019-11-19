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
PySide2.QtOpenGL, except for defaults which are replaced by "...".
"""

# Module PySide2.QtOpenGL
import PySide2
from PySide2.support.signature import typing
from PySide2.support.signature.mapping import (
    Virtual, Missing, Invalid, Default, Instance)

class Object(object): pass

import shiboken2 as Shiboken
Shiboken.Object = Object

import PySide2.QtCore
import PySide2.QtGui
import PySide2.QtWidgets
import PySide2.QtOpenGL


class QGL(Shiboken.Object):

    class FormatOption(object): ...
    NoStencilBuffer          : FormatOption = ... # 0x200000
    StereoBuffers            : FormatOption = ... # 0x40
    NoDeprecatedFunctions    : FormatOption = ... # 0x4000000
    SingleBuffer             : FormatOption = ... # 0x10000
    DepthBuffer              : FormatOption = ... # 0x2
    NoAccumBuffer            : FormatOption = ... # 0x100000
    Rgba                     : FormatOption = ... # 0x4
    DirectRendering          : FormatOption = ... # 0x80
    SampleBuffers            : FormatOption = ... # 0x200
    NoOverlay                : FormatOption = ... # 0x1000000
    StencilBuffer            : FormatOption = ... # 0x20
    IndirectRendering        : FormatOption = ... # 0x800000
    DeprecatedFunctions      : FormatOption = ... # 0x400
    NoDepthBuffer            : FormatOption = ... # 0x20000
    AccumBuffer              : FormatOption = ... # 0x10
    NoStereoBuffers          : FormatOption = ... # 0x400000
    ColorIndex               : FormatOption = ... # 0x40000
    AlphaChannel             : FormatOption = ... # 0x8
    NoAlphaChannel           : FormatOption = ... # 0x80000
    DoubleBuffer             : FormatOption = ... # 0x1
    HasOverlay               : FormatOption = ... # 0x100
    NoSampleBuffers          : FormatOption = ... # 0x2000000

    class FormatOptions(object): ...


class QGLBuffer(Shiboken.Object):

    class Access(object): ...
    ReadOnly                 : Access = ... # 0x88b8
    WriteOnly                : Access = ... # 0x88b9
    ReadWrite                : Access = ... # 0x88ba

    class Type(object): ...
    IndexBuffer              : Type = ... # 0x8893
    VertexBuffer             : Type = ... # 0x8892
    PixelPackBuffer          : Type = ... # 0x88eb
    PixelUnpackBuffer        : Type = ... # 0x88ec

    class UsagePattern(object): ...
    StaticCopy               : UsagePattern = ... # 0x88e6
    StaticRead               : UsagePattern = ... # 0x88e5
    DynamicRead              : UsagePattern = ... # 0x88e9
    StaticDraw               : UsagePattern = ... # 0x88e4
    StreamRead               : UsagePattern = ... # 0x88e1
    DynamicDraw              : UsagePattern = ... # 0x88e8
    StreamDraw               : UsagePattern = ... # 0x88e0
    DynamicCopy              : UsagePattern = ... # 0x88ea
    StreamCopy               : UsagePattern = ... # 0x88e2

    @typing.overload
    def __init__(self): ...
    @typing.overload
    def __init__(self, other: PySide2.QtOpenGL.QGLBuffer): ...
    @typing.overload
    def __init__(self, type: PySide2.QtOpenGL.QGLBuffer.Type): ...

    @typing.overload
    def allocate(self, count: int): ...
    @typing.overload
    def allocate(self, data: int, count: int=...): ...
    def bind(self) -> bool: ...
    def bufferId(self) -> int: ...
    def create(self) -> bool: ...
    def destroy(self): ...
    def isCreated(self) -> bool: ...
    def map(self, access: PySide2.QtOpenGL.QGLBuffer.Access) -> int: ...
    def read(self, offset: int, data: int, count: int) -> bool: ...
    @typing.overload
    @staticmethod
    def release(): ...
    @typing.overload
    @staticmethod
    def release(type: PySide2.QtOpenGL.QGLBuffer.Type): ...
    def setUsagePattern(self, value: PySide2.QtOpenGL.QGLBuffer.UsagePattern): ...
    def size(self) -> int: ...
    def type(self) -> PySide2.QtOpenGL.QGLBuffer.Type: ...
    def unmap(self) -> bool: ...
    def usagePattern(self) -> PySide2.QtOpenGL.QGLBuffer.UsagePattern: ...
    def write(self, offset: int, data: int, count: int=...): ...


class QGLColormap(Shiboken.Object):

    @typing.overload
    def __init__(self): ...
    @typing.overload
    def __init__(self, arg__1: PySide2.QtOpenGL.QGLColormap): ...

    def __copy__(self): ...
    def entryColor(self, idx: int) -> PySide2.QtGui.QColor: ...
    def entryRgb(self, idx: int) -> int: ...
    def find(self, color: int) -> int: ...
    def findNearest(self, color: int) -> int: ...
    def handle(self) -> int: ...
    def isEmpty(self) -> bool: ...
    @typing.overload
    def setEntry(self, idx: int, color: PySide2.QtGui.QColor): ...
    @typing.overload
    def setEntry(self, idx: int, color: int): ...
    def setHandle(self, ahandle: int): ...
    def size(self) -> int: ...


class QGLContext(Shiboken.Object):

    class BindOption(object): ...
    MipmapBindOption         : BindOption = ... # 0x2
    TemporarilyCachedBindOption: BindOption = ... # 0x40
    InternalBindOption       : BindOption = ... # 0x14
    CanFlipNativePixmapBindOption: BindOption = ... # 0x20
    PremultipliedAlphaBindOption: BindOption = ... # 0x4
    LinearFilteringBindOption: BindOption = ... # 0x8
    InvertedYBindOption      : BindOption = ... # 0x1
    NoBindOption             : BindOption = ... # 0x0
    DefaultBindOption        : BindOption = ... # 0xb
    MemoryManagedBindOption  : BindOption = ... # 0x10

    class BindOptions(object): ...

    def __init__(self, format: PySide2.QtOpenGL.QGLFormat): ...

    @staticmethod
    def areSharing(context1: PySide2.QtOpenGL.QGLContext, context2: PySide2.QtOpenGL.QGLContext) -> bool: ...
    @typing.overload
    def bindTexture(self, fileName: str) -> int: ...
    @typing.overload
    def bindTexture(self, image: PySide2.QtGui.QImage, target: int, format: int, options: PySide2.QtOpenGL.QGLContext.BindOptions) -> int: ...
    @typing.overload
    def bindTexture(self, image: PySide2.QtGui.QImage, target: int=..., format: int=...) -> int: ...
    @typing.overload
    def bindTexture(self, pixmap: PySide2.QtGui.QPixmap, target: int, format: int, options: PySide2.QtOpenGL.QGLContext.BindOptions) -> int: ...
    @typing.overload
    def bindTexture(self, pixmap: PySide2.QtGui.QPixmap, target: int=..., format: int=...) -> int: ...
    def chooseContext(self, shareContext: Optional[PySide2.QtOpenGL.QGLContext]=...) -> bool: ...
    def colorIndex(self, c: PySide2.QtGui.QColor) -> int: ...
    def contextHandle(self) -> PySide2.QtGui.QOpenGLContext: ...
    def create(self, shareContext: Optional[PySide2.QtOpenGL.QGLContext]=...) -> bool: ...
    @staticmethod
    def currentContext() -> PySide2.QtOpenGL.QGLContext: ...
    def deleteTexture(self, tx_id: int): ...
    def device(self) -> PySide2.QtGui.QPaintDevice: ...
    def deviceIsPixmap(self) -> bool: ...
    def doneCurrent(self): ...
    @typing.overload
    def drawTexture(self, point: PySide2.QtCore.QPointF, textureId: int, textureTarget: int=...): ...
    @typing.overload
    def drawTexture(self, target: PySide2.QtCore.QRectF, textureId: int, textureTarget: int=...): ...
    def format(self) -> PySide2.QtOpenGL.QGLFormat: ...
    @staticmethod
    def fromOpenGLContext(platformContext: PySide2.QtGui.QOpenGLContext) -> PySide2.QtOpenGL.QGLContext: ...
    def initialized(self) -> bool: ...
    def isSharing(self) -> bool: ...
    def isValid(self) -> bool: ...
    def makeCurrent(self): ...
    def moveToThread(self, thread: PySide2.QtCore.QThread): ...
    def overlayTransparentColor(self) -> PySide2.QtGui.QColor: ...
    def requestedFormat(self) -> PySide2.QtOpenGL.QGLFormat: ...
    def reset(self): ...
    def setDevice(self, pDev: PySide2.QtGui.QPaintDevice): ...
    def setFormat(self, format: PySide2.QtOpenGL.QGLFormat): ...
    def setInitialized(self, on: bool): ...
    @staticmethod
    def setTextureCacheLimit(size: int): ...
    def setValid(self, valid: bool): ...
    def setWindowCreated(self, on: bool): ...
    def swapBuffers(self): ...
    @staticmethod
    def textureCacheLimit() -> int: ...
    def windowCreated(self) -> bool: ...


class QGLFormat(Shiboken.Object):

    class OpenGLContextProfile(object): ...
    CoreProfile              : OpenGLContextProfile = ... # 0x1
    NoProfile                : OpenGLContextProfile = ... # 0x0
    CompatibilityProfile     : OpenGLContextProfile = ... # 0x2

    class OpenGLVersionFlag(object): ...
    OpenGL_Version_3_1       : OpenGLVersionFlag = ... # 0x2000
    OpenGL_Version_3_0       : OpenGLVersionFlag = ... # 0x1000
    OpenGL_Version_3_3       : OpenGLVersionFlag = ... # 0x8000
    OpenGL_Version_3_2       : OpenGLVersionFlag = ... # 0x4000
    OpenGL_Version_1_3       : OpenGLVersionFlag = ... # 0x4
    OpenGL_Version_1_2       : OpenGLVersionFlag = ... # 0x2
    OpenGL_Version_1_1       : OpenGLVersionFlag = ... # 0x1
    OpenGL_Version_1_5       : OpenGLVersionFlag = ... # 0x10
    OpenGL_Version_1_4       : OpenGLVersionFlag = ... # 0x8
    OpenGL_ES_CommonLite_Version_1_0: OpenGLVersionFlag = ... # 0x100
    OpenGL_Version_None      : OpenGLVersionFlag = ... # 0x0
    OpenGL_ES_CommonLite_Version_1_1: OpenGLVersionFlag = ... # 0x400
    OpenGL_Version_4_2       : OpenGLVersionFlag = ... # 0x40000
    OpenGL_Version_2_0       : OpenGLVersionFlag = ... # 0x20
    OpenGL_Version_4_3       : OpenGLVersionFlag = ... # 0x80000
    OpenGL_ES_Version_2_0    : OpenGLVersionFlag = ... # 0x800
    OpenGL_ES_Common_Version_1_0: OpenGLVersionFlag = ... # 0x80
    OpenGL_ES_Common_Version_1_1: OpenGLVersionFlag = ... # 0x200
    OpenGL_Version_2_1       : OpenGLVersionFlag = ... # 0x40
    OpenGL_Version_4_0       : OpenGLVersionFlag = ... # 0x10000
    OpenGL_Version_4_1       : OpenGLVersionFlag = ... # 0x20000

    class OpenGLVersionFlags(object): ...

    @typing.overload
    def __init__(self): ...
    @typing.overload
    def __init__(self, options: PySide2.QtOpenGL.QGL.FormatOptions, plane: int=...): ...
    @typing.overload
    def __init__(self, other: PySide2.QtOpenGL.QGLFormat): ...

    def __copy__(self): ...
    def accum(self) -> bool: ...
    def accumBufferSize(self) -> int: ...
    def alpha(self) -> bool: ...
    def alphaBufferSize(self) -> int: ...
    def blueBufferSize(self) -> int: ...
    @staticmethod
    def defaultFormat() -> PySide2.QtOpenGL.QGLFormat: ...
    @staticmethod
    def defaultOverlayFormat() -> PySide2.QtOpenGL.QGLFormat: ...
    def depth(self) -> bool: ...
    def depthBufferSize(self) -> int: ...
    def directRendering(self) -> bool: ...
    def doubleBuffer(self) -> bool: ...
    @staticmethod
    def fromSurfaceFormat(format: PySide2.QtGui.QSurfaceFormat) -> PySide2.QtOpenGL.QGLFormat: ...
    def greenBufferSize(self) -> int: ...
    @staticmethod
    def hasOpenGL() -> bool: ...
    @staticmethod
    def hasOpenGLOverlays() -> bool: ...
    def hasOverlay(self) -> bool: ...
    def majorVersion(self) -> int: ...
    def minorVersion(self) -> int: ...
    @staticmethod
    def openGLVersionFlags() -> PySide2.QtOpenGL.QGLFormat.OpenGLVersionFlags: ...
    def plane(self) -> int: ...
    def profile(self) -> PySide2.QtOpenGL.QGLFormat.OpenGLContextProfile: ...
    def redBufferSize(self) -> int: ...
    def rgba(self) -> bool: ...
    def sampleBuffers(self) -> bool: ...
    def samples(self) -> int: ...
    def setAccum(self, enable: bool): ...
    def setAccumBufferSize(self, size: int): ...
    def setAlpha(self, enable: bool): ...
    def setAlphaBufferSize(self, size: int): ...
    def setBlueBufferSize(self, size: int): ...
    @staticmethod
    def setDefaultFormat(f: PySide2.QtOpenGL.QGLFormat): ...
    @staticmethod
    def setDefaultOverlayFormat(f: PySide2.QtOpenGL.QGLFormat): ...
    def setDepth(self, enable: bool): ...
    def setDepthBufferSize(self, size: int): ...
    def setDirectRendering(self, enable: bool): ...
    def setDoubleBuffer(self, enable: bool): ...
    def setGreenBufferSize(self, size: int): ...
    def setOption(self, opt: PySide2.QtOpenGL.QGL.FormatOptions): ...
    def setOverlay(self, enable: bool): ...
    def setPlane(self, plane: int): ...
    def setProfile(self, profile: PySide2.QtOpenGL.QGLFormat.OpenGLContextProfile): ...
    def setRedBufferSize(self, size: int): ...
    def setRgba(self, enable: bool): ...
    def setSampleBuffers(self, enable: bool): ...
    def setSamples(self, numSamples: int): ...
    def setStencil(self, enable: bool): ...
    def setStencilBufferSize(self, size: int): ...
    def setStereo(self, enable: bool): ...
    def setSwapInterval(self, interval: int): ...
    def setVersion(self, major: int, minor: int): ...
    def stencil(self) -> bool: ...
    def stencilBufferSize(self) -> int: ...
    def stereo(self) -> bool: ...
    def swapInterval(self) -> int: ...
    def testOption(self, opt: PySide2.QtOpenGL.QGL.FormatOptions) -> bool: ...
    @staticmethod
    def toSurfaceFormat(format: PySide2.QtOpenGL.QGLFormat) -> PySide2.QtGui.QSurfaceFormat: ...


class QGLFramebufferObject(PySide2.QtGui.QPaintDevice):

    class Attachment(object): ...
    CombinedDepthStencil     : Attachment = ... # 0x1
    NoAttachment             : Attachment = ... # 0x0
    Depth                    : Attachment = ... # 0x2

    @typing.overload
    def __init__(self, size: PySide2.QtCore.QSize, attachment: PySide2.QtOpenGL.QGLFramebufferObject.Attachment, target: int=..., internal_format: int=...): ...
    @typing.overload
    def __init__(self, size: PySide2.QtCore.QSize, format: PySide2.QtOpenGL.QGLFramebufferObjectFormat): ...
    @typing.overload
    def __init__(self, size: PySide2.QtCore.QSize, target: int=...): ...
    @typing.overload
    def __init__(self, width: int, height: int, attachment: PySide2.QtOpenGL.QGLFramebufferObject.Attachment, target: int=..., internal_format: int=...): ...
    @typing.overload
    def __init__(self, width: int, height: int, format: PySide2.QtOpenGL.QGLFramebufferObjectFormat): ...
    @typing.overload
    def __init__(self, width: int, height: int, target: int=...): ...

    def attachment(self) -> PySide2.QtOpenGL.QGLFramebufferObject.Attachment: ...
    def bind(self) -> bool: ...
    @staticmethod
    def bindDefault() -> bool: ...
    @staticmethod
    def blitFramebuffer(target: PySide2.QtOpenGL.QGLFramebufferObject, targetRect: PySide2.QtCore.QRect, source: PySide2.QtOpenGL.QGLFramebufferObject, sourceRect: PySide2.QtCore.QRect, buffers: int=..., filter: int=...): ...
    def devType(self) -> int: ...
    @typing.overload
    def drawTexture(self, point: PySide2.QtCore.QPointF, textureId: int, textureTarget: int=...): ...
    @typing.overload
    def drawTexture(self, target: PySide2.QtCore.QRectF, textureId: int, textureTarget: int=...): ...
    def format(self) -> PySide2.QtOpenGL.QGLFramebufferObjectFormat: ...
    def handle(self) -> int: ...
    @staticmethod
    def hasOpenGLFramebufferBlit() -> bool: ...
    @staticmethod
    def hasOpenGLFramebufferObjects() -> bool: ...
    def isBound(self) -> bool: ...
    def isValid(self) -> bool: ...
    def metric(self, metric: PySide2.QtGui.QPaintDevice.PaintDeviceMetric) -> int: ...
    def paintEngine(self) -> PySide2.QtGui.QPaintEngine: ...
    def release(self) -> bool: ...
    def size(self) -> PySide2.QtCore.QSize: ...
    def texture(self) -> int: ...
    def toImage(self) -> PySide2.QtGui.QImage: ...


class QGLFramebufferObjectFormat(Shiboken.Object):

    @typing.overload
    def __init__(self): ...
    @typing.overload
    def __init__(self, other: PySide2.QtOpenGL.QGLFramebufferObjectFormat): ...

    def __copy__(self): ...
    def attachment(self) -> PySide2.QtOpenGL.QGLFramebufferObject.Attachment: ...
    def internalTextureFormat(self) -> int: ...
    def mipmap(self) -> bool: ...
    def samples(self) -> int: ...
    def setAttachment(self, attachment: PySide2.QtOpenGL.QGLFramebufferObject.Attachment): ...
    def setInternalTextureFormat(self, internalTextureFormat: int): ...
    def setMipmap(self, enabled: bool): ...
    def setSamples(self, samples: int): ...
    def setTextureTarget(self, target: int): ...
    def textureTarget(self) -> int: ...


class QGLPixelBuffer(PySide2.QtGui.QPaintDevice):

    @typing.overload
    def __init__(self, size: PySide2.QtCore.QSize, format: PySide2.QtOpenGL.QGLFormat=..., shareWidget: Optional[PySide2.QtOpenGL.QGLWidget]=...): ...
    @typing.overload
    def __init__(self, width: int, height: int, format: PySide2.QtOpenGL.QGLFormat=..., shareWidget: Optional[PySide2.QtOpenGL.QGLWidget]=...): ...

    @typing.overload
    def bindTexture(self, fileName: str) -> int: ...
    @typing.overload
    def bindTexture(self, image: PySide2.QtGui.QImage, target: int=...) -> int: ...
    @typing.overload
    def bindTexture(self, pixmap: PySide2.QtGui.QPixmap, target: int=...) -> int: ...
    def bindToDynamicTexture(self, texture: int) -> bool: ...
    def context(self) -> PySide2.QtOpenGL.QGLContext: ...
    def deleteTexture(self, texture_id: int): ...
    def devType(self) -> int: ...
    def doneCurrent(self) -> bool: ...
    @typing.overload
    def drawTexture(self, point: PySide2.QtCore.QPointF, textureId: int, textureTarget: int=...): ...
    @typing.overload
    def drawTexture(self, target: PySide2.QtCore.QRectF, textureId: int, textureTarget: int=...): ...
    def format(self) -> PySide2.QtOpenGL.QGLFormat: ...
    def generateDynamicTexture(self) -> int: ...
    def handle(self) -> int: ...
    @staticmethod
    def hasOpenGLPbuffers() -> bool: ...
    def isValid(self) -> bool: ...
    def makeCurrent(self) -> bool: ...
    def metric(self, metric: PySide2.QtGui.QPaintDevice.PaintDeviceMetric) -> int: ...
    def paintEngine(self) -> PySide2.QtGui.QPaintEngine: ...
    def releaseFromDynamicTexture(self): ...
    def size(self) -> PySide2.QtCore.QSize: ...
    def toImage(self) -> PySide2.QtGui.QImage: ...
    def updateDynamicTexture(self, texture_id: int): ...


class QGLShader(PySide2.QtCore.QObject):

    class ShaderType(object): ...

    class ShaderTypeBit(object): ...
    Fragment                 : ShaderTypeBit = ... # 0x2
    Vertex                   : ShaderTypeBit = ... # 0x1
    Geometry                 : ShaderTypeBit = ... # 0x4

    @typing.overload
    def __init__(self, type: PySide2.QtOpenGL.QGLShader.ShaderType, context: PySide2.QtOpenGL.QGLContext, parent: Optional[PySide2.QtCore.QObject]=...): ...
    @typing.overload
    def __init__(self, type: PySide2.QtOpenGL.QGLShader.ShaderType, parent: Optional[PySide2.QtCore.QObject]=...): ...

    @typing.overload
    def compileSourceCode(self, source: PySide2.QtCore.QByteArray) -> bool: ...
    @typing.overload
    def compileSourceCode(self, source: str) -> bool: ...
    @typing.overload
    def compileSourceCode(self, source: str) -> bool: ...
    def compileSourceFile(self, fileName: str) -> bool: ...
    @staticmethod
    def hasOpenGLShaders(type: PySide2.QtOpenGL.QGLShader.ShaderType, context: Optional[PySide2.QtOpenGL.QGLContext]=...) -> bool: ...
    def isCompiled(self) -> bool: ...
    def log(self) -> str: ...
    def shaderId(self) -> int: ...
    def shaderType(self) -> PySide2.QtOpenGL.QGLShader.ShaderType: ...
    def sourceCode(self) -> PySide2.QtCore.QByteArray: ...


class QGLShaderProgram(PySide2.QtCore.QObject):

    @typing.overload
    def __init__(self, context: PySide2.QtOpenGL.QGLContext, parent: Optional[PySide2.QtCore.QObject]=...): ...
    @typing.overload
    def __init__(self, parent: Optional[PySide2.QtCore.QObject]=...): ...

    def addShader(self, shader: PySide2.QtOpenGL.QGLShader) -> bool: ...
    @typing.overload
    def addShaderFromSourceCode(self, type: PySide2.QtOpenGL.QGLShader.ShaderType, source: PySide2.QtCore.QByteArray) -> bool: ...
    @typing.overload
    def addShaderFromSourceCode(self, type: PySide2.QtOpenGL.QGLShader.ShaderType, source: str) -> bool: ...
    @typing.overload
    def addShaderFromSourceCode(self, type: PySide2.QtOpenGL.QGLShader.ShaderType, source: str) -> bool: ...
    def addShaderFromSourceFile(self, type: PySide2.QtOpenGL.QGLShader.ShaderType, fileName: str) -> bool: ...
    @typing.overload
    def attributeLocation(self, name: PySide2.QtCore.QByteArray) -> int: ...
    @typing.overload
    def attributeLocation(self, name: str) -> int: ...
    @typing.overload
    def attributeLocation(self, name: str) -> int: ...
    def bind(self) -> bool: ...
    @typing.overload
    def bindAttributeLocation(self, name: PySide2.QtCore.QByteArray, location: int): ...
    @typing.overload
    def bindAttributeLocation(self, name: str, location: int): ...
    @typing.overload
    def bindAttributeLocation(self, name: str, location: int): ...
    @typing.overload
    def disableAttributeArray(self, location: int): ...
    @typing.overload
    def disableAttributeArray(self, name: str): ...
    @typing.overload
    def enableAttributeArray(self, location: int): ...
    @typing.overload
    def enableAttributeArray(self, name: str): ...
    def geometryInputType(self) -> int: ...
    def geometryOutputType(self) -> int: ...
    def geometryOutputVertexCount(self) -> int: ...
    @staticmethod
    def hasOpenGLShaderPrograms(context: Optional[PySide2.QtOpenGL.QGLContext]=...) -> bool: ...
    def isLinked(self) -> bool: ...
    def link(self) -> bool: ...
    def log(self) -> str: ...
    def maxGeometryOutputVertices(self) -> int: ...
    def programId(self) -> int: ...
    def release(self): ...
    def removeAllShaders(self): ...
    def removeShader(self, shader: PySide2.QtOpenGL.QGLShader): ...
    @typing.overload
    def setAttributeArray2D(self, location: int, values: PySide2.QtGui.QVector2D, stride: int=...): ...
    @typing.overload
    def setAttributeArray2D(self, name: str, values: PySide2.QtGui.QVector2D, stride: int=...): ...
    @typing.overload
    def setAttributeArray3D(self, location: int, values: PySide2.QtGui.QVector3D, stride: int=...): ...
    @typing.overload
    def setAttributeArray3D(self, name: str, values: PySide2.QtGui.QVector3D, stride: int=...): ...
    @typing.overload
    def setAttributeArray4D(self, location: int, values: PySide2.QtGui.QVector4D, stride: int=...): ...
    @typing.overload
    def setAttributeArray4D(self, name: str, values: PySide2.QtGui.QVector4D, stride: int=...): ...
    @typing.overload
    def setAttributeBuffer(self, location: int, type: int, offset: int, tupleSize: int, stride: int=...): ...
    @typing.overload
    def setAttributeBuffer(self, name: str, type: int, offset: int, tupleSize: int, stride: int=...): ...
    @typing.overload
    def setAttributeValue(self, location: int, value: PySide2.QtGui.QColor): ...
    @typing.overload
    def setAttributeValue(self, location: int, value: PySide2.QtGui.QVector2D): ...
    @typing.overload
    def setAttributeValue(self, location: int, value: PySide2.QtGui.QVector3D): ...
    @typing.overload
    def setAttributeValue(self, location: int, value: PySide2.QtGui.QVector4D): ...
    @typing.overload
    def setAttributeValue(self, location: int, value: float): ...
    @typing.overload
    def setAttributeValue(self, location: int, x: float, y: float): ...
    @typing.overload
    def setAttributeValue(self, location: int, x: float, y: float, z: float): ...
    @typing.overload
    def setAttributeValue(self, location: int, x: float, y: float, z: float, w: float): ...
    @typing.overload
    def setAttributeValue(self, name: str, value: PySide2.QtGui.QColor): ...
    @typing.overload
    def setAttributeValue(self, name: str, value: PySide2.QtGui.QVector2D): ...
    @typing.overload
    def setAttributeValue(self, name: str, value: PySide2.QtGui.QVector3D): ...
    @typing.overload
    def setAttributeValue(self, name: str, value: PySide2.QtGui.QVector4D): ...
    @typing.overload
    def setAttributeValue(self, name: str, value: float): ...
    @typing.overload
    def setAttributeValue(self, name: str, x: float, y: float): ...
    @typing.overload
    def setAttributeValue(self, name: str, x: float, y: float, z: float): ...
    @typing.overload
    def setAttributeValue(self, name: str, x: float, y: float, z: float, w: float): ...
    def setGeometryInputType(self, inputType: int): ...
    def setGeometryOutputType(self, outputType: int): ...
    def setGeometryOutputVertexCount(self, count: int): ...
    @typing.overload
    def setUniformValue(self, location: int, color: PySide2.QtGui.QColor): ...
    @typing.overload
    def setUniformValue(self, location: int, point: PySide2.QtCore.QPoint): ...
    @typing.overload
    def setUniformValue(self, location: int, point: PySide2.QtCore.QPointF): ...
    @typing.overload
    def setUniformValue(self, location: int, size: PySide2.QtCore.QSize): ...
    @typing.overload
    def setUniformValue(self, location: int, size: PySide2.QtCore.QSizeF): ...
    @typing.overload
    def setUniformValue(self, location: int, value: int): ...
    @typing.overload
    def setUniformValue(self, location: int, value: int): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix2x2): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix2x3): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix2x4): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix3x2): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix3x3): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix3x4): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix4x2): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix4x3): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QMatrix4x4): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QTransform): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QVector2D): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QVector3D): ...
    @typing.overload
    def setUniformValue(self, location: int, value: PySide2.QtGui.QVector4D): ...
    @typing.overload
    def setUniformValue(self, location: int, value: float): ...
    @typing.overload
    def setUniformValue(self, location: int, x: float, y: float): ...
    @typing.overload
    def setUniformValue(self, location: int, x: float, y: float, z: float): ...
    @typing.overload
    def setUniformValue(self, location: int, x: float, y: float, z: float, w: float): ...
    @typing.overload
    def setUniformValue(self, name: str, color: PySide2.QtGui.QColor): ...
    @typing.overload
    def setUniformValue(self, name: str, point: PySide2.QtCore.QPoint): ...
    @typing.overload
    def setUniformValue(self, name: str, point: PySide2.QtCore.QPointF): ...
    @typing.overload
    def setUniformValue(self, name: str, size: PySide2.QtCore.QSize): ...
    @typing.overload
    def setUniformValue(self, name: str, size: PySide2.QtCore.QSizeF): ...
    @typing.overload
    def setUniformValue(self, name: str, value: int): ...
    @typing.overload
    def setUniformValue(self, name: str, value: int): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix2x2): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix2x3): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix2x4): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix3x2): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix3x3): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix3x4): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix4x2): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix4x3): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QMatrix4x4): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QTransform): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QVector2D): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QVector3D): ...
    @typing.overload
    def setUniformValue(self, name: str, value: PySide2.QtGui.QVector4D): ...
    @typing.overload
    def setUniformValue(self, name: str, value: float): ...
    @typing.overload
    def setUniformValue(self, name: str, x: float, y: float): ...
    @typing.overload
    def setUniformValue(self, name: str, x: float, y: float, z: float): ...
    @typing.overload
    def setUniformValue(self, name: str, x: float, y: float, z: float, w: float): ...
    @typing.overload
    def setUniformValueArray2D(self, location: int, values: PySide2.QtGui.QVector2D, count: int): ...
    @typing.overload
    def setUniformValueArray2D(self, name: str, values: PySide2.QtGui.QVector2D, count: int): ...
    @typing.overload
    def setUniformValueArray2x2(self, location: int, values: PySide2.QtGui.QMatrix2x2, count: int): ...
    @typing.overload
    def setUniformValueArray2x2(self, name: str, values: PySide2.QtGui.QMatrix2x2, count: int): ...
    @typing.overload
    def setUniformValueArray2x3(self, location: int, values: PySide2.QtGui.QMatrix2x3, count: int): ...
    @typing.overload
    def setUniformValueArray2x3(self, name: str, values: PySide2.QtGui.QMatrix2x3, count: int): ...
    @typing.overload
    def setUniformValueArray2x4(self, location: int, values: PySide2.QtGui.QMatrix2x4, count: int): ...
    @typing.overload
    def setUniformValueArray2x4(self, name: str, values: PySide2.QtGui.QMatrix2x4, count: int): ...
    @typing.overload
    def setUniformValueArray3D(self, location: int, values: PySide2.QtGui.QVector3D, count: int): ...
    @typing.overload
    def setUniformValueArray3D(self, name: str, values: PySide2.QtGui.QVector3D, count: int): ...
    @typing.overload
    def setUniformValueArray3x2(self, location: int, values: PySide2.QtGui.QMatrix3x2, count: int): ...
    @typing.overload
    def setUniformValueArray3x2(self, name: str, values: PySide2.QtGui.QMatrix3x2, count: int): ...
    @typing.overload
    def setUniformValueArray3x3(self, location: int, values: PySide2.QtGui.QMatrix3x3, count: int): ...
    @typing.overload
    def setUniformValueArray3x3(self, name: str, values: PySide2.QtGui.QMatrix3x3, count: int): ...
    @typing.overload
    def setUniformValueArray3x4(self, location: int, values: PySide2.QtGui.QMatrix3x4, count: int): ...
    @typing.overload
    def setUniformValueArray3x4(self, name: str, values: PySide2.QtGui.QMatrix3x4, count: int): ...
    @typing.overload
    def setUniformValueArray4D(self, location: int, values: PySide2.QtGui.QVector4D, count: int): ...
    @typing.overload
    def setUniformValueArray4D(self, name: str, values: PySide2.QtGui.QVector4D, count: int): ...
    @typing.overload
    def setUniformValueArray4x2(self, location: int, values: PySide2.QtGui.QMatrix4x2, count: int): ...
    @typing.overload
    def setUniformValueArray4x2(self, name: str, values: PySide2.QtGui.QMatrix4x2, count: int): ...
    @typing.overload
    def setUniformValueArray4x3(self, location: int, values: PySide2.QtGui.QMatrix4x3, count: int): ...
    @typing.overload
    def setUniformValueArray4x3(self, name: str, values: PySide2.QtGui.QMatrix4x3, count: int): ...
    @typing.overload
    def setUniformValueArray4x4(self, location: int, values: PySide2.QtGui.QMatrix4x4, count: int): ...
    @typing.overload
    def setUniformValueArray4x4(self, name: str, values: PySide2.QtGui.QMatrix4x4, count: int): ...
    @typing.overload
    def setUniformValueArrayInt(self, location: int, values: int, count: int): ...
    @typing.overload
    def setUniformValueArrayInt(self, name: str, values: int, count: int): ...
    @typing.overload
    def setUniformValueArrayUint(self, location: int, values: int, count: int): ...
    @typing.overload
    def setUniformValueArrayUint(self, name: str, values: int, count: int): ...
    def shaders(self) -> List[PySide2.QtOpenGL.QGLShader]: ...
    @typing.overload
    def uniformLocation(self, name: PySide2.QtCore.QByteArray) -> int: ...
    @typing.overload
    def uniformLocation(self, name: str) -> int: ...
    @typing.overload
    def uniformLocation(self, name: str) -> int: ...


class QGLWidget(PySide2.QtWidgets.QWidget):

    @typing.overload
    def __init__(self, context: PySide2.QtOpenGL.QGLContext, parent: Optional[PySide2.QtWidgets.QWidget]=..., shareWidget: Optional[PySide2.QtOpenGL.QGLWidget]=..., f: PySide2.QtCore.Qt.WindowFlags=...): ...
    @typing.overload
    def __init__(self, format: PySide2.QtOpenGL.QGLFormat, parent: Optional[PySide2.QtWidgets.QWidget]=..., shareWidget: Optional[PySide2.QtOpenGL.QGLWidget]=..., f: PySide2.QtCore.Qt.WindowFlags=...): ...
    @typing.overload
    def __init__(self, parent: Optional[PySide2.QtWidgets.QWidget]=..., shareWidget: Optional[PySide2.QtOpenGL.QGLWidget]=..., f: PySide2.QtCore.Qt.WindowFlags=...): ...

    def autoBufferSwap(self) -> bool: ...
    @typing.overload
    def bindTexture(self, fileName: str) -> int: ...
    @typing.overload
    def bindTexture(self, image: PySide2.QtGui.QImage, target: int, format: int, options: PySide2.QtOpenGL.QGLContext.BindOptions) -> int: ...
    @typing.overload
    def bindTexture(self, image: PySide2.QtGui.QImage, target: int=..., format: int=...) -> int: ...
    @typing.overload
    def bindTexture(self, pixmap: PySide2.QtGui.QPixmap, target: int, format: int, options: PySide2.QtOpenGL.QGLContext.BindOptions) -> int: ...
    @typing.overload
    def bindTexture(self, pixmap: PySide2.QtGui.QPixmap, target: int=..., format: int=...) -> int: ...
    def colormap(self) -> PySide2.QtOpenGL.QGLColormap: ...
    def context(self) -> PySide2.QtOpenGL.QGLContext: ...
    @staticmethod
    def convertToGLFormat(img: PySide2.QtGui.QImage) -> PySide2.QtGui.QImage: ...
    def deleteTexture(self, tx_id: int): ...
    def doneCurrent(self): ...
    def doubleBuffer(self) -> bool: ...
    @typing.overload
    def drawTexture(self, point: PySide2.QtCore.QPointF, textureId: int, textureTarget: int=...): ...
    @typing.overload
    def drawTexture(self, target: PySide2.QtCore.QRectF, textureId: int, textureTarget: int=...): ...
    def event(self, arg__1: PySide2.QtCore.QEvent) -> bool: ...
    def format(self) -> PySide2.QtOpenGL.QGLFormat: ...
    def glDraw(self): ...
    def glInit(self): ...
    def grabFrameBuffer(self, withAlpha: bool=...) -> PySide2.QtGui.QImage: ...
    def initializeGL(self): ...
    def initializeOverlayGL(self): ...
    def isSharing(self) -> bool: ...
    def isValid(self) -> bool: ...
    def makeCurrent(self): ...
    def makeOverlayCurrent(self): ...
    def overlayContext(self) -> PySide2.QtOpenGL.QGLContext: ...
    def paintEngine(self) -> PySide2.QtGui.QPaintEngine: ...
    def paintEvent(self, arg__1: PySide2.QtGui.QPaintEvent): ...
    def paintGL(self): ...
    def paintOverlayGL(self): ...
    def qglClearColor(self, c: PySide2.QtGui.QColor): ...
    def qglColor(self, c: PySide2.QtGui.QColor): ...
    def renderPixmap(self, w: int=..., h: int=..., useContext: bool=...) -> PySide2.QtGui.QPixmap: ...
    @typing.overload
    def renderText(self, x: float, y: float, z: float, str: str, fnt: PySide2.QtGui.QFont=...): ...
    @typing.overload
    def renderText(self, x: int, y: int, str: str, fnt: PySide2.QtGui.QFont=...): ...
    def resizeEvent(self, arg__1: PySide2.QtGui.QResizeEvent): ...
    def resizeGL(self, w: int, h: int): ...
    def resizeOverlayGL(self, w: int, h: int): ...
    def setAutoBufferSwap(self, on: bool): ...
    def setColormap(self, map: PySide2.QtOpenGL.QGLColormap): ...
    def swapBuffers(self): ...
    def updateGL(self): ...
    def updateOverlayGL(self): ...

# eof
