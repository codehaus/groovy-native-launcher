# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

# THIS CODE IS STILL VERY EXPERIMENTAL AND FAR FROM DOING ANYTHING USEFUL

# A program for generation of c sources and associated build and resource files
# for a native program to launch a java application.

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

module Jlaunchgenerator

module EasilyInitializable
  def initialize( sym_to_value_hash = nil )
    puts "initializing w/ " + sym_to_value_hash.to_s 
    self.init_attrs( sym_to_value_hash )
  end
  def init_attrs( sym_to_value_hash = nil )
    if sym_to_value_hash
      sym_to_value_hash.each_pair { |k,v|
        setter_name = k.to_s.gsub( /\s/, '_' ) + '='
        if self.respond_to?( setter_name )
          self.send( setter_name, v ) 
        else
          self.instance_variable_set( "@" + setter_name.chomp( '=' )  , v )
        end
      }
    end
  end
end
  
class Executable
  attr_reader :name
  attr_accessor :variables
  def initialize( name )
    @name = name
  end
end

class Variable
  include EasilyInitializable
  attr_accessor :name, :cygwin_conversion, :value_alternatives
  def value=( val )
    self.value_alternatives = [ val ]
  end
  def value_alternatives=( values )
    @value_alternatives = values.collect { |v| DynString.new( v ) }
  end
end

class VariableAccess
  include EasilyInitializable
  attr_accessor :name
  
  def VariableAccess.create( name )
    if name =~ /\A[A-Z_]+\Z/
      EnvVarAccess.new( name )
    elsif name =~ /\A-/
      InputParamAccess.new( name )
    else
      VariableAccess.new( name )
    end
  end
end

class EnvVarAccess < VariableAccess
  
end

class InputParamAccess < VariableAccess
  
end

class DynString
  def initialize( definition )
    @parts = []
    i = 0 
    while i < definition.length
      dollar_index = definition.index( '${', i )
      if dollar_index
        end_index = definition.index( '}', dollar_index )
        raise "unterminated variable reference in #{definition}" unless end_index
        varref = definition[ dollar_index + 2, end_index - 2 ]
        # TODO
        puts varref
        i = end_index + 1
      else
        break 
      end
    end
  end
end

end # module jlaunchgenerator

include Jlaunchgenerator

executables = []

# read the yaml into more usable form
rawdata.each_pair { | execname, execdata |

  exec = Executable.new( execname )
  executables << exec
  
  variables = execdata[ 'variables' ]
  if variables
    exec.variables = variables.collect { |varname, data|
      v = Variable.new( data )
      v.name = varname
    }
  end
}

puts "done."
