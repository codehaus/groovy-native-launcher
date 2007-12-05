$Revision$
$Date$

Author:  Antti Karanta (Antti dot Karanta (at) iki dot fi) 


What it is
----------

This is a native launcher for groovy, i.e. here you can find c sources and
build files to build a native executable to launch groovy scripts, much the 
same way java binary is used to launch java programs.


Design principles
-----------------

For maximum portability, the source is written in ANSI-C using POSIX functions. Anything
that can be done using these is done so without using external libraries / extensions.

E.g. alloca function has not been used for memory handling as it is (unfortunately) 
not in POSIX standard and memory handling can be done using malloc and friends 
(albeit on the heap, not on the stack).

The launcher has been built in such a way that only groovy.c has groovy dependent stuff.
It is simple to use the other source files to build a native laucher for any
java program.


Licencing
---------

The sources are provided with Apache 2 licence as that was thought to be liberal 
enough so that these can be used for any purpose commercial or open source. If
you are for some reason restricted by that licence, please contact the author and
we'll work it out - the purpose is for these sources to be as widely usable as possible.

Porting to currently unsupported operating systems
--------------------------------------------------

Currently the implementation of the following things are platform dependent:

  * location and name of jvm dynamic library (jvmstarter.c)
  * how a dynamic library is loaded. If your system has dlfcn.h (like linux, solaris and os-x),
    this works out of the box
  * directory handling (jst_fileutils.c). If your system has dirent.h this works out of the box.
  * getting the location of the current process' executable (jst_fileutils.c)
  

More information
----------------

See http://docs.codehaus.org/display/GROOVY/Native+Launcher
