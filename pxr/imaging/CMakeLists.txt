set(DIRS
    garch
    hf
    cameraUtil
    pxOsd
    glf
    hio
    hgi
    hgiGL
    hgiMetal
    hgiInterop
    mtlf
    hd
    hdSt
    hdx
    plugin
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
