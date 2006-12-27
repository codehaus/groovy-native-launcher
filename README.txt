$Revision$
$Date$

Author:  Antti Karanta (Antti dot Karanta (at) iki dot fi) 

 * This is still work in progress - there are some bugs, support for some oses is lacking etc. Feel free to contribute.

 * To port to other os:es you need to do some ifdefs for: 
   - the func contains platform specific calls to load a dynamic library and find a method from therein
   - define path separator and file separator for the platform
   - provide the paths where the jvm dyn lib file resides on that platform

 TODO:
 - test more
 - find out path to jvm dyn lib on os-x
 - find out how to figure out where the current executable lives on os-x



