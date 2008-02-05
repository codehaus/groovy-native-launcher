# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

# THIS CODE IS STILL VERY EXPERIMENTAL AND FAR FROM DOING ANYTHING USEFUL

# A program for generation of c sources and associated build and resource files
# for a native program to launch a java application.

require 'yaml'
require 'singleton'


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

# provides a constructor that accepts a hash containing string/symbol => value pairs
# that are used for setting object properties.
module EasilyInitializable
  def initialize( sym_to_value_hash = nil )
    # puts "initializing w/ " + sym_to_value_hash.to_s 
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
  
# represents an executable we are generating sources for
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

# represents a segment of c code (possibly enclosed in a block) that 
# evaluates to a (char*) value
class ValueEvaluator

  def ValueEvaluator.create( value )
    case value
      when String
        VariableAccess.create( value )
      when Hash
        platform = value[ 'platform' ]
        comp_def = value[ 'compiler define' ]
        if platform
          
        elsif comp_def
          raise "do not know how to handle evaluation for hash #{value}"
        end
      else
        raise "do not know how to handle evaluation of an object of class #{value.class}"
    end
  end
  
end

# represents a c code section that is only present on a certain platform, i.e.
# separated by an appropriate preprocessor if, e.g. #if defined( _WIN32 ) || defined( __APPLE__ )
class PlatformDependentValueEvaluator
  WINDOWS = '_WIN32'.freeze
  OSX     = '__APPLE__'.freeze
  LINUX   = '__linux__'.freeze
  SOLARIS = '__sun__'.freeze
  
  attr_accessor :platforms

  def platform=( p )
    self.platforms = [ p ]
  end
  
  # the evaluator this evaluator wraps (w/ platform dependence)
  attr_accessor :evaluator
  
end

class VariableAccess < ValueEvaluator
  include EasilyInitializable
  attr_accessor :name
  
  def VariableAccess.create( name )
    if name =~ /\A[A-Z_]+\Z/
      EnvVarAccess.new( :name => name )
    elsif name =~ /\A-/
      InputParamAccess.new( :name => name )
    elsif name =~ /\Aprepdef:/
      PreProcessorDefineAccess.new( :name => name[ 8...(name.length) ] )
    elsif name =~ /\Areg:/
      #TODO
    else
      realname = ( name =~ /\Avar:/ ) ? name[ 4...(name.length) ] : name
      VariableAccess.new( :name => realname )
    end
  end
  
  def to_s
    "#{self.class.name}: #{self.name}"
  end
  
end

class EnvVarAccess < VariableAccess
  
end

class PreProcessorDefineAccess < VariableAccess
  
end

class InputParamAccess < VariableAccess
  
end

class FileSeparator
  include Singleton
  def to_s
    'FILE_SEP'
  end
end

# represents a string constructed in a c program from parts
# Note that escaping is a little funny to make it more convenient to write the strings.
# The catch is that \ only escapes / and $, not \. Thus is is not possible to have a literal string
# \/. This should not be a problem. If it turns out to be one, we can add support, although for
# backwards compatibility (if needed at that stage) it may need to be a special hash key.
class DynString
  def initialize( definition )
    parts = []
    if definition.size > 0
      i = 0
      s = ''
      # TODO: take into account case where definition is an empty string
      while i < definition.length
        c = definition[ i ]
        if c == ?\ 
          if ( i + 1 <= definition.length - 1 ) && [ ?/, ?$ ].include?( definition[ i + 1 ] ) 
            s << definition[ i + 1 ]
            i += 2
          else
            s << ?\ 
            i += 1
          end
          next
        end
        
        if [ ?/, ?$ ].include?( c )

          case definition[ i ]
            when ?$
              begin_index = i + 2
              if ( definition.length - 1 > begin_index ) && ( definition[ i + 1 ] == ?{ )           
                end_index = 
                if definition.length - 1 > begin_index + 5 && definition[ begin_index..( begin_index + 4 ) ] = 'reg:'
                  # handle registry entry
                  # TODO: take into account that registry entry values can be nested
                else
                  definition.index( '}', begin_index )
                end
                raise "unterminated variable reference in #{definition}" unless end_index
                if s.length > 0
                  parts << s
                  s = ''
                end
                varref = definition[ begin_index...end_index ]
                
                parts << VariableAccess.create( varref )
                
                i = end_index + 1                  
              else
                s << definition[ i ]
                i += 1
              end
            when ?/
              if s.length > 0
                parts << s
                s = ''
              end
              parts << FileSeparator.instance
              i += 1
          end
          
        else
          s << definition[ i ]
          i += 1
        end
                
      end # while
      
      if s.length > 0
        # store the literal str 
        parts << s
      end
    else # in case we have just an empty string as input
      parts << ''
    end

    @parts = parts
    #puts "analysoitu " + definition
    #parts.each { |p| puts "  " + p.to_s }
  end # initialize()
end # class DynString

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
