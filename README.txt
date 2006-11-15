

 * This is still work in progress - there are some bugs, support for some oses is lacking etc. Feel free to contribute.

 * If you wish to use the jvmstarter.c to launch you own app and want to provide directories from which all jar
   files need to be included in the startup class path, on windows you need to pass -DDIRSUPPORT to the compiler and get 
   dirent.h from somewhere; cygwin seems to provide it, another one that works is 
   this one: http:www.uku.fi/~tronkko/dirent.h

 * To port to other os:es you need to do some ifdefs for: 
   - the func contains platform specific calls to load a dynamic library and find a method from therein
   - define path separator and file separator for the platform
   - provide the paths where the jvm dyn lib file resides on that platform

 TODO:
 - test more
 - find out path to jvm dyn lib on os-x



Author:  Antti Karanta (Antti dot Karanta (at) iki dot fi) 
$Revision$
$Date$
