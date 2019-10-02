#!/usr/bin/python
import sys
import os
import subprocess
import PySide
import OpenGL
from distutils.dir_util import copy_tree
from os.path import isdir, isfile, join



def extract_files(path, cond, files):
    if not os.path.exists(path):
        return
    
    for file in os.listdir(path):
        if file[0] != '.' and cond(join(path, file)):
            files.append(os.path.join(path, file))



def extract_files_recursive(path, cond, files):
    # r=root, d=directories, f = files
    for r, d, f in os.walk(path):
        for file in f:
            if cond(path+file):
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



def is_object_file(file):
    first_line = open(file).readline().rstrip()
    not_bash_script = first_line != "#! /bin/sh"
    not_python_script = first_line != "#!/System/Library/Frameworks/Python.framework/Versions/2.7/bin/python"
    return not_bash_script and not_python_script



def replace_string_in_file(path, old_string, new_string):
    if not os.path.exists(path):
        return

    with open(path, 'r') as file :
      contents = file.read()

    contents = contents.replace(old_string, new_string)
    
    with open(path, 'w') as file:
      file.write(contents)



def make_relocatable(install_path, buildPython, qt_path="/usr/local/opt/qt@4"):
    files = []
    
    extract_files_recursive(install_path + '/bin/', is_object_file, files)
    extract_files_recursive(install_path + '/lib/', (lambda file: '.so' in file or '.dylib' in file), files)
    extract_files_recursive(install_path + '/plugin/', (lambda file: '.so' in file or '.dylib' in file), files)
    extract_files_recursive(install_path + '/share/', (lambda file: '.so' in file or '.dylib' in file), files)
    extract_files_recursive(install_path + '/tests', (lambda file: '.so' in file or '.dylib' in file), files)
    extract_files(install_path + '/tests', (lambda file: isfile(file) and open(file).readline().rstrip()[0] != "#"), files)

    add_rpath_to_files(install_path+ '/lib/', files)
    change_absolute_to_relative(files, install_path)

    replace_string_in_file(install_path + '/pxrConfig.cmake', install_path, "REPLACE_ME")
    replace_string_in_file(install_path + '/cmake/pxrTargets.cmake', install_path, "REPLACE_ME")
    replace_string_in_file(install_path + '/cmake/pxrTargets-release.cmake', install_path, "REPLACE_ME")

    if buildPython:
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
        extract_files_recursive(install_path + '/lib/python/PySide', (lambda file: '.so' in file or '.dylib' in file), files)
        extract_files_recursive(install_path + '/lib/qt@4', (lambda file: '.so' in file or '.dylib' in file), files)

        change_absolute_to_relative(files, qt_base, install_path + '/lib/qt@4')
        change_absolute_to_relative(files, qt_cellar_base, install_path + '/lib/qt@4')
