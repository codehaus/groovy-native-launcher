//  A library for easy creation of a native launcher for Java applications.
//
//  Copyright (c) 2006 Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi) 
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the License is
//  distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
//  implied. See the License for the specific language governing permissions and limitations under the
//  License.
//
//  Author:  Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi) 

#if defined ( _WIN32 ) && defined ( _cwcompat )

#  if defined( __cplusplus )
    extern "C" {
#  endif

  typedef void ( *cygwin_initfunc_type)() ;
  typedef void ( *cygwin_conversionfunc_type)( const char*, char* ) ;
  
  extern cygwin_initfunc_type       cygwin_initfunc            ;
  extern cygwin_conversionfunc_type cygwin_posix2win_path      ;
  extern cygwin_conversionfunc_type cygwin_posix2win_path_list ;

// if cygwin is loaded, the above pointers are != NULL, otherwise they are NULL
#define CYGWIN_LOADED cygwin_posix2win_path
  
  /** returns > 0 iff cygwin could be loaded. Note that it's not usually an error to be unable to load
   * cygwin1.dll - that just means the code is not executing in cygwin shell and can proceed w/out doing any
   * conversions. Returns 0 if cygwin1.dll could not be loaded. Returns < 0 on error. */
  int jst_cygwinInit() ;  
  
  void jst_cygwinRelease() ;
  
#  if defined( __cplusplus )
     }
#  endif

#endif

