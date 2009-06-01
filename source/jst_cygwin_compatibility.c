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


  extern cygwin_initfunc_type       cygwin_initfunc            = NULL ;
  // see http://cygwin.com/cygwin-api/func-cygwin-conv-to-win32-path.html
  extern cygwin_conversionfunc_type cygwin_posix2win_path      = NULL ;
  extern cygwin_conversionfunc_type cygwin_posix2win_path_list = NULL ;

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

  // 2**15
  #  define PAD_SIZE 32768

  typedef struct {
    void* backup ;
    void* stackbase ;
    void* end ;
    unsigned char padding[ PAD_SIZE ] ; // sizeof( unsigned char ) == 1, i.e. a single byte
  } CygPadding ;

  static CygPadding *g_pad ;

  extern int runCygwinCompatibly( int argc, char** argv, int (*mainproc)( int argc, char** argv ) ) {


    // NOTE: This code is experimental and is not compiled into the executable by default.
    //       When building w/ rant, do
    //       scons -c
    //       if compiling in cygwin, cygwin support enabled is the default:
    //       scons
    //       if compiling in elsewhere (e.g. mingw), use
    //       scons cygwinsupport=True

    // Dynamically loading the cygwin dll is a lot more complicated than the loading of an ordinary dll. Please see
    // http://cygwin.com/faq/faq.programming.html#faq.programming.msvs-mingw
    // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/cygwin/how-cygtls-works.txt?rev=1.1&content-type=text/x-cvsweb-markup&cvsroot=uberbaum
    // "If you load cygwin1.dll dynamically from a non-cygwin application, it is
    // vital that the bottom CYGTLS_PADSIZE bytes of the stack are not in use
    // before you call cygwin_dll_init()."
    // See also
    // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/testsuite/winsup.api/cygload.cc?rev=1.1&content-type=text/x-cvsweb-markup&cvsroot=uberbaum
    // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/testsuite/winsup.api/cygload.h?rev=1.2&content-type=text/x-cvsweb-markup&cvsroot=uberbaum

    // these are static as the stack manipulation will mess them up if they are reserved on the stack
    // "pad" is of course on the stack as its sole purpose is to reserve the space needed by cygwin
    // on the stack
    static int mainRval ;
    static size_t delta ;
    CygPadding pad ;
    static void* sbase ;

    g_pad = &pad ;
    pad.end = pad.padding + PAD_SIZE ;

    // TODO: try this out:
    // .intel_syntax noprefix
    #if defined( __GNUC__ )
      __asm__ (
          "movl %%fs:4, %0"
          :"=r"( sbase )
        ) ;
    #else
      __asm {
          push eax
          mov eax, fs:[ 4 ]
          mov sbase, eax
          pop eax
      }
    #endif

    g_pad->stackbase = sbase ;

    delta = (size_t)g_pad->stackbase - (size_t)g_pad->end ;

    if ( delta ) {
      g_pad->backup = malloc( delta ) ;
      if( !( g_pad->backup) ) {
        fprintf( stderr, "error: out of mem when copying stack state\n" ) ;
        return -1 ;
      }
      memcpy( g_pad->backup, g_pad->end, delta ) ;
    }

    mainRval = mainproc( argc, argv ) ;

    // clean up the stack (is it necessary? we are exiting the program anyway...)
    if ( delta ) {
      memcpy( g_pad->end, g_pad->backup, delta ) ;
      free( g_pad->backup ) ;
    }

    return mainRval ;

  }
#elif defined( _WIN32 )

// this is here just to keep (ms cl) compiler from complaining about empty translation units
static void cygDummy() {}

#endif

