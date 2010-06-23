# -*- mode:python; coding:utf-8; -*-
# jedit: :mode=python:

#  Copyright � 2010 Antti Karanta
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

import unittest

import supportModule
import nativelauncher


class StringUtilsTestCase ( unittest.TestCase ) :

    def testStartsWith( self ) :
        self.assertTrue( nativelauncher.jst_startsWith( 'foobar', 'foo' ) )
        self.assertTrue( nativelauncher.jst_startsWith( 'foobar', '' ) )
        self.assertFalse( nativelauncher.jst_startsWith( 'foobar', 'bar' ) )
        
    def testEndsWith( self ) :
        self.assertFalse( nativelauncher.jst_endsWith( 'foobar', 'foo' ) )
        self.assertTrue( nativelauncher.jst_endsWith( 'foobar', '' ) )
        self.assertTrue( nativelauncher.jst_endsWith( 'foobar', 'bar' ) )


def runTests ( path , architecture ) :
    return supportModule.runTests ( path , architecture , StringUtilsTestCase )

if __name__ == '__main__' :
    print 'Run tests using command "scons test".'
