# Copyright Antti Karanta <Antti dot Karanta at hornankuusi dot fi>, all rights reserved.

module JLauncherUtils

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

end # module JLauncherUtils

class Array
  
  # returns the first index for which the given block (mandatory) returns true (meaning not nil nor false)
  # nil if none found
  def index_of &block 
    raise "block must be given" unless block_given?
    max = self.size
    index = 
    for i in 0...max
      val = yield self[ i ]
      break i if val 
    end
    index.respond_to?( :to_int ) ? index : nil
  end

  def last=( value )
    if self.empty?
      self << value
    else
      self[ self.size - 1 ] = value
    end
  end

end 
  
class String

  # a helper method telling whether the character at ind in str is escaped by \
  def escaped?( ind )
    return false if ind == 0 || ind >= self.size
    count = 0 # the number of \s
    i = ind - 1
    while i >= 0
      break if ( self[ i ] != ?\\ ) 
      count += 1
      i -= 1
    end
    return false if count == 0
    ( count % 2 ) != 0
  end
  
  
end   

module Kernel
  
  # defines a setter that can take a single string or a list of strings that are turned into ValueEvaluators 
  def value_evaluator_accessor( *names )
    names.each { |name|
      attr_reader name
      define_method( "#{name}=" ) { |values|
      # FIXME - these aren't necessarily dynstrings, they may be hashes, in which case ValueEvaluator is more appropriate
      # TODO: check unit tests for dynstring that they handle creating valueevaluators correctly
        vals = Array === values ? values.collect { |v| Jlaunchgenerator::ValueEvaluator.create( v ) } : Jlaunchgenerator::ValueEvaluator.create( values )
        instance_variable_set( "@#{name}", vals )
      }
    }
  end
  
end
