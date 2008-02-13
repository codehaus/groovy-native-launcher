# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

require 'jlauncher_utils'
require 'singleton'

module Jlaunchgenerator

# returns a list of Executables as defined in the given yaml file
def Jlaunchgenerator.load_file( yaml_file )
  rawdata = File.open( yaml_file ) { |f|
    YAML::load( f )  
  }
  
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

  return executables
end


# represents an executable we are generating sources for
class Executable  
  include JLauncherUtils::EasilyInitializable
  
  attr_reader :name
  attr_accessor :variables
  
  def application_home=( value )
    @apphome_alternatives = 
    ( ( Array === value ) ? value : [ value ] ).collect { |v| ValueEvaluator.create( v ) }
  end
  
  def generate_c_source( dir, filename = self.name + '.c' )
    # TODO
    puts "generating " + dir + '/' + filename
  end
  
end

class Variable
  include JLauncherUtils::EasilyInitializable
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
        actual_value = value[ 'value' ] 
        actual_val_d = DynString.new( actual_value ) if actual_value
        platform     = value[ 'platform' ]
        comp_def     = value[ 'compiler define' ]
        relative_loc = value[ 'relative to executable location' ]
        if platform
          
        elsif comp_def
          
        elsif relative_loc
          PathRelativeToExecutableLocation.new( relative_loc )
        else
          raise "do not know how to handle evaluation for hash #{value.inspect}"
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
  include JLauncherUtils::EasilyInitializable
  attr_accessor :name
  
  def VariableAccess.create( name )
    if name =~ /\A[A-Z_]+\Z/
      EnvVarAccess.new( :name => name )
    elsif name =~ /\A-/
      InputParamAccess.new( :name => name )
    elsif name =~ /\Aprepdef:/
      PreProcessorDefineAccess.new( :name => name[ 8...(name.length) ] )
    elsif name =~ /\Areg:/
      ds = DynString.new( name[ 4...(name.length) ] ) 
      WindowsRegistryAccess.new( :definition => ds )
    else
      realname = ( name =~ /\Avar:/ ) ? name[ 4...(name.length) ] : name
      VariableAccess.new( :name => realname )
    end
  end
  
  def to_s
    "#{self.class.name}: #{self.name}"
  end
  
end

class PathRelativeToExecutableLocation
  attr_accessor :path
  
  def initialize( path )
    @path = path
  end
  
  def to_s
    "executable_location/" + @path
  end
  
end

class EnvVarAccess < VariableAccess
  
end

class PreProcessorDefineAccess < VariableAccess
  
end

class InputParamAccess < VariableAccess
  
end

class WindowsRegistryAccess < VariableAccess
  def definition=( defin )
    # TODO
    
  end
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
    raise "bug: illegal param type #{definition.class.name}" unless definition.respond_to? :to_str    
    definition = definition.to_str
    parts = []
    if definition.size > 0
      i = 0
      s = ''
      
      while i < definition.length
        
        c = definition[ i ]
        if c == ?\\ 
          if ( i + 1 <= definition.length - 1 ) && [ ?/, ?$ ].include?( definition[ i + 1 ] ) 
            s << definition[ i + 1 ]
            i += 2
          else
            s << ?\\ 
            i += 1
          end
          next
        end
        
        if [ ?/, ?$ ].include?( c )

          case c
            when ?$
              begin_index = i + 2
              if ( definition.length - 1 > begin_index ) && # this is not the last character 
                 ( definition[ i + 1 ] == ?{ ) # next char is {
                 
                end_index = 
                if ( definition.length - 1 > begin_index + 5 ) && ( definition[ begin_index..( begin_index + 3 ) ] == 'reg:' )
                  # handle registry entry
                  end_i = nil
                  # look for ending {
                  # take into account that registry entry values can be nested (one level) and contain any references
                  nesting = 0
                  for j in (begin_index + 6)...(definition.length)
                    ch = definition[ j ]
                    if ch == ?$ && !escaped?( definition, j ) && j < definition.length - 1 && definition[ j + 1 ] == ?{
                      nesting += 1
                      #raise "too deep nesting in " + definition if nesting > 1
                    elsif ch == ?}
                      if nesting > 0 && definition[ j - 1 ] != ?\\ 
                        nesting -= 1
                      else 
                        end_i = j
                        break
                      end
                    end
                  end
                  end_i
                else
                  definition.index( '}', begin_index )
                end
                raise "unterminated variable reference in #{definition}" unless end_index
                if s.length > 0
                  parts << s
                  s = ''
                end
                varref = definition[ begin_index...end_index ]
                
                parts << ValueEvaluator.create( varref )
                
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
  
  # a helper method telling whether the character at ind in str is escaped by \
  def escaped?( str, ind )
    c = str[ ind ]
    return false if ind == 0
    count = 0 # the number of \s
    i = ind - 1
    while i >= 0
      break if ( str[ i ] != ?\\ ) 
      count += 1
      i -= 1
    end
    return false if count == 0
    ( count % 2 ) == ( ( c == ?\\ ) ? 1 : 0 ) # odd number of \s to escape a \, even otherwise 
  end
  private :escaped?

  def each( &block )
    @parts.each( &block )
  end
  
  def size
    @parts.size
  end
  
  def []( index )
    @parts[ index ]
  end

  def to_s
    print self.class.name
    puts ': '
    self.each { |part| 
      print ' -- '
      print part.class.name 
      print ' : '
      puts part 
    }
  end
  
end # class DynString

end # module jlaunchgenerator
