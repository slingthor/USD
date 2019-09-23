set(hgiGL "")
if (PXR_ENABLE_GL_SUPPORT)
    set(hgiGL hgiGL)
endif()

set(DIRS
    garch
    hf
    hio
    cameraUtil
    pxOsd
    glf
    mtlf
    hgi
    ${hgiGL}
    hd
    hdSt
    hdx
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
