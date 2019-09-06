#!/usr/bin/python
import sys
import os
import subprocess
import PySide
import OpenGL
from distutils.dir_util import copy_tree


def extract_object_files(path, files):
    # r=root, d=directories, f = files
    for r, d, f in os.walk(path):
        for file in f:
            if '.so' in file or '.dylib' in file:
                files.append(os.path.join(r, file))



def add_rpath_to_files(path, files):
    for f in files:
        dir_path=os.path.dirname(f)
        path_between = os.path.relpath(path, dir_path)
        if path_between != ".":
            subprocess.call(['install_name_tool', '-add_rpath', '@loader_path/' + path_between, f])
        


def change_absolute_to_relative(files, path_to_replace, custom_path=""):
    for f in files:
        otool_output = subprocess.check_output(['otool', '-L', f])
        for line in otool_output.splitlines()[1:]:
            extracted_path = line.split()[0]
            replace_path_idx = extracted_path.find(path_to_replace)
            if replace_path_idx != -1:
                cur_dir_path=os.path.dirname(f)
                if custom_path == "":
                    path_between = os.path.relpath(extracted_path, cur_dir_path)
                else:
                    inside_idx = replace_path_idx + len(path_to_replace)
                    suffix_path = extracted_path[inside_idx:]
                    path_between = os.path.relpath(custom_path+suffix_path, cur_dir_path)
                subprocess.call(['install_name_tool', '-change', extracted_path, '@loader_path/' + path_between, f])



def make_relocatable(install_path, qt_path="/usr/local/opt/qt@4"):
    files = []
    extract_object_files(install_path + '/lib/', files)
    extract_object_files(install_path + '/plugin/', files)

    add_rpath_to_files(install_path+ '/lib/', files)
    change_absolute_to_relative(files, install_path)


    pyside_path = PySide.__file__
    pyside_index = pyside_path.find("__init__.py")
    pyside_path = pyside_path[:pyside_index]

    openGL_path = OpenGL.__file__
    opengl_index = openGL_path.find("__init__.py")
    openGL_path = openGL_path[:opengl_index]

    subprocess.call(['chmod', '-R', '+w', install_path + "/lib"])
    
    copy_tree(pyside_path, install_path + "/lib/python/PySide")
    copy_tree(openGL_path, install_path + "/lib/python/OpenGL")
    copy_tree(qt_path, install_path + "/lib/qt@4")

    subprocess.call(['chmod', '-R', '+w', install_path + "/lib/qt@4"])

    qt_base="/usr/local/opt/qt@4"
    qt_cellar_base="/usr/local/Cellar/qt@4/4.8.7_5"


    files=[]
    extract_object_files(install_path + '/lib/python/PySide', files)
    extract_object_files(install_path + '/lib/qt@4', files)

    change_absolute_to_relative(files, qt_base, install_path + '/lib/qt@4')
    change_absolute_to_relative(files, qt_cellar_base, install_path + '/lib/qt@4')
