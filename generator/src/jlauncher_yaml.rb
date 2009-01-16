# Copyright Antti Karanta <Antti dot Karanta at hornankuusi dot fi>, all rights reserved.

require 'yaml'
require 'jlauncher'


module Jlaunchgenerator::YAMLHandling

  include Jlaunchgenerator

  # returns a list of Executables as defined in the given yaml file
  def Jlaunchgenerator.load_yaml_file( yaml_file )
    
    rawdata = YAML.load_file( yaml_file )  
    
    executables = []
    
    # read the yaml into more usable form
    rawdata.each_pair { | execname, execdata |
    
      exec = Executable.new( :name => execname )
      
      YAMLHandling.handle_variable_definitions( exec, execdata )
          
      general = execdata[ 'general' ]
      raise "part 'general' missing for executable #{exec.name} in the spec yaml file " + yaml_file unless general
      
      exec.application_home = general[ 'application home' ]
      
      exec.main_class = general[ 'main class' ]
      
      executables << exec
    }
  
    return executables
    
  end

  
  def self.handle_variable_definitions( exec, execdata )
    variables = execdata[ 'variables' ]
    if variables
      exec.variables = variables.collect do |varname, data|
        v = Variable.new data
        v.name = varname
        v
      end
    end
  end

  
end # Jlaunchgenerator::YAMLHandling
