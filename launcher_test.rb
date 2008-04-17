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

# you can give the dir containing the executable to test as a command line parameter, e.g.
# ruby launcher_test.rb build_rant_windows


require "test/unit"
require 'pathname'
require 'rbconfig'
require 'English'
require 'fileutils'

require 'osarch'


$dir_containing_executable ||= ARGV[ 0 ] || get_builddir()

raise "GROOVY_HOME must be defined to be able to run the tests" unless ENV[ 'GROOVY_HOME' ]

HOST_OS = Config::CONFIG[ 'host_os' ]


exe_name = 'groovy' + ( HOST_OS =~ /win/ && HOST_OS !~ /darwin/ ? '.exe' : '' )
EXE_FILE = ( Pathname.new( $dir_containing_executable ) + exe_name ).to_s

raise "#{EXE_FILE} does not exist" unless File.exist?( EXE_FILE )

puts "testing #{EXE_FILE}"

class LauncherTest < Test::Unit::TestCase

  def excecution_succeeded?( execstatement, expected_output_pattern )
    output = `#{execstatement}`
    assert_equal 0, $CHILD_STATUS.exitstatus
    assert_match( expected_output_pattern, output )    
  end
  
  def test_version
    excecution_succeeded?( "#{EXE_FILE} -v", /groovy/i ) 
  end

  def test_server_or_client_jvm
    excecution_succeeded?( "#{EXE_FILE} -server -e \"println System.getProperty('java.vm.name')\"", /server/i )
    excecution_succeeded?( "#{EXE_FILE} -e \"println System.getProperty('java.vm.name')\"",         /client/i ) 
  end

  def test_passing_jvm_parameter
    excecution_succeeded?( "#{EXE_FILE} -Xmx300m -e \"println Runtime.runtime.maxMemory()\"", /\A3\d{8}(?:\D|\Z)/ )
  end

  # the scriptfilename (the file passed to groovy for execution) is not the same as the file to write to disc on cygwin
  # (as cygwin ruby wants file name to open in cygwin format)
  def test_launching_script( scriptfile = 'justatest.groovy', filetowrite = scriptfile )
    
    File.open( filetowrite, 'w' ) { |f|
      f << "println 'hello ' + args[ 0 ]\n"
    }
    begin
      excecution_succeeded?( "#{EXE_FILE} #{scriptfile} world", /hello world/ )       
    ensure
      FileUtils.rm filetowrite
    end
    
  end
  
  def test_exit_status
    `#{EXE_FILE} -Xmx300m -e "System.exit( 123 )"`
    assert_equal 123, $CHILD_STATUS.exitstatus
  end

  if HOST_OS =~ /cygwin/
    def test_cygwin_compatibility
      # generate a groovy source file
      # get path to it in cygwin format
      # try to execute
      cygfilename = '/tmp/justatest.groovy'
      test_launching_script( cygfilename )
      
      # try to execute the same file w/ windows style path
      winfilename = `cygpath -w #{cygfilename}`.gsub( '\\', '/' ).strip
      test_launching_script( winfilename, cygfilename )

    end
  end
    
    
end
