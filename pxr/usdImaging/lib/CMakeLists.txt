set(DIRS
    usdImaging
    usdviewq
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()

if (APPLE)
    add_subdirectory(usdImagingMetal)
else()
    add_subdirectory(usdImagingGL)
endif()
