set(DIRS
    garch
    hf
    cameraUtil
    pxOsd
    glf
    hgi
    hgiGL
    hgiMetal
    hgiInterop
    hio
    mtlf
    hd
    hdSt
    hdx
    plugin
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
