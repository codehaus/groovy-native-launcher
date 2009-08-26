# -*- mode:python; coding:utf-8; -*-
# jedit: :mode=python:

#  Groovy -- A native launcher for Groovy
#
#  Copyright © 2007-9 Russel Winder
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
#  $Revision$
#  $Date$

import platform
import os
import sys

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

unameResult = platform.uname ( )

#  toolchain and msvsversion options are processed here, other options are processed in source/SConscript
#  These have effects that need to be taken into account when creating the environment.
#
#  There is an issue when using Windows that Visual C++ has precedence of GCC and sometimes you really have
#  to use GCC even when Visual C++ is present.

toolchain = ARGUMENTS.get ( 'toolchain' , False ) 
msvsVersion = ARGUMENTS.get ( 'msvsversion' , False ) 
if ( toolchain ) :
    if msvsVersion :
        environment = Environment ( tools = [ toolchain ] , MSVS_VERSION = msvsVersion )
    else :
        environment = Environment ( tools = [ toolchain ] )
elif msvsVersion :
    environment = Environment ( MSVS_VERSION = msvsVersion )
else :
    environment = Environment ( )

environment['Architecture'] = unameResult[0]

#  Windows cmd and MSYS shell present the same information to Python since both are using the Windows native
#  Python.  We need to distinguish these (as was done with the earlier Rant build system).  Try executing
#  uname directly as a command, if it fails this is Windows native, if it succeeds then we are using MSYS.

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
    width = 32
else :
    print 'Width must be 32 or 64, 32 is the default if nothing specified. Value given was' , width
    Exit ( 1 )
environment['Width'] = width

#  The Client VM test in tests/groovyTest.py has to know whether this is a 32-bit or 64-bit VM test, so put
#  the width value into the shell environment so that it can be picked up during the test.

os.environ['Width'] = str ( width )

#  Distinguish the build directory and the sconsign file by architecture, shell, processor, and compiler so
#  that multiple builds for different architectures can happen concurrently using the same source tree.
 
discriminator = environment['Architecture'] + '_' + unameResult[4] + '_' + environment['CC']
if width != 32 : discriminator = discriminator + '_' + str ( width )
buildDirectory = 'build_scons_' + discriminator

environment.SConsignFile ( '.sconsign_' + discriminator )

#  All information about the actual build itself is in the subsidiary script.

executables = SConscript ( 'source/SConscript' , exports = 'environment' , variant_dir = buildDirectory , duplicate = 0 )

#  From here down is about the targets that the user will want to make use of.

Default ( Alias ( 'compile' , executables ) )

def runLauncherTests ( target , source , env ) :
    for item in source :
        ( root , ext ) = os.path.splitext ( item.name )
        sys.path.append ( 'tests' )
        try :
            module = __import__ ( root + 'Test' )
            if not module.runTests ( item.path , environment['PLATFORM'] ) :
                Exit ( 1 )
        except ImportError :
            pass

Command ( 'test' , executables , runLauncherTests )

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
    test
    install

are provided.  compile is the default.  Possible options are:

    debug=<True|*False*>
    cygwinsupport=<True|*False*>
    toolchain=mingw (to use mingw even if msvs is installed)
    msvsversion=<version> (to use specific version if several versions are installed)
    extramacros=<list-of-c-macro-definitions>
''' )
