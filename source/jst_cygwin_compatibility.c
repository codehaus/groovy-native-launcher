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


// Note that cygwin compatibility requires some stack manipulation in 
// the beginning of main and thus can not be extracted into a totally
// separate library.


#if defined( _WIN32 ) && defined( _cwcompat )

# include <Windows.h>

#include <assert.h>

# include "jvmstarter.h"
# include "jst_cygwin_compatibility.h"
# include "jst_dynmem.h"

#  if !defined( PATH_MAX )
#    define PATH_MAX MAX_PATH
#  endif


  cygwin_initfunc_type       cygwin_initfunc            = NULL ;
  // see http://cygwin.com/cygwin-api/func-cygwin-conv-to-win32-path.html
  cygwin_conversionfunc_type cygwin_posix2win_path      = NULL ;
  cygwin_conversionfunc_type cygwin_posix2win_path_list = NULL ;

  static HINSTANCE g_cygwinDllHandle = NULL ;
  
  int jst_cygwinInit() {
          
    g_cygwinDllHandle = LoadLibrary( "cygwin1.dll" ) ;
  
    if ( g_cygwinDllHandle ) {
      SetLastError( 0 ) ;
      cygwin_initfunc            = (cygwin_initfunc_type)      GetProcAddress( g_cygwinDllHandle, "cygwin_dll_init" ) ;
      // only proceed getting more proc addresses if the last call was successfull in order not to hide the original error
      if ( cygwin_initfunc       ) cygwin_posix2win_path      = (cygwin_conversionfunc_type)GetProcAddress( g_cygwinDllHandle, "cygwin_conv_to_win32_path"       ) ;
      if ( cygwin_posix2win_path ) cygwin_posix2win_path_list = (cygwin_conversionfunc_type)GetProcAddress( g_cygwinDllHandle, "cygwin_posix_to_win32_path_list" ) ;
      
      if ( !cygwin_initfunc || !cygwin_posix2win_path || !cygwin_posix2win_path_list ) {
        fprintf( stderr, "strange bug: could not find appropriate init and conversion functions inside cygwin1.dll\n" ) ;
        jst_printWinError( GetLastError() ) ;
        return -1 ;
      }
      cygwin_initfunc() ;
    } // if cygwin1.dll is not found, just carry on. It means we're not inside cygwin shell and don't need to care about cygwin path conversions
    
    return g_cygwinDllHandle ? 1 : 0 ;
  }

  extern void jst_cygwinRelease() {   
    if ( g_cygwinDllHandle ) {
      FreeLibrary( g_cygwinDllHandle ) ;
      g_cygwinDllHandle          = NULL ;
      cygwin_initfunc            = NULL ;
      cygwin_posix2win_path      = NULL ;
      cygwin_posix2win_path_list = NULL ;
      // TODO: maybe clean up the stack here (the cygwin stack buffer) ?
    }
  }

  
  /** Path conversion from cygwin format to win format. This func contains the general logic, and thus
   * requires a pointer to a cygwin conversion func. */
  static char* convertPath( cygwin_conversionfunc_type conversionFunc, char* path ) {
    char  temp[ MAX_PATH + 1 ] ;

    // FIXME
    
    if ( conversionFunc ) {
      conversionFunc( path, temp ) ;
    } else {
      strcpy( temp, path ) ;
    }
    
    return jst_strdup( temp ) ;
    
  }
  
  /** returns NULL on error, does not modify the param. Result must be freed by caller. */
  extern char* jst_convertCygwin2winPath( char* path ) {
    return convertPath( cygwin_posix2win_path, path ) ;  
  }
  
  
  
  /** see above */
  extern char* jst_convertCygwin2winPathList( char* path ) {
    return convertPath( cygwin_posix2win_path_list, path ) ;
  }

#endif

