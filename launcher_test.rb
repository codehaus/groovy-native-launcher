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
require 'fileutils'


require 'osarch'

$dir_containing_executable ||= ( ARGV.size > 0 && ARGV[ 0 ] ) || get_builddir()


exe_name = 'groovy' + ( Config::CONFIG[ 'host_os' ] =~ /win/ ? '.exe' : '' )

EXE_FILE = ( Pathname.new( $dir_containing_executable ) + exe_name ).to_s

raise "#{EXE_FILE} does not exist" unless File.exist?( EXE_FILE )

class LauncherTest < Test::Unit::TestCase

  def excecution_succeeded?( output, expected_output_pattern )
    assert $CHILD_STATUS.exitstatus == 0
    assert( output =~ expected_output_pattern )    
  end
  
  def test_version
    excecution_succeeded?( `#{EXE_FILE} -v`, /groovy/i ) 
  end

  def test_server_or_client_jvm
    excecution_succeeded?( `#{EXE_FILE} -server -e "println System.getProperty('java.vm.name')"`, /server/i )
    excecution_succeeded?( `#{EXE_FILE} -e "println System.getProperty('java.vm.name')"`,         /client/i ) 
  end

  def test_passing_jvm_parameter
    excecution_succeeded?( `#{EXE_FILE} -Xmx300m -e "println Runtime.runtime.maxMemory()"`, /\A3\d{8}\Z/ )
  end

  def test_launching_script
    tempfile = 'justatest.groovy'
    File.open( tempfile, 'w' ) { |f|
      f << "println 'hello ' + args[ 0 ]\n"
    }
    begin
      excecution_succeeded?( `#{EXE_FILE} #{tempfile} world`, /hello world/ )       
    ensure
      FileUtils.rm tempfile
    end
    
  end
  
end
