# -*- mode:ruby coding:utf-8 -*-
# jedit: :mode=ruby:

#  Groovy -- A native launcher for Groovy
#
#  Copyright © 2006- Russel Winder & Antti Karanta
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
#  Author : Russel Winder <russel@russel.org.uk> & Antti Karanta <Antti dot Karanta at hornankuusi dot fi>

# TODO: Use the name of the architecture specific directory in ${JAVA_HOME}/include as representation.

LINUX = 'linux'
SOLARIS = 'solaris'
MAC_OS_X = 'darwin'
CYGWIN = 'cygwin'
MSYS = 'msys'
WINDOWS = 'windows'

def unknownArchitecture ( ) raise "Unrecognized architecture -- #{Architecture}" end

def get_architecture()
  begin
     case `uname -s`
        when /Linux/ then LINUX
        when /SunOS/ then SOLARIS
        when /Darwin/ then MAC_OS_X   
        when /CYGWIN/ then CYGWIN
        when /MINGW32/ then MSYS  
        else
          unknownArchitecture
        end
  rescue
    if ENV[ 'WINDIR' ]
      WINDOWS
    else
      unknownArchitecture
    end
  end
end

def get_builddir()
  begin
    "build_rant_#{get_architecture()}_#{`uname -m`.strip( )}"
  rescue
    "build_rant_#{get_architecture()}"
  end
end
  
