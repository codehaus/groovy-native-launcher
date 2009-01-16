# Copyright Antti Karanta <Antti dot Karanta at hornankuusi dot fi>, all rights reserved.

# TODO: rename to Jaijutsu or Jaido, mimicking http://en.wikipedia.org/wiki/Iaijutsu
#       See also http://en.wikipedia.org/wiki/Battōjutsu and
#       http://en.wikipedia.org/wiki/Iaidō   
#       Jaijutsu seems more appropriate as Iaijursu is solely concerned w/ drawing the sword.

require 'jlauncher_utils'
require 'singleton'

module Jlaunchgenerator


# represents an executable we are generating sources for
class Executable  
  include JLauncherUtils::EasilyInitializable
  
  attr_reader :name #, :apphome_alternatives
  attr_accessor :variables
  value_evaluator_accessor :application_home
  value_evaluator_accessor :main_class
  
#  def application_home=( value )
#    @apphome_alternatives = ValueEvaluator.strings_to_value_evaluators( value )
#  end
  
  def generate_c_source( dir, filename = self.name + '.c' )
    # TODO
    puts "generating " + dir + '/' + filename
  end
  
end

class Variable
  include JLauncherUtils::EasilyInitializable
  attr_accessor :name, :cygwin_conversion, :value_alternatives
  def value= val 
    self.value_alternatives = [ val ]
  end
  def value_alternatives=( values )
    @value_alternatives = values.collect { |v| DynString.parse( v ) }
  end
end

# represents a segment of c code (possibly enclosed in a block) that 
# evaluates to a (char*) value
class ValueEvaluator

  # value may be a single string or a list of strings
  # if message is provided, value may not be nil and message is included in the raised exception if it is
  def self.strings_to_value_evaluators( value, message = nil )
    raise message if message && !value
     ( ( Array === value ) ? value : [ value ] ).collect { |v| ValueEvaluator.create( v ) }
  end


  def self.create( value )
    case value
      when String
        DynString.parse( value )
      when Hash
        actual_value = value[ 'value' ] 
        actual_val_d = DynString.parse( actual_value ) if actual_value
        prep_filter  = value[ 'preprocessor filter' ]
        relative_loc = value[ 'relative to executable location' ]
        if prep_filter
          v = value.dup
          v.delete( 'preprocessor filter' )
          inner = ValueEvaluator.create( v ) 
          PreprocessorFilteredValueEvaluator.new( :filter => prep_filter, :evaluator => inner )
        elsif relative_loc
          PathRelativeToExecutableLocation.new( relative_loc )
        elsif actual_value
          DynString.parse( actual_value )
        else
          raise "do not know how to handle evaluation for hash #{value.inspect}"
        end
      else
        raise "do not know how to handle evaluation of an object of class #{value.class}"
    end
  end
  
end

# represents a c code section that is only present depending on a preprocessor filter, i.e.
# separated by an appropriate preprocessor if, e.g. #if defined( _WIN32 ) || defined( __APPLE__ )
class PreprocessorFilteredValueEvaluator
  WINDOWS = '_WIN32'.freeze
  OSX     = '__APPLE__'.freeze
  LINUX   = '__linux__'.freeze
  SOLARIS = '__sun__'.freeze
  
  NAME2DEFINE = {
    'windows' => WINDOWS,
    'osx'     => OSX,
    'linux'   => LINUX,
    'solaris' => SOLARIS
  }

  include JLauncherUtils::EasilyInitializable
  
  attr_reader :filter

  def filter=( p )
    s = p.dup
    NAME2DEFINE.each_pair { |key,value| s.gsub!( key, "defined( #{value} )" ) }
    @filter = s
  end
  
  # the evaluator this evaluator wraps (w/ platform dependence)
  attr_accessor :evaluator
  
end

class VariableAccess < ValueEvaluator
  include JLauncherUtils::EasilyInitializable
  attr_accessor :name
  
  def VariableAccess.create( id, value = nil )
    if id =~ /\A[A-Z_]+\Z/
      EnvVarAccess.new( :name => id )
    elsif id == 'env'
      EnvVarAccess.new( :name => value )
    elsif id =~ /\A-/
      InputParamAccess.new( :name => id )
    elsif id == 'prepdef'
      PreProcessorDefineAccess.new( :name => value )
    elsif id == 'reg'
      WindowsRegistryAccess.new( :definition => value )
    else
      VariableAccess.new( :name => id )
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
  
  def definition=( defin )

    first = defin.first
    last  = defin.last
    unless String === first
      #puts "\n eka " + first.to_s
      #puts "\n toka " + last.to_s
      raise "the name of the main key must ATM be constant. If this is a problem, please file a change request. Expression #{defin}"
    end
       
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

class PathSeparator
  include Singleton
  def to_s
    'PATH_SEP'
  end
end


# represents a string constructed in a c program from parts
# Note that escaping is a little funny to make it more convenient to write the strings.
# The catch is that \ only escapes / and $, not \. Thus is is not possible to have a literal string
# \/. This should not be a problem. If it turns out to be one, we can add support, although for
# backwards compatibility (if needed at that stage) it may need to be a special hash key.
class DynString

  def initialize( parts )
    @parts = parts
  end
  

  def DynString.parse( definition )
    parts, index = parse_recursively( definition )
    raise "bug: string not parsed correctly, end index #{index}/#{definition.length}" unless index == definition.length
    
    DynString.new( parts )
  end

  # returns type, index where to continue parsing and whether the first returned value is
  # a type identifier (such as 'reg' or 'var') and there is thus a value part to parse, 
  # or whether it is a self sufficient value (e.g. 'MYAPP_HOME')
  def DynString.extract_dynstring_type( str, index )
    type = ''
    startindex = index
    has_following_value = false
    
    while true
      if str.length > index
        c = str[ index ]
        if c == ?: || c == ?}
          index += 1
          has_following_value = c == ?:
          break
        else
          #raise "illegal character #{c.chr} for identifier at #{index} in #{str}" unless ((?a)..(?z)) === c
          type << c
        end
      else 
        raise "out of bounds when looking for end of identifier at index #{startindex} in #{str}"
      end
      index += 1
    end
    
    raise "no type indentifier provided at #{index} in #{str}" if type.empty?
    
    [ type, index, has_following_value ]
  end
  
  def DynString.flush_parsed( parts, s )
    if s.length > 0
      parts << s
      ''
    else 
      s
    end    
  end
  
  # returns an array of evaluable parts and the index from which to continue parsing
  def DynString.parse_recursively( definition, startindex = 0, continue_to_end_of_string = true )
    raise "bug: illegal param type #{definition.class.name}" unless definition.respond_to? :to_str
    definition = definition.to_str

    parts = []
    i = startindex
    if definition.size > 0
      s = ''
      
      while i < definition.length
        
        c = definition[ i ]
        
        case c
          when ?\\
            if ( i + 1 <= definition.length - 1 ) 
              s << definition[ i + 1 ]
              i += 2
            else
              s << ?\\ if definition.escaped?( i )
              i += 1
            end
          when ?$
            if ( definition.length > i + 1 ) && # this is not the last character 
               ( definition[ i + 1 ] == ?{ ) # next char is {

              s = flush_parsed( parts, s )
              
              dstring_type, i, has_following_value = extract_dynstring_type( definition, i + 2 )
              
              parts << 
              if has_following_value
                innerparts, i = parse_recursively( definition, i, false )

                VariableAccess.create( dstring_type, DynString.new( innerparts ) )
              else
                VariableAccess.create( dstring_type )
              end
                                
            else
              s << definition[ i ]
              i += 1
            end
          when ?/
            s = flush_parsed( parts, s )
            parts << FileSeparator.instance
            i += 1
          when ?: 
            s = flush_parsed( parts, s )
            parts << PathSeparator.instance
            i += 1
          when ?}
            if continue_to_end_of_string
              raise "unmatched } at index #{i} in #{definition}"
            else 
              i += 1
              break
            end
          else
            s << definition[ i ]
            i += 1
        end
          
        raise "unterminated expression in #{definition}" if !continue_to_end_of_string && i >= definition.length
                          
      end # while
      
      if s.length > 0
        # store the literal str 
        parts << s
      end
    else # in case we have just an empty string as input
      parts << ''
    end

    #puts "analysoitu " + definition
    #parts.each { |p| puts "  " + p.to_s }
    [ parts, i ]
  end # parse_recursively
  
  class << self
    private :parse_recursively
  end
 
   
  
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
