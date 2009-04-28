# -*- mode:python; coding:utf-8; -*-
# jedit: :mode=python:

#  Groovy -- A native launcher for Groovy
#
#  Copyright © 2008-9 Russel Winder
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

import re
import unittest

import supportModule

#  The actual test cases are collected together in this class.

class GantTestCase ( unittest.TestCase ) :

    def gantExecutionTest ( self , command , expectedOutput = '' , expectedReturnCode = None , extraMessage = None , prefixCommand = '' ) :
        '''Execute a Groovy command and then test that the return code is right and the output is right.'''
        ( returnCode , output ) = supportModule.executeCommand ( command , prefixCommand )
        self.assertEqual ( returnCode , expectedReturnCode )
        if type ( expectedOutput ) == type ( re.compile ( 'a' ) ) :
            assert expectedOutput.search ( output ) != None , 'Failed to match ' +  ( extraMessage if extraMessage else output + ' against ' + str ( expectedOutput ) )
        elif type( expectedOutput ) == LambdaType :
            assert expectedOutput ( output ) , 'failed ' + str ( expectedOutput )
        else :
            self.assertEqual ( output , expectedOutput )

    def testVersion ( self ) :
        pattern = 'Gant version '
        self.gantExecutionTest ( '-V' , re.compile ( pattern ) , None , pattern )

#  The entry point for SCons to use.

def runTests ( path , architecture ) :
    return supportModule.runTests ( path , architecture , GantTestCase )

if __name__ == '__main__' :
    print 'Run tests using command "scons test".'
