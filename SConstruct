# -*- mode:python; coding:utf-8; -*-
# jedit: :mode=python:

#  Groovy -- A native launcher for Groovy
#
#  Copyright © 2007-10 Russel Winder
#
#  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in
#  compliance with the License. You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software distributed under the License is
#  distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
#  implied. See the License for the specific language governing permissions and limitations under the
#  License.
#
#  Author : Russel Winder <russel.winder@concertant.com>

import os
import sys

from distutils.sysconfig import get_python_inc as getPythonIncludePaths
from distutils.sysconfig import get_python_lib as getPythonLibraryPaths

#  For Bamboo continuous integration, the test results need to be output as XML files.  Provide a command
#  line option to switch the feature on.  Have to put the value into the shell environment so that its value
#  is available in other Python code.  Because of this always use strings.

xmlTestOutputDirectory = 'test-reports'
xmlOutputRequired = ARGUMENTS.get ( 'xmlOutputRequired' , 'False' )
os.environ['xmlTestOutputDirectory'] = xmlTestOutputDirectory
os.environ['xmlOutputRequired' ] = xmlOutputRequired

#  Once we have an environment, we can distinguish things according to the PLATFORM which is one of posix,
#  darwin, sunos, cygwin, win32 for the machines tested to date.  This does not distinguish the same OS on
#  different architectures where this is an issue.  For Python this is not an issue, but we are compiling C
#  so it is.  We therefore use uname to provide better discrimination.  This will give Linux, SunOS, Darwin,
#  CYGWIN_NT-5.[12], Windows as possible values for the operating system (no different from PLATFORM
#  really), and values like i686 for the processor where it can be determined, i.e not on Windows but on all
#  other systems.  Combine this with the compiler in use and we have a complete platform specification so
#  that we can have multiple concurrent builds for different architectures all in the same source hierarchy.

unameResult = os.uname ( )

#  There is an issue when using Windows that Visual C++ has precedence over GCC and sometimes you really
#  have to use GCC even when Visual C++ is present.  Use the command line variables toolchain and
#  msvsversion to handle this case.
#
#  We need to make use of the users' environment, especially PATH, when initializing the tools, so as to
#  ensure all the commands we are going to use are available.  In particular an issue arises usign SWIG on
#  Windows, the swig.exe executable is never in one of the standard places.  So always pick up the user PATH
#  when creating the environment.  SCons appears not to have a way of only amending the PATH.

toolchain = ARGUMENTS.get ( 'toolchain' , False ) 
msvsVersion = ARGUMENTS.get ( 'msvsversion' , False ) 
if ( toolchain ) :
    if msvsVersion :
        environment = Environment ( tools = [ toolchain , 'swig' ] , MSVS_VERSION = msvsVersion , ENV = os.environ )
    else :
        environment = Environment ( tools = [ toolchain , 'swig' ] , ENV = os.environ )
elif msvsVersion :
    environment = Environment ( MSVS_VERSION = msvsVersion , ENV = os.environ )
else :
    environment = Environment ( ENV = os.environ )

environment['Architecture'] = unameResult[0]

Export ( 'environment' )

#  Windows cmd and MSYS shell present the same information to Python since both are using the Windows native
#  Python.  We need to distinguish these.  Try executing uname directly as a command, if it fails this is
#  Windows native, if it succeeds then we are using MSYS.

if environment['Architecture'] == 'Windows' :
    result = os.popen ( 'uname -s' ).read ( ).strip ( )
    if result != '' : environment['Architecture'] = result

#  A platform may support both 32-bit and 64-bit builds, e,g, Solaris builds on SPARC v9.  By default we
#  assume a 32-bit build.  Use a command line parameter width to determine if a 64-bit build is to be
#  attempted.

width = int ( ARGUMENTS.get ( 'width' , -1 ) )
if width == 64 or width == 32 :
    pass
elif width == -1 :
    if environment['Architecture'] == 'Linux' :
        width = 64 if unameResult[4] == 'x86_64' else 32
    else :
        width = 32
else :
    print 'Width must be 32 or 64. Value given was' , width
    Exit ( 1 )
environment['Width'] = width

#  The Client VM test in tests/groovyTest.py has to know whether this is a 32-bit or 64-bit VM test, so put
#  the width value into the shell environment so that it can be picked up during the test.

os.environ['Width'] = str ( width )

#  Distinguish the build directory and the sconsign file by architecture, shell, processor, and compiler so
#  that multiple builds for different architectures can happen concurrently using the same source tree.
 
discriminator = environment['Architecture'] + '_' + unameResult[4] + '_' + environment['CC'] + '_' + str ( width )  
buildDirectory = 'build_scons_' + discriminator

environment.SConsignFile ( '.sconsign_' + discriminator )

if environment['CC'] == 'gcc' : environment.Append ( CCFLAGS = [ '-m' + str ( width ) ] , LINKFLAGS = [ '-m' + str ( width ) ] )
else :
    if environment['Width'] == 64 :
        raise Exception , '64-bit build for non-GCC not yet set up.'

#  Deal with debug and profile command line parameters.

debug = eval ( ARGUMENTS.get ( 'debug' ,  'False' ) )
profile = eval ( ARGUMENTS.get ( 'profile' , 'False' ) )

if debug and profile :
    print 'Profiling should be done on optimized executable, not debug version.'
    Exit ( 1 )

if profile :
    if environment['CC'] == 'cl' :
        environment.Append ( CCFLAGS  = [ '-Zi' ] , LINKFLAGS = [ '-debug' , '-fixed:no' ] )

if debug :
    if environment['CC'] == 'cl' or environment['CC'] == 'icl' :
        environment.Append ( CPPDEFINES= [ '_CRTDBG_MAP_ALLOC' ] , CCFLAGS = [ '-MDd' , '-Od' , '-Zi' , '-RTCcsu' ] , LINKFLAGS = [ '-debug' ] )
    elif environment['CC'] == 'gcc' :
        environment.Append ( CCFLAGS = [ '-g' ] )
    else :
        print 'Debug mode not set up for compiler' , environment['CC']
        Exit ( 1 )
else :
    environment.Append ( CPPDEFINES = [ 'NDEBUG' ] )
    if environment['CC'] == 'cl' or environment['CC'] == 'icl' :
        #  The -MD option is important - it is required when using JNI. 
        #  See for example http://java.sun.com/docs/books/jni/html/start.html#27008 and
        #  http://java.sun.com/docs/books/jni/html/invoke.html#28755
        environment.Append ( CCFLAGS = [ '-MD' , '-Ox' , '-GF' ] , LINKFLAGS = [ '-opt:ref' ] )
        if float( environment[ 'MSVS_VERSION' ] ) < 9.0 :
            environment.Append ( LINKFLAGS = [ '-opt:nowin98' ] )
        if not profile :
            #  Global optimization in msvc is incompatible w/ generating debug info (-Zi + friends).
            environment.Append ( CCFLAGS = [ '-GL' ] , LINKFLAGS = [ '-ltcg' ] )
    elif environment['CC'] == 'gcc' :
        environment.Append ( CCFLAGS = [ '-O3' ] ) 
        #  Mac OS X Leopard ld ignores this option as being obsolete.
        #  Solaris GCC appears to use Sun ld if present rather than binutils ld.
        environment.Append ( LINKFLAGS = [ '-Wl,-s' ] )
    else :
        print 'Strip mode not set for compiler' , environment['CC']

cygwinsupport = eval ( ARGUMENTS.get ( 'cygwinsupport' , 'True' ) )
if cygwinsupport :
    if environment['PLATFORM'] in [ 'win32' , 'mingw' , 'cygwin' ] : # TODO: add win64 once it is figured out what name it goes by
        environment.Append ( CPPDEFINES = [ '_cwcompat' ] )

extraMacrosString = ARGUMENTS.get ( 'extramacros' , '' )
for macro in extraMacrosString.split ( ) :
    environment.Append ( CPPDEFINES = [ macro ] )

# TODO: maybe add looking up jdk location from windows registry on windows?

defaultJDKLocations = {
    'Darwin' : [ '/System/Library/Frameworks/JavaVM.framework/Home' ] ,
    'Linux' : [ '/usr/lib/jvm/java-6-sun' , '/usr/lib/jvm/java-6-openjdk' , '/usr/lib/jvm/java-5-sun' , '/usr/lib/jvm/java-5-openjdk' ] ,
    'SunOS' : [ '/usr/jdk/latest' ] ,
    }

try :
    javaHome = os.environ['JAVA_HOME']
except :
    javaHome = '' 
if javaHome == '' :
    try :
        putativeDirectoryList = defaultJDKLocations [ environment['Architecture'] ]
    except :
        print 'JAVA_HOME is not defined and no default location can be inferred.'
        Exit ( 1 )
    for putativeDirectory in putativeDirectoryList :
        if os.path.isdir ( putativeDirectory ) :
            javaHome = putativeDirectory
            environment.Append ( CPPPATH = [ javaHome ] )
            break
    else :
        print 'JAVA_HOME not defined and none of', putativeDirectoryList , 'found.'
        Exit ( 1 )

if environment['PLATFORM'] == 'cygwin' :
    javaHome = os.popen ( 'cygpath --unix "' + javaHome + '"' ).read ( ).strip ( )
    environment.Append ( CCFLAGS = [ '-mno-cygwin' ] , LINKFLAGS = [ '-s' , '-mno-cygwin' ] )
    windowsEnvironment = environment.Clone ( )
    windowsEnvironment.Append ( LINKFLAGS = [ '-mwindows' ] )
    Export ( 'windowsEnvironment' )

#  Map from uname operating system names (environment['Architecture']) to include directory names.

def includeDirectoryName( architecture ) :
    if architecture.startswith( 'CYGWIN' ) or architecture.startswith( 'MINGW' ) or architecture == 'Windows' :
        return 'win32'
    return {
        'Linux' : 'linux' ,
        'SunOS' : 'solaris' ,
        'Darwin' : 'darwin' ,
    }[ architecture ]

environment.Append ( CPPPATH = [ os.path.join ( javaHome , 'include'  , includeDirectoryName( environment[ 'Architecture' ] ) ) , os.path.join ( javaHome , 'include' ) ] )
if environment['CC'] == 'cl' or environment['CC'] == 'icl' :
    # -Wall produces screenfulls of useless warnings about win header files unless the following warnings are omitted:
    # c4206 == translation unit is empty (if not compiling cygwin compatible binary, jst_cygwincompatibility.c is empty)
    # c4668 == 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
    # c4820 == padding added into a struct 
    # This one is disabled as there's a lot of this done intentionally (and there seems to be no way to tell cl that it's intentional):
    # c4706 == assignment in conditional expression
    # this one's disabled as it does not seem very interesting:
    # c4711 == function 'function' selected for inline expansion
    #
    environment.Append ( 
        CPPDEFINES = [ 'WIN32_LEAN_AND_MEAN' , 'VC_EXTRALEAN' , '_CRT_SECURE_NO_WARNINGS' , '_CRT_NONSTDC_NO_DEPRECATE', '_MBCS' ] ,
        CCFLAGS = [ '-TC' , '-Wall' , '-wd4206' , '-wd4255' , '-wd4668' , '-wd4706' , '-wd4711' , '-wd4820' ] ,
        LIBS = [ 'advapi32' ] ,
        LINKFLAGS = [ '-incremental:no' ]
        )
    if float( environment['MSVS_VERSION'] ) < 9.0 :
        environment.Append ( CCFLAGS = [ '-Wp64' ] )
    else :
        environment['SHLINKCOM'] = [ environment['SHLINKCOM'], 'mt -nologo -manifest ${TARGET}.manifest -outputresource:${TARGET};2' ]
        environment['LINKCOM'] = [ environment['LINKCOM'], 'mt -nologo -manifest ${TARGET}.manifest -outputresource:${TARGET};1' ]
        # does not work
        #Clean( '.', '*.manifest' )
                            
    windowsEnvironment = environment.Clone ( )
    windowsEnvironment.Append ( LINKFLAGS = [ '-subsystem:windows' , '-entry:mainCRTStartup' ] )
    environment.Append ( LINKFLAGS = [ '-subsystem:console' ] )
    Export ( 'windowsEnvironment' )
elif environment['CC'] == 'gcc' :
    #  Cygwin specific options were set earlier.
    environment.Append ( CCFLAGS = [ '-W', '-Wall', '-Wundef', '-Wcast-align', '-Wno-unused-parameter', '-Wshadow', '-Wredundant-decls' ] )
    if environment['PLATFORM'] == 'win32' :
        # TODO:  Find out what the right options are here for the MinGW GCC.
        windowsEnvironment = environment.Clone ( )
        windowsEnvironment.Append ( LINKFLAGS = [ '-mwindows' ] )
        Export ( 'windowsEnvironment' )
else :
    print 'Assuming default options for compiler' , environment[ 'CC' ]

if environment['Architecture'] in [ 'Linux' ] : environment.Append ( LIBS = [ 'dl' ] )

if environment['PLATFORM'] == 'darwin' :
    environment.Append ( LINKFLAGS = [ '-framework' ,  'CoreFoundation' ] )

#  As well as preparing executables for people to use, a shared library for test is also produced and this
#  involves SWIG.  Note the removal of the shared library prefix, this is basically to ensure the whole SWIG
#  infrastructure works as required when making Python native extensions.
#
#  The current way of using this environment relies on the fact that object files for shared libraries are
#  different from object files for executables.  This is true on Linux, Mac OS X and Solaris using GCC.

swigEnvironment = environment.Clone (
    SWIGFLAGS = [ '-Wall', '-python' ] ,
    SWIGPATH = environment['CPPPATH'] ,
    SHLIBPREFIX = ''
    )
swigEnvironment.Append ( CPPPATH = [ getPythonIncludePaths ( ) , '#source' ] )

if environment[ 'PLATFORM' ] in [ 'win32' , 'mingw' , 'cygwin' ] :
    swigEnvironment.Append ( LIBPATH = [ getPythonLibraryPaths( standard_lib = True ) + 's' ] )
    swigEnvironment['SHLIBSUFFIX'] = '.pyd'

if environment['PLATFORM'] == 'darwin' :
    swigEnvironment.Append ( LINKFLAGS = [ '-framework' , 'Python' ] )
    swigEnvironment.Prepend ( SWIGPATH = [ '#sourceForDarwinSWIG' ] )

Export ( 'swigEnvironment' )

#  All information about the actual build itself is in the subsidiary script.

( executables , sharedLibrary ) = SConscript ( 'source/SConscript' , variant_dir = buildDirectory , duplicate = 0 )

#  From here down is about the targets that the user will want to make use of.

Default ( Alias ( 'compile' , executables ) )

def runLauncherTests ( target , source , env ) :
    testsFailed = False
    for item in source :
        ( root , ext ) = os.path.splitext ( item.name )
        if ext in [ '.ext', '.lib' , '.pyd' , '.so' ] : continue
        sys.path.append ( 'tests' )
        sys.path.append ( buildDirectory )
        testModuleName = root + 'Test'
        try :
            module = __import__ ( testModuleName )
            print 'Running tests in' , testModuleName
            if not module.runTests ( item.path , environment['PLATFORM'] ) :
                print '  Tests failed.'
                testsFailed = True
            else :
                print '  Tests succeeded.'
        except ImportError , ie :
            print 'Cannot execute test for' , root
            testsFailed = True
    if testsFailed : Exit ( 1 )

Alias ( 'testLib' , sharedLibrary )

Command ( 'test' , ( executables , sharedLibrary ) , runLauncherTests )

#  Have to take account of the detritus created by a JVM failure -- never arises on Ubuntu or Mac OS X, but
#  does arise on Solaris 10.

Clean ( '.' ,
        Glob ( '*~' ) + Glob ( '.*~' ) + Glob ( '*/*~' )
        + Glob ( '*.pyc' ) + Glob ( '*/*.pyc' )
        + Glob ( 'hs_err_pid*.log' )
        + [ buildDirectory , xmlTestOutputDirectory , 'core' ]
        )

defaultPrefix = '/usr/local'
defaultInstallBinDirSubdirectory = 'bin'
defaultInstallBinDir = os.path.join ( defaultPrefix , defaultInstallBinDirSubdirectory )
prefix = defaultPrefix
installBinDir = defaultInstallBinDir
if os.environ.has_key ( 'PREFIX' ) :
    prefix = os.environ['PREFIX']
    installBinDir =  os.path.join ( prefix , defaultInstallBinDirSubdirectory )
if os.environ.has_key ( 'BINDIR' ) : installBinDir = os.environ['BINDIR']
defaultPrefix = prefix
defaultInstallBinDir = installBinDir
localBuildOptions = 'local.build.options'
if os.access ( localBuildOptions , os.R_OK ) :
    execfile ( localBuildOptions )
if prefix != defaultPrefix :
    if installBinDir == defaultInstallBinDir : installBinDir =  os.path.join ( prefix , defaultInstallBinDirSubdirectory )
if ARGUMENTS.has_key ( 'prefix' ) :
    prefix = ARGUMENTS.get ( 'prefix' )
    installBinDir =  os.path.join ( prefix , defaultInstallBinDirSubdirectory )
installBinDir = ARGUMENTS.get ( 'installBinDir' , installBinDir )

target = Install ( installBinDir , executables )
Alias ( 'install' , target , Chmod ( target , 0511 ) )

Help ( '''The targets:

    compile
    testLib
    test
    install

are provided.  compile is the default.  Possible options are:

    debug=<True|*False*>
    cygwinsupport=<True|*False*>
    toolchain=mingw (to use mingw even if msvs is installed)
    msvsversion=<version> (to use specific version if several versions are installed)
    extramacros=<list-of-c-macro-definitions>
''' )
