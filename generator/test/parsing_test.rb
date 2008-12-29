# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

$: << File.dirname( __FILE__ ) + '/../src'

require 'jlauncher'

require 'test/unit'

# to see the build and run environment
#require 'rbconfig'    
#Config::CONFIG.each_pair { |k,v| puts "#{k}: #{v}" }


include Jlaunchgenerator

class ParsingTest < Test::Unit::TestCase

  def test_env_var
    ds = DynString.parse( "${FOOBAR_HOME}" )
    assert_equal( 1, ds.size )
    ev = ds[ 0 ]
    assert_instance_of( EnvVarAccess, ev )
    assert_equal( 'FOOBAR_HOME', ev.name )
    
    ds = DynString.parse( "foo/${HOME}" ) 
    assert_equal( 3, ds.size )
    s  = ds[ 0 ]
    fs = ds[ 1 ]
    ev = ds[ 2 ]
    assert_equal( 'foo', s )
    assert_same( FileSeparator.instance, fs )
    assert_instance_of( EnvVarAccess, ev )
    assert_equal( 'HOME', ev.name )
    
  end
  
  def test_escaping
    ds = DynString.parse( 'foo\/bar' )
    assert_equal( 1, ds.size )
    assert_equal( 'foo/bar', ds[ 0 ] )
  end
  
  def test_basic_registry_access
    # basic registry access
    ds = DynString.parse( "${reg:\\\\\\\\HKEY_LOCAL_MACHINE\\\\SOFTWARE\\\\JavaSoft\\\\Java Development Kit[JavaHome]}" )
    assert_equal 1, ds.size
    ra = ds.first
    assert_instance_of( WindowsRegistryAccess, ra )
    assert_equal( 'HKEY_LOCAL_MACHINE', ra.main_key )
    value_id = ra.value_id
    assert_equal( 'JavaHome', value_id.first )
    assert_equal( 1, value_id.size )
    assert_equal( "SOFTWARE\\JavaSoft\\Java Development Kit", ra.sub_key.first )
    assert_equal( 1, ra.sub_key.size )
    
  end
  
  def test_nested_registry_access

    # yeah, you can get migraine with all those backslashes. This is why it looks so horrible:
    # we need to get a string that has the escaping \s in it. Thus, in order to get literal \\ (escaped \)
    # we have to have \\\\ in the string below, as ruby eats half of them.
    # In other words, ruby considers them as escape chars, as does DynString parsing.
    ds = DynString.parse( "${reg:\\\\\\\\HKEY_LOCAL_MACHINE\\\\SOFTWARE\\\\JavaSoft\\\\Java Development Kit\\\\${reg:\\\\\\\\HKEY_LOCAL_MACHINE\\\\SOFTWARE\\\\JavaSoft\\\\Java Development Kit[CurrentVersion]}[JavaHome]}" )
    
    assert_equal( 1, ds.size )
    ra = ds[ 0 ]
    assert_instance_of( WindowsRegistryAccess, ra )
    
    assert_equal( 'HKEY_LOCAL_MACHINE', ra.main_key )
    value_id = ra.value_id
    assert_equal( 'JavaHome', value_id.first )
    ds2 = ra.sub_key
    assert_equal( 2, ds2.size )
    assert_equal( "SOFTWARE\\JavaSoft\\Java Development Kit\\", ds2[ 0 ] )
    ra2 = ds2[ 1 ]
    assert_instance_of( WindowsRegistryAccess, ra2 )
    assert_equal( 'HKEY_LOCAL_MACHINE', ra2.main_key )
    assert_equal( 'CurrentVersion', ra2.value_id.first )
    assert_equal( 1, ra2.value_id.size )
  end
  
  def test_dyn_string_creation
    ds = DynString.parse( "\\\\" )
    assert_equal( 1, ds.size )
    s = ds.first
    assert_instance_of( String, s )
    assert_equal( 1, s.size )
    assert_equal( ?\\, s[ 0 ] )
  end

  def no_test_compiler_define
    definition = { 
      'preprocessor filter' => '!windows',
      'value' => "${prepdef:GROOVY_HOME}"
    }
    o = ValueEvaluator.create( definition )
    assert_instance_of( PreprocessorFilteredValueEvaluator, o )
    assert_equal( "!defined( _WIN32 )", o.filter )
    # TODO
    v = o.value
    assert_instance( TODO, o )
  end
  
end

