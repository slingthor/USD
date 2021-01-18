#
# Copyright 2017 Pixar
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#
from __future__ import print_function

from distutils.spawn import find_executable

import argparse
import codecs
import contextlib
import copy
import ctypes
import datetime
import distutils
import fnmatch
import glob
import locale
import multiprocessing
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import sysconfig
import tarfile
import zipfile

if sys.version_info.major >= 3:
    from urllib.request import urlopen
else:
    from urllib2 import urlopen

# Helpers for printing output
verbosity = 1

def Print(msg):
    if verbosity > 0:
        print(msg)

def PrintWarning(warning):
    if verbosity > 0:
        print("WARNING:", warning)

def PrintStatus(status):
    if verbosity >= 1:
        print("STATUS:", status)

def PrintInfo(info):
    if verbosity >= 2:
        print("INFO:", info)

def PrintCommandOutput(output):
    if verbosity >= 3:
        sys.stdout.write(output)

def PrintError(error):
    if verbosity >= 3 and sys.exc_info()[1] is not None:
        import traceback
        traceback.print_exc()
    print ("ERROR:", error)

# cross platform defaults
# macOS can target macOS and iOS platforms
crossPlatform = None;

# Helpers for determining platform
def Windows():
    return platform.system() == "Windows"
def Linux():
    return platform.system() == "Linux"
def MacOS():
    return (platform.system() == "Darwin") and crossPlatform is None
def iOS():
    return (platform.system() == "Darwin") and crossPlatform == "iOS"

def Python3():
    return sys.version_info.major == 3

def GetLocale():
    return sys.stdout.encoding or locale.getdefaultlocale()[1] or "UTF-8"

def GetCommandOutput(command, captureStdErr=True):
    """Executes the specified command and returns output or None.
    If command contains pipes (i.e '|'s), creates a subprocess for
    each pipe in command, returning the output from the last subcommand
    or None if any of the subcommands result in a CalledProcessError"""

    result = None

    args = shlex.split(command)
    commands = []
    cmd_args = []
    while args:
        arg = args.pop(0)
        if arg == '|':
            commands.append((cmd_args))
            cmd_args = []
        else:
            cmd_args.append(arg)
    commands.append((cmd_args))

    pipes = []
    while len(commands) > 1:
        # We have some pipes
        command = commands.pop(0)
        stdin = pipes[-1].stdout if pipes else None
        try:
            pipe = subprocess.Popen(command, stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.PIPE if captureStdErr else None)
            pipes.append(pipe)
        except subprocess.CalledProcessError:
            return None

    # The last command actually returns a result
    command = commands[0]
    try:
        stdin = pipes[-1].stdout if pipes else None
        result = subprocess.check_output(
            command,
            stdin = stdin,
            stderr=subprocess.STDOUT if captureStdErr else None).decode('utf-8').strip()
    except subprocess.CalledProcessError:
        pass

    # clean-up
    for pipe in pipes:
        pipe.wait()
    
    return result

def GetMacArch():
    macArch = GetCommandOutput('arch').strip()
    if macArch == "i386" or macArch == "x86_64":
        macArch = "x86_64"
    else:
        macArch = "arm64"
    return macArch

def SupportsMacOSUniversalBinaries():
    MacOS_SDK = GetCommandOutput('/usr/bin/xcodebuild -version').split(' ')[1]
    return MacOS() and MacOS_SDK >= "10.16"

def GetXcodeDeveloperDirectory():
    """Returns the active developer directory as reported by 'xcode-select -p'.
    Returns None if none is set."""
    if not MacOS() and not iOS():
        return None

    return GetCommandOutput("xcode-select -p")

def CheckCodeSignID():
    SDKVersion  = GetCommandOutput('xcodebuild -version').strip()[6:10]
    codeSignIDs = GetCommandOutput('security find-identity -v -p codesigning')
    if codeSignIDs is None:
        codeSignIDs = ""

    codeSignID = os.environ.get('XCODE_ATTRIBUTE_CODE_SIGN_ID')
    if codeSignID is not None:
        # Edge case for ad-hoc codesigning in iOS, which requires setting 
        # CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY = "" to generate an Xcode project
        # while "-" is being used by the codesign command line tool
        if codeSignID == "":
            codeSignID = "-"
    elif SDKVersion >= "11.0" and codeSignIDs.find("Apple Development") != -1:
        codeSignID = "Apple Development"
    elif codeSignIDs.find("Mac Developer") != -1:
        codeSignID = "Mac Developer"
    else:
        PrintError("Unable to identify code signing identity. " +
            "Please specify by setting the XCODE_ATTRIBUTE_CODE_SIGN_ID environment " +
            "variable to the one you'd like to use. \n" +
            "If you don't have a code signing identity, you can create one using Xcode:\n" +
            "https://help.apple.com/xcode/mac/current/#/dev154b28f09 \n")
        sys.exit(1)

    # Validate that we have a codesign ID that both exists and isn't ambiguous
    if codeSignIDs.count(codeSignID) != 1 and codeSignID != "-":
        PrintError("Unable to identify code signing identity. " +
            "Please specify by setting the XCODE_ATTRIBUTE_CODE_SIGN_ID environment " +
            "variable to the one you'd like to use. Options are:\n" + codeSignIDs)
        sys.exit(1)

    os.environ['CODE_SIGN_ID'] = codeSignID
    return codeSignID

def GetVisualStudioCompilerAndVersion():
    """Returns a tuple containing the path to the Visual Studio compiler
    and a tuple for its version, e.g. (14, 0). If the compiler is not found
    or version number cannot be determined, returns None."""
    if not Windows():
        return None

    msvcCompiler = find_executable('cl')
    if msvcCompiler:
        # VisualStudioVersion environment variable should be set by the
        # Visual Studio Command Prompt.
        match = re.search(
            r"(\d+)\.(\d+)",
            os.environ.get("VisualStudioVersion", ""))
        if match:
            return (msvcCompiler, tuple(int(v) for v in match.groups()))
    return None

def IsVisualStudioVersionOrGreater(desiredVersion):
    if not Windows():
        return False

    msvcCompilerAndVersion = GetVisualStudioCompilerAndVersion()
    if msvcCompilerAndVersion:
        _, version = msvcCompilerAndVersion
        return version >= desiredVersion
    return False

def IsVisualStudio2019OrGreater():
    VISUAL_STUDIO_2019_VERSION = (16, 0)
    return IsVisualStudioVersionOrGreater(VISUAL_STUDIO_2019_VERSION)

def IsVisualStudio2017OrGreater():
    VISUAL_STUDIO_2017_VERSION = (15, 0)
    return IsVisualStudioVersionOrGreater(VISUAL_STUDIO_2017_VERSION)

def IsVisualStudio2015OrGreater():
    VISUAL_STUDIO_2015_VERSION = (14, 0)
    return IsVisualStudioVersionOrGreater(VISUAL_STUDIO_2015_VERSION)

def IsMayaPython():
    """Determine whether we're running in Maya's version of Python. When 
    building against Maya's Python, there are some additional restrictions
    on what we're able to build."""
    try:
        import maya
        return True
    except:
        pass

    return False

def GetPythonInfo():
    """Returns a tuple containing the path to the Python executable, shared
    library, and include directory corresponding to the version of Python
    currently running. Returns None if any path could not be determined.

    This function is used to extract build information from the Python 
    interpreter used to launch this script. This information is used
    in the Boost and USD builds. By taking this approach we can support
    having USD builds for different Python versions built on the same
    machine. This is very useful, especially when developers have multiple
    versions installed on their machine, which is quite common now with 
    Python2 and Python3 co-existing.
    """
    # First we extract the information that can be uniformly dealt with across
    # the platforms:
    pythonExecPath = sys.executable
    pythonVersion = sysconfig.get_config_var("py_version_short")  # "2.7"
    pythonVersionNoDot = sysconfig.get_config_var("py_version_nodot") # "27"

    # Lib path is unfortunately special for each platform and there is no
    # config_var for it. But we can deduce it for each platform, and this
    # logic works for any Python version.
    def _GetPythonLibraryFilename():
        if Windows():
            return "python" + pythonVersionNoDot + ".lib"
        elif Linux():
            return sysconfig.get_config_var("LDLIBRARY")
        elif MacOS():
            return "libpython" + pythonVersion + ".dylib"
        else:
            raise RuntimeError("Platform not supported")

    # XXX: Handle the case where this script is being called using Maya's
    # Python since the sysconfig variables are set up differently in Maya.
    # Ideally we would not have any special Maya knowledge in here at all.
    if IsMayaPython():
        pythonBaseDir = sysconfig.get_config_var("base")

        # On Windows, the "base" path points to a "Python\" subdirectory
        # that contains the DLLs for site-package modules but not the
        # directories for the headers and .lib file we need -- those 
        # are one level up.
        if Windows():
            pythonBaseDir = os.path.dirname(pythonBaseDir)
        
        pythonIncludeDir = os.path.join(pythonBaseDir, "include",
                                        "python" + pythonVersion)
        pythonLibPath = os.path.join(pythonBaseDir, "lib",
                                     _GetPythonLibraryFilename())
    else:
        pythonIncludeDir = sysconfig.get_config_var("INCLUDEPY")
        if Windows():
            pythonBaseDir = sysconfig.get_config_var("base")
            pythonLibPath = os.path.join(pythonBaseDir, "libs",
                                         _GetPythonLibraryFilename())
        elif Linux():
            pythonLibDir = sysconfig.get_config_var("LIBDIR")
            pythonMultiarchSubdir = sysconfig.get_config_var("multiarchsubdir")
            if pythonMultiarchSubdir:
                pythonLibDir = pythonLibDir + pythonMultiarchSubdir
            pythonLibPath = os.path.join(pythonLibDir,
                                         _GetPythonLibraryFilename())
        elif MacOS():
            pythonBaseDir = sysconfig.get_config_var("base")
            pythonLibPath = os.path.join(pythonBaseDir, "lib",
                                         _GetPythonLibraryFilename())
        else:
            raise RuntimeError("Platform not supported")

    return (pythonExecPath, pythonLibPath, pythonIncludeDir, pythonVersion)

def GetCPUCount():
    try:
        return multiprocessing.cpu_count()
    except NotImplementedError:
        return 1

def Run(cmd, logCommandOutput = True):
    """Run the specified command in a subprocess."""
    PrintInfo('Running "{cmd}"'.format(cmd=cmd))

    with codecs.open("log.txt", "a", "utf-8") as logfile:
        logfile.write(datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))
        logfile.write("\n")
        logfile.write(cmd)
        logfile.write("\n")

        # Let exceptions escape from subprocess calls -- higher level
        # code will handle them.
        if logCommandOutput:
            p = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, 
                                 stderr=subprocess.STDOUT)
            while True:
                l = p.stdout.readline().decode(GetLocale(), 'replace')
                if l:
                    logfile.write(l)
                    PrintCommandOutput(l)
                elif p.poll() is not None:
                    break
        else:
            p = subprocess.Popen(shlex.split(cmd))
            p.wait()

    if p.returncode != 0:
        # If verbosity >= 3, we'll have already been printing out command output
        # so no reason to print the log file again.
        if verbosity < 3:
            with open("log.txt", "r") as logfile:
                Print(logfile.read())
        raise RuntimeError("Failed to run '{cmd}'\nSee {log} for more details."
                           .format(cmd=cmd, log=os.path.abspath("log.txt")))

@contextlib.contextmanager
def CurrentWorkingDirectory(dir):
    """Context manager that sets the current working directory to the given
    directory and resets it to the original directory when closed."""
    curdir = os.getcwd()
    os.chdir(dir)
    try: yield
    finally: os.chdir(curdir)

def CreateUniversalBinaries(context, libNames, x86Dir, armDir):
    lipoCommands = []
    xcodeRoot = subprocess.check_output(["xcode-select", "--print-path"]).strip()
    lipoBinary = "{XCODE_ROOT}/Toolchains/XcodeDefault.xctoolchain/usr/bin/lipo".format(XCODE_ROOT=xcodeRoot)
    for libName in libNames:
        outputName = os.path.join(context.instDir, "lib", libName)
        if not os.path.islink("{x86Dir}/{libName}".format(x86Dir=x86Dir, libName=libName)):
            if os.path.exists(outputName):
                os.remove(outputName)
            lipoCmd = "{lipo} -create {x86Dir}/{libName} {armDir}/{libName} -output {outputName}".format(
                lipo=lipoBinary, x86Dir=x86Dir, armDir=armDir, libName=libName, outputName=outputName)
            lipoCommands.append(lipoCmd)
            Run(lipoCmd)
    for libName in libNames:
        if os.path.islink("{x86Dir}/{libName}".format(x86Dir=x86Dir, libName=libName)):
            outputName = os.path.join(context.instDir, "lib", libName)
            if os.path.exists(outputName):
                os.unlink(outputName)
            targetName = os.readlink("{x86Dir}/{libName}".format(x86Dir=x86Dir, libName=libName))
            targetName = os.path.basename(targetName)
            os.symlink("{instDir}/lib/{libName}".format(instDir=context.instDir, libName=targetName),
                outputName)
    return lipoCommands

def CopyFiles(context, src, dest):
    """Copy files like shutil.copy, but src may be a glob pattern."""
    filesToCopy = glob.glob(src)
    if not filesToCopy:
        raise RuntimeError("File(s) to copy {src} not found".format(src=src))

    instDestDir = os.path.join(context.instDir, dest)
    for f in filesToCopy:
        PrintCommandOutput("Copying {file} to {destDir}\n"
                           .format(file=f, destDir=instDestDir))
        shutil.copy(f, instDestDir)

def CopyDirectory(context, srcDir, destDir):
    """Copy directory like shutil.copytree."""
    instDestDir = os.path.join(context.instDir, destDir)
    if os.path.isdir(instDestDir):
        shutil.rmtree(instDestDir)    

    PrintCommandOutput("Copying {srcDir} to {destDir}\n"
                       .format(srcDir=srcDir, destDir=instDestDir))
    shutil.copytree(srcDir, instDestDir)

def FormatMultiProcs(numJobs, generator):
    tag = "-j"
    if generator:
        if "Visual Studio" in generator:
            tag = "/M:"
        elif "Xcode" in generator:
            tag = "-j "

    return "{tag}{procs}".format(tag=tag, procs=numJobs)

def RunCMake(context, force, buildArgs = None, hostPlatform = False):
    """Invoke CMake to configure, build, and install a library whose 
    source code is located in the current working directory."""
    # Create a directory for out-of-source builds in the build directory
    # using the name of the current working directory.
    targetIOS = (not hostPlatform) and iOS()
    targetMacOS = MacOS() or hostPlatform

    srcDir = os.getcwd()
    instDir = (context.usdInstDir if srcDir == context.usdSrcDir
               else context.instDir)
    buildDir = os.path.join(context.buildDir, os.path.split(srcDir)[1])
    if force and os.path.isdir(buildDir):
        shutil.rmtree(buildDir)

    if not os.path.isdir(buildDir):
        os.makedirs(buildDir)

    generator = context.cmakeGenerator

    # On Windows, we need to explicitly specify the generator to ensure we're
    # building a 64-bit project. (Surely there is a better way to do this?)
    # TODO: figure out exactly what "vcvarsall.bat x64" sets to force x64
    if generator is None:
        if Windows():
            if IsVisualStudio2019OrGreater():
                generator = "Visual Studio 16 2019"
            elif IsVisualStudio2017OrGreater():
                generator = "Visual Studio 15 2017 Win64"
            else:
                generator = "Visual Studio 14 2015 Win64"
        elif targetMacOS or targetIOS:
            generator = "Xcode"

    if generator is not None:
        generator = '-G {gen}'.format(gen=generator)

    if IsVisualStudio2019OrGreater():
        generator = generator + " -A x64"

    toolset = context.cmakeToolset
    if toolset is not None:
        toolset = '-T "{toolset}"'.format(toolset=toolset)

    # On MacOS, enable the use of @rpath for relocatable builds.
    osx_rpath = None
    if targetMacOS or targetIOS:
        osx_rpath = "-DCMAKE_MACOSX_RPATH=ON"

    extraArgs = copy.deepcopy(buildArgs)

    # TEMPORARY WORKAROUND
    if targetMacOS or targetIOS:
        extraArgs.append('-DCMAKE_IGNORE_PATH="/usr/lib;/usr/local/lib;/lib" ')

        # CMake 3.19.0 and later defaults to use Xcode's Modern Build System.
        # This causes the external dependencies to fail with "Multiple commands produce the same output" errors.
        # Fix: Force CMake to use the old build system.
        if GetCMakeVersion() >= (3, 19, 0):
            extraArgs.append('-T buildsystem=1')

    if targetMacOS:
        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            extraArgs.append('-DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO')
            extraArgs.append('-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64')
        else:
            extraArgs.append('-DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=YES')
            MacArch = GetCommandOutput('arch').strip()
            if MacArch == "i386" or MacArch == "x86_64":
                extraArgs.append('-DCMAKE_OSX_ARCHITECTURES=x86_64')
            else:
                extraArgs.append('-DCMAKE_OSX_ARCHITECTURES=arm64')

    if targetIOS:
        # Add the default iOS toolchain file if one isn't aready specified
        if not any("-DCMAKE_TOOLCHAIN_FILE=" in s for s in extraArgs):
            extraArgs.append(
                '-DCMAKE_TOOLCHAIN_FILE={usdSrcDir}/cmake/toolchains/ios.toolchain.cmake '
                .format(usdSrcDir=context.usdSrcDir))

        CODE_SIGN_ID = CheckCodeSignID()

        DEVELOPMENT_TEAM = os.environ.get('XCODE_ATTRIBUTE_DEVELOPMENT_TEAM')
        if DEVELOPMENT_TEAM is None and not CODE_SIGN_ID == "-":
            x509subject = GetCommandOutput('security find-certificate -c "{}" -p | openssl x509 -subject | head -1'.format(CODE_SIGN_ID)).strip()
            # Extract the Organizational Unit (OU field) from the cert
            DEVELOPMENT_TEAM = [elm for elm in x509subject.split('/') if elm.startswith('OU')][0].split('=')[1]

        # Edge case for iOS
        if CODE_SIGN_ID == "-":
            CODE_SIGN_ID = ""

        extraArgs.append(
                '-DIOS_PLATFORM=OS '
                '-DENABLE_VISIBILITY=1 '
                '-DAPPLEIOS=1 '
                '-DENABLE_ARC=0 '
                '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="{codesignid}" '
                '-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM={developmentTeam} '
                '-DPYTHON_INCLUDE_DIR=/System/Library/Frameworks/Python.framework/Versions/2.7/include/python2.7  '
                '-DPYTHON_LIBRARY=/System/Library/Frameworks/Python.framework/Versions/2.7/lib '
                '-DPYTHON_EXECUTABLE:FILEPATH=/usr/bin/python '.format(
                    codesignid=CODE_SIGN_ID,
                    developmentTeam=DEVELOPMENT_TEAM))

    # We use -DCMAKE_BUILD_TYPE for single-configuration generators 
    # (Ninja, make), and --config for multi-configuration generators 
    # (Visual Studio); technically we don't need BOTH at the same
    # time, but specifying both is simpler than branching
    config=("Debug" if context.buildDebug else "Release")

    with CurrentWorkingDirectory(buildDir):
        Run('cmake '
            '-DCMAKE_INSTALL_PREFIX="{instDir}" '
            '-DCMAKE_PREFIX_PATH="{depsInstDir}" '
            '-DCMAKE_BUILD_TYPE={config} '
            '{osx_rpath} '
            '{generator} '
            '{toolset} '
            '{extraArgs} '
            '"{srcDir}"'
            .format(instDir=instDir,
                    depsInstDir=context.instDir,
                    config=config,
                    srcDir=srcDir,
                    osx_rpath=(osx_rpath or ""),
                    generator=(generator or ""),
                    toolset=(toolset or ""),
                    extraArgs=(" ".join(extraArgs) if extraArgs else "")))

        Run("cmake --build . --config {config} --target install -- {multiproc}"
            .format(config=config,
                    multiproc=FormatMultiProcs(context.numJobs, generator)))

def GetCMakeVersion():
    """
    Returns the CMake version as tuple of integers (major, minor) or
    (major, minor, patch) or None if an error occured while launching cmake and
    parsing its output.
    """

    output_string = GetCommandOutput("cmake --version")
    if not output_string:
        PrintWarning("Could not determine cmake version -- please install it "
                     "and adjust your PATH")
        return None

    # cmake reports, e.g., "... version 3.14.3"
    match = re.search(r"version (\d+)\.(\d+)(\.(\d+))?", output_string)
    if not match:
        PrintWarning("Could not determine cmake version")
        return None

    major, minor, patch_group, patch = match.groups()
    if patch_group is None:
        return (int(major), int(minor))
    else:
        return (int(major), int(minor), int(patch))

def PatchFile(filename, patches, multiLineMatches=False):
    """Applies patches to the specified file. patches is a list of tuples
    (old string, new string)."""
    if multiLineMatches:
        oldLines = [open(filename, 'r').read()]
    else:
        oldLines = open(filename, 'r').readlines()
    newLines = oldLines
    for (oldString, newString) in patches:
        newLines = [s.replace(oldString, newString) for s in newLines]
    if newLines != oldLines:
        PrintInfo("Patching file {filename} (original in {oldFilename})..."
                  .format(filename=filename, oldFilename=filename + ".old"))
        shutil.copy(filename, filename + ".old")
        open(filename, 'w').writelines(newLines)

def DownloadFileWithCurl(url, outputFilename):
    # Don't log command output so that curl's progress
    # meter doesn't get written to the log file.
    Run("curl {progress} -L -o {filename} {url}".format(
        progress="-#" if verbosity >= 2 else "-s",
        filename=outputFilename, url=url), 
        logCommandOutput=False)

def DownloadFileWithPowershell(url, outputFilename):
    # It's important that we specify to use TLS v1.2 at least or some
    # of the downloads will fail.
    cmd = "powershell [Net.ServicePointManager]::SecurityProtocol = \
            [Net.SecurityProtocolType]::Tls12; \"(new-object \
            System.Net.WebClient).DownloadFile('{url}', '{filename}')\""\
            .format(filename=outputFilename, url=url)

    Run(cmd,logCommandOutput=False)

def DownloadFileWithUrllib(url, outputFilename):
    r = urlopen(url)
    with open(outputFilename, "wb") as outfile:
        outfile.write(r.read())

def GetDownloadCacheFileName(srcDir, url):
    filename = url.split("/")[-1]
    return srcDir + '/cache/' + filename

def DownloadFromCache(srcDir, url, outputFilename):
    filepath = GetDownloadCacheFileName(srcDir, url)
    shutil.copy(os.path.abspath(filepath), outputFilename)

def DownloadURL(url, context, force, dontExtract = None):
    """Download and extract the archive file at given URL to the
    source directory specified in the context. 

    dontExtract may be a sequence of path prefixes that will
    be excluded when extracting the archive.

    Returns the absolute path to the directory where files have 
    been extracted."""
    with CurrentWorkingDirectory(context.srcDir):
        # Extract filename from URL and see if file already exists. 
        filename = url.split("/")[-1]
        if force and os.path.exists(filename):
            os.remove(filename)

        if os.path.exists(filename):
            PrintInfo("{0} already exists, skipping download"
                      .format(os.path.abspath(filename)))
        else:
            if context.downloader == DownloadFromCache:
                PrintInfo("Copying {0} to {1}"
                          .format(GetDownloadCacheFileName(context.usdSrcDir, url), os.path.abspath(filename)))
            else:
                PrintInfo("Downloading {0} to {1}"
                          .format(url, os.path.abspath(filename)))

            # To work around occasional hiccups with downloading from websites
            # (SSL validation errors, etc.), retry a few times if we don't
            # succeed in downloading the file.
            maxRetries = 5
            lastError = None

            # Download to a temporary file and rename it to the expected
            # filename when complete. This ensures that incomplete downloads
            # will be retried if the script is run again.
            tmpFilename = filename + ".tmp"
            if os.path.exists(tmpFilename):
                os.remove(tmpFilename)

            for i in range(maxRetries):
                try:
                    if context.downloader == DownloadFromCache:
                        context.downloader(context.usdSrcDir, url, tmpFilename)
                    else:
                        context.downloader(url, tmpFilename)
                    break
                except Exception as e:
                    PrintCommandOutput("Retrying download due to error: {err}\n"
                                       .format(err=e))
                    lastError = e
            else:
                errorMsg = str(lastError)
                if "SSL: TLSV1_ALERT_PROTOCOL_VERSION" in errorMsg:
                    errorMsg += ("\n\n"
                                 "Your OS or version of Python may not support "
                                 "TLS v1.2+, which is required for downloading "
                                 "files from certain websites. This support "
                                 "was added in Python 2.7.9."
                                 "\n\n"
                                 "You can use curl to download dependencies "
                                 "by installing it in your PATH and re-running "
                                 "this script.")
                raise RuntimeError("Failed to download {url}: {err}"
                                   .format(url=url, err=errorMsg))

            shutil.move(tmpFilename, filename)

        # Open the archive and retrieve the name of the top-most directory.
        # This assumes the archive contains a single directory with all
        # of the contents beneath it.
        archive = None
        rootDir = None
        members = None
        try:
            if tarfile.is_tarfile(filename):
                archive = tarfile.open(filename)
                rootDir = archive.getnames()[0].split('/')[0]
                if dontExtract != None:
                    members = (m for m in archive.getmembers() 
                               if not any((fnmatch.fnmatch(m.name, p)
                                           for p in dontExtract)))
            elif zipfile.is_zipfile(filename):
                archive = zipfile.ZipFile(filename)
                rootDir = archive.namelist()[0].split('/')[0]
                if dontExtract != None:
                    members = (m for m in archive.getnames() 
                               if not any((fnmatch.fnmatch(m, p)
                                           for p in dontExtract)))
            else:
                raise RuntimeError("unrecognized archive file type")

            with archive:
                extractedPath = os.path.abspath(rootDir)
                if force and os.path.isdir(extractedPath):
                    shutil.rmtree(extractedPath)

                if os.path.isdir(extractedPath):
                    PrintInfo("Directory {0} already exists, skipping extract"
                              .format(extractedPath))
                else:
                    PrintInfo("Extracting archive to {0}".format(extractedPath))

                    # Extract to a temporary directory then move the contents
                    # to the expected location when complete. This ensures that
                    # incomplete extracts will be retried if the script is run
                    # again.
                    tmpExtractedPath = os.path.abspath("extract_dir")
                    if os.path.isdir(tmpExtractedPath):
                        shutil.rmtree(tmpExtractedPath)

                    archive.extractall(tmpExtractedPath, members=members)

                    shutil.move(os.path.join(tmpExtractedPath, rootDir),
                                extractedPath)
                    shutil.rmtree(tmpExtractedPath)

                return extractedPath
        except Exception as e:
            # If extraction failed for whatever reason, assume the
            # archive file was bad and move it aside so that re-running
            # the script will try downloading and extracting again.
            shutil.move(filename, filename + ".bad")
            raise RuntimeError("Failed to extract archive {filename}: {err}"
                               .format(filename=filename, err=e))

############################################################
# 3rd-Party Dependencies

AllDependencies = list()
AllDependenciesByName = dict()

class Dependency(object):
    def __init__(self, name, installer, *files):
        self.name = name
        self.installer = installer
        self.filesToCheck = files

        AllDependencies.append(self)
        AllDependenciesByName.setdefault(name.lower(), self)

    def Exists(self, context):
        return all([os.path.isfile(os.path.join(context.instDir, f))
                    for f in self.filesToCheck])

class PythonDependency(object):
    def __init__(self, name, getInstructions, moduleNames):
        self.name = name
        self.getInstructions = getInstructions
        self.moduleNames = moduleNames

    def Exists(self, context):
        # If one of the modules in our list imports successfully, we are good.
        for moduleName in self.moduleNames:
            try:
                pyModule = __import__(moduleName)
                return True
            except:
                pass

        return False

def AnyPythonDependencies(deps):
    return any([type(d) is PythonDependency for d in deps])

############################################################
# zlib

ZLIB_URL = "https://github.com/madler/zlib/archive/v1.2.11.zip"

def InstallZlib(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(ZLIB_URL, context, force)):

        if iOS():
            # Replace test executables with static libraries to avoid issues with code signing.
            PatchFile("CMakeLists.txt", 
                [("add_executable(example test/example.c)", "add_library(example STATIC test/example.c)"),
                 ("add_executable(minigzip test/minigzip.c)", "add_library(minigzip STATIC test/minigzip.c)")])

        RunCMake(context, force, buildArgs)
        return os.getcwd()

ZLIB = Dependency("zlib", InstallZlib, "include/zlib.h")

############################################################
# boost

if MacOS() or iOS():
    BOOST_URL = "https://downloads.sourceforge.net/project/boost/boost/1.70.0/boost_1_70_0.tar.bz2"
    BOOST_VERSION_FILE = "include/boost/version.hpp"
elif Linux():
    if Python3():
        BOOST_URL = "https://downloads.sourceforge.net/project/boost/boost/1.70.0/boost_1_70_0.tar.gz"
    else:
        BOOST_URL = "https://downloads.sourceforge.net/project/boost/boost/1.61.0/boost_1_61_0.tar.bz2"
    BOOST_VERSION_FILE = "include/boost/version.hpp"
elif Windows():
    # The default installation of boost on Windows puts headers in a versioned 
    # subdirectory, which we have to account for here. In theory, specifying 
    # "layout=system" would make the Windows install match Linux/MacOS, but that 
    # causes problems for other dependencies that look for boost.
    #
    # boost 1.70 is required for Visual Studio 2019. For simplicity, we use
    # this version for all older Visual Studio versions as well.
    BOOST_URL = "https://downloads.sourceforge.net/project/boost/boost/1.70.0/boost_1_70_0.tar.gz"
    BOOST_VERSION_FILE = "include/boost-1_70/boost/version.hpp"

def InstallBoost_Helper(context, force, buildArgs):
    # Documentation files in the boost archive can have exceptionally
    # long paths. This can lead to errors when extracting boost on Windows,
    # since paths are limited to 260 characters by default on that platform.
    # To avoid this, we skip extracting all documentation.
    #
    # For some examples, see: https://svn.boost.org/trac10/ticket/11677
    dontExtract = ["*/doc/*", "*/libs/*/doc/*"]

    with CurrentWorkingDirectory(DownloadURL(BOOST_URL, context, force, 
                                             dontExtract)):

        # The following 4 patches could be discarded if we bump up boost version
        if (MacOS() or iOS()) and not Python3():
            PatchFile("tools/build/src/engine/make.c",
                [('#include "jam.h"\n', '#include "jam.h"\n#include "output.h"\n')])
            PatchFile("tools/build/src/engine/execcmd.c",
                [('#include "jam.h"\n', '#include "jam.h"\n#include "output.h"\n')])
            PatchFile("tools/build/src/engine/filesys.h",
                [('void filelist_free( FILELIST * list );\n',
                  'void filelist_free( FILELIST * list );\n'
                  'int filelist_empty( FILELIST * list );\n'
                  'file_info_t * file_query( OBJECT * const path );\n'
                  'int file_collect_dir_content_( file_info_t * const dir );\n'
                  'int file_collect_archive_content_( file_archive_info_t * const archive );')])
            PatchFile("tools/build/src/engine/modules/path.c",
                [('#include "../timestamp.h"\n', '#include "../timestamp.h"\n#include "../filesys.h"\n')])

        # GitHub: https://github.com/boostorg/build/issues/440
        # Incorrect comparison of version number on Darwin #440
        if MacOS() or iOS():
            PatchFile("tools/build/src/tools/darwin.jam",
                [('    if $(real-version) < "4.0.0"\n    {\n'
                  '        flags darwin.compile.c++ OPTIONS $(condition) : -fcoalesce-templates ;\n    }',
                  '#    if $(real-version) < "4.0.0"\n#    {\n'
                  '#        flags darwin.compile.c++ OPTIONS $(condition) : -fcoalesce-templates ;\n#    }'),
                 ('    if $(real-version) < "4.2.0"\n    {\n'
                  '        flags darwin.compile OPTIONS $(condition) : -Wno-long-double ;\n    }',
                  '#    if $(real-version) < "4.2.0"\n#    {\n'
                  '#       flags darwin.compile OPTIONS $(condition) : -Wno-long-double ;\n#    }')], True)


        bootstrap = "bootstrap.bat" if Windows() else "./bootstrap.sh"
        bootstrapCmd = '{bootstrap} --prefix="{instDir}"'.format(
            bootstrap=bootstrap, instDir=context.instDir)
        Run(bootstrapCmd)

        # b2 supports at most -j64 and will error if given a higher value.
        num_procs = min(64, context.numJobs)

        b2_settings = [
            '--prefix="{instDir}"'.format(instDir=context.instDir),
            '--build-dir="{buildDir}"'.format(buildDir=context.buildDir),
            '-j{procs}'.format(procs=num_procs),
            'address-model=64',
            'threading=multi', 
            'variant={variant}'
                .format(variant="debug" if context.buildDebug else "release"),
            '--with-atomic',
            '--with-program_options',
            '--with-regex'
        ]

        projectPath = 'user-config.jam'
        b2_settings.append("--user-config=user-config.jam")
        if os.path.exists(projectPath): 
            os.remove(projectPath)

        with open(projectPath, 'w') as projectFile:
            projectFile.write('\n')

        if context.buildPython:
            b2_settings.append("--with-python")
            pythonInfo = GetPythonInfo()
            if Windows():
                # Unfortunately Boost build scripts require the Python folder 
                # that contains the executable on Windows
                pythonPath = os.path.dirname(pythonInfo[0])
            else:
                # While other platforms want the complete executable path
                pythonPath = pythonInfo[0]
            # This is the only platform-independent way to configure these
            # settings correctly and robustly for the Boost jam build system.
            # There are Python config arguments that can be passed to bootstrap 
            # but those are not available in boostrap.bat (Windows) so we must 
            # take the following approach:
            with open(projectPath, 'a') as projectFile:
                # Note that we must escape any special characters, like 
                # backslashes for jam, hence the mods below for the path 
                # arguments. Also, if the path contains spaces jam will not
                # handle them well. Surround the path parameters in quotes.
                line = 'using python : %s : "%s" : "%s" ;\n\n' % (pythonInfo[3], 
                       pythonPath.replace('\\', '\\\\'), 
                       pythonInfo[2].replace('\\', '\\\\'))
                projectFile.write(line)

        if context.buildOIIO:
            b2_settings.append("--with-date_time")

        if context.buildOIIO or context.enableOpenVDB:
            b2_settings.append("--with-system")
            b2_settings.append("--with-thread")

        if context.enableOpenVDB:
            b2_settings.append("--with-iostreams")

            # b2 with -sNO_COMPRESSION=1 fails with the following error message:
            #     error: at [...]/boost_1_61_0/tools/build/src/kernel/modules.jam:107
            #     error: Unable to find file or target named
            #     error:     '/zlib//zlib'
            #     error: referred to from project at
            #     error:     'libs/iostreams/build'
            #     error: could not resolve project reference '/zlib'

            # But to avoid an extra library dependency, we can still explicitly
            # exclude the bzip2 compression from boost_iostreams (note that
            # OpenVDB uses blosc compression).
            b2_settings.append("-sNO_BZIP2=1")

        if context.buildOIIO:
            b2_settings.append("--with-filesystem")

        if force:
            b2_settings.append("-a")

        b2_toolset = ''

        if Windows():
            # toolset parameter for Visual Studio documented here:
            # https://github.com/boostorg/build/blob/develop/src/tools/msvc.jam
            if context.cmakeToolset == "v142":
                b2_toolset.append("toolset=msvc-14.2")
            elif context.cmakeToolset == "v141":
                b2_toolset.append("toolset=msvc-14.1")
            elif context.cmakeToolset == "v140":
                b2_toolset.append("toolset=msvc-14.0")
            elif IsVisualStudio2019OrGreater():
                b2_toolset = "toolset=msvc-14.2"
            elif IsVisualStudio2017OrGreater():
                b2_toolset = "toolset=msvc-14.1"
            else:
                b2_toolset = "toolset=msvc-14.0"

        if MacOS():
            # Must specify toolset=clang to ensure install_name for boost
            # libraries includes @rpath
            b2_toolset = "toolset=clang"

        sdkPath = ''
        if MacOS() or iOS():
            xcodeRoot = GetCommandOutput('xcode-select --print-path').strip()

            if MacOS():
                sdkPath = GetCommandOutput('xcrun --sdk macosx --show-sdk-path').strip()
            else:
                sdkPath = GetCommandOutput('xcrun --sdk iphoneos --show-sdk-path').strip()

        if iOS():
            b2_toolset = "toolset=darwin-iphone"

            b2_settings.append("architecture=arm")
            b2_settings.append("target-os=iphone")
            b2_settings.append("define=_LITTLE_ENDIAN")
            b2_settings.append("cxxflags=\"-std=c++14 -stdlib=libc++ -arch=arm64\"")
            b2_settings.append("linkflags=\"-stdlib=libc++\"");
            b2_settings.append("link=static")

            newLines = [
                'using darwin : iphone\n',
                ': {XCODE_ROOT}/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
                    .format(XCODE_ROOT=xcodeRoot),
                ' -arch arm64 -mios-version-min=10.0 -fembed-bitcode -Wno-unused-local-typedef -Wno-nullability-completeness -DBOOST_AC_USE_PTHREADS -DBOOST_SP_USE_PTHREADS -g -DNDEBUG\n',
                ': <striper> <root>{XCODE_ROOT}/Platforms/iPhoneOS.platform/Developer\n'
                    .format(XCODE_ROOT=xcodeRoot),
                ': <architecture>arm <target-os>iphone <address-model>64\n',
                ';'
            ]

            iOSVersion = GetCommandOutput('xcodebuild -sdk ' + sdkPath + ' -version  SDKVersion').strip()

            b2_settings.append("macosx-version=iphone-{IOS_SDK_VERSION}".format(
                IOS_SDK_VERSION=iOSVersion))

            with open(projectPath, 'w') as projectFile:
                projectFile.writelines(newLines)
        else:
            b2_settings.append("link=shared")
            b2_settings.append("runtime-link=shared")

        # Add on any user-specified extra arguments.
        b2_settings += buildArgs

        b2 = "b2" if Windows() else "./b2"

        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            newLines = [
                'using clang-darwin : x86_64\n',
                ': {XCODE_ROOT}/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++\n'
                    .format(XCODE_ROOT=xcodeRoot),
                ': <compileflags>"-target x86_64-apple-macos10.15 -isysroot {SDK_PATH} -std=c++14 -stdlib=libc++" <linkflags>"-target x86_64-apple-macos10.15 -isysroot {SDK_PATH}" address-model=64 architecture=x86_64\n'
                    .format(SDK_PATH=sdkPath),
                ';\n\n'
                'using clang-darwin : arm64\n',
                ': {XCODE_ROOT}/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++\n'
                    .format(XCODE_ROOT=xcodeRoot),
                ': <compileflags>"-target arm64-apple-macos10.15 -isysroot {SDK_PATH} -std=c++14 -stdlib=libc++" <linkflags>"-target arm64-apple-macos10.15 -isysroot {SDK_PATH}" address-model=64 architecture=arm64\n'
                    .format(SDK_PATH=sdkPath),
                ';\n\n'
            ]

            with open(projectPath, 'a') as projectFile:
                projectFile.writelines(newLines)

        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            b2_toolset = "toolset=clang-darwin-x86_64"
            b2_settings[0] = '--prefix="{instDir}/_tmp/x86_64"'.format(instDir=context.instDir)
        b2CmdPrimary = '{b2} {toolset} {options} install'.format(
            b2=b2, toolset=b2_toolset, options=" ".join(b2_settings))
        Run(b2CmdPrimary)

        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            b2_toolset = "toolset=clang-darwin-arm64"
            b2_settings[0] = '--prefix="{instDir}/_tmp/arm64"'.format(instDir=context.instDir)
                
            b2CmdSecondary = '{b2} {toolset} {options} install'.format(
                b2=b2, toolset=b2_toolset, options=" ".join(b2_settings))
            Run(b2CmdSecondary)

        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            CopyDirectory(context, os.path.join(context.instDir, "_tmp/x86_64/include/boost"), "include/boost")

            x86Dir = os.path.join(context.instDir, "_tmp/x86_64/lib")
            armDir = os.path.join(context.instDir, "_tmp/arm64/lib")
            libNames = [f for f in os.listdir(x86Dir) if os.path.isfile(os.path.join(x86Dir, f))]
            lipoCommands = CreateUniversalBinaries(context, libNames, x86Dir, armDir)

            shutil.rmtree(os.path.join(context.instDir, "_tmp"))

        # Output paths that are of interest
        with open(os.path.join(context.usdInstDir, 'boostBuild.txt'), 'wt') as file:
            file.write('ARCHIVE:' + BOOST_URL.split("/")[-1] + '\n')
            file.write('BUILDFOLDER:' + os.path.split(os.getcwd())[1] + '\n')
            file.write('BOOTSTRAP:' + bootstrapCmd + '\n')
            file.write('B2PRIMARY:' + b2CmdPrimary + '\n')
            if context.buildUniversal and SupportsMacOSUniversalBinaries():
                file.write('B2SECONDARY:' + b2CmdSecondary + '\n')
                file.write('LIPO:' + ','.join(lipoCommands) + '\n')

        if iOS():
            for filename in os.listdir(context.instDir + "/lib"):
                if filename.startswith("libboost_"):
                    oldFilename = context.instDir + "/lib/" + filename
                    head, _sep, tail = filename.rpartition(".a")

                    newFilename = context.instDir + "/lib/" + head + ".dylib"

                    if os.path.exists(newFilename): 
                        os.remove(newFilename)
                    os.rename(oldFilename, newFilename)

        return os.getcwd()


def InstallBoost(context, force, buildArgs):
    # Boost's build system will install the version.hpp header before
    # building its libraries. We make sure to remove it in case of
    # any failure to ensure that the build script detects boost as a 
    # dependency to build the next time it's run.
    p = ""
    try:
        p = InstallBoost_Helper(context, force, buildArgs)
    except:
        versionHeader = os.path.join(context.instDir, BOOST_VERSION_FILE)
        if os.path.isfile(versionHeader):
            try: os.remove(versionHeader)
            except: pass
        raise
    return p

BOOST = Dependency("boost", InstallBoost, BOOST_VERSION_FILE)

############################################################
# Intel TBB

if Windows():
    TBB_URL = "https://github.com/oneapi-src/oneTBB/releases/download/2017_U6/tbb2017_20170412oss_win.zip"
elif MacOS() or iOS():
    TBB_URL = "https://github.com/oneapi-src/oneTBB/archive/2019_U7.tar.gz"
else:
    TBB_URL = "https://github.com/oneapi-src/oneTBB/archive/2017_U6.tar.gz"

def InstallTBB(context, force, buildArgs):
    if Windows():
        return InstallTBB_Windows(context, force, buildArgs)
    elif Linux() or MacOS() or iOS():
        return InstallTBB_LinuxOrMacOS(context, force, buildArgs)

def InstallTBB_Windows(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TBB_URL, context, force)):
        # On Windows, we simply copy headers and pre-built DLLs to
        # the appropriate location.

        if buildArgs:
            PrintWarning("Ignoring build arguments {}, TBB is "
                         "not built from source on this platform."
                         .format(buildArgs))

        CopyFiles(context, "bin\\intel64\\vc14\\*.*", "bin")
        CopyFiles(context, "lib\\intel64\\vc14\\*.*", "lib")
        CopyDirectory(context, "include\\serial", "include\\serial")
        CopyDirectory(context, "include\\tbb", "include\\tbb")
        return os.getcwd()

def InstallTBB_LinuxOrMacOS(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TBB_URL, context, force)):
        # Note: TBB installation fails on OSX when cuda is installed, a 
        # suggested fix:
        # https://github.com/spack/spack/issues/6000#issuecomment-358817701
        if MacOS():
            PatchFile("build/macos.inc", 
                    [("shell clang -v ", "shell clang --version ")])
        # TBB does not support out-of-source builds in a custom location.
        if iOS():
            PatchFile("build/macos.clang.inc", 
                [("ifeq ($(arch),$(filter $(arch),armv7 armv7s arm64))",
                  "ifeq ($(arch),$(filter $(arch),armv7 armv7s arm64))\n"
                  "    CPLUS_FLAGS += -mios-version-min=10.0 -fembed-bitcode\n")])

            PatchFile("include/tbb/tbb_machine.h", 
                            [("    inline void __TBB_Pause(int32_t) {",
                              "#include <unistd.h>\n"
                              "    inline void __TBB_Pause(int32_t) {"),
                             ("        __TBB_Yield();",
                              "        usleep(1);")])
            buildArgs.append('compiler=clang target=ios arch=arm64 extra_inc=big_iron.inc ')


        if iOS():
            PatchFile("include/tbb/machine/macos_common.h", 
                [("#define __TBB_Yield()  sched_yield()",
                  "#define __TBB_Yield()  __TBB_Pause(1)")])
        elif MacOS():
            PatchFile("include/tbb/machine/macos_common.h", 
                [("#define __TBB_Yield()  sched_yield()",
                  "#if defined(__aarch64__)\n"
                  "#define __TBB_Yield()  sched_yield()\n"
                  "#else\n"
                  "#define __TBB_Yield()  __TBB_Pause(1)\n"
                  "#endif\n")])
        if MacOS() or iOS():
            PatchFile("src/tbb/custom_scheduler.h", 
                [("const int yield_threshold = 100;",
                  "const int yield_threshold = 10;")])
            PatchFile("build/ios.macos.inc", 
                [("export SDKROOT:=$(shell xcodebuild -sdk -version | grep -o -E '/.*SDKs/iPhoneOS.*' 2>/dev/null)",
                  "export SDKROOT:=$(shell xcodebuild -sdk -version | grep -o -E '/.*SDKs/iPhoneOS.*' 2>/dev/null | head -1)")])
            if MacOS():
                PatchFile("build/macos.clang.inc", 
                    [("LIBDL = -ldl",
                      "LIBDL = -ldl\n"
                      "export SDKROOT:=$(shell xcodebuild -sdk -version | grep -o -E '/.*SDKs/MacOSX.*' 2>/dev/null | head -1)"),
                     ("-m64",
                      "-m64 -arch x86_64")],
                    True)

        archPrimary = GetMacArch()
        archSecondary = ""
        if (archPrimary == "x86_64"):
            archPrimary = "intel64"
            archSecondary = "arm64"
        else:
            archSecondary = "arm64"

        makeTBBCmdPrimary = 'make -j{procs} arch={arch} {buildArgs}'.format(
            arch=archPrimary,
            procs=context.numJobs, 
            buildArgs=" ".join(buildArgs))
        Run(makeTBBCmdPrimary)
        
        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            makeTBBCmdSecondary = "make -j{procs} arch={arch} {buildArgs}".format(
                arch=archSecondary,
                procs=context.numJobs,
                buildArgs=" ".join(buildArgs))

            Run(makeTBBCmdSecondary)

        # Install both release and debug builds. USD requires the debug
        # libraries when building in debug mode, and installing both
        # makes it easier for users to install dependencies in some
        # location that can be shared by both release and debug USD
        # builds. Plus, the TBB build system builds both versions anyway.
        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            x86Files = glob.glob(os.getcwd() + "/build/*intel64*_release/libtbb*.*")
            armFiles = glob.glob(os.getcwd() + "/build/*arm64*_release/libtbb*.*")
            libNames = [os.path.basename(x) for x in x86Files]
            x86Dir = os.path.dirname(x86Files[0])
            armDir = os.path.dirname(armFiles[0])

            lipoCommandsRelease = CreateUniversalBinaries(context, libNames, x86Dir, armDir)

            x86Files = glob.glob(os.getcwd() + "/build/*intel64*_debug/libtbb*.*")
            armFiles = glob.glob(os.getcwd() + "/build/*arm64*_debug/libtbb*.*")
            libNames = [os.path.basename(x) for x in x86Files]
            x86Dir = os.path.dirname(x86Files[0])
            armDir = os.path.dirname(armFiles[0])

            lipoCommandsDebug = CreateUniversalBinaries(context, libNames, x86Dir, armDir)
        else:
            CopyFiles(context, "build/*_release/libtbb*.*", "lib")
            CopyFiles(context, "build/*_debug/libtbb*.*", "lib")

        # Output paths that are of interest
        with open(os.path.join(context.usdInstDir, 'tbbBuild.txt'), 'wt') as file:
            file.write('ARCHIVE:' + TBB_URL.split("/")[-1] + '\n')
            file.write('BUILDFOLDER:' + os.path.split(os.getcwd())[1] + '\n')
            file.write('MAKEPRIMARY:' + makeTBBCmdPrimary + '\n')

            if context.buildUniversal and SupportsMacOSUniversalBinaries():
                file.write('MAKESECONDARY:' + makeTBBCmdSecondary + '\n')
                file.write('LIPO_RELEASE:' + ','.join(lipoCommandsRelease) + '\n')
                file.write('LIPO_DEBUG:' + ','.join(lipoCommandsDebug) + '\n')

        CopyDirectory(context, "include/serial", "include/serial")
        CopyDirectory(context, "include/tbb", "include/tbb")
        return os.getcwd()

TBB = Dependency("TBB", InstallTBB, "include/tbb/tbb.h")

############################################################
# JPEG

def InstallJPEG(context, force, buildArgs):
    if Windows():
        return InstallJPEG_Turbo("https://github.com/libjpeg-turbo/libjpeg-turbo/archive/1.5.1.zip",
            context, force, buildArgs)
    elif MacOS() or iOS():
        return InstallJPEG_Turbo("https://github.com/libjpeg-turbo/libjpeg-turbo/archive/2.0.1.zip",
            context, force, buildArgs)
    else:
        return InstallJPEG_Lib("https://www.ijg.org/files/jpegsrc.v9b.tar.gz",
            context, force, buildArgs)

def InstallJPEG_Turbo(jpeg_url, context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(jpeg_url, context, force)):
        extraJPEGArgs = buildArgs;

        if MacOS():
            extraJPEGArgs.append("-DWITH_SIMD=FALSE")
            extraJPEGArgs.append("-DENABLE_STATIC=TRUE")

        if iOS():
            extraJPEGArgs.append('-DCMAKE_SYSTEM_PROCESSOR=aarch64');

            # Replace test and utility executables with static libraries to avoid issues with code signing.
            PatchFile("CMakeLists.txt",
                [("add_executable(tjunittest tjunittest.c tjutil.c md5/md5.c md5/md5hl.c)",
                  "add_library(tjunittest STATIC tjunittest.c tjutil.c md5/md5.c md5/md5hl.c)"),
                 ("add_executable(tjbench tjbench.c tjutil.c)",
                  "add_library(tjbench STATIC tjbench.c tjutil.c)"),
                 ("add_executable(tjexample tjexample.c)",
                  "add_library(tjexample STATIC tjexample.c)"),
                 ("add_executable(tjunittest-static tjunittest.c tjutil.c md5/md5.c",
                  "add_library(tjunittest-static STATIC tjunittest.c tjutil.c md5/md5.c"),
                 ("add_executable(tjbench-static tjbench.c tjutil.c)",
                  "add_library(tjbench-static STATIC tjbench.c tjutil.c)"),
                 ("add_executable(cjpeg-static cjpeg.c cdjpeg.c rdgif.c rdppm.c rdswitch.c",
                  "add_library(cjpeg-static STATIC cjpeg.c cdjpeg.c rdgif.c rdppm.c rdswitch.c"),
                 ("add_executable(djpeg-static djpeg.c cdjpeg.c rdcolmap.c rdswitch.c wrgif.c",
                  "add_library(djpeg-static STATIC djpeg.c cdjpeg.c rdcolmap.c rdswitch.c wrgif.c"),
                 ("add_executable(jpegtran-static jpegtran.c cdjpeg.c rdswitch.c transupp.c)",
                  "add_library(jpegtran-static STATIC jpegtran.c cdjpeg.c rdswitch.c transupp.c)"),
                 ("add_executable(rdjpgcom rdjpgcom.c)", "add_library(rdjpgcom STATIC rdjpgcom.c)"),
                 ("add_executable(wrjpgcom wrjpgcom.c)", "add_library(wrjpgcom STATIC wrjpgcom.c)"),
                 ("add_subdirectory(md5)", "# add_subdirectory(md5)")])

            PatchFile("sharedlib/CMakeLists.txt",
                [("add_executable(cjpeg ../cjpeg.c ../cdjpeg.c ../rdgif.c ../rdppm.c",
                  "add_library(cjpeg STATIC ../cjpeg.c ../cdjpeg.c ../rdgif.c ../rdppm.c"),
                 ("add_executable(djpeg ../djpeg.c ../cdjpeg.c ../rdcolmap.c ../rdswitch.c",
                  "add_library(djpeg STATIC ../djpeg.c ../cdjpeg.c ../rdcolmap.c ../rdswitch.c"),
                 ("add_executable(jpegtran ../jpegtran.c ../cdjpeg.c ../rdswitch.c ../transupp.c)",
                  "add_library(jpegtran STATIC ../jpegtran.c ../cdjpeg.c ../rdswitch.c ../transupp.c)"),
                 ("add_executable(jcstest ../jcstest.c)",
                  "add_library(jcstest STATIC ../jcstest.c)")])

        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            extraJPEGArgs.append("-DWITH_SIMD=FALSE")
            PatchFile("CMakeLists.txt", 
                [("add_library(simd OBJECT jsimd_none.c)", "add_library(simd STATIC jsimd_none.c)"),
                ("add_executable(wrjpgcom wrjpgcom.c)",
                "add_executable(wrjpgcom wrjpgcom.c)\n\n"
                "if(ENABLE_STATIC)\n"
                "  target_link_libraries(jpeg-static simd)\n"
                "  if(WITH_TURBOJPEG)\n"
                "    target_link_libraries(turbojpeg-static simd)\n"
                "  endif()\n"
                "endif()\n"
                "if(WITH_TURBOJPEG)\n"
                "  target_link_libraries(turbojpeg simd)\n"
                "endif()\n")])
            PatchFile("sharedlib/CMakeLists.txt", 
                [("target_link_libraries(cjpeg jpeg)",
                "target_link_libraries(jpeg simd)\n\n"
                "target_link_libraries(cjpeg jpeg)")
                ])
            PatchFile("CMakeLists.txt", [("$<TARGET_OBJECTS:simd>", "")], True)
            PatchFile("sharedlib/CMakeLists.txt", [("$<TARGET_OBJECTS:simd>", "")], True)

        RunCMake(context, force, extraJPEGArgs)
        return os.getcwd()

def InstallJPEG_Lib(jpeg_url, context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(jpeg_url, context, force)):
        configureCmd = './configure --prefix="{instDir}" '.format(instDir=context.instDir) + \
            '--disable-static --enable-shared ' + \
            '{buildArgs}'.format(buildArgs=" ".join(buildArgs))
        Run(configureCmd)

        makeCmd = 'make -j{procs} install'.format(procs=context.numJobs)
        Run(makeCmd)

        # Output paths that are of interest
        with open(os.path.join(context.usdInstDir, 'jpegBuild.txt'), 'wt') as file:
            file.write('ARCHIVE:' + jpeg_url.split("/")[-1] + '\n')
            file.write('BUILDFOLDER:' + os.path.split(os.getcwd())[1] + '\n')
            file.write('CONFIGURE:' + configureCmd + '\n')
            file.write('MAKE:' + makeCmd + '\n')

        return os.getcwd()

JPEG = Dependency("JPEG", InstallJPEG, "include/jpeglib.h")

############################################################
# TIFF

TIFF_URL = "https://download.osgeo.org/libtiff/tiff-4.0.7.zip"

def InstallTIFF(context, force, buildArgs):
    scriptFolder = os.path.dirname(os.path.abspath(__file__))
    with CurrentWorkingDirectory(DownloadURL(TIFF_URL, context, force)):
        # libTIFF has a build issue on Windows where tools/tiffgt.c
        # unconditionally includes unistd.h, which does not exist.
        # To avoid this, we patch the CMakeLists.txt to skip building
        # the tools entirely. We do this on Linux and MacOS as well
        # to avoid requiring some GL and X dependencies.
        #
        # We also need to skip building tests, since they rely on
        # the tools we've just elided.
        PatchFile("CMakeLists.txt",
                   [("add_subdirectory(tools)", "# add_subdirectory(tools)"),
                    ("add_subdirectory(test)", "# add_subdirectory(test)")])

        if MacOS() or iOS():
            patchPath = os.path.join(os.path.dirname(scriptFolder), 'patches')

            devout = open(os.devnull, 'w')
            subprocess.call(['git', 'apply', '--reject', '--whitespace=fix', 
                patchPath + '/0001-tif_fax3.h-allow-0-length-run-in-DECODE2D.patch'],
                stdout=devout, stderr=devout)

            subprocess.call(['git', 'apply', '--reject', '--whitespace=fix', 
                patchPath + '/0001-tif_fax3-better-fix-for-CVE-2011-0192.patch'],
                stdout=devout, stderr=devout)

            subprocess.call(['git', 'apply', '--reject', '--whitespace=fix', 
                patchPath + '/0001-tif_fax3.h-check-for-buffer-overflow-in-EXPAND2D-bef.patch'],
                stdout=devout, stderr=devout)

            subprocess.call(['git', 'apply', '--reject', '--whitespace=fix', 
                patchPath + '/0001-tif_fax3-more-buffer-overflow-checks-in-Fax3Decode2D.patch'],
                stdout=devout, stderr=devout)
        
        if iOS():
            # Skip contrib to avoid issues with code signing.
            PatchFile("CMakeLists.txt",
                    [("add_subdirectory(contrib)", "# add_subdirectory(contrib)")])
			
        # The libTIFF CMakeScript says the ld-version-script 
        # functionality is only for compilers using GNU ld on 
        # ELF systems or systems which provide an emulation; therefore
        # skipping it completely on mac and windows.
        if MacOS() or iOS() or Windows():
            extraArgs = ["-Dld-version-script=OFF"]
        else:
            extraArgs = []
        extraArgs += buildArgs
        RunCMake(context, force, extraArgs)
        return os.getcwd()

TIFF = Dependency("TIFF", InstallTIFF, "include/tiff.h")

############################################################
# PNG

PNG_URL = "https://downloads.sourceforge.net/project/libpng/libpng16/older-releases/1.6.29/libpng-1.6.29.tar.gz"

def InstallPNG(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(PNG_URL, context, force)):
        extraPNGArgs = buildArgs;
        extraPNGArgs.append("-DCMAKE_C_FLAGS=\"-DPNG_ARM_NEON_OPT=0\"");

        if (context.buildUniversal and SupportsMacOSUniversalBinaries()) or (GetMacArch() == "arm64"):
            extraPNGArgs.append("-DCMAKE_C_FLAGS=\"-DPNG_ARM_NEON_OPT=0\"");

        if iOS():
            extraPNGArgs.append('-DCMAKE_SYSTEM_PROCESSOR=aarch64');
            extraPNGArgs.append('-DPNG_ARM_NEON=off');

            # Skip tests to avoid issues with code signing.
            # Replace utility executables with static libraries to avoid issues with code signing.
            PatchFile("CMakeLists.txt",
                [("option(PNG_TESTS  \"Build libpng tests\" ON)",
                  "option(PNG_TESTS  \"Build libpng tests\" OFF)"),
                 ("add_executable(pngfix ${pngfix_sources})",
                  "add_library(pngfix STATIC ${pngfix_sources})"),
                 ("add_executable(png-fix-itxt ${png_fix_itxt_sources})",
                  "add_library(png-fix-itxt STATIC ${png_fix_itxt_sources})")])

        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            PatchFile("scripts/genout.cmake.in",
                [("CMAKE_OSX_ARCHITECTURES",
                  "CMAKE_OSX_INTERNAL_ARCHITECTURES")])

            extraPNGArgs.append('-DCMAKE_OSX_INTERNAL_ARCHITECTURES=' + GetMacArch())

        RunCMake(context, force, extraPNGArgs)
        return os.getcwd()

PNG = Dependency("PNG", InstallPNG, "include/png.h")

############################################################
# BASISU
BASISU_URL = "https://github.com/BinomialLLC/basis_universal/archive/master.zip"

def DownloadBasisUniversalTexture(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(BASISU_URL, context, force)):
        PatchFile("CmakeLists.txt",
                [("basisu_tool.cpp", "#basisu_tool.cpp"),
                 ("add_executable", "add_library"),
                 ("add_custom_command", "#add_custom_command"),
                 ("install(TARGETS basisu DESTINATION bin)",
                  "install(TARGETS basisu DESTINATION lib)")],
                multiLineMatches=True)
        RunCMake(context, force, buildArgs)


        basisuIncDir = os.path.join("include", "basisu")
        basisuTranscoderIncDir = os.path.join(basisuIncDir, "transcoder")
        with CurrentWorkingDirectory(context.instDir):
            if not os.path.isdir(basisuIncDir):
                os.makedirs(basisuIncDir)
                os.makedirs(basisuTranscoderIncDir)

        CopyFiles(context, "*.h", basisuIncDir)
        CopyFiles(context, 
            os.path.join("transcoder", "*.inc"), 
            basisuTranscoderIncDir)
        CopyFiles(context, 
            os.path.join("transcoder", "*.h"),
            basisuTranscoderIncDir)
    return os.getcwd()

BASISU = Dependency("BASISU", DownloadBasisUniversalTexture, 
                    "include/basisu/transcoder/basisu.h")

############################################################
# IlmBase/OpenEXR

# Security vulnerability in future versions:
# https://github.com/AcademySoftwareFoundation/openexr/issues/728
# We might need to apply the following patch when bumping up the version:
# https://github.com/AcademySoftwareFoundation/openexr/pull/730
OPENEXR_URL = "https://github.com/openexr/openexr/archive/v2.2.0.zip"

def InstallOpenEXR(context, force, buildArgs):
    srcDir = DownloadURL(OPENEXR_URL, context, force)

    if iOS():
        updateOpenEXRIOS(context, srcDir)

        # Skip utils, examples, and tests to avoid issues with code signing.
        # Replace utility executables with static libraries to avoid issues with code signing.
        PatchFile(srcDir + "/IlmBase/CMakeLists.txt",
            [("ADD_SUBDIRECTORY ( HalfTest )", "# ADD_SUBDIRECTORY ( HalfTest )"),
             ("ADD_SUBDIRECTORY ( IexTest )", "# ADD_SUBDIRECTORY ( IexTest )"),
             ("ADD_SUBDIRECTORY ( ImathTest )", "# ADD_SUBDIRECTORY ( ImathTest )")])

        PatchFile(srcDir + "/IlmBase/Half/CMakeLists.txt",
            [("ADD_EXECUTABLE ( eLut eLut.cpp )",
              "ADD_LIBRARY ( eLut STATIC eLut.cpp )"),
             ("ADD_EXECUTABLE ( toFloat toFloat.cpp )",
              "ADD_LIBRARY ( toFloat STATIC toFloat.cpp )")])

        PatchFile(srcDir + "/OpenEXR/CMakeLists.txt",
            [("ADD_SUBDIRECTORY ( IlmImfExamples )", "# ADD_SUBDIRECTORY ( IlmImfExamples )"),
             ("ADD_SUBDIRECTORY ( IlmImfTest )", "# ADD_SUBDIRECTORY ( IlmImfTest )"),
             ("ADD_SUBDIRECTORY ( IlmImfUtilTest )", "# ADD_SUBDIRECTORY ( IlmImfUtilTest )"),
             ("ADD_SUBDIRECTORY ( IlmImfFuzzTest )", "# ADD_SUBDIRECTORY ( IlmImfFuzzTest )"),
             ("ADD_SUBDIRECTORY ( exrheader )", "# ADD_SUBDIRECTORY ( exrheader )"),
             ("ADD_SUBDIRECTORY ( exrmaketiled )", "# ADD_SUBDIRECTORY ( exrmaketiled )"),
             ("ADD_SUBDIRECTORY ( exrstdattr )", "# ADD_SUBDIRECTORY ( exrstdattr )"),
             ("ADD_SUBDIRECTORY ( exrmakepreview )", "# ADD_SUBDIRECTORY ( exrmakepreview )"),
             ("ADD_SUBDIRECTORY ( exrenvmap )", "# ADD_SUBDIRECTORY ( exrenvmap )"),
             ("ADD_SUBDIRECTORY ( exrmultiview )", "# ADD_SUBDIRECTORY ( exrmultiview )"),
             ("ADD_SUBDIRECTORY ( exrmultipart )", "# ADD_SUBDIRECTORY ( exrmultipart )")])

        PatchFile(srcDir + "/OpenEXR/exr2aces/CMakeLists.txt",
            [("ADD_EXECUTABLE ( exr2aces",
              "ADD_LIBRARY ( exr2aces STATIC")])
        PatchFile(srcDir + "/OpenEXR/exrbuild/CMakeLists.txt",
            [("ADD_EXECUTABLE ( exrbuild",
              "ADD_LIBRARY ( exrbuild STATIC")])
        PatchFile(srcDir + "/OpenEXR/IlmImf/CMakeLists.txt",
            [("ADD_EXECUTABLE ( dwaLookups",
              "ADD_LIBRARY ( dwaLookups STATIC"),
             ("ADD_EXECUTABLE ( b44ExpLogTable",
              "ADD_LIBRARY ( b44ExpLogTable STATIC")])

    if MacOS():
        PatchFile(srcDir + "/OpenEXR/CMakeLists.txt",
            [("SET (OPENEXR_LIBSUFFIX \"\")",
              "SET (OPENEXR_LIBSUFFIX \"\")\n"
              "FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h \"\n"
              "#undef OPENEXR_IMF_HAVE_GCC_INLINE_ASM_AVX\n"
              "#ifndef __aarch64__\n"
              "#define OPENEXR_IMF_HAVE_GCC_INLINE_ASM_AVX 1\n"
              "#endif\n\")\n")])

    ilmbaseSrcDir = os.path.join(srcDir, "IlmBase")
    with CurrentWorkingDirectory(ilmbaseSrcDir):
        # openexr 2.2 has a bug with Ninja:
        # https://github.com/openexr/openexr/issues/94
        # https://github.com/openexr/openexr/pull/142
        # Fix commit here:
        # https://github.com/openexr/openexr/commit/8eed7012c10f1a835385d750fd55f228d1d35df9
        # Merged here:
        # https://github.com/openexr/openexr/commit/b206a243a03724650b04efcdf863c7761d5d5d5b
        if context.cmakeGenerator == "Ninja":
            PatchFile(
                os.path.join('Half', 'CMakeLists.txt'),
                [
                    ("TARGET eLut POST_BUILD",
                     "OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/eLut.h"),
                    ("  COMMAND eLut > ${CMAKE_CURRENT_BINARY_DIR}/eLut.h",
                     "  COMMAND eLut ARGS > ${CMAKE_CURRENT_BINARY_DIR}/eLut.h\n"
                        "  DEPENDS eLut"),
                    ("TARGET toFloat POST_BUILD",
                     "OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/toFloat.h"),
                    ("  COMMAND toFloat > ${CMAKE_CURRENT_BINARY_DIR}/toFloat.h",
                     "  COMMAND toFloat ARGS > ${CMAKE_CURRENT_BINARY_DIR}/toFloat.h\n"
                        "  DEPENDS toFloat"),

                    ("  ${CMAKE_CURRENT_BINARY_DIR}/eLut.h\n"
                         "  OBJECT_DEPENDS\n"
                         "  ${CMAKE_CURRENT_BINARY_DIR}/toFloat.h\n",
                     '  "${CMAKE_CURRENT_BINARY_DIR}/eLut.h;${CMAKE_CURRENT_BINARY_DIR}/toFloat.h"\n'),
                ],
                multiLineMatches=True)
        RunCMake(context, force, buildArgs)

        # fake IlmBase src folder
        dummySrcDir = os.path.join(context.srcDir, 'IlmBase')
        if not os.path.isdir(dummySrcDir):
            os.mkdir(dummySrcDir)
        with open(os.path.join(dummySrcDir, 'metadata.txt'), 'wt') as file:
            file.write('NAME:' + 'IlmBase' + '\n')
            file.write('PATH:' + ilmbaseSrcDir + '\n')

    openexrSrcDir = os.path.join(srcDir, "OpenEXR")
    with CurrentWorkingDirectory(openexrSrcDir):
        RunCMake(context, force,
                 ['-DILMBASE_PACKAGE_PREFIX="{instDir}"'
                  .format(instDir=context.instDir)] + buildArgs)
    
    # manually output metadata.txt
    with open(os.path.join(srcDir, 'metadata.txt'), 'wt') as file:
        file.write('NAME:' + 'OpenEXR' + '\n')
        file.write('PATH:' + openexrSrcDir + '\n')
    return ''

def updateOpenEXRIOS(context, srcDir):
    # IlmBase
    destDir = srcDir + "/IlmBase/Half"

    f = context.usdSrcDir + "/third_party/IlmBase/eLut.h"
    PrintCommandOutput("Copying {file} to {destDir}\n"
                           .format(file=f, destDir=destDir))
    shutil.copy(f, destDir)

    f = context.usdSrcDir + "/third_party/IlmBase/toFloat.h"
    PrintCommandOutput("Copying {file} to {destDir}\n"
                           .format(file=f, destDir=destDir))
    shutil.copy(f, destDir)

    PatchFile(destDir + "/CMakeLists.txt", 
              [("eLut >",
                "cp ${CMAKE_CURRENT_SOURCE_DIR}/eLut.h"),
               ("toFloat >",
                "cp ${CMAKE_CURRENT_SOURCE_DIR}/toFloat.h")])

    # OpenEXR
    destDir = srcDir + "/OpenEXR/IlmImf"

    f = context.usdSrcDir + "/third_party/OpenEXR/b44ExpLogTable.h"
    PrintCommandOutput("Copying {file} to {destDir}\n"
                           .format(file=f, destDir=destDir))
    shutil.copy(f, destDir)

    f = context.usdSrcDir + "/third_party/OpenEXR/dwaLookups.h"
    PrintCommandOutput("Copying {file} to {destDir}\n"
                           .format(file=f, destDir=destDir))
    shutil.copy(f, destDir)

    PatchFile(destDir + "/CMakeLists.txt", 
              [("${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/b44ExpLogTable >",
                "cp ${CMAKE_CURRENT_SOURCE_DIR}/b44ExpLogTable.h"),
               ("${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/dwaLookups >",
                "cp ${CMAKE_CURRENT_SOURCE_DIR}/dwaLookups.h")])

OPENEXR = Dependency("OpenEXR", InstallOpenEXR, "include/OpenEXR/ImfVersion.h")

############################################################
# Ptex

PTEX_URL = "https://github.com/wdas/ptex/archive/v2.1.28.zip"

def InstallPtex(context, force, buildArgs):
    if Windows():
        return InstallPtex_Windows(context, force, buildArgs)
    else:
        return InstallPtex_LinuxOrMacOS(context, force, buildArgs)

def InstallPtex_Windows(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(PTEX_URL, context, force)):
        # Ptex has a bug where the import library for the dynamic library and
        # the static library both get the same name, Ptex.lib, and as a
        # result one clobbers the other. We hack the appropriate CMake
        # file to prevent that. Since we don't need the static library we'll
        # rename that.
        #
        # In addition src\tests\CMakeLists.txt adds -DPTEX_STATIC to the 
        # compiler but links tests against the dynamic library, causing the 
        # links to fail. We patch the file to not add the -DPTEX_STATIC
        PatchFile('src\\ptex\\CMakeLists.txt', 
                  [("set_target_properties(Ptex_static PROPERTIES OUTPUT_NAME Ptex)",
                    "set_target_properties(Ptex_static PROPERTIES OUTPUT_NAME Ptexs)")])
        PatchFile('src\\tests\\CMakeLists.txt',
                  [("add_definitions(-DPTEX_STATIC)", 
                    "# add_definitions(-DPTEX_STATIC)")])

        # Patch Ptex::String to export symbol for operator<< 
        # This is required for newer versions of OIIO, which make use of the
        # this operator on Windows platform specifically.
        PatchFile('src\\ptex\\Ptexture.h',
                  [("std::ostream& operator << (std::ostream& stream, const Ptex::String& str);",
                    "PTEXAPI std::ostream& operator << (std::ostream& stream, const Ptex::String& str);")])


        RunCMake(context, force, buildArgs)
        return os.getcwd()

def InstallPtex_LinuxOrMacOS(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(PTEX_URL, context, force)):

        if iOS():
            # Skip utils and tests to avoid issues with code signing.
            PatchFile("CMakeLists.txt",
                [("add_subdirectory(src/utils)", "# add_subdirectory(src/utils)"),
                 ("add_subdirectory(src/tests)", "# add_subdirectory(src/tests)")])

        RunCMake(context, force, buildArgs)
        return os.getcwd()

PTEX = Dependency("Ptex", InstallPtex, "include/PtexVersion.h")

############################################################
# BLOSC (Compression used by OpenVDB)

# Using latest blosc since neither the version OpenVDB recommends
# (1.5) nor the version we test against (1.6.1) compile on Mac OS X
# Sierra (10.12) or Mojave (10.14).
BLOSC_URL = "https://github.com/Blosc/c-blosc/archive/v1.17.0.zip"

def InstallBLOSC(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(BLOSC_URL, context, force)):
        extraArgs = []

        skip_x86_intrinsics = False
        if MacOS or iOS():
            skip_x86_intrinsics = True

        if skip_x86_intrinsics:
            extraArgs.append('-DDEACTIVATE_SSE2=ON')
            extraArgs.append('-DDEACTIVATE_AVX2=ON')

        if MacOS() or iOS():
            PatchFile("internal-complibs/zlib-1.2.8/gzlib.c",
                [("#  define LSEEK lseek\n", "#include <unistd.h>\n#  define LSEEK lseek\n")])
            PatchFile("internal-complibs/zlib-1.2.8/gzread.c",
                [('#include "gzguts.h"', '#include <unistd.h>\n#include "gzguts.h"')])
            PatchFile("internal-complibs/zlib-1.2.8/gzwrite.c",
                [('#include "gzguts.h"', '#include <unistd.h>\n#include "gzguts.h"')])

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs

        RunCMake(context, force, extraArgs)
        return os.getcwd()

BLOSC = Dependency("Blosc", InstallBLOSC, "include/blosc.h")

############################################################
# OpenVDB

# Using version 6.1.0 since it has reworked its CMake files so that
# there are better options to not compile the OpenVDB binaries and to
# not require additional dependencies such as GLFW. Note that version
# 6.1.0 does require CMake 3.3 though.

OPENVDB_URL = "https://github.com/AcademySoftwareFoundation/openvdb/archive/v6.1.0.zip"

def InstallOpenVDB(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(OPENVDB_URL, context, force)):
        extraArgs = [
            '-DOPENVDB_BUILD_PYTHON_MODULE=OFF',
            '-DOPENVDB_BUILD_BINARIES=OFF',
            '-DOPENVDB_BUILD_UNITTESTS=OFF'
        ]

        # Make sure to use boost installed by the build script and not any
        # system installed boost
        extraArgs.append('-DBoost_NO_BOOST_CMAKE=On')
        extraArgs.append('-DBoost_NO_SYSTEM_PATHS=True')

        extraArgs.append('-DBLOSC_ROOT="{instDir}"'
                         .format(instDir=context.instDir))
        extraArgs.append('-DTBB_ROOT="{instDir}"'
                         .format(instDir=context.instDir))
        # OpenVDB needs Half type from IlmBase
        extraArgs.append('-DILMBASE_ROOT="{instDir}"'
                         .format(instDir=context.instDir))

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs

        RunCMake(context, force, extraArgs)
        return os.getcwd()

OPENVDB = Dependency("OpenVDB", InstallOpenVDB, "include/openvdb/openvdb.h")

############################################################
# OpenImageIO

OIIO_URL = "https://github.com/OpenImageIO/oiio/archive/Release-2.1.16.0.zip"

def InstallOpenImageIO(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(OIIO_URL, context, force)):
        extraArgs = ['-DOIIO_BUILD_TESTS=OFF',
                     '-DUSE_PYTHON=OFF',
                     '-DSTOP_ON_WARNING=OFF']

        if context.buildOIIOTools:
            extraArgs.append('-DOIIO_BUILD_TOOLS=ON')
        else:
            extraArgs.append('-DOIIO_BUILD_TOOLS=OFF')

        # OIIO's FindOpenEXR module circumvents CMake's normal library 
        # search order, which causes versions of OpenEXR installed in
        # /usr/local or other hard-coded locations in the module to
        # take precedence over the version we've built, which would 
        # normally be picked up when we specify CMAKE_PREFIX_PATH. 
        # This may lead to undefined symbol errors at build or runtime. 
        # So, we explicitly specify the OpenEXR we want to use here.
        extraArgs.append('-DOPENEXR_HOME="{instDir}"'
                         .format(instDir=context.instDir))

        # If Ptex support is disabled in USD, disable support in OpenImageIO
        # as well. This ensures OIIO doesn't accidentally pick up a Ptex
        # library outside of our build.
        if not context.enablePtex:
            extraArgs.append('-DUSE_PTEX=OFF')

        # Make sure to use boost installed by the build script and not any
        # system installed boost
        extraArgs.append('-DBoost_NO_BOOST_CMAKE=On')
        extraArgs.append('-DBoost_NO_SYSTEM_PATHS=True')

        if iOS():
            PatchFile("src/libutil/sysutil.cpp", 
                   [("if (system (newcmd.c_str()) != -1)", "if (true)")])
            PatchFile("CMakeLists.txt",
                    [("set (CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)",
                        "set (CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)\n"
                        "cmake_policy (SET CMP0008 NEW)")])

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs

        RunCMake(context, force, extraArgs)
        return os.getcwd()

OPENIMAGEIO = Dependency("OpenImageIO", InstallOpenImageIO,
                         "include/OpenImageIO/oiioversion.h")

############################################################
# OpenColorIO
# For USD on mac, supply a version in the cache folder
# Use v1.1.0 on MacOS and Windows since v1.0.9 doesn't build properly on
# those platforms.

OCIO_URL = None
if Linux():
    OCIO_URL = "https://github.com/imageworks/OpenColorIO/archive/v1.0.9.zip"
else:
    OCIO_URL = "https://github.com/imageworks/OpenColorIO/archive/v1.1.0.zip"

def InstallOpenColorIO(context, force, buildArgs):
    forceBuildFromCache = context.buildOCIOCached
    usedURL = "ocio.zip" if forceBuildFromCache else OCIO_URL
    if forceBuildFromCache:
        globalDownloader = context.downloader
        globalDownloaderName = context.downloaderName
        context.downloader = DownloadFromCache
        context.downloaderName = "cache"
    with CurrentWorkingDirectory(DownloadURL(usedURL, context, force)):
        extraArgs = ['-DOCIO_BUILD_TRUELIGHT=OFF',
                     '-DOCIO_BUILD_APPS=OFF',
                     '-DOCIO_BUILD_NUKE=OFF',
                     '-DOCIO_BUILD_DOCS=OFF',
                     '-DOCIO_BUILD_TESTS=OFF',
                     '-DOCIO_BUILD_PYGLUE=OFF',
                     '-DOCIO_BUILD_JNIGLUE=OFF',
                     '-DOCIO_STATIC_JNIGLUE=OFF',
                     '-DOCIO_USE_SSE=OFF']

        if MacOS():
            if context.buildUniversal and SupportsMacOSUniversalBinaries():
                arch = "x86_64;arm64"
            else:
                arch = GetMacArch()

            PatchFile("CMakeLists.txt",
                    [('CMAKE_ARGS      ${TINYXML_CMAKE_ARGS}',
                    'CMAKE_ARGS      ${TINYXML_CMAKE_ARGS}\n' +
                    '            CMAKE_CACHE_ARGS -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH:BOOL=TRUE'
                    ' -DCMAKE_OSX_ARCHITECTURES:STRING="{arch}"'.format(arch=arch)),
                    ('CMAKE_ARGS      ${YAML_CPP_CMAKE_ARGS}',
                    'CMAKE_ARGS      ${YAML_CPP_CMAKE_ARGS}\n' +
                    '            CMAKE_CACHE_ARGS -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH:BOOL=TRUE'
                    ' -DCMAKE_OSX_ARCHITECTURES:STRING="{arch}"'.format(arch=arch))])
            
        # The OCIO build treats all warnings as errors but several come up
        # on various platforms, including:
        # - On gcc6, v1.1.0 emits many -Wdeprecated-declaration warnings for
        #   std::auto_ptr
        # - On clang, v1.1.0 emits a -Wself-assign-field warning. This is fixed
        #   in https://github.com/AcademySoftwareFoundation/OpenColorIO/commit/0be465feb9ac2d34bd8171f30909b276c1efa996
        #
        # To avoid build failures we force all warnings off for this build.
        if GetVisualStudioCompilerAndVersion():
            # This doesn't work because CMake stores default flags for
            # MSVC in CMAKE_CXX_FLAGS and this would overwrite them.
            # However, we don't seem to get any warnings on Windows
            # (at least with VS2015 and 2017).
            # extraArgs.append('-DCMAKE_CXX_FLAGS=/w') 
            pass
        else:
            extraArgs.append('-DCMAKE_CXX_FLAGS=-w')
        #if using version 2 of OCIO we patch a different config path as it resides elsewere
        cfgPath = None
        if not forceBuildFromCache: 
            cfgPath = "src/core/Config.cpp"
        else:
            cfgPath = "src/OpenColorIO/Config.cpp"
        PatchFile(cfgPath,
                   [("cacheidnocontext_ = cacheidnocontext_;", 
                     "cacheidnocontext_ = rhs.cacheidnocontext_;")])

        if iOS() or MacOS():
            extraArgs.append('-DCMAKE_CXX_FLAGS="-Wno-unused-function -Wno-unused-const-variable -Wno-unused-private-field"')
            extraArgs.append('-DOCIO_USE_SSE=OFF')

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs

        RunCMake(context, force, extraArgs)
        if forceBuildFromCache:
            #Set downloader back to the global one
            context.downloader = globalDownloader
            context.downloaderName = globalDownloaderName

        return os.getcwd()

OPENCOLORIO = Dependency("OpenColorIO", InstallOpenColorIO,
                         "include/OpenColorIO/OpenColorABI.h")

############################################################
# OpenSubdiv

OPENSUBDIV_URL = "https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v3_4_3.zip"

def InstallOpenSubdiv(context, force, buildArgs):
    srcOSDDir = DownloadURL(OPENSUBDIV_URL, context, force)
    with CurrentWorkingDirectory(srcOSDDir):
        extraArgs = [
            '-DNO_EXAMPLES=ON',
            '-DNO_TUTORIALS=ON',
            '-DNO_REGRESSION=ON',
            '-DNO_DOC=ON',
            '-DNO_OMP=ON',
            '-DNO_CUDA=ON',
            '-DNO_OPENCL=ON',
            '-DNO_DX=ON',
            '-DNO_TESTS=ON',
            '-DNO_GLEW=ON',
            '-DNO_GLFW=ON',
        ]

        # If Ptex support is disabled in USD, disable support in OpenSubdiv
        # as well. This ensures OSD doesn't accidentally pick up a Ptex
        # library outside of our build.
        if not context.enablePtex:
            extraArgs.append('-DNO_PTEX=ON')

        # NOTE: For now, we disable TBB in our OpenSubdiv build.
        # This avoids an issue where OpenSubdiv will link against
        # all TBB libraries it finds, including libtbbmalloc and
        # libtbbmalloc_proxy. On Linux and MacOS, this has the
        # unwanted effect of replacing the system allocator with
        # tbbmalloc.
        extraArgs.append('-DNO_TBB=ON')

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs
        sdkroot = os.environ.get('SDKROOT')

        if iOS():
            PatchFile(srcOSDDir + "/cmake/iOSToolchain.cmake", 
                [("set(SDKROOT $ENV{SDKROOT})",
                  "set(CMAKE_TRY_COMPILE_TARGET_TYPE \"STATIC_LIBRARY\")\n"
                  "set(SDKROOT $ENV{SDKROOT})")])

            # We build for macOS in order to leverage the STRINGIFY binary built
            srcOSDmacOSDir = srcOSDDir + "_macOS"
            if os.path.isdir(srcOSDmacOSDir):
                shutil.rmtree(srcOSDmacOSDir)
            shutil.copytree(srcOSDDir, srcOSDmacOSDir)

            # Install macOS dependencies into a temporary directory, to avoid iOS space polution
            tempContext = copy.copy(context)
            tempContext.instDir = tempContext.instDir + "/macOS"
            with CurrentWorkingDirectory(srcOSDmacOSDir):
                RunCMake(tempContext, force, extraArgs, True)

            shutil.rmtree(tempContext.instDir)

            buildDirmacOS = os.path.join(context.buildDir, os.path.split(srcOSDmacOSDir)[1])

            extraArgs.append('-DNO_CLEW=ON')
            extraArgs.append('-DNO_OPENGL=ON')
            extraArgs.append('-DSTRINGIFY_LOCATION={buildDirmacOS}/bin/{variant}/stringify'
                             .format(buildDirmacOS=buildDirmacOS, variant="Debug" if context.buildDebug else "Release"))
            extraArgs.append('-DCMAKE_TOOLCHAIN_FILE={srcOSDDir}/cmake/iOSToolchain.cmake'
                             .format(srcOSDDir=srcOSDDir))
            extraArgs.append('-DCMAKE_OSX_ARCHITECTURES=arm64')

            os.environ['SDKROOT'] = GetCommandOutput('xcrun --sdk iphoneos --show-sdk-path').strip()

        # OpenSubdiv seems to error when building on windows w/ Ninja...
        # ...so just use the default generator (ie, Visual Studio on Windows)
        # until someone can sort it out
        oldGenerator = context.cmakeGenerator
        if oldGenerator == "Ninja" and Windows():
            context.cmakeGenerator = None

        # OpenSubdiv 3.3 and later on MacOS occasionally runs into build
        # failures with multiple build jobs. Workaround this by using
        # just 1 job for now. See:
        # https://github.com/PixarAnimationStudios/OpenSubdiv/issues/1194
        oldNumJobs = context.numJobs
        if MacOS():
            context.numJobs = 1

        try:
            RunCMake(context, force, extraArgs)
        finally:
            context.cmakeGenerator = oldGenerator
            context.numJobs = oldNumJobs

        if sdkroot is None:
            os.unsetenv('SDKROOT')
        else:
            os.environ['SDKROOT'] = sdkroot
        return os.getcwd()

OPENSUBDIV = Dependency("OpenSubdiv", InstallOpenSubdiv, 
                        "include/opensubdiv/version.h")

############################################################
# PyOpenGL

def GetPyOpenGLInstructions():
    return ('PyOpenGL is not installed. If you have pip '
            'installed, run "pip install PyOpenGL" to '
            'install it, then re-run this script.\n'
            'If PyOpenGL is already installed, you may need to '
            'update your PYTHONPATH to indicate where it is '
            'located.')

PYOPENGL = PythonDependency("PyOpenGL", GetPyOpenGLInstructions, 
                            moduleNames=["OpenGL"])

############################################################
# PySide

def GetPySideInstructions():
    # For licensing reasons, this script cannot install PySide itself.
    if Windows():
        # There is no distribution of PySide2 for Windows for Python 2.7.
        # So use PySide instead. See the following for more details:
        # https://wiki.qt.io/Qt_for_Python/Considerations#Missing_Windows_.2F_Python_2.7_release
        return ('PySide is not installed. If you have pip '
                'installed, run "pip install PySide" '
                'to install it, then re-run this script.\n'
                'If PySide is already installed, you may need to '
                'update your PYTHONPATH to indicate where it is '
                'located.')
    else:                       
        return ('PySide2 is not installed. If you have pip '
                'installed, run "pip install PySide2" '
                'to install it, then re-run this script.\n'
                'If PySide2 is already installed, you may need to '
                'update your PYTHONPATH to indicate where it is '
                'located.')

PYSIDE = PythonDependency("PySide", GetPySideInstructions,
                          moduleNames=["PySide", "PySide2"])

############################################################
# HDF5

HDF5_URL = "https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.10/hdf5-1.10.0-patch1/src/hdf5-1.10.0-patch1.zip"

def InstallHDF5(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(HDF5_URL, context, force)):
        if context.buildUniversal and SupportsMacOSUniversalBinaries():
            PatchFile("config/cmake_ext_mod/ConfigureChecks.cmake", 
                    [("if (ARCH_LENGTH GREATER 1)", "if (FALSE)")])

        RunCMake(context, force,
                 ['-DBUILD_TESTING=OFF',
                  '-DHDF5_BUILD_TOOLS=OFF',
                  '-DHDF5_BUILD_EXAMPLES=OFF'] + buildArgs)
        return os.getcwd()
                 
HDF5 = Dependency("HDF5", InstallHDF5, "include/hdf5.h")

############################################################
# Alembic

ALEMBIC_URL = "https://github.com/alembic/alembic/archive/1.7.10.zip"

def InstallAlembic(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(ALEMBIC_URL, context, force)):
        cmakeOptions = ['-DUSE_BINARIES=OFF', '-DUSE_TESTS=OFF']
        if context.enableHDF5:
            # HDF5 requires the H5_BUILT_AS_DYNAMIC_LIB macro be defined if
            # it was built with CMake as a dynamic library.
            cmakeOptions += [
                '-DUSE_HDF5=ON',
                '-DHDF5_ROOT="{instDir}"'.format(instDir=context.instDir),
                '-DCMAKE_CXX_FLAGS="-D H5_BUILT_AS_DYNAMIC_LIB"']
                
            if Windows():
                # Alembic doesn't link against HDF5 libraries on Windows 
                # whether or not USE_HDF5=ON or not.  There is a line to link 
                # against HDF5 on DARWIN so we hijack it to also link on WIN32.
                PatchFile("lib\\Alembic\\CMakeLists.txt", 
                          [("ALEMBIC_SHARED_LIBS AND DARWIN",
                            "ALEMBIC_SHARED_LIBS AND DARWIN OR ALEMBIC_SHARED_LIBS AND WIN32")])
        else:
           cmakeOptions += ['-DUSE_HDF5=OFF']
                 
        cmakeOptions += buildArgs

        RunCMake(context, force, cmakeOptions)
        return os.getcwd()

ALEMBIC = Dependency("Alembic", InstallAlembic, "include/Alembic/Abc/Base.h")

############################################################
# Draco

DRACO_URL = "https://github.com/google/draco/archive/master.zip"

def InstallDraco(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(DRACO_URL, context, force)):
        cmakeOptions = ['-DBUILD_USD_PLUGIN=ON']
        cmakeOptions += buildArgs
        RunCMake(context, force, cmakeOptions)
        return os.getcwd()

DRACO = Dependency("Draco", InstallDraco, "include/draco/compression/decode.h")

############################################################
# MaterialX

MATERIALX_URL = "https://github.com/materialx/MaterialX/archive/v1.37.3.zip"

def InstallMaterialX(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(MATERIALX_URL, context, force)):
        # USD requires MaterialX to be built as a shared library on Linux
        # Currently MaterialX does not support shared builds on Windows or MacOS
        cmakeOptions = []
        if Linux() or MacOS():
            cmakeOptions += ['-DMATERIALX_BUILD_SHARED_LIBS=ON']

        if MacOS() or iOS():
            cmakeOptions.append("-DEXTERNAL_LINK_FLAGS='-undefined dynamic_lookup'")

        cmakeOptions += buildArgs;

        RunCMake(context, force, cmakeOptions)
        return os.getcwd()

MATERIALX = Dependency("MaterialX", InstallMaterialX, "include/MaterialXCore/Library.h")

############################################################
# Embree
# For MacOS we use version 3.7.0 to include a fix from Intel
# to build on Catalina.
if MacOS():
    EMBREE_URL = "https://github.com/embree/embree/archive/v3.7.0.tar.gz"
else:
    EMBREE_URL = "https://github.com/embree/embree/archive/v3.2.2.tar.gz"

def InstallEmbree(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(EMBREE_URL, context, force)):
        extraArgs = [
            '-DTBB_ROOT={instDir}'.format(instDir=context.instDir),
            '-DEMBREE_TUTORIALS=OFF',
            '-DEMBREE_ISPC_SUPPORT=OFF'
        ]

        # By default Embree fails to build on Visual Studio 2015 due
        # to an internal compiler issue that is worked around via the
        # following flag. For more details see:
        # https://github.com/embree/embree/issues/157
        if IsVisualStudio2015OrGreater() and not IsVisualStudio2017OrGreater():
            extraArgs.append('-DCMAKE_CXX_FLAGS=/d2SSAOptimizer-')

        extraArgs += buildArgs

        RunCMake(context, force, extraArgs)
        return os.getcwd()

EMBREE = Dependency("Embree", InstallEmbree, "include/embree3/rtcore.h")                  

############################################################
# USD

def InstallUSD(context, force, buildArgs):
    with CurrentWorkingDirectory(context.usdSrcDir):
        extraArgs = []

        extraArgs.append('-DPXR_PREFER_SAFETY_OVER_SPEED=' + 
                         'ON' if context.safetyFirst else 'OFF')

        if context.buildPython:
            extraArgs.append('-DPXR_ENABLE_PYTHON_SUPPORT=ON')
            if Python3():
                extraArgs.append('-DPXR_USE_PYTHON_3=ON')

            # CMake has trouble finding the executable, library, and include
            # directories when there are multiple versions of Python installed.
            # This can lead to crashes due to USD being linked against one
            # version of Python but running through some other Python
            # interpreter version. This primarily shows up on macOS, as it's
            # common to have a Python install that's separate from the one
            # included with the system.
            #
            # To avoid this, we try to determine these paths from Python
            # itself rather than rely on CMake's heuristics.
            pythonInfo = GetPythonInfo()
            if pythonInfo:
                extraArgs.append('-DPYTHON_EXECUTABLE="{pyExecPath}"'
                                 .format(pyExecPath=pythonInfo[0]))
                extraArgs.append('-DPYTHON_LIBRARY="{pyLibPath}"'
                                 .format(pyLibPath=pythonInfo[1]))
                extraArgs.append('-DPYTHON_INCLUDE_DIR="{pyIncPath}"'
                                 .format(pyIncPath=pythonInfo[2]))
        else:
            extraArgs.append('-DPXR_ENABLE_PYTHON_SUPPORT=OFF')

        if context.buildShared:
            extraArgs.append('-DBUILD_SHARED_LIBS=ON')
        elif context.buildMonolithic:
            extraArgs.append('-DPXR_BUILD_MONOLITHIC=ON')

        if context.buildDebug:
            extraArgs.append('-DTBB_USE_DEBUG_BUILD=ON')
        else:
            extraArgs.append('-DTBB_USE_DEBUG_BUILD=OFF')
        
        if context.buildDocs:
            extraArgs.append('-DPXR_BUILD_DOCUMENTATION=ON')
        else:
            extraArgs.append('-DPXR_BUILD_DOCUMENTATION=OFF')
    
        if context.buildTests:
            extraArgs.append('-DPXR_BUILD_TESTS=ON')
        else:
            extraArgs.append('-DPXR_BUILD_TESTS=OFF')

        if context.buildExamples:
            extraArgs.append('-DPXR_BUILD_EXAMPLES=ON')
        else:
            extraArgs.append('-DPXR_BUILD_EXAMPLES=OFF')

        if context.buildTutorials:
            extraArgs.append('-DPXR_BUILD_TUTORIALS=ON')
        else:
            extraArgs.append('-DPXR_BUILD_TUTORIALS=OFF')

        if context.buildTools:
            extraArgs.append('-DPXR_BUILD_USD_TOOLS=ON')
        else:
            extraArgs.append('-DPXR_BUILD_USD_TOOLS=OFF')
            
        if context.buildImaging:
            extraArgs.append('-DPXR_BUILD_IMAGING=ON')
            if context.enablePtex:
                extraArgs.append('-DPXR_ENABLE_PTEX_SUPPORT=ON')
            else:
                extraArgs.append('-DPXR_ENABLE_PTEX_SUPPORT=OFF')

            if context.enableOpenVDB:
                extraArgs.append('-DPXR_ENABLE_OPENVDB_SUPPORT=ON')
            else:
                extraArgs.append('-DPXR_ENABLE_OPENVDB_SUPPORT=OFF')

            if context.buildEmbree:
                extraArgs.append('-DPXR_BUILD_EMBREE_PLUGIN=ON')
            else:
                extraArgs.append('-DPXR_BUILD_EMBREE_PLUGIN=OFF')

            if context.buildPrman:
                if context.prmanLocation:
                    extraArgs.append('-DRENDERMAN_LOCATION="{location}"'
                                     .format(location=context.prmanLocation))
                extraArgs.append('-DPXR_BUILD_PRMAN_PLUGIN=ON')
            else:
                extraArgs.append('-DPXR_BUILD_PRMAN_PLUGIN=OFF')                
            
            if context.buildBasisu:
                extraArgs.append('-DPXR_BUILD_BASISU_PLUGIN=ON')
            else:
                extraArgs.append('-DPXR_BUILD_BASISU_PLUGIN=OFF')

            if context.buildOIIO:
                extraArgs.append('-DPXR_BUILD_OPENIMAGEIO_PLUGIN=ON')
            else:
                extraArgs.append('-DPXR_BUILD_OPENIMAGEIO_PLUGIN=OFF')
                
            if context.buildOCIO:
                extraArgs.append('-DPXR_BUILD_OPENCOLORIO_PLUGIN=ON')
            else:
                extraArgs.append('-DPXR_BUILD_OPENCOLORIO_PLUGIN=OFF')
        
            if context.enableOpenGL:
                extraArgs.append('-DPXR_ENABLE_GL_SUPPORT=ON')
            else:
                extraArgs.append('-DPXR_ENABLE_GL_SUPPORT=OFF')

            if MacOS() or iOS():
                extraArgs.append('-DPXR_ENABLE_METAL_SUPPORT=ON')
            else:
                extraArgs.append('-DPXR_ENABLE_METAL_SUPPORT=OFF')

            extraArgs += buildArgs

        else:
            extraArgs.append('-DPXR_BUILD_IMAGING=OFF')

        if context.buildUsdImaging:
            extraArgs.append('-DPXR_BUILD_USD_IMAGING=ON')
        else:
            extraArgs.append('-DPXR_BUILD_USD_IMAGING=OFF')

        if context.buildUsdview:
            extraArgs.append('-DPXR_BUILD_USDVIEW=ON')
        else:
            extraArgs.append('-DPXR_BUILD_USDVIEW=OFF')

        if context.buildAlembic:
            extraArgs.append('-DPXR_BUILD_ALEMBIC_PLUGIN=ON')
            if context.enableHDF5:
                extraArgs.append('-DPXR_ENABLE_HDF5_SUPPORT=ON')

                # CMAKE_PREFIX_PATH isn't sufficient for the FindHDF5 module 
                # to find the HDF5 we've built, so provide an extra hint.
                extraArgs.append('-DHDF5_ROOT="{instDir}"'
                                 .format(instDir=context.instDir))
            else:
                extraArgs.append('-DPXR_ENABLE_HDF5_SUPPORT=OFF')
        else:
            extraArgs.append('-DPXR_BUILD_ALEMBIC_PLUGIN=OFF')

        if context.buildDraco:
            extraArgs.append('-DPXR_BUILD_DRACO_PLUGIN=ON')
            draco_root = (context.dracoLocation
                          if context.dracoLocation else context.instDir)
            extraArgs.append('-DDRACO_ROOT="{}"'.format(draco_root))
        else:
            extraArgs.append('-DPXR_BUILD_DRACO_PLUGIN=OFF')

        if context.buildMaterialX:
            extraArgs.append('-DPXR_BUILD_MATERIALX_PLUGIN=ON')
        else:
            extraArgs.append('-DPXR_BUILD_MATERIALX_PLUGIN=OFF')

        if Windows():
            # Increase the precompiled header buffer limit.
            extraArgs.append('-DCMAKE_CXX_FLAGS="/Zm150"')

        if MacOS() or iOS():
            extraArgs.append('-DCMAKE_CXX_FLAGS="-x objective-c++"')

        if iOS():
            # some build options are implicit with this
            extraArgs.append('-G Xcode')
        
		# Make sure to use boost installed by the build script and not any
        # system installed boost
        extraArgs.append('-DBoost_NO_BOOST_CMAKE=On')
        extraArgs.append('-DBoost_NO_SYSTEM_PATHS=True')

        extraArgs += buildArgs

        RunCMake(context, force, extraArgs)

        # fake src folder, to communicate original USD source folder
        srcDir = os.path.join(context.srcDir, 'USD')
        if not os.path.isdir(srcDir):
            os.mkdir(srcDir)
        with open(os.path.join(srcDir, 'metadata.txt'), 'wt') as file:
            file.write('NAME:' + 'USD' + '\n')
            file.write('PATH:' + context.usdSrcDir + '\n')
        return ''

USD = Dependency("USD", InstallUSD, "include/pxr/pxr.h")

############################################################
# Install script

programDescription = """\
Installation Script for USD

Builds and installs USD and 3rd-party dependencies to specified location.

- Libraries:
The following is a list of libraries that this script will download and build
as needed. These names can be used to identify libraries for various script
options, like --force or --build-args.

{libraryList}

- Downloading Libraries:
If curl or powershell (on Windows) are installed and located in PATH, they
will be used to download dependencies. Otherwise, a built-in downloader will 
be used.

- Specifying Custom Build Arguments:
Users may specify custom build arguments for libraries using the --build-args
option. This values for this option must take the form <library name>,<option>. 
For example:

%(prog)s --build-args boost,cxxflags=... USD,-DPXR_STRICT_BUILD_MODE=ON ...
%(prog)s --build-args USD,"-DPXR_STRICT_BUILD_MODE=ON -DPXR_HEADLESS_TEST_MODE=ON" ...

These arguments will be passed directly to the build system for the specified 
library. Multiple quotes may be needed to ensure arguments are passed on 
exactly as desired. Users must ensure these arguments are suitable for the
specified library and do not conflict with other options, otherwise build 
errors may occur.

- Python Versions and DCC Plugins:
Some DCCs (most notably, Maya) may ship with and run using their own version of
Python. In that case, it is important that USD and the plugins for that DCC are
built using the DCC's version of Python and not the system version. This can be
done by running %(prog)s using the DCC's version of Python.

For example, to build USD on macOS for use in Maya 2019, run:

/Applications/Autodesk/maya2019/Maya.app/Contents/bin/mayapy %(prog)s --no-usdview ...

Note that this is primarily an issue on macOS, where a DCC's version of Python
is likely to conflict with the version provided by the system. On other
platforms, %(prog)s *should* be run using the system Python and *should not*
be run using the DCC's Python.
""".format(
    libraryList=" ".join(sorted([d.name for d in AllDependencies])))

parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description=programDescription)

parser.add_argument("install_dir", type=str, 
                    help="Directory where USD will be installed")
parser.add_argument("-n", "--dry_run", dest="dry_run", action="store_true",
                    help="Only summarize what would happen")
                    
group = parser.add_mutually_exclusive_group()
group.add_argument("-v", "--verbose", action="count", default=1,
                   dest="verbosity",
                   help="Increase verbosity level (1-3)")
group.add_argument("-q", "--quiet", action="store_const", const=0,
                   dest="verbosity",
                   help="Suppress all output except for error messages")

group = parser.add_mutually_exclusive_group()
group.add_argument("--macOS", action="store_const", default=None, const=None,
                   dest="crossPlatform",
                   help="Target macOS platform")
group.add_argument("--iOS", action="store_const", const="iOS",
                   dest="crossPlatform",
                   help="Target iOS platform")

group = parser.add_mutually_exclusive_group()
group.add_argument("--cache", dest="use_download_cache", action="store_true",
                   default=False,
                   help="Copy dependencies from repository folder instead of downloading")
group.add_argument("--no-cache", dest="use_download_cache", action="store_false",
                   help="Download dependencies, don't use the cache download folder")

group = parser.add_mutually_exclusive_group()
group.add_argument("--make-relocatable", dest="make_relocatable",
                   action="store_true", default=True,
                   help="MacOS only: Run make_relocatable.py script to make the instalation folder relocatable in the system")
group.add_argument("--no-make-relocatable", dest="make_relocatable",
                   action="store_false",
                   help="MacOS only: Don't run the make_relocatable.sh script")

group = parser.add_argument_group(title="Build Options")
group.add_argument("-j", "--jobs", type=int, default=GetCPUCount(),
                   help=("Number of build jobs to run in parallel. "
                         "(default: # of processors [{0}])"
                         .format(GetCPUCount())))
group.add_argument("--build", type=str,
                   help=("Build directory for USD and 3rd-party dependencies " 
                         "(default: <install_dir>/build)"))
group.add_argument("--build-args", type=str, nargs="*", default=[],
                   help=("Custom arguments to pass to build system when "
                         "building libraries (see docs above)"))
group.add_argument("--force", type=str, action="append", dest="force_build",
                   default=[],
                   help=("Force download and build of specified library "
                         "(see docs above)"))
group.add_argument("--force-all", action="store_true",
                   help="Force download and build of all libraries")
group.add_argument("--generator", type=str,
                   help=("CMake generator to use when building libraries with "
                         "cmake"))
group.add_argument("--toolset", type=str,
                   help=("CMake toolset to use when building libraries with "
                         "cmake"))

group = parser.add_argument_group(title="3rd Party Dependency Build Options")
group.add_argument("--src", type=str,
                   help=("Directory where dependencies will be downloaded "
                         "(default: <install_dir>/src)"))
group.add_argument("--inst", type=str,
                   help=("Directory where dependencies will be installed "
                         "(default: <install_dir>)"))

group = parser.add_argument_group(title="USD Options")

(SHARED_LIBS, MONOLITHIC_LIB) = (0, 1)
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--build-shared", dest="build_type",
                      action="store_const", const=SHARED_LIBS, 
                      default=SHARED_LIBS,
                      help="Build individual shared libraries (default)")
subgroup.add_argument("--build-monolithic", dest="build_type",
                      action="store_const", const=MONOLITHIC_LIB,
                      help="Build a single monolithic shared library")

group.add_argument("--debug", dest="build_debug", action="store_true",
                    help="Build with debugging information")

subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--tests", dest="build_tests", action="store_true",
                      default=False, help="Build unit tests")
subgroup.add_argument("--no-tests", dest="build_tests", action="store_false",
                      help="Do not build unit tests (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--examples", dest="build_examples", action="store_true",
                      default=True, help="Build examples (default)")
subgroup.add_argument("--no-examples", dest="build_examples", action="store_false",
                      help="Do not build examples")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--tutorials", dest="build_tutorials", action="store_true",
                      default=True, help="Build tutorials (default)")
subgroup.add_argument("--no-tutorials", dest="build_tutorials", action="store_false",
                      help="Do not build tutorials")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--tools", dest="build_tools", action="store_true",
                     default=True, help="Build USD tools (default)")
subgroup.add_argument("--no-tools", dest="build_tools", action="store_false",
                      help="Do not build USD tools")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--docs", dest="build_docs", action="store_true",
                      default=False, help="Build documentation")
subgroup.add_argument("--no-docs", dest="build_docs", action="store_false",
                      help="Do not build documentation (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--python", dest="build_python", action="store_true",
                      default=True, help="Build python based components "
                                         "(default)")
subgroup.add_argument("--no-python", dest="build_python", action="store_false",
                      help="Do not build python based components")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--universal", dest="universal", action="store_true",
                      default=False, help="Build universal binaries on MacOS ")
subgroup.add_argument("--no-universal", dest="universal", action="store_false",
                      help="Do not build universal binaries on MacOS (default)")
subgroup.add_argument("--prefer-safety-over-speed", dest="safety_first",
                      action="store_true", default=True, help=
                      "Enable extra safety checks (which may negatively "
                      "impact performance) against malformed input files "
                      "(default)")
subgroup.add_argument("--prefer-speed-over-safety", dest="safety_first",
                      action="store_false", help=
                      "Disable performance-impacting safety checks against "
                      "malformed input files")

(NO_IMAGING, IMAGING, USD_IMAGING) = (0, 1, 2)

group = parser.add_argument_group(title="Imaging and USD Imaging Options")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--imaging", dest="build_imaging", 
                      action="store_const", const=IMAGING, default=USD_IMAGING,
                      help="Build imaging component")
subgroup.add_argument("--usd-imaging", dest="build_imaging", 
                      action="store_const", const=USD_IMAGING,
                      help="Build imaging and USD imaging components (default)")
subgroup.add_argument("--no-imaging", dest="build_imaging", 
                      action="store_const", const=NO_IMAGING,
                      help="Do not build imaging or USD imaging components")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--ptex", dest="enable_ptex", action="store_true", 
                      default=False, 
                      help="Enable Ptex support in imaging")
subgroup.add_argument("--no-ptex", dest="enable_ptex", 
                      action="store_false",
                      help="Disable Ptex support in imaging (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--openvdb", dest="enable_openvdb", action="store_true", 
                      default=False, 
                      help="Enable OpenVDB support in imaging")
subgroup.add_argument("--no-openvdb", dest="enable_openvdb", 
                      action="store_false",
                      help="Disable OpenVDB support in imaging (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--usdview", dest="build_usdview",
                      action="store_true", default=True,
                      help="Build usdview (default)")
subgroup.add_argument("--no-usdview", dest="build_usdview",
                      action="store_false", 
                      help="Do not build usdview")

group = parser.add_argument_group(title="Imaging Plugin Options")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--embree", dest="build_embree", action="store_true",
                      default=False,
                      help="Build Embree sample imaging plugin")
subgroup.add_argument("--no-embree", dest="build_embree", action="store_false",
                      help="Do not build Embree sample imaging plugin (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--prman", dest="build_prman", action="store_true",
                      default=False,
                      help="Build Pixar's RenderMan imaging plugin")
subgroup.add_argument("--no-prman", dest="build_prman", action="store_false",
                      help="Do not build Pixar's RenderMan imaging plugin (default)")
group.add_argument("--prman-location", type=str,
                   help="Directory where Pixar's RenderMan is installed.")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--openimageio", dest="build_oiio", action="store_true", 
                      default=False,
                      help="Build OpenImageIO plugin for USD")
subgroup.add_argument("--no-openimageio", dest="build_oiio", action="store_false",
                      help="Do not build OpenImageIO plugin for USD (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--openimageio-tools", dest="build_oiio_tools", action="store_true",
                      default=False,
                      help="Build OpenImageIO tools (OpenImageIO plugin must be enabled)")
subgroup.add_argument("--no-openimageio-tools", dest="build_oiio_tools", action="store_false",
                      help="Do not build OpenImageIO tools (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--opencolorio", dest="build_ocio", action="store_true", 
                      default=False,
                      help="Build OpenColorIO plugin for USD")
subgroup.add_argument("--opencoloriocached", dest="build_ocio_cached", action="store_true", 
                      default=False,
                      help="Build OpenColorIO plugin for USD from cache")
subgroup.add_argument("--no-opencolorio", dest="build_ocio", action="store_false",
                      help="Do not build OpenColorIO plugin for USD (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--basisu", dest="build_basisu", action="store_true", 
                      default=False,
                      help="Build Basis Universal Texture plugin for USD")
subgroup.add_argument("--no-basisu", dest="build_basisu", action="store_false",
                      help="Do not build Basis Universal Texture plugin for USD (default)")

group = parser.add_argument_group(title="Alembic Plugin Options")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--alembic", dest="build_alembic", action="store_true", 
                      default=False,
                      help="Build Alembic plugin for USD")
subgroup.add_argument("--no-alembic", dest="build_alembic", action="store_false",
                      help="Do not build Alembic plugin for USD (default)")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--hdf5", dest="enable_hdf5", action="store_true", 
                      default=False,
                      help="Enable HDF5 support in the Alembic plugin")
subgroup.add_argument("--no-hdf5", dest="enable_hdf5", action="store_false",
                      help="Disable HDF5 support in the Alembic plugin (default)")

group = parser.add_argument_group(title="Draco Plugin Options")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--draco", dest="build_draco", action="store_true", 
                      default=False,
                      help="Build Draco plugin for USD")
subgroup.add_argument("--no-draco", dest="build_draco", action="store_false",
                      help="Do not build Draco plugin for USD (default)")
group.add_argument("--draco-location", type=str,
                   help="Directory where Draco is installed.")

group = parser.add_argument_group(title="MaterialX Plugin Options")
subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--materialx", dest="build_materialx", action="store_true", 
                      default=False,
                      help="Build MaterialX plugin for USD")
subgroup.add_argument("--no-materialx", dest="build_materialx", action="store_false",
                      help="Do not build MaterialX plugin for USD (default)")

subgroup = group.add_mutually_exclusive_group()
subgroup.add_argument("--opengl", dest="enable_opengl", action="store_true", 
                      default=True,
                      help="Enable OpenGL support in Storm (default)")
subgroup.add_argument("--no-opengl", dest="enable_opengl", action="store_false",
                      help="Disable OpenGL support in Storm")
args = parser.parse_args()

class InstallContext:
    def __init__(self, args):
        # Assume the USD source directory is in the parent directory
        self.usdSrcDir = os.path.normpath(
            os.path.join(os.path.abspath(os.path.dirname(__file__)), ".."))

        # Directory where USD will be installed
        self.usdInstDir = os.path.abspath(args.install_dir)

        # Directory where dependencies will be installed
        self.instDir = (os.path.abspath(args.inst) if args.inst 
                        else self.usdInstDir)

        # Directory where dependencies will be downloaded and extracted
        self.srcDir = (os.path.abspath(args.src) if args.src
                       else os.path.join(self.usdInstDir, "src"))
        
        # Directory where USD and dependencies will be built
        self.buildDir = (os.path.abspath(args.build) if args.build
                         else os.path.join(self.usdInstDir, "build"))

        # Determine which downloader to use.  The reason we don't simply
        # use urllib2 all the time is that some older versions of Python
        # don't support TLS v1.2, which is required for downloading some
        # dependencies.
        
        if args.use_download_cache and (MacOS() or iOS()):
            self.downloader = DownloadFromCache
            self.downloaderName = "cache"
        elif find_executable("curl"):
            self.downloader = DownloadFileWithCurl
            self.downloaderName = "curl"
        elif Windows() and find_executable("powershell"):
            self.downloader = DownloadFileWithPowershell
            self.downloaderName = "powershell"
        else:
            self.downloader = DownloadFileWithUrllib
            self.downloaderName = "built-in"

        self.use_download_cache = args.use_download_cache

        # MacOS Only
        self.make_relocatable = args.make_relocatable

        # CMake generator and toolset
        self.cmakeGenerator = args.generator
        self.cmakeToolset = args.toolset

        # Number of jobs
        self.numJobs = args.jobs
        if self.numJobs <= 0:
            raise ValueError("Number of jobs must be greater than 0")

        # Build arguments
        self.buildArgs = dict()
        for a in args.build_args:
            (depName, _, arg) = a.partition(",")
            if not depName or not arg:
                raise ValueError("Invalid argument for --build-args: {}"
                                 .format(a))
            if depName.lower() not in AllDependenciesByName:
                raise ValueError("Invalid library for --build-args: {}"
                                 .format(depName))

            self.buildArgs.setdefault(depName.lower(), []).append(arg)

        # Build type
        self.buildDebug = args.build_debug;
        self.buildShared = (args.build_type == SHARED_LIBS)
        self.buildMonolithic = (args.build_type == MONOLITHIC_LIB)

        # Build options
        self.buildUniversal = args.universal
        self.safetyFirst = args.safety_first

        # Dependencies that are forced to be built
        self.forceBuildAll = args.force_all
        self.forceBuild = [dep.lower() for dep in args.force_build]

        # Optional components
        self.buildTests = args.build_tests
        self.buildDocs = args.build_docs
        self.buildPython = args.build_python
        self.buildExamples = args.build_examples
        self.buildTutorials = args.build_tutorials
        self.buildTools = args.build_tools

        # - Imaging
        self.buildImaging = (args.build_imaging == IMAGING or
                             args.build_imaging == USD_IMAGING)
        self.enablePtex = self.buildImaging and args.enable_ptex
        self.enableOpenVDB = self.buildImaging and args.enable_openvdb

        # - USD Imaging
        self.buildUsdImaging = (args.build_imaging == USD_IMAGING)

        # - usdview
        self.buildUsdview = (self.buildUsdImaging and 
                             self.buildPython and 
                             args.build_usdview)

        # - Imaging plugins
        self.buildEmbree = self.buildImaging and args.build_embree
        self.buildPrman = self.buildImaging and args.build_prman
        self.prmanLocation = (os.path.abspath(args.prman_location)
                               if args.prman_location else None)                               
        self.buildOIIO = args.build_oiio
        self.buildOIIOTools = args.build_oiio_tools
        self.buildOCIO = args.build_ocio or args.build_ocio_cached
        self.buildOCIOCached = args.build_ocio_cached

        # - Basisu Plugin
        self.buildBasisu = args.build_basisu

        # - Alembic Plugin
        self.buildAlembic = args.build_alembic
        self.enableHDF5 = self.buildAlembic and args.enable_hdf5

        # - Draco Plugin
        self.buildDraco = args.build_draco
        self.dracoLocation = (os.path.abspath(args.draco_location)
                                if args.draco_location else None)

        # - MaterialX Plugin
        self.buildMaterialX = args.build_materialx

        self.enableOpenGL = args.enable_opengl
    def GetBuildArguments(self, dep):
        return self.buildArgs.get(dep.name.lower(), [])
       
    def ForceBuildDependency(self, dep):
        # Never force building a Python dependency, since users are required
        # to build these dependencies themselves.
        if type(dep) is PythonDependency:
            return False
        return self.forceBuildAll or dep.name.lower() in self.forceBuild

try:
    context = InstallContext(args)
except Exception as e:
    PrintError(str(e))
    sys.exit(1)

# Build target platform
crossPlatform = args.crossPlatform

context.enableOpenGL = context.enableOpenGL and not iOS()

verbosity = args.verbosity

# Augment PATH on Windows so that 3rd-party dependencies can find libraries
# they depend on. In particular, this is needed for building IlmBase/OpenEXR.
extraPaths = []
extraPythonPaths = []
if Windows():
    extraPaths.append(os.path.join(context.instDir, "lib"))
    extraPaths.append(os.path.join(context.instDir, "bin"))

if extraPaths:
    paths = os.environ.get('PATH', '').split(os.pathsep) + extraPaths
    os.environ['PATH'] = os.pathsep.join(paths)

if extraPythonPaths:
    paths = os.environ.get('PYTHONPATH', '').split(os.pathsep) + extraPythonPaths
    os.environ['PYTHONPATH'] = os.pathsep.join(paths)

# Determine list of dependencies that are required based on options
# user has selected.
requiredDependencies = [ZLIB, BOOST, TBB]

if context.buildAlembic:
    if context.enableHDF5:
        requiredDependencies += [HDF5]
    requiredDependencies += [OPENEXR, ALEMBIC]

if context.buildDraco:
    requiredDependencies += [DRACO]

if context.buildMaterialX:
    requiredDependencies += [MATERIALX]

if context.buildBasisu:
    requiredDependencies += [BASISU]

if context.buildImaging:
    if context.enablePtex:
        requiredDependencies += [PTEX]

    requiredDependencies += [OPENSUBDIV]
    
    if context.enableOpenVDB:
        requiredDependencies += [BLOSC, BOOST, OPENEXR, OPENVDB, TBB]
    
    if context.buildOIIO:
        requiredDependencies += [BOOST, JPEG, TIFF, PNG, OPENEXR, OPENIMAGEIO]

    if context.buildOCIO:
        requiredDependencies += [OPENCOLORIO]

    if context.buildEmbree:
        requiredDependencies += [TBB, EMBREE]
                             
if context.buildUsdview:
    requiredDependencies += [PYOPENGL, PYSIDE]

# Assume zlib already exists on Linux platforms and don't build
# our own. This avoids potential issues where a host application
# loads an older version of zlib than the one we'd build and link
# our libraries against.
if Linux():
    requiredDependencies.remove(ZLIB)

# Error out if user is building monolithic library on windows with draco plugin
# enabled. This currently results in missing symbols.
if context.buildDraco and context.buildMonolithic and Windows():
    PrintError("Draco plugin can not be enabled for monolithic build on Windows")
    sys.exit(1)

# Error out if user explicitly specified building usdview without required
# components. Otherwise, usdview will be silently disabled. This lets users
# specify "--no-python" without explicitly having to specify "--no-usdview",
# for instance.
if "--usdview" in sys.argv:
    if not context.buildUsdImaging:
        PrintError("Cannot build usdview when usdImaging is disabled.")
        sys.exit(1)
    if not context.buildPython:
        PrintError("Cannot build usdview when Python support is disabled.")
        sys.exit(1)

# Error out if running Maya's version of Python and attempting to build
# usdview.
if IsMayaPython():
    if context.buildUsdview:
        PrintError("Cannot build usdview when building against Maya's version "
                   "of Python. Maya does not provide access to the 'OpenGL' "
                   "Python module. Use '--no-usdview' to disable building "
                   "usdview.")
        sys.exit(1)

dependenciesToBuild = []
for dep in requiredDependencies:
    if context.ForceBuildDependency(dep) or not dep.Exists(context):
        if dep not in dependenciesToBuild:
            dependenciesToBuild.append(dep)

# Verify toolchain needed to build required dependencies
if (not find_executable("g++") and
    not find_executable("clang") and
    not GetXcodeDeveloperDirectory() and
    not GetVisualStudioCompilerAndVersion()):
    PrintError("C++ compiler not found -- please install a compiler")
    sys.exit(1)

if find_executable("python"):
    # Error out if a 64bit version of python interpreter is not found
    # Note: Ideally we should be checking the python binary found above, but
    # there is an assumption (for very valid reasons) at other places in the
    # script that the python process used to run this script will be found.
    isPython64Bit = (ctypes.sizeof(ctypes.c_voidp) == 8)
    if not isPython64Bit:
        PrintError("64bit python not found -- please install it and adjust your"
                   "PATH")
        sys.exit(1)

    # Error out on Windows with Python 3.8+. USD currently does not support
    # these versions due to:
    # https://docs.python.org/3.8/whatsnew/3.8.html#bpo-36085-whatsnew
    isPython38 = (sys.version_info.major >= 3 and
                  sys.version_info.minor >= 8)
    if Windows() and isPython38:
        PrintError("Python 3.8+ is not supported on Windows")
        sys.exit(1)

else:
    PrintError("python not found -- please ensure python is included in your "
               "PATH")
    sys.exit(1)

if find_executable("cmake"):
    # Check cmake requirements
    if Windows():
        # Windows build depend on boost 1.70, which is not supported before
        # cmake version 3.14
        cmake_required_version = (3, 14)
    else:
        cmake_required_version = (3, 12)
    cmake_version = GetCMakeVersion()
    if not cmake_version:
        PrintError("Failed to determine CMake version")
        sys.exit(1)

    if cmake_version < cmake_required_version:
        def _JoinVersion(v):
            return ".".join(str(n) for n in v)
        PrintError("CMake version {req} or later required to build USD, "
                   "but version found was {found}".format(
                       req=_JoinVersion(cmake_required_version),
                       found=_JoinVersion(cmake_version)))
        sys.exit(1)
else:
    PrintError("CMake not found -- please install it and adjust your PATH")
    sys.exit(1)

if context.buildDocs:
    if not find_executable("doxygen"):
        PrintError("doxygen not found -- please install it and adjust your PATH")
        sys.exit(1)
        
    if not find_executable("dot"):
        PrintError("dot not found -- please install graphviz and adjust your "
                   "PATH")
        sys.exit(1)

if PYSIDE in requiredDependencies:
    # The USD build will skip building usdview if pyside2-uic or pyside-uic is
    # not found, so check for it here to avoid confusing users. This list of 
    # PySide executable names comes from cmake/modules/FindPySide.cmake
    pyside2Uic = ["pyside2-uic", "python2-pyside2-uic", "pyside2-uic-2.7"]
    found_pyside2Uic = any([find_executable(p) for p in pyside2Uic])
    pysideUic = ["pyside-uic", "python2-pyside-uic", "pyside-uic-2.7"]
    found_pysideUic = any([find_executable(p) for p in pysideUic])
    if not found_pyside2Uic and not found_pysideUic:
        if Windows():
            # Windows does not support PySide2 with Python2.7
            PrintError("pyside-uic not found -- please install PySide and"
                       " adjust your PATH. (Note that this program may be named"
                       " {0} depending on your platform)"
                   .format(" or ".join(pysideUic)))
        else:
            PrintError("pyside2-uic not found -- please install PySide2 and"
                       " adjust your PATH. (Note that this program may be"
                       " named {0} depending on your platform)"
                       .format(" or ".join(pyside2Uic)))
        sys.exit(1)

if JPEG in requiredDependencies:
    # NASM is required to build libjpeg-turbo
    if (Windows() and not find_executable("nasm")):
        PrintError("nasm not found -- please install it and adjust your PATH")
        sys.exit(1)

# Summarize
summaryMsg = """
Building with settings:
  USD source directory          {usdSrcDir}
  USD install directory         {usdInstDir}
  3rd-party source directory    {srcDir}
  3rd-party install directory   {instDir}
  Build directory               {buildDir}
  CMake generator               {cmakeGenerator}
  CMake toolset                 {cmakeToolset}
  Downloader                    {downloader}
  
  Apple:
    Build universal binaries    {buildUniversalBinaries}
    Make relocatable            {make_relocatable}
    Use download cache          {use_download_cache}

  Building                      {buildType}
    Cross Platform              {targetPlatform}
    Config                      {buildConfig}
    Imaging                     {buildImaging}
      Ptex support:             {enablePtex}
      OpenVDB support:          {enableOpenVDB}
      OpenImageIO support:      {buildOIIO} 
      OpenColorIO support:      {buildOCIO} 
      PRMan support:            {buildPrman}
      OpenGL:                   {enableOpenGL}
      Metal:                    {enableMetal}
      BasisUniversal support:   {buildBasisu}
    UsdImaging                  {buildUsdImaging}
      usdview:                  {buildUsdview}
    Python support              {buildPython}
      Python 3:                 {enablePython3}
    Documentation               {buildDocs}
    Tests                       {buildTests}
    Examples                    {buildExamples}
    Tutorials                   {buildTutorials}
    Tools                       {buildTools}
    Alembic Plugin              {buildAlembic}
      HDF5 support:             {enableHDF5}
    Draco Plugin                {buildDraco}
    MaterialX Plugin            {buildMaterialX}

  Dependencies                  {dependencies}"""

if context.buildArgs:
    summaryMsg += """
  Build arguments               {buildArgs}"""

def FormatBuildArguments(buildArgs):
    s = ""
    for depName in sorted(buildArgs.keys()):
        args = buildArgs[depName]
        s += """
                                {name}: {args}""".format(
            name=AllDependenciesByName[depName].name,
            args=" ".join(args))
    return s.lstrip()

summaryMsg = summaryMsg.format(
    usdSrcDir=context.usdSrcDir,
    usdInstDir=context.usdInstDir,
    srcDir=context.srcDir,
    buildDir=context.buildDir,
    instDir=context.instDir,
    cmakeGenerator=("Default" if not context.cmakeGenerator
                    else context.cmakeGenerator),
    cmakeToolset=("Default" if not context.cmakeToolset
                  else context.cmakeToolset),
    downloader=(context.downloaderName),
    buildUniversalBinaries=("On" if context.buildUniversal and SupportsMacOSUniversalBinaries() else "Off"),
    use_download_cache=("On" if context.use_download_cache else "Off"),
    make_relocatable=("On" if context.make_relocatable and MacOS() else "Off"),
    dependencies=("None" if not dependenciesToBuild else 
                  ", ".join([d.name for d in dependenciesToBuild])),
    buildArgs=FormatBuildArguments(context.buildArgs),
    buildType=("Shared libraries" if context.buildShared
               else "Monolithic shared library" if context.buildMonolithic
               else ""),
    targetPlatform=("Off" if not iOS() else "On - iOS"),
	buildConfig=("Debug" if context.buildDebug else "Release"),
    buildImaging=("On" if context.buildImaging else "Off"),
    enablePtex=("On" if context.enablePtex else "Off"),
    enableOpenVDB=("On" if context.enableOpenVDB else "Off"),
    buildOIIO=("On" if context.buildOIIO else "Off"),
    buildOCIO=("On" if (context.buildOCIO or context.buildOCIOCached) else "Off"),
    buildPrman=("On" if context.buildPrman else "Off"),
    buildUsdImaging=("On" if context.buildUsdImaging else "Off"),
    buildUsdview=("On" if context.buildUsdview else "Off"),
    buildPython=("On" if context.buildPython else "Off"),
    enablePython3=("On" if Python3() else "Off"),
    buildDocs=("On" if context.buildDocs else "Off"),
    buildTests=("On" if context.buildTests else "Off"),
    buildExamples=("On" if context.buildExamples else "Off"),
    buildTutorials=("On" if context.buildTutorials else "Off"),
    buildTools=("On" if context.buildTools else "Off"),
    buildAlembic=("On" if context.buildAlembic else "Off"),
    buildDraco=("On" if context.buildDraco else "Off"),
    buildMaterialX=("On" if context.buildMaterialX else "Off"),
    enableHDF5=("On" if context.enableHDF5 else "Off"),
    enableOpenGL=("On" if context.enableOpenGL else "Off"),
    enableMetal=("On" if iOS() or MacOS() else "Off"),
    buildBasisu=("On" if context.buildBasisu else "Off"))

Print(summaryMsg)

if args.dry_run:
    sys.exit(0)

# Scan for any dependencies that the user is required to install themselves
# and print those instructions first.
pythonDependencies = \
    [dep for dep in dependenciesToBuild if type(dep) is PythonDependency]
if pythonDependencies:
    for dep in pythonDependencies:
        Print(dep.getInstructions())
    sys.exit(1)

# Ensure directory structure is created and is writable.
for dir in [context.usdInstDir, context.instDir, context.srcDir, 
            context.buildDir]:
    try:
        if os.path.isdir(dir):
            testFile = os.path.join(dir, "canwrite")
            open(testFile, "w").close()
            os.remove(testFile)
        else:
            os.makedirs(dir)
    except Exception as e:
        PrintError("Could not write to directory {dir}. Change permissions "
                   "or choose a different location to install to."
                   .format(dir=dir))
        sys.exit(1)

# Output dependency order
with open(context.usdInstDir + '/dependencies.txt', 'wt') as file:
    def GetName(dep):
        return dep.name
    deps = ','.join(map(GetName, requiredDependencies))
    file.write('BUILDORDER:' + deps + '\n')

try:
    # Download and install 3rd-party dependencies, followed by USD.
    for dep in dependenciesToBuild + [USD]:
        PrintStatus("Installing {dep}...".format(dep=dep.name))
        sourcePath = dep.installer(context, 
                      buildArgs=context.GetBuildArguments(dep),
                      force=context.ForceBuildDependency(dep))
        if (os.path.isdir(sourcePath)):
            with open(os.path.join(sourcePath, 'metadata.txt'), 'wt') as file:
                file.write('NAME:' + dep.name + '\n')
                file.write('PATH:' + sourcePath + '\n')
except Exception as e:
    PrintError(str(e))
    sys.exit(1)

# Done. Print out a final status message.
requiredInPythonPath = set([
    os.path.join(context.usdInstDir, "lib", "python")
])
requiredInPythonPath.update(extraPythonPaths)

requiredInPath = set([
    os.path.join(context.usdInstDir, "bin")
])
requiredInPath.update(extraPaths)

if Windows():
    requiredInPath.update([
        os.path.join(context.usdInstDir, "lib"),
        os.path.join(context.instDir, "bin"),
        os.path.join(context.instDir, "lib")
    ])

if args.make_relocatable:
    CheckCodeSignID()
    from make_relocatable import make_relocatable
    make_relocatable(context.usdInstDir, context.buildPython, iOS(), verbosity > 1)

Print("""
Success! To use USD, please ensure that you have:""")

if context.buildPython:
    Print("""
    The following in your PYTHONPATH environment variable:
    {requiredInPythonPath}""".format(
        requiredInPythonPath="\n    ".join(sorted(requiredInPythonPath))))

Print("""
    The following in your PATH environment variable:
    {requiredInPath}
""".format(requiredInPath="\n    ".join(sorted(requiredInPath))))
    
if context.buildPrman:
    Print("See documentation at http://openusd.org/docs/RenderMan-USD-Imaging-Plugin.html "
          "for setting up the RenderMan plugin.\n")
