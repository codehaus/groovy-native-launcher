
To compile on windows:

cl /Dwin /I%JAVA_HOME%\inlude /I%JAVA_HOME%\inlude\win32 groovy.c jvmstarter.c

NOTES:

 * On windows you need to get dirent.h from somewhere
  (I suppose cygwin and such provide it?) or 
   e.g. from http:www.uku.fi/~tronkko/dirent.h

 * To port to other os:es you need to do some ifdefs for: 
   - the func contains platform specific calls to load a dynamic library and find a method from therein
   - define path separator and file separator for the platform

 TODO::
 - test more
 - try at least on win, lnx, sls, osx
 - find out why long dir names / spaces in dirnames make dll loading fail on win
 - release the dyn lib reference once the jvm is turned off
 - correct classpath handling


Author:  Antti Karanta (Antti dot Karanta (at) iki dot fi) 
$Revision$
$Date$
