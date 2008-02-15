# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

$: << File.basename( __FILE__, '.*' ) + '/../src'

require "test/unit"
require 'jlauncher_utils'

class ArrayExtensionTest < Test::Unit::TestCase
  
  def test_index_of
    a = [ 1, 2, 3 ]
    v = a.index_of { |x| x == 2 }
    assert_equal( 1, v )
    v = a.index_of { |x| x == 20 }
    assert_nil( v )
  end
  
end

class StringExtensionTest < Test::Unit::TestCase
  
  def test_escape
    s = "foo\\\\$bar\\x\\\$boo"
    assert s.escaped?( 4 )
    assert !s.escaped?( 3 )
    assert !s.escaped?( 5 )
    assert s.escaped?( 10 )
    assert s.escaped?( 12 )
  end
  
end

