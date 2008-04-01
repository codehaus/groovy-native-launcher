# Copyright Antti Karanta <Antti dot Karanta at hornankuusi dot fi>, all rights reserved.

# THIS CODE IS STILL VERY EXPERIMENTAL AND FAR FROM DOING ANYTHING USEFUL

# A program for generation of c sources and associated build and resource files
# for a native program to launch a java application.

$: << File.dirname( __FILE__ ) + '/generator/src'

require 'jlauncher'

require 'yaml'


if ARGV.size == 0 || ARGV.size > 2 || !( ARGV & [ '-h', '--help', '-?' ] ).empty?
  puts( "usage: #{File.basename( __FILE__, '.*' )} <input yaml file> (<target dir>)" )
  puts( "if tharget dir is not provided, output is directed to the same folder as the input file" )
  exit
end

yaml_file = ARGV[ 0 ]

raise "input file #{yaml_file} does not exist" unless File.exists?( yaml_file )

output_dir = ( ARGV.size == 2 ) ? ARGV[ 1 ] : File.dirname( yaml_file )


execs = Jlaunchgenerator.load_file( yaml_file )

execs.each { |exec|
  exec.generate_c_source( output_dir )
}

puts "done!"
