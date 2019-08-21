#!/bin/bash

function readme
{
    echo
    echo "Usage: fix the hard-coded paths in the USD-related binaries"
    echo "Please provide the path to the USD build folder"
    echo "e.g ./make_relocatable.sh /Users/tim/Desktop/usd/build"
    echo
}

# Check if the number of arguments is less than 1
if (( $# < 1 ))
then
    readme
    exit
fi



# Process the first argument
end=`echo -n $1 | tail -c 1`
if [ "$end" == "/" ]
then
    basepath=${1/%?/}
else
    basepath=$1
fi



# Run otool to determine the hard-coded absolute path
otool -l $basepath/lib/libar.dylib | sed 1d |  {
absolute=""

while read i
do
    # Search for LC_RPATH and the hard-coded path should 2 rows down
    if [[ "$i" == 'cmd LC_RPATH' ]]; then
        read i
        read i
        # The absolute path is the 2nd string in the row
        absolute=`echo $i | awk '{print $2}'`
    fi
done



# Use install_name_tool to change the absolute path to a path relative to @loader_path
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/Ar/_ar.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/Sdf/_sdf.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/Kind/_kind.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/Usd/_usd.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/UsdGeom/_usdGeom.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/UsdShade/_usdShade.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/UsdUtils/_usdUtils.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/UsdImagingGL/_usdImagingGL.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/UsdAppUtils/_usdAppUtils.so
install_name_tool -add_rpath @loader_path/../../.. $basepath/lib/python/pxr/Usdviewq/_usdviewq.so

install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libusdAppUtils.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libusdImagingGL.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libusdImaging.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libhd.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libhdx.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libhdSt.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libosdGPU.3.3.1.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libglf.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/libGLEW.2.0.0.dylib $basepath/lib/libmtlf.dylib

install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/../../../libGLEW.2.0.0.dylib $basepath/lib/python/pxr/UsdImagingGL/_usdImagingGL.so
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/../../../libGLEW.2.0.0.dylib $basepath/lib/python/pxr/UsdAppUtils/_usdAppUtils.so
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/../../../libGLEW.2.0.0.dylib $basepath/lib/python/pxr/Mtlf/_mtlf.so
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/../../../libGLEW.2.0.0.dylib $basepath/lib/python/pxr/Glf/_glf.so

install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/../../lib/libGLEW.2.0.0.dylib $basepath/plugin/usd/hdStormGL.dylib
install_name_tool -change $absolute/libGLEW.2.0.0.dylib  @loader_path/../../lib/libGLEW.2.0.0.dylib $basepath/plugin/usd/hdStormMetal.dylib
}
