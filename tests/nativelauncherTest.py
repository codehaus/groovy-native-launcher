
import os
import re
import unittest
import shutil
import platform

import supportModule
import nativelauncher

class JVMFindingTestCase ( unittest.TestCase ) :

    def testNativeFuncsAccessible( self ) :
        # launchermodule = __import__( 'nativelauncher' )
        self.assertEqual( 0, nativelauncher.cvar._jst_debug )
        

#  The entry point for SCons to use.

def runTests ( path , architecture ) :
    return supportModule.runTests ( path , architecture , JVMFindingTestCase )

if __name__ == '__main__' :
    print 'Run tests using command "scons test".'

