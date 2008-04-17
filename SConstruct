# -*- mode:python encoding:UTF-8 -*-
# jedit: :mode=python:

#  Groovy -- A native launcher for Groovy
#
#  Copyright © 2007-8 Russel Winder
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
#  Author : Russel Winder <russel@russel.org.uk>
#  $Revision$
#  $Date$

import platform
import os

#  The in-built PLATFORM key does not necessarily provide proper discrimination of Posix compliant systems,
#  and does not distinguish same operating system on different architectures, so we are forced to do things
#  a bit more unamish.

environment = Environment ( Name = 'groovy' )

unameResult = platform.uname ( )
environment['Architecture'] = unameResult[0]
discriminator = unameResult[0] + '_' + unameResult[4]
buildDirectory = 'build_scons_' + discriminator

environment.SConsignFile ( '.sconsign_' + discriminator )

executable = SConscript ( 'source/SConscript' , exports = 'environment' , build_dir = buildDirectory , duplicate = 0 )

Default ( Alias ( 'compile' , executable ) )
Command ( 'test' , executable , 'GROOVY_HOME=' + os.environ['GROOVY_HOME'] + ' ruby launcher_test.rb ' + buildDirectory )

Help ( '''The targets:

    compile
    test

are provided.  compile is the default.''' )

#  Have take account of the detritus created by a JVM failure -- never arises on Ubuntu or Mac OS X, but
#  does arise on Solaris 10.

Clean ( '.' , Glob ( '*~' ) + Glob ( '*/*~' ) + Glob ( 'hs_err_pid*.log' ) + [ buildDirectory , 'core' ] )
