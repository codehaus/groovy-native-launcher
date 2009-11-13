#!/usr/bin/env python

# before this gets integrated into the build, you need to build it like this:
# swig -Wall -python -Isource -I%JAVA_HOME%\include -I%JAVA_HOME%\include\win32 -outdir . -o tests/nativelauncher_wrap.c source/nativelauncher.i
# python setup.py build_ext --inplace

# TODO: there may be separate issues in 64-bit compilation, 
#       see swig documentation 30.2.7 "Compiling for 64-bit platforms"
#       if there are problems

"""
setup.py file for SWIG example
"""

from distutils.core import setup, Extension
from glob import glob

import sys
import os

sys.path.append( 'tests' )

from supportModule import findCFilesWithoutMain

testCSources = findCFilesWithoutMain( 'source' )

for f in glob( 'tests/*.c' ) :
    testCSources.append( f )

includeDirs = [ 'source' ]

javaHome = os.environ[ 'JAVA_HOME' ]

javaInclude = os.path.join( javaHome,'include' )

includeDirs.append( javaInclude )

for f in os.listdir( javaInclude ) :
    f = os.path.join( javaInclude, f )
    if os.path.isdir( f ) :
        includeDirs.append( f )

libs = []

if os.name == 'nt' :
    libs.append( 'advapi32' )
else :
    libs.append( 'dl' )

nlauncher_module = Extension( '_nativelauncher',
                              sources = testCSources,
                              include_dirs = includeDirs,
                              libraries = libs,                              
                            )

setup ( name = 'nativelauncher',
        version = '1.0',
        author = "Antti Karanta",
        description = """Exposing native launcher internal c functions for unit testing in python""",
        ext_modules = [ nlauncher_module ],
        py_modules = [ "nativelauncher" ],
       )
       
