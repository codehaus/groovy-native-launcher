# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

$: << File.basename( __FILE__, '.*' ) + '/../src'

require 'jlauncher'

require 'test/unit'

include Jlaunchgenerator

class ParsingTest < Test::Unit::TestCase

  def test_env_var
    ds = DynString.new( "${FOOBAR_HOME}" )
    assert_equal( 1, ds.size )
    ev = ds[ 0 ]
    assert_instance_of( EnvVarAccess, ev )
    assert_equal( 'FOOBAR_HOME', ev.name )
    
    ds = DynString.new( "foo/${HOME}" ) 
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
    ds = DynString.new( 'foo\/bar' )
    assert_equal( 1, ds.size )
    assert_equal( 'foo/bar', ds[ 0 ] )
  end
  
  def test_registry_access
    ds = DynString.new( "${reg:\\\\HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Development Kit\\${reg:\\\\HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Development Kit[CurrentVersion]}[JavaHome]}" )
    assert_equal( 1, ds.size )
    ra = ds[ 0 ]
    assert_instance_of( WindowsRegistryAccess, ra )
    
    assert_equal( 'HKEY_LOCAL_MACHINE', ra.main_key )
    assert_equal( 'JavaHome', ra.value_id )
    ds2 = ra.subkey
    assert_equal( 2, ds2.size )
    assert_equals( "SOFTWARE\\JavaSoft\\Java Development Kit\\", ds2[ 0 ] )
    ra2 = ds2[ 1 ]
    assert_instance_of( WindowsRegistryAccess, ra2 )
    assert_equals( 'HKEY_LOCAL_MACHINE', ra2.main_key )
    
  end
  
end
