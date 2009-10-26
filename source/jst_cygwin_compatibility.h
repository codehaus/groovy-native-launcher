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

#if defined ( _WIN32 ) && defined ( _cwcompat ) && !defined( JST_CYGIN_COMPATIBILITY_H )

#  define JST_CYGIN_COMPATIBILITY_H

#  if defined( __cplusplus )
    extern "C" {
#  endif

  typedef void ( *cygwin_initfunc_type)( void ) ;
  typedef void ( *cygwin_conversionfunc_type)( const char*, char* ) ;

  extern cygwin_initfunc_type       cygwin_initfunc            ;
  extern cygwin_conversionfunc_type cygwin_posix2win_path      ;
  extern cygwin_conversionfunc_type cygwin_posix2win_path_list ;

// if cygwin is loaded, the above pointers are != NULL, otherwise they are NULL
#define CYGWIN_LOADED cygwin_posix2win_path

  /** Returns the handle to cygwin dll if it is found and can be loaded, 0 otherwise.
   * This needs to be called before jst_cygwinInit */
  HINSTANCE jst_loadCygwinDll( void ) ;

  /** returns > 0 iff necessary cygwin functions could be loaded. Returns 0 if cygwin1.dll is not be loaded. Returns < 0 on error.
   * NOTE: call jst_loadCygwinDll before calling this function. */
  int jst_cygwinInit( void ) ;

  void jst_cygwinRelease( void ) ;

  int runCygwinCompatibly( int argc, char** argv, int (*mainproc)( int argc, char** argv ) ) ;

#  if defined( __cplusplus )
     }
#  endif

#endif

