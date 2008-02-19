# Copyright Antti Karanta <Antti dot Karanta at iki dot fi>, all rights reserved.

dir = File.dirname( __FILE__ )
$: << dir

#TODO loop over all files here that are not this file and require them.
# Test if ruby is smart enough to skip this file even if it's not programmatically omitted.
#Dir.each_file( dir ) { |f| 
Dir.foreach( dir ) { |file| 
  require( file.chomp( '.rb') ) if file =~ /\.rb\Z/ && File.basename( file ) != File.basename( __FILE__ )
  
}