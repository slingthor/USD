set(DIRS
    garch
    hf
    hio
    cameraUtil
    pxOsd
    glf
    mtlf
    hgi
    hd
    hdSt
    hdx
)

if (${PXR_ENABLE_GL_SUPPORT})
    add_subdirectory(hgiGL)
endif()

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()

