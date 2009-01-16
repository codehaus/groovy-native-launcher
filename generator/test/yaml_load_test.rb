# Copyright Antti Karanta <Antti dot Karanta at hornankuusi dot fi>, all rights reserved.

$: << File.dirname( __FILE__ ) + '/../src'

require 'test/unit'
require 'jlauncher'
require 'jlauncher_yaml'

class YamlLoadTest < Test::Unit::TestCase

  def setup
    @execs = Jlaunchgenerator.load_yaml_file( File.dirname( __FILE__ ) + '/groovylauncher.yaml' )
    assert @execs
    @sampleapp = @execs[ 0 ]
  end
  
  def test_exec_name    
    assert 'xgroovy', @sampleapp.name
  end
  
  def test_general_variable_definitions
    
    vars = @sampleapp.variables
    assert_instance_of Array, vars
    assert_equal 1, vars.size
    var1 = vars[ 0 ]
    assert_equal 'conf', var1.name
    assert var1.cygwin_conversion
    
    values = var1.value_alternatives
    assert_equal 3, values.size
    
    assert_instance_of Jlaunchgenerator::InputParamAccess, values[ 0 ][ 0 ]
    assert_instance_of Jlaunchgenerator::EnvVarAccess,     values[ 1 ][ 0 ]
    assert_instance_of Jlaunchgenerator::VariableAccess,   values[ 2 ][ 0 ]
    assert_instance_of Jlaunchgenerator::FileSeparator,    values[ 2 ][ 1 ]
    
  end

  def test_apphome
    
    homes = @sampleapp.application_home
    assert_equal 3, homes.size
    
  end

  def test_main_class
    main_class_ds = @sampleapp.main_class
    assert_equal 1, main_class_ds.size
    assert_equal 'org.codehaus.groovy.tools.GroovyStarter', main_class_ds[ 0 ] 
  end

end

