if (APPLEIOS)
    message(STATUS
        "Skipping ${PXR_PACKAGE} because iOS doesn't support utility binaries")
    return()
endif()

set(DIRS
    usdcat
    usdchecker
    usddiff
    usddumpcrate
    usdedit
    usdresolve
    usdstitch
    usdstitchclips
    usdzip
    sdfdump
    sdffilter
    usdtree
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()

