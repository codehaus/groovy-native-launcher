
import os
import re
import unittest
import shutil
import platform
import ctypes

import supportModule


width = ctypes.sizeof( ctypes.c_voidp ) * 8

class PlatformTestCase ( unittest.TestCase ) :

    # TODO: make a test that all of present build, python and java 
    #       are of same width (32 or 64 bit). If this is not true, 
    #       many tests will fail with mysterious error messages
    #       by which it may not be clear to what the problem is

    def testWidthsMatch( self ) :
        self.assertEqual( width, supportModule.width, 'this python process and your build have different word size' )
        # TODO: test that the jvm at JAVA_HOME has the same width
        pass



def runTests ( path , architecture ) :
    return supportModule.runTests ( path , architecture , PlatformTestCase )

if __name__ == '__main__' :
    print 'Run tests using command "scons test".'
