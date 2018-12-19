if (NOT APPLEIOS)
    message(STATUS
        "Skipping ${PXR_PACKAGE} because iOS doesn't support these binaries")
    return()
endif()

set(DIRS
    usdview
    testusdview
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
