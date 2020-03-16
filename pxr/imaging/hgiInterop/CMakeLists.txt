set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hgiInterop)

if (NOT ${PXR_BUILD_STORM_PLUGINS} OR NOT ${PXR_ENABLE_GL_SUPPORT} OR NOT ${PXR_ENABLE_METAL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_BUILD_STORM_PLUGINS or PXR_ENABLE_GL_SUPPORT or PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

if (APPLEIOS)
    message(STATUS
        "Skipping ${PXR_PACKAGE} on iOS")
    return()
endif()

set(optionalLibraries "")
set(optionalSource "")

if (APPLE)
    # Compile cpp files as obj-c++ to allow inline use of Metal API
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -x objective-c++")

    FIND_LIBRARY(COREVIDEO_LIBRARY CoreVideo)

    list(APPEND optionalLibraries ${COREVIDEO_LIBRARY} hgiMetal)
    list(APPEND optionalSource metal)
endif()

pxr_library(hgiInterop
    LIBRARIES
        gf
        plug
        tf
        hgi
        hgiGL
        ${optionalLibraries}

    PRIVATE_CLASSES
        ${optionalSource}

    PUBLIC_CLASSES
        hgiInterop

    PUBLIC_HEADERS
        api.h
)
