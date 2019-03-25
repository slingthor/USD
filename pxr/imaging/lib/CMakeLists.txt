set(DIRS
    garch
    hf
    hio
    cameraUtil
    pxOsd
    glf
    mtlf
    hd
    hdSt
    hdx
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()

