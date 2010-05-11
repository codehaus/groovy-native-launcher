# -*- mode:python; coding:utf-8; -*-
# jedit: :mode=python:

#  Groovy -- A native launcher for Groovy
#
#  Copyright © 2007-10 Russel Winder & Antti Karanta
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
#  Author : Russel Winder <russel.winder@concertant.com>, Antti Karanta <firstname dot lastname (at) hornankuusi dot fi>

import os
import sys

class NativeLauncherTester :
    
    def __init__( self, buildDirectory ) :
        self._buildDirectory = buildDirectory

    def runLauncherTests ( self, target , source , env ) :
        
        testsFailed = False
        sys.path.append ( 'tests' )
        sys.path.append ( self._buildDirectory )
    
        alreadyExecuted = []
    
        print
    
        for item in source :
            ( root , ext ) = os.path.splitext ( item.name )
            #if ext in [ '.ext', '.lib' ] : continue
            testModuleName = root + 'Test'
            testModuleName = testModuleName.lstrip( '_' ) 
            if not os.path.exists( os.path.join( 'tests', testModuleName + '.py' ) ) : 
                print "info: no tests exist for ", item.name, "\n"
                continue
            
            if testModuleName in alreadyExecuted : continue
    
            try :
                module = __import__ ( testModuleName )
            except ImportError , ie :
                print 'Error loading tests for ' , root, ' :: ' , ie 
                testsFailed = True
            else :
                print 'Running test ' , testModuleName
                if not module.runTests ( item.path , env[ 'PLATFORM' ] ) :
                    print '  Tests failed.'
                    testsFailed = True
                    
            alreadyExecuted.append( testModuleName )
            print
            
        if testsFailed : Exit ( 1 )

