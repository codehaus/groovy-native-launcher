# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

$: << File.basename( __FILE__, '.*' ) + '/..'

require 'test/unit'

class YamlLoadTest < Test::Unit::TestCase
  
  def test_foo
    assert(true)
  end
  
end