'''Autogenerated by xml_generate script, do not edit!'''
from OpenGL import platform as _p, arrays
# Code generation uses this
from OpenGL.raw.GL import _types as _cs
# End users want this...
from OpenGL.raw.GL._types import *
from OpenGL.raw.GL import _errors
from OpenGL.constant import Constant as _C

import ctypes
_EXTENSION_NAME = 'GL_EXT_separate_shader_objects'
def _f( function ):
    return _p.createFunction( function,_p.PLATFORM.GL,'GL_EXT_separate_shader_objects',error_checker=_errors._error_checker)
GL_ACTIVE_PROGRAM_EXT=_C('GL_ACTIVE_PROGRAM_EXT',0x8B8D)
GL_ACTIVE_PROGRAM_EXT=_C('GL_ACTIVE_PROGRAM_EXT',0x8B8D)
GL_ALL_SHADER_BITS_EXT=_C('GL_ALL_SHADER_BITS_EXT',0xFFFFFFFF)
GL_FRAGMENT_SHADER_BIT_EXT=_C('GL_FRAGMENT_SHADER_BIT_EXT',0x00000002)
GL_PROGRAM_PIPELINE_BINDING_EXT=_C('GL_PROGRAM_PIPELINE_BINDING_EXT',0x825A)
GL_PROGRAM_SEPARABLE_EXT=_C('GL_PROGRAM_SEPARABLE_EXT',0x8258)
GL_VERTEX_SHADER_BIT_EXT=_C('GL_VERTEX_SHADER_BIT_EXT',0x00000001)
@_f
@_p.types(None,_cs.GLuint)
def glActiveProgramEXT(program):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLuint)
def glActiveShaderProgramEXT(pipeline,program):pass
@_f
@_p.types(None,_cs.GLuint)
def glBindProgramPipelineEXT(pipeline):pass
@_f
@_p.types(_cs.GLuint,_cs.GLenum,arrays.GLcharArray)
def glCreateShaderProgramEXT(type,string):pass
@_f
@_p.types(_cs.GLuint,_cs.GLenum,_cs.GLsizei,ctypes.POINTER( ctypes.POINTER( _cs.GLchar )))
def glCreateShaderProgramvEXT(type,count,strings):pass
@_f
@_p.types(None,_cs.GLsizei,arrays.GLuintArray)
def glDeleteProgramPipelinesEXT(n,pipelines):pass
@_f
@_p.types(None,_cs.GLsizei,arrays.GLuintArray)
def glGenProgramPipelinesEXT(n,pipelines):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLsizei,arrays.GLsizeiArray,arrays.GLcharArray)
def glGetProgramPipelineInfoLogEXT(pipeline,bufSize,length,infoLog):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLenum,arrays.GLintArray)
def glGetProgramPipelineivEXT(pipeline,pname,params):pass
@_f
@_p.types(_cs.GLboolean,_cs.GLuint)
def glIsProgramPipelineEXT(pipeline):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLenum,_cs.GLint)
def glProgramParameteriEXT(program,pname,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLfloat)
def glProgramUniform1fEXT(program,location,v0):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLfloatArray)
def glProgramUniform1fvEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLint)
def glProgramUniform1iEXT(program,location,v0):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLintArray)
def glProgramUniform1ivEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLuint)
def glProgramUniform1uiEXT(program,location,v0):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLuintArray)
def glProgramUniform1uivEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLfloat,_cs.GLfloat)
def glProgramUniform2fEXT(program,location,v0,v1):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLfloatArray)
def glProgramUniform2fvEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLint,_cs.GLint)
def glProgramUniform2iEXT(program,location,v0,v1):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLintArray)
def glProgramUniform2ivEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLuint,_cs.GLuint)
def glProgramUniform2uiEXT(program,location,v0,v1):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLuintArray)
def glProgramUniform2uivEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLfloat,_cs.GLfloat,_cs.GLfloat)
def glProgramUniform3fEXT(program,location,v0,v1,v2):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLfloatArray)
def glProgramUniform3fvEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLint,_cs.GLint,_cs.GLint)
def glProgramUniform3iEXT(program,location,v0,v1,v2):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLintArray)
def glProgramUniform3ivEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLuint,_cs.GLuint,_cs.GLuint)
def glProgramUniform3uiEXT(program,location,v0,v1,v2):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLuintArray)
def glProgramUniform3uivEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLfloat,_cs.GLfloat,_cs.GLfloat,_cs.GLfloat)
def glProgramUniform4fEXT(program,location,v0,v1,v2,v3):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLfloatArray)
def glProgramUniform4fvEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLint,_cs.GLint,_cs.GLint,_cs.GLint)
def glProgramUniform4iEXT(program,location,v0,v1,v2,v3):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLintArray)
def glProgramUniform4ivEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLuint,_cs.GLuint,_cs.GLuint,_cs.GLuint)
def glProgramUniform4uiEXT(program,location,v0,v1,v2,v3):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,arrays.GLuintArray)
def glProgramUniform4uivEXT(program,location,count,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix2fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix2x3fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix2x4fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix3fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix3x2fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix3x4fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix4fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix4fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix4x2fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLint,_cs.GLsizei,_cs.GLboolean,arrays.GLfloatArray)
def glProgramUniformMatrix4x3fvEXT(program,location,count,transpose,value):pass
@_f
@_p.types(None,_cs.GLuint,_cs.GLbitfield,_cs.GLuint)
def glUseProgramStagesEXT(pipeline,stages,program):pass
@_f
@_p.types(None,_cs.GLenum,_cs.GLuint)
def glUseShaderProgramEXT(type,program):pass
@_f
@_p.types(None,_cs.GLuint)
def glValidateProgramPipelineEXT(pipeline):pass
