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
      s = name[ 4...(name.length) ] 
      WindowsRegistryAccess.new( :definition => s )
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

  attr_reader :main_key, :sub_key, :value_id
  
  def definition=( defi )
    defin = DynString.new( defi )
    first = defin.first
    last  = defin.last
    raise "the name of the main key must ATM be constant. If this is a problem, please file a change request" unless
      String === first 
    raise "main key could not be figured out from #{defi}" unless first =~ /\A\\\\([A-Z_]+)\\/
    @main_key = $1
    first = first[ (@main_key.size + 3)...(first.size) ]
    
    values = defin[ 1...(defin.size) ]
    values.collect! { |v|
      v.respond_to?( :size ) && v.respond_to?( :first ) && v.size == 1 && String === v.first ? v.first : v
    }
    values.insert( 0, first ) unless first.empty?
    
    indexer_start_index = values.index_of { |v| String === v && v.include?( ?[ ) }
    
    # look up the element in "values" that has the starting [. Split at that. Put the previous
    # parts into subkey parts. put the rest into value_id_parts ( remove the closing ] )    
    raise "registry value reference #{defi} does not contain proper indexer for querying a registry value" unless indexer_start_index 
          
    str = values[ indexer_start_index ]
    parts = str.split( '[' )
    raise "invalid registry entry #{defi} : two starting [s in #{str}" unless parts.size == 2
    
    values[ indexer_start_index ] = parts.first
    values.insert( indexer_start_index + 1, parts.last )

    last = values.last
    raise "registry value reference #{defi} does not contain proper indexer for querying a registry value" unless String === last && last[ last.size - 1 ] == ?]
    last = last.chop
    
    if last.empty?
      values.pop
    else
      values.last = last
    end
    
    delete_empties = lambda { |item| item.respond_to?(:size) && item.size == 0 }
    
    @sub_key  = values[ 0..indexer_start_index ].delete_if &delete_empties 
    @value_id = values[ (indexer_start_index + 1)...(values.size) ].delete_if &delete_empties
            
  end
  
  def to_s
    self.class.name + " -- \nmain key #{@main_key}\nsub key #{@sub_key}\nvalue_id #{@value_id}"
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
          if ( i + 1 <= definition.length - 1 ) # && [ ?/, ?$ ].include?( definition[ i + 1 ] ) 
            s << definition[ i + 1 ]
            i += 2
          else
            s << ?\\ if definition.escaped?( i )
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
                    if ch == ?$ && !definition.escaped?( j ) && j < definition.length - 1 && definition[ j + 1 ] == ?{
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
  
  def method_missing( sym, *args, &block )
    if @parts.respond_to?( sym )
      @parts.send( sym, *args, &block )
    else
      raise NoMethodError, "undefined method `#{sym}' for #{self}"
    end
  end

  def respond_to?( sym )
    super( sym ) || @parts.respond_to?( sym )
  end

  def to_s
    s = self.class.name + " -- "
    self.each_with_index { |element, idx| s << idx.to_s << ' : ' << element.to_s << "\n" }
    s
  end
  
end # class DynString

end # module jlaunchgenerator
