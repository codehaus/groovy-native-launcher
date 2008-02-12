# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

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

