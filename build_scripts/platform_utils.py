import platform
import os
import shutil
import copy

build_usd = None
apple_utils = None

def Context():
    return build_usd.context

def Windows():
    return platform.system() == "Windows"
def Linux():
    return platform.system() == "Linux"
def MacOS():
    return platform.system() == "Darwin"

def targetNative():
    return Context().targetNative
def targetX86():
    return Context().targetX86
def targetArm64():
    return Context().targetARM64
def targetIos():
    return Context().targetIos
def targetUniversal():
    return Context().targetUniversal

class Boost:
    def PrepareB2Environment(self):
        if Context().targetIos:
            os.environ['SDKROOT'] = build_usd.GetCommandOutput(
                'xcrun --sdk macosx --show-sdk-path').strip()

    def AppendBootstrapCmd(self):
        macOSArch = ""
        bootstrapCmd = ""
        if MacOS():
            if targetX86():
                macOSArch = "-arch x86_64"
            elif targetArm64() or targetIos():
                macOSArch = "-arch arm64"
            elif targetUniversal():
                macOSArch="-arch arm64 -arch x86_64"

            if macOSArch:
                bootstrapCmd += " cxxflags=\"{0}\" cflags=\"{0}\" linkflags=\"{0}\"".format(macOSArch)
            bootstrapCmd += " --with-toolset=clang"
        return bootstrapCmd

    def AppendB2Settings(self, b2_settings):
        if not targetIos():
            b2_settings.append('link=shared')
            b2_settings.append('runtime-link=shared')

        if Windows():
            # toolset parameter for Visual Studio documented here:
            # https://github.com/boostorg/build/blob/develop/src/tools/msvc.jam
            if Context().cmakeToolset == "v142":
                b2_settings.append("toolset=msvc-14.2")
            elif Context().cmakeToolset == "v141":
                b2_settings.append("toolset=msvc-14.1")
            elif Context().cmakeToolset == "v140":
                b2_settings.append("toolset=msvc-14.0")
            elif build_usd.IsVisualStudio2019OrGreater():
                b2_settings.append("toolset=msvc-14.2")
            elif build_usd.IsVisualStudio2017OrGreater():
                b2_settings.append("toolset=msvc-14.1")
            else:
                b2_settings.append("toolset=msvc-14.0")

        if MacOS():
            # Must specify toolset=clang to ensure install_name for boost
            # libraries includes @rpath
            if Context().targetIos:
                b2_settings.append("toolset=darwin-iphone")
                b2_settings.append("target-os=iphone")
                b2_settings.append("define=_LITTLE_ENDIAN")
                b2_settings.append("link=static")
                iOSVersion = 16.0
                b2_settings.append("--user-config=user-config.jam")
                b2_settings.append("macosx-version=iphone-{IOS_SDK_VERSION}".format(
                    IOS_SDK_VERSION=iOSVersion))
            else:
                b2_settings.append("toolset=clang")

            # Specify target for macOS cross-compilation.
            macOSArchitecture = ""
            macOSArch = ""
            if Context().targetX86:
                macOSArchitecture = "architecture=x86"
                macOSArch = "-arch x86_64"
            elif Context().targetARM64 or Context().targetIos:
                macOSArchitecture = "architecture=arm"
                macOSArch = "-arch arm64"
            elif Context().targetUniversal:
                macOSArchitecture = "architecture=combined"
                macOSArch = "-arch arm64 -arch x86_64"

            if macOSArchitecture:
                b2_settings.append(macOSArchitecture)

            if macOSArch:
                cxxFlags = ""
                linkFlags = ""
                if Context().targetIos:
                    cxxFlags = "{0} -std=c++14 -stdlib=libc++".format(macOSArch)
                    linkFlags = "{0} -stdlib=libc++".format(macOSArch)
                else:
                    cxxFlags = "{0}".format(macOSArch)
                    linkFlags = "{0}".format(macOSArch)


                b2_settings.append("cxxflags=\"{0}\"".format(cxxFlags))
                b2_settings.append("cflags=\"{0}\"".format(macOSArch))
                b2_settings.append("linkflags=\"{0}\"".format(linkFlags))

    def PrepareExtraProjects(self):
        if Context().targetIos:
            xcodeRoot = build_usd.GetCommandOutput('xcode-select --print-path').strip()
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
            projectPath = 'user-config.jam'

            if os.path.exists(projectPath):
                os.remove(projectPath)
            with open(projectPath, 'w') as projectFile:
                projectFile.write('\n')
                projectFile.writelines(newLines)

class OpenSubdiv:
    def __init__(self):
        self.buildDirmacOS = ""
    def HandlePlatformSpecific(self, srcOSDDir, extraEnv, force, extraArgs):
        self.buildDirmacOS = ""
        self.sdkroot = None
        if MacOS():
            self.sdkroot = os.environ.get('SDKROOT')
            Context().numJobs = 1

            if apple_utils.GetMacTargetArch(Context()) != apple_utils.GetMacArch() and not targetIos():
                # For macOS cross-compilation it is necessary to build stringify
                # on the host architecture.  This is then passed into the second
                # phase of building OSD with the STRINGIFY_LOCATION parameter.
                stringifyDir = srcOSDDir + "_stringify"
                if os.path.isdir(stringifyDir):
                    shutil.rmtree(stringifyDir)
                shutil.copytree(srcOSDDir, stringifyDir)

                stringifyContext = copy.copy(Context())
                stringifyContext.targetArch = apple_utils.GetMacArch()

                with build_usd.CurrentWorkingDirectory(stringifyDir):
                    build_usd.RunCMake(stringifyContext, force, extraArgs)

                buildStringifyDir = os.path.join(stringifyContext.buildDir, os.path.split(srcOSDDir)[1] + "_stringify")

                extraArgs.append('-DSTRINGIFY_LOCATION={buildStringifyDir}/bin/stringify'
                                 .format(buildStringifyDir=buildStringifyDir))

                extraEnv[0]["OSD_CMAKE_CROSSCOMPILE"] = "1"

                # Patch CMakeLists.txt to enable cross-compiling and pick up the stringify location.
                build_usd.PatchFile("CMakeLists.txt",
                          [("set_property(GLOBAL PROPERTY USE_FOLDERS ON)",
                            "set_property(GLOBAL PROPERTY USE_FOLDERS ON)\nif(DEFINED ENV{OSD_CMAKE_CROSSCOMPILE})\nset(CMAKE_CROSSCOMPILING 1)\nendif()")])
            elif targetIos():
                build_usd.PatchFile(srcOSDDir + "/cmake/iOSToolchain.cmake",
                          [("set(SDKROOT $ENV{SDKROOT})",
                            "set(CMAKE_TRY_COMPILE_TARGET_TYPE \"STATIC_LIBRARY\")\n"
                            "set(SDKROOT $ENV{SDKROOT})"),
                           ("set(CMAKE_SYSTEM_PROCESSOR arm)",
                            "set(CMAKE_SYSTEM_PROCESSOR arm64)\n"
                            "set(NAMED_LANGUAGE_SUPPORT OFF)\n"
                            "set(PLATFORM \"OS64\")\n"
                            "set(ENABLE_BITCODE OFF)"),
                           ])
                build_usd.PatchFile(srcOSDDir + "/opensubdiv/CMakeLists.txt",
                          [("if (BUILD_SHARED_LIBS AND NOT WIN32 AND NOT IOS)",
                            "if (BUILD_SHARED_LIBS AND NOT WIN32)")])

                # We build for macOS in order to leverage the STRINGIFY binary built
                srcOSDmacOSDir = srcOSDDir + "_macOS"
                if os.path.isdir(srcOSDmacOSDir):
                    shutil.rmtree(srcOSDmacOSDir)
                shutil.copytree(srcOSDDir, srcOSDmacOSDir)

                # Install macOS dependencies into a temporary directory, to avoid iOS space polution
                tempContext = copy.copy(Context())
                tempContext.instDir = tempContext.instDir + "/macOS"
                with build_usd.CurrentWorkingDirectory(srcOSDmacOSDir):
                    build_usd.RunCMake(tempContext, force, extraArgs, hostPlatform=True)
                shutil.rmtree(tempContext.instDir)

                self.buildDirmacOS = os.path.join(Context().buildDir, os.path.split(srcOSDmacOSDir)[1])

                extraArgs.append('-DNO_CLEW=ON')
                extraArgs.append('-DNO_OPENGL=ON')
                extraArgs.append('-DSTRINGIFY_LOCATION={buildDirmacOS}/bin/{variant}/stringify'
                                 .format(buildDirmacOS=self.buildDirmacOS,
                                         variant="Debug" if Context().buildDebug else "Release"))
                extraArgs.append('-DCMAKE_TOOLCHAIN_FILE={srcOSDDir}/cmake/iOSToolchain.cmake -DPLATFORM=\'OS64\''
                                 .format(srcOSDDir=srcOSDDir))
                extraArgs.append('-DCMAKE_OSX_ARCHITECTURES=arm64')
                os.environ['SDKROOT'] = build_usd.GetCommandOutput('xcrun --sdk iphoneos --show-sdk-path').strip()
                extraEnv[0] = os.environ.copy()
    def Cleanup(self):
        if self.sdkroot is None:
            os.unsetenv('SDKROOT')
        else:
            os.environ['SDKROOT'] = self.sdkroot
        if self.buildDirmacOS != "":
            shutil.rmtree(self.buildDirmacOS)


openSubdiv = OpenSubdiv()
boost = Boost()