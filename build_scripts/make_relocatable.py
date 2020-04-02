#!/usr/bin/python
import sys
import os
import subprocess
import threading
from distutils.dir_util import copy_tree
from os.path import isdir, isfile, join
from shutil import copy, copyfile

SDKVersion  = subprocess.check_output(['xcodebuild', '-version']).strip()[6:10]
codeSignIDs = subprocess.check_output(['security', 'find-identity', '-v', '-p', 'codesigning'])

codeSignID = "-"
if os.environ.get('CODE_SIGN_ID'):
    codeSignID = os.environ.get('CODE_SIGN_ID')
elif SDKVersion >= "11.0" and codeSignIDs.find("Apple Development") != -1:
    codeSignID = "Apple Development"
elif codeSignIDs.find("Mac Developer") != -1:
    codeSignID = "Mac Developer"

# Validate that we have a codesign ID that both exists and isn't ambiguous
if codeSignIDs.count(codeSignID) != 1:
    print("Codesign Error: Unable to identify signing identity. " +
        "Pleas specify by setting the XCODE_ATTRIBUTE_CODE_SIGN_ID environment " +
        "variable to the one you'd like to use. Options are:\n" + codeSignIDs +
        "Skipping make_relocatable")
    codeSignID = None

devout = open(os.devnull, 'w')

def extract_files(path, cond, files):
    if not os.path.exists(path):
        return
    
    for file in os.listdir(path):
        if file[0] != '.' and cond(join(path, file)):
            files.append(os.path.join(path, file))


def extract_files_recursive(path, cond, files):
    # r=root, d=directories, f = files
    for r, d, f in os.walk(path, topdown=False):
        for file in f:
            name = os.path.join(r, file)
            if cond(name):
                # print("Adding: " + name)
                files.append(name)
        # .frameworks are directories - which are valid as codesigning targets
        for directory in d:
            name = os.path.join(r, directory)
            if cond(name):
                files.append(name)

def Run(cmd):
    subprocess.call(cmd, stdout=devout, stderr=devout)


def add_rpath_to_files(path, files):
    threads = []
    for f in files:
        dir_path=os.path.dirname(f)
        path_between = os.path.relpath(path, dir_path)
        if path_between != ".":
            t = threading.Thread(target=Run, args=(['install_name_tool', '-add_rpath', '@loader_path/' + path_between, f],))
#            subprocess.call(['install_name_tool', '-add_rpath', '@loader_path/' + path_between, f],
#                stdout=devout, stderr=devout)
            t.start()
            threads.append(t)

    for t in threads:
        t.join()


def remove_rpath(path, files):
    threads = []
    for f in files:
#        subprocess.call(['install_name_tool', '-delete_rpath', path, f],
#            stdout=devout, stderr=devout)
        t = threading.Thread(target=Run, args=(['install_name_tool', '-delete_rpath', path, f],))
        t.start()
        threads.append(t)

    for t in threads:
        t.join()


def change_absolute_to_relative(files, path_to_replace, custom_path=""):
    threads = []
    for f in files:
        otool_output = ""

        p = subprocess.Popen(['otool', '-L', f], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        otool_output, err = p.communicate(b"")
        returncode = p.returncode

        if returncode != 0:
            return
        for line in otool_output.splitlines():
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
#                subprocess.call(['install_name_tool', '-change', extracted_path, '@loader_path/' + path_between, f],
#                    stdout=devout, stderr=devout)
                t = threading.Thread(target=Run, args=(['install_name_tool', '-change', extracted_path, '@loader_path/' + path_between, f],))
                t.start()
                threads.append(t)

    for t in threads:
        t.join()


def codesign_files(files):
    threads = []
    for f in files:
        t = threading.Thread(target=Run, args=(['codesign', '-f', '-s', '{codesignid}'.format(codesignid=codeSignID), f],))
        t.start()
        threads.append(t)
#        subprocess.call(['codesign', '-f', '-s', '{codesignid}'.format(codesignid=codeSignID), f],
#            stdout=devout, stderr=devout)

    for t in threads:
        t.join()


def is_object_file(file):
    if os.path.isdir(file):
        return False
    first_line = open(file).readline().rstrip()
    return not first_line.startswith("#!")



def replace_string_in_file(path, old_string, new_string):
    if not os.path.exists(path):
        return

    with open(path, 'r') as file :
      contents = file.read()

    contents = contents.replace(old_string, new_string)
    
    with open(path, 'w') as file:
      file.write(contents)




def make_relocatable(install_path, buildPython, iOS, verbose_output=False):
    files = []
    if codeSignID == None:
        return

    if verbose_output:
        global devout
        devout = sys.stdout

    # Patch pxr build config files
    replace_string_in_file(install_path + '/pxrConfig.cmake', install_path, "$ENV{USD_PATH}")
    replace_string_in_file(install_path + '/cmake/pxrTargets.cmake', install_path, "$ENV{USD_PATH}")
    replace_string_in_file(install_path + '/cmake/pxrTargets-release.cmake', install_path, "$ENV{USD_PATH}")

    if iOS:
        return

    #path of the usd repo folder
    file_path = os.path.abspath(__file__)
    src_path  = os.path.abspath(os.path.join(file_path,"../..")) 

    if buildPython:
        import PySide2
        import OpenGL
        
        pyside_file = os.path.abspath(PySide2.__file__)
        pyside_path = os.path.dirname(pyside_file)

        openGL_file = os.path.abspath(OpenGL.__file__)
        openGL_path = os.path.dirname(openGL_file)

        subprocess.call(['chmod', '-R', '+w', install_path + "/lib"],
            stdout=devout)
        
        copy_tree(pyside_path, install_path + "/lib/python/PySide2")
        copy_tree(openGL_path, install_path + "/lib/python/OpenGL")
        copy_tree(os.path.dirname(pyside_path) + "/shiboken2", install_path+"/lib/python/shiboken2")
        copy_tree(src_path + "/lib/python/pysideuic", install_path+"/lib/python/pysideuic")
        copy_tree(src_path + "/lib/python/pyside2uic", install_path+"/lib/python/pyside2uic")
        copy(src_path + "/lib/python/pyside-uic", install_path+"/lib/python/pyside-uic")
        copy(src_path + "/lib/python/pyside2-uic", install_path+"/lib/python/pyside2-uic")
    
    def file_check(file):
        name = os.path.basename(file)
        return '.so' in name or '.dylib' in name or '.framework' in name

    def file_check_qt_exec(file):
        return '.framework/Versions/5/Qt' in file        

    extract_files_recursive(install_path + '/bin/', is_object_file, files)
    extract_files_recursive(install_path + '/lib/python/PySide2/Qt/lib/', file_check_qt_exec, files)
    extract_files_recursive(install_path + '/lib/', file_check, files)
    extract_files_recursive(install_path + '/plugin/', file_check, files)
    extract_files_recursive(install_path + '/share/', file_check, files)
    extract_files_recursive(install_path + '/tests', file_check, files)
    extract_files(install_path + '/tests', (lambda file: isfile(file) and open(file).readline().rstrip()[0] != "#"), files)

    remove_rpath(install_path + '/lib', files)
    add_rpath_to_files(install_path + '/lib/', files)
    change_absolute_to_relative(files, install_path)
    codesign_files(files)

    ctest_files = []
    extract_files_recursive(install_path + '/build/', (lambda file: 'CTestTestfile' in file), ctest_files)
    for file in ctest_files:
        replace_string_in_file(file, install_path , "$ENV{USD_BUILD}")
        replace_string_in_file(file, src_path , "$ENV{USD_BUILD}")

    copy(src_path+"/build_scripts/Hydra.command", install_path+"/Hydra.command")
