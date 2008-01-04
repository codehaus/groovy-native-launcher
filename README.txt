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


TODO
----

 * According to a compiler define, the home folder for groovy is either a hard coded location (well known location) or it is looked up
   dynamically. Note that in certain cases dynamic location lookup can pose a security hole.
 * refactor parameter classification into separate functions, possibly a separate source file
   options:
   - value into system property
   - value to classpath (a sysprop, yes, but needs to be handled separaterly)
   - value used as java home
   - map to another launchee param (single or double)
   - optional: if not set, 
     * use the value of an env var
     * use hard coded default (can refer to install dir by ${app_home}.
   * what about the other way around, i.e. specifying that a sys prop / launchee param is to
     be created using a param value / env var value / hard coded default value?
 * cygwin support
   * after the basics are working, make this generic (so that a param can be designated to have a value that must be cygwinized)
 * add the possibility to define "recursive jar dirs", i.e. directories where jars are searched for recursively.
 * add a check that the java home found can actually be used (i.e. it is valid). ATM the first java home is used; in some
   cases it may not be valid (e.g. if there are stale win registry entries, if the java executable found on path
   is not contained in a jdk/jre (even after following the symlinks). This can happen if the executable is a hard link.
 * the terminating suffixes is really only necessary in case where the name of the input file begins w/ "-". 
   Maybe it is such a special case that it can be ignored? In that case the terminating suffixes part could be
   removed.
 * add an option to restrict which vendor's java implementations are used
 * add an option to restrict the used jre/jdk version to be exactly something, greater than something, between some values etc.
   Have a look at how eclipse plugins define the required version of their dependant plugins in their manifest.mf
   A problem that needs to be solved: how to reliably tell the version of a java impl w/out actually loading and starting the jvm?
 * see if there is a standard how the jvm aliases are stored in some file in the jdk. If so, it is possible to remove the hard coded
   paths to client / server jvms and support aliases for them
