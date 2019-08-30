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



# TODO: Add option to specify which python to use
pyside_base=`python -c 'import PySide; print(PySide.__file__)'`
pyside_base=${pyside_base%%__init__.pyc}

openGL_base=`python -c 'import OpenGL; print(OpenGL.__file__)'`
openGL_base=${openGL_base%%__init__.pyc}

qt_base="/usr/local/opt/qt@4"
qt_cellar_base="/usr/local/Cellar/qt@4"


cp -r $pyside_base $basepath/lib/python/PySide
cp -r $openGL_base $basepath/lib/python/OpenGL

# Argument 2 specifies a custom location for qt(e.g for when we cache it in the usd src directory)
if [ -d $2 ]
then
    cp -r $2        $basepath/lib/qt@4
else
    cp -r $qt_base  $basepath/lib/qt@4
fi

chmod -R +w $basepath/lib/qt@4

install_name_tool -change $qt_base/lib/QtCore.framework/Versions/4/QtCore @loader_path/../../qt@4/lib/QtCore.framework/Versions/4/QtCore  $basepath/lib/python/PySide/QtCore.so
install_name_tool -change $qt_base/lib/QtCore.framework/Versions/4/QtCore @loader_path/../../qt@4/lib/QtCore.framework/Versions/4/QtCore  $basepath/lib/python/PySide/libpyside-python2.7.1.2.dylib
install_name_tool -change $qt_base/lib/QtGui.framework/Versions/4/QtGui   @loader_path/../../qt@4/lib/QtGui.framework/Versions/4/QtGui    $basepath/lib/python/PySide/QtGui.so
install_name_tool -change $qt_base/lib/QtCore.framework/Versions/4/QtCore @loader_path/../../qt@4/lib/QtCore.framework/Versions/4/QtCore  $basepath/lib/python/PySide/QtGui.so
install_name_tool -change $qt_base/lib/QtCore.framework/Versions/4/QtCore @loader_path/../../qt@4/lib/QtCore.framework/Versions/4/QtCore  $basepath/lib/python/PySide/QtOpenGL.so
install_name_tool -change $qt_base/lib/QtGui.framework/Versions/4/QtGui   @loader_path/../../qt@4/lib/QtGui.framework/Versions/4/QtGui    $basepath/lib/python/PySide/QtOpenGL.so
install_name_tool -change $qt_cellar_base/4.8.7_5/lib/QtCore.framework/Versions/4/QtCore  @loader_path/../../qt@4/lib/QtCore.framework/Versions/4/QtCore     $basepath/lib/qt@4/lib/QtCore.framework/Versions/4/QtCore
install_name_tool -change $qt_cellar_base/4.8.7_5/lib/QtCore.framework/Versions/4/QtCore  @loader_path/../../../QtCore.framework/Versions/4/QtCore           $basepath/lib/qt@4/lib/QtGui.framework/Versions/4/QtGui
install_name_tool -change $qt_base/lib/QtOpenGL.framework/Versions/4/QtOpenGL             @loader_path/../../qt@4/lib/QtOpenGL.framework/Versions/4/QtOpenGL $basepath/lib/python/PySide/QtOpenGL.so
install_name_tool -change $qt_cellar_base/4.8.7_5/lib/QtGui.framework/Versions/4/QtGui    @loader_path/../../qt@4/lib/QtGui.framework/Versions/4/QtGui       $basepath/lib/python/PySide/QtOpenGL.so
install_name_tool -change $qt_cellar_base/4.8.7_5/lib/QtGui.framework/Versions/4/QtGui    @loader_path/../../../QtGui.framework/Versions/4/QtGui             $basepath/lib/qt@4/lib/QtOpenGL.framework/Versions/4/QtOpenGL
install_name_tool -change $qt_cellar_base/4.8.7_5/lib/QtCore.framework/Versions/4/QtCore  @loader_path/../../../QtCore.framework/Versions/4/QtCore           $basepath/lib/qt@4/lib/QtOpenGL.framework/Versions/4/QtOpenGL
