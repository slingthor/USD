set(DIRS
    hf
    hio
    garch
    cameraUtil
    pxOsd
    glf
    hgi
    hgiGL
    hgiMetal
    hgiVulkan
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
