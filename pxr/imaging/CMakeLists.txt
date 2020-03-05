set(DIRS
    garch
    hf
    hio
    cameraUtil
    pxOsd
    glf
    hgi
    hgiGL
    hgiInterop
    hgiMetal
    mtlf
    hd
    hdSt
    hdx
    plugin
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
