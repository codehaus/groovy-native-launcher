
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
        
# FIXME move these to a separate test class. 

    def testModifyingSwigFileToBeMSVCDebugCompliant( self ) :
        import tempfile
        # create a temporary file with the appropriate #include
        sampleFile = tempfile.TemporaryFile( 'w+' )
        try :
            sampleFile.writelines( [ "\n",
                                     "/* Python.h has to appear first */\n", 
                                     "#include <Python.h>\n",
                                     "bar\n",
                                     "\n"] )
            
            sampleFile.flush()
            sampleFile.seek( 0 )

            supportModule.surroundPythonHIncludeWithGuards( sampleFile )
            
            # check that the said import in said file is now ok
            sampleFile.seek( 0 )
            lines = sampleFile.readlines()
            
            expectedLines = [
                "\n",
                "/* Python.h has to appear first */\n", 
                "#if defined( _DEBUG )\n",
                "#  define _DEBUG_WAS_DEFINED\n",
                "#  undef _DEBUG\n",
                "#endif\n",
                "#include <Python.h>\n",
                "#if defined( _DEBUG_WAS_DEFINED )\n",
                "#  define _DEBUG\n",
                "#endif\n",
                "bar\n",
                "\n"
            ]
            
            self.assertEqual( expectedLines, lines )
            
            # call func to modify the file again
            supportModule.surroundPythonHIncludeWithGuards( sampleFile )
            # check there are no further modifications
            sampleFile.seek( 0 )
            linesNow = sampleFile.readlines()
            self.assertEqual( lines, linesNow )
        finally :
            sampleFile.close()

#  The entry point for SCons to use.

def runTests ( path , architecture ) :
    return supportModule.runTests ( path , architecture , JVMFindingTestCase )

if __name__ == '__main__' :
    print 'Run tests using command "scons test".'
###    unittest.main()

