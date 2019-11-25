set(DIRS
    garch
    hf
    hio
    cameraUtil
    pxOsd
    glf
    mtlf
    hgi
    hgiGL
    hgiMetal
    hd
    hdSt
    hdx
    plugin
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
