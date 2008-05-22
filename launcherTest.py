#! /usr/bin/env python
# -*- mode:python coding:utf-8 -*-

#  Groovy -- A native launcher for Groovy
#
#  Copyright © 2008 Russel Winder
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

#  This tests relaize din this Python script originated from the ones in the launcher_test.rb script by
#  Antti Karanta.

#  The direrctory in which the current executable is stored is calculated by SCons and includes the compiler
#  in use.  This means this test script is not entirely standalone but must be initiated by SCons.

import os
import re
import tempfile
import unittest

#  The relative path to the Groovy executable.
executablePath = None

#  The value of environment['PLATFORM'], so the values are posix, cygwin, win32.
architecture = None

#  Temporary file names generated by Python can contain characters that are not permitted as Java class
#  names.  This function generates temporary files that are compatible.

def javaNameCompatibleTemporaryFile ( ) :
    while True :
        file = tempfile.NamedTemporaryFile ( )
        if re.compile ( '[-]' ).search ( file.name ) == None : break
        file.close ( )
    return file

#  Execute a Groovy activity returning a tuple of the return value and the output.  On Ubuntu and Cygwin the
#  return value is an amalgam of signal and return code.  Fortunately we know that the signal is in the low
#  byt and the return value in the next byte.

def executeGroovy ( command ) :
    process = os.popen ( executablePath + ' ' + command )
    output = process.read ( ).strip ( )
    returnCode = process.close ( )
    if returnCode != None :
        #  Different platforms do different things with the return value.  This sucks.
        if platform in [ 'posix' , 'cygwin' ] :
            returnCode >>= 8
    return ( returnCode , output )
  
#  The actual test cases are collected together in this class.

class LauncherTestCase ( unittest.TestCase ) :

    def groovyExecutionTest ( self , command , expectedReturnCode , expectedOutput , extra = None ) :
        '''Execute a Groovy command and then test that the return code is right and the output is right.'''
        ( returnCode , output ) = executeGroovy ( command )
        self.assertEqual ( returnCode , expectedReturnCode )
        if type ( expectedOutput ) == type ( re.compile ( 'a' ) ) :
            assert expectedOutput.search ( output ) != None , 'Failed to match ' + extra
        else :
            self.assertEqual ( output, expectedOutput )

    def testVersion ( self ) :
        pattern = 'Groovy Version: .* JVM: '
        self.groovyExecutionTest ( '-v' , None , re.compile ( pattern ) , pattern )

    def testPassingJVMParameter ( self ) :
        self.groovyExecutionTest ( '-Xmx300m -e "println Runtime.runtime.maxMemory ( )"' , None , '312213504' )

    def testServerVM ( self ) :
        self.groovyExecutionTest ( '-server -e "println System.getProperty ( \'java.vm.name\')"' , None , 'Java HotSpot(TM) Server VM' )

    def testClientVM ( self ) :
        self.groovyExecutionTest ( '-e "println System.getProperty ( \'java.vm.name\')"' , None , 'Java HotSpot(TM) Client VM' )

    def testExitStatus ( self ) :
        self.groovyExecutionTest ( '-e "System.exit ( 123 )"' , 123 , '' )

    def testLaunchingScript ( self ) :
        #  There is a weird problem with using filenames on MSYS, assume the same is true for Windwoes.
        tmpFile = file ( 'flobadob' , 'w+' ) if platform == 'win32' else javaNameCompatibleTemporaryFile ( )
        tmpFile.write ( 'println \'hello \' + args[ 0 ]\n' )
        tmpFile.flush( )
        self.groovyExecutionTest ( tmpFile.name + ' world' , None , 'hello world' )
        tmpFile.close ( )
        if platform == 'win32' : os.remove ( tmpFile.name )

class CygwinLauncherTestCase ( LauncherTestCase ) :
    pass

#  The entry point for SCons to use.

def runLauncherTests ( path , architecture ) :
    global executablePath, platform
    executablePath = path
    platform = architecture
    suite = unittest.makeSuite ( CygwinLauncherTestCase , 'test' ) if platform == 'cygwin' else unittest.makeSuite ( LauncherTestCase , 'test' )
    runner = unittest.TextTestRunner ( )
    runner.run ( suite )

if __name__ == '__main__' :
    print 'Run tests using command "scons test".'