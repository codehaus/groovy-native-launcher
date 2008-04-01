#  Groovy -- A native launcher for Groovy
#
#  Copyright © 2006-7 Antti Karanta <Antti dot Karanta at hornankuusi dot fi>
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


require "test/unit"
require 'pathname'
require 'rbconfig'
require 'English'

$dir_containing_executable ||= ARGV.size > 0 && ARGV[ 0 ]

raise "dir containing the executable to test must be given as a parameter" unless $dir_containing_executable

exe_name = 'groovy' + ( Config::CONFIG[ 'host_os' ] =~ /win/ ? '.exe' : '' )

EXE_FILE = ( Pathname.new( $dir_containing_executable ) + exe_name ).to_s

raise "#{EXE_FILE} does not exist" unless File.exist?( EXE_FILE )

class LauncherTest < Test::Unit::TestCase
  
  def test_version
    output = `#{EXE_FILE} -v`
    assert( output =~ /groovy/i )
  end

# build_rant_linux_i686/groovy -server -e "println System.getProperty('java.vm.name')"
# -> must contain "server" (ignore case)
# w/out -server => must contain client

# pass -Xmx300m and check that it takes effect

# generate a groovy source file, run that and check the output
  
end
