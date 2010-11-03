
import os
import re
import unittest
import shutil
import platform
import ctypes
import tempfile
import subprocess

import supportModule


width = ctypes.sizeof( ctypes.c_voidp ) * 8

class PlatformTestCase ( unittest.TestCase ) :

    # TODO: make a test that all of present build, python and java 
    #       are of same width (32 or 64 bit). If this is not true, 
    #       many tests will fail with mysterious error messages
    #       by which it may not be clear to what the problem is

    def testBuildWidthMatchesPythonInterpreterWidth( self ) :
        self.assertEqual( width, supportModule.width, 'this python process and your build have different word size' )

    def testJavaWidthMatchesBuildWidth( self ) :
        javaSource = """
public class WidthTeller {
    public static void main( String[] args ) {
        System.out.println( System.getProperty( "sun.arch.data.model" ) ) ;
    }
}
"""
        tmpdir = tempfile.gettempdir()
        javaSourceFileName = os.path.join( tmpdir, "WidthTeller.java" ) 
        with open( javaSourceFileName, "w" ) as javaSourceFile :
            javaSourceFile.write( javaSource )
        javaHome = os.environ[ "JAVA_HOME" ]
        javac = os.path.join( javaHome, 'bin', 'javac' )
        subprocess.check_call( [ javac, javaSourceFileName ] )
        java = os.path.join( javaHome, 'bin', 'java' )
        result = subprocess.check_output( [ java, '-cp', tmpdir, 'WidthTeller' ] )
        result = result.strip()
        message = "JAVA_HOME points to a Java installation with different word size ({0}bit) than the current build ({1}bit).\nThe binaries produced are not affected, but the tests will fail.".format( result, width )
        self.assertEqual( str( supportModule.width ), result, message )
        # TODO: delete java source and compiled bytecode



def runTests ( path , architecture ) :
    return supportModule.runTests ( path , architecture , PlatformTestCase )

if __name__ == '__main__' :
    print 'Run tests using command "scons test".'
