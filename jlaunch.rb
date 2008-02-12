# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

# THIS CODE IS STILL VERY EXPERIMENTAL AND FAR FROM DOING ANYTHING USEFUL

# A program for generation of c sources and associated build and resource files
# for a native program to launch a java application.

$: << 'generator_src'

require 'jlauncher_utils'

require 'yaml'


if ARGV.size == 0 || ARGV[ 0 ] == '-h' || ARGV[ 0 ] == '--help'
  puts( "usage: #{File.basename( __FILE__, '.*' )} <input yaml file>" )
  exit
end

yaml_file = ARGV[ 0 ]

raise "input file #{yaml_file} does not exist" unless File.exists?( yaml_file )

rawdata = File.open( yaml_file ) { |f|
  YAML::load( f )  
}


include Jlaunchgenerator

executables = []

# read the yaml into more usable form
rawdata.each_pair { | execname, execdata |

  exec = Executable.new( :name => execname )
  executables << exec
  
  variables = execdata[ 'variables' ]
  if variables
    exec.variables = variables.collect { |varname, data|
      v = Variable.new( data )
      v.name = varname
    }
  end

  general = execdata[ 'general' ]
  raise "part 'general' missing for executable #{exec.name} in the spec yaml file " + yaml_file unless general
  exec.init_attrs( general )
}

puts "done!"
