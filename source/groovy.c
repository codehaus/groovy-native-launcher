//  Groovy -- A native launcher for Groovy
//
//  Copyright (c) 2006 Antti Karanta (Antti dot Karanta (at) iki dot fi) 
//
//  Licensed under the Apache License, Version 2.0 (the "License") ; you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the License is
//  distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
//  implied. See the License for the specific language governing permissions and limitations under the
//  License.
//
//  Author:  Antti Karanta (Antti dot Karanta (at) iki dot fi) 
//  $Revision$
//  $Date$

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>

#if defined ( __APPLE__ )
#  include <TargetConditionals.h>
#elif defined ( _WIN32 )
#  include <Windows.h>
#endif


// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )

  typedef void ( *cygwin_initfunc_type)() ;
  typedef void ( *cygwin_conversionfunc_type)( const char*, char* ) ;

  static cygwin_initfunc_type       cygwin_initfunc            = NULL ;
  static cygwin_conversionfunc_type cygwin_posix2win_path      = NULL ;
  static cygwin_conversionfunc_type cygwin_posix2win_path_list = NULL ;

  /** Path conversion from cygwin format to win format. This func contains the general logic, and thus
   * requires a pointer to a cygwin conversion func. */
  static char* convertPath( cygwin_conversionfunc_type conversionFunc, char* path ) {
    char*  temp ;
    
    if ( !( temp = jst_malloc( MAX_PATH * sizeof( char ) ) ) ) return NULL ; 

    if ( conversionFunc ) {
      conversionFunc( path, temp ) ;
    } else {
      strcpy( temp, path ) ;
    }
    
    temp = jst_realloc( temp, strlen( temp ) + 1 ) ;
    assert( temp ) ; // we're shrinking, so this should go fine
  
    return temp ;
    
  }
  
  /** returns NULL on error, does not modify the param. Result must be freed by caller. */
  static char* jst_convertCygwin2winPath( char* path ) {
    return convertPath( cygwin_posix2win_path, path ) ;  
  }
  
  
  
  /** see above */
  static char* jst_convertCygwin2winPathList( char* path ) {
    return convertPath( cygwin_posix2win_path_list, path ) ;
  }

#endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

#include <jni.h>

#include "jvmstarter.h"

#if !defined( PATH_MAX )
#  define PATH_MAX MAX_PATH
#endif
  
#define GROOVY_CONF "groovy-starter.conf"

/** If groovy-starter.jar exists, returns path to that. If not, the first jar whose name starts w/ "groovy-x" (where x is a digit) is returned.
 * The previous is appropriate for groovy <= 1.0, the latter for groovy >= 1.1
 * Returns NULL on error, otherwise dynallocated string (which caller must free). */
static char* findGroovyStartupJar( const char* groovyHome ) {
  char *firstGroovyJarFound = NULL,
        *startupJar         = NULL,
        *groovyLibDir       = NULL, 
        **jarNames          = NULL, 
        *jarName ;
  int i = 0 ;

  if ( !( groovyLibDir = jst_createFileName( groovyHome, "lib", NULL ) ) ) goto end ;

  if ( !jst_fileExists( groovyLibDir ) ) {
    fprintf( stderr, "Lib dir %s does not exist\n", groovyLibDir ) ; 
    goto end ;
  }
  
  if ( !( jarNames = jst_getFileNames( groovyLibDir, "groovy-", ".jar" ) ) ) goto end ;
  
  while ( ( jarName = jarNames[ i++ ] ) ) {
    if ( strcmp( "groovy-starter.jar", jarName ) == 0 ) { // groovy < 1.1
      if ( !( startupJar = jst_createFileName( groovyLibDir, jarName, NULL ) ) ) goto end ;
      break ;
    }
    if ( !firstGroovyJarFound ) {
      size_t jarNameLen = strlen( jarName ) ;
       // we are looking for groovy-[0-9]+\.+[0-9]+.*\.jar. As tegexes 
       // aren't available, we'll just check that the char following 
       // groovy- is a digit
       if ( jarNameLen >= 8 && isdigit( jarName[ 7 ] ) &&
            !( firstGroovyJarFound = jst_createFileName( groovyLibDir, jarName, NULL ) ) ) goto end ;
    }
  } // while
  
  if ( !startupJar && firstGroovyJarFound ) {
    if ( !( startupJar = jst_strdup( firstGroovyJarFound ) ) ) goto end ;
  }

  end:
  if ( jarNames            ) free( jarNames ) ;
  if ( groovyLibDir        ) free( groovyLibDir ) ;
  if ( firstGroovyJarFound ) free( firstGroovyJarFound ) ;
  return startupJar ;
   
}

/** Checks that the given dir is a valid groovy dir.
 * 0 => false, 1 => true, -1 => error */
static int isValidGroovyHome( const char* dir ) {
  char *gconfFile = NULL ;
  int  rval = -1 ;

  gconfFile = jst_createFileName( dir, "conf", GROOVY_CONF, NULL ) ; 
  if ( !gconfFile ) goto end ;
 
  rval = jst_fileExists( gconfFile ) ? 1 : 0 ;

  end :
  if ( gconfFile ) free( gconfFile ) ;
  return rval ;
}

/** returns null on error, otherwise pointer to groovy home. 
 * First tries to see if the current executable is located in a groovy installation's bin directory. If not, groovy
 * home env var is looked up. If neither succeed, an error msg is printed.
 * Do NOT modify the returned string, make a copy. */
char* getGroovyHome() {
  static char *_ghome = NULL ;
  
  char *appHome = NULL ;
  int  validGroovyHome ;
  
  if ( _ghome ) return _ghome ;
  
  if ( !( appHome = jst_getExecutableHome() ) ) return NULL ;
  // make a copy of exec home str
  if ( !( appHome = jst_strdup( appHome ) ) ) return NULL ;
  
  if ( jst_pathToParentDir( appHome ) ) {
    validGroovyHome = isValidGroovyHome( appHome ) ;
    if ( validGroovyHome == -1 ) goto end ;
    if ( validGroovyHome ) {
      if ( !( _ghome = jst_strdup( appHome ) ) ) goto end ;
    }
  }
  
  
  if ( !_ghome ) {
    char *ghome = getenv( "GROOVY_HOME" ) ;
    if ( ghome ) {
      if ( _jst_debug ) {
        fprintf( stderr, "warning: the groovy executable is not located in groovy installation's bin directory, resorting to using GROOVY_HOME=%s\n", ghome ) ;
      }
      validGroovyHome = isValidGroovyHome( ghome ) ;
      if ( validGroovyHome == -1 ) goto end ;
      if ( validGroovyHome ) {
        _ghome = ghome ;
      } else {
        fprintf( stderr, "error: binary is not in groovy installation's bin dir and GROOVY_HOME=%s does not point to a groovy installation\n", ghome ) ;
      }
    } else {
      fprintf( stderr, "error: could not find groovy installation - either the binary must reside in groovy installation's bin dir or GROOVY_HOME must be set\n" ) ;
    }
  } else if ( _jst_debug ) {
    fprintf( stderr, "debug: groovy home located based on executable location: %s\n", _ghome ) ;
  }

  end : 
  
  if ( appHome ) free( appHome   ) ;
  return _ghome ;
}

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )
// - - - - - - - - - - - - -
// cygwin compatibility
// - - - - - - - - - - - - -
static int rest_of_main( int argc, char** argv ) ;
static int mainRval ;
// 2**15
#define PAD_SIZE 32768
typedef struct {
  void* backup ;
  void* stackbase ;
  void* end ;
  byte padding[ PAD_SIZE ] ; 
} CygPadding ;

static CygPadding *g_pad ;

#endif


// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

// the parameters accepted by groovy (note that -cp / -classpath / --classpath & --conf 
// are handled separately below
static const JstParamInfo groovyParameters[] = {
  { "-D",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "--define",    JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "-c",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "--encoding",  JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-h",          JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "--help",      JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "-d",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--debug",     JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-e",          JST_DOUBLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "-i",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-l",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-n",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-p",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-v",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--version",   JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { NULL,          0,                0, 0 }
} ;

static const JstParamInfo groovycParameters[] = {
  { "--encoding",  JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-F",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "-J",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "-d",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "-e",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--exception", JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--version",   JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-h",          JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "--help",      JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "-j",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--jointCompilation", JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-v",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--version",   JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { NULL,          0,                0, 0 }
} ;

static const JstParamInfo gantParameters[] = {
  { "-c",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--usecache",  JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-n",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--dry-run",   JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-D",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "-P",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-T",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--targets",   JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-V",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--version",   JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-d",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--cachedir",  JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-f",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--gantfile",  JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-h",          JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "--help",      JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "-l",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--gantlib",   JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-p",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--projecthelp", JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-q",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--quiet",     JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-s",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--silent",    JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },  
  { "-v",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--verbose",   JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { NULL,          0,                0, 0 }
} ;

static const JstParamInfo groovyshParameters[] = {
  { "-C",          JST_PREFIX_PARAM, 0, JST_TO_LAUNCHEE },
  { "--color",     JST_PREFIX_PARAM, 0, JST_TO_LAUNCHEE },
  { "-D",          JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "--define",    JST_DOUBLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "-T",          JST_PREFIX_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "--terminal",  JST_PREFIX_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "-V",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "--version",   JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE }, 
  { "-d",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--debug",     JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-h",          JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "--help",      JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { "-q",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--quiet",     JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "-v",          JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { "--verbose",   JST_SINGLE_PARAM, 0, JST_TO_LAUNCHEE },
  { NULL,          0,                0, 0 }
} ;

static const JstParamInfo java2groovyParameters[] = {
  { "-h",          JST_SINGLE_PARAM, 1, JST_TO_LAUNCHEE },
  { NULL,          0,                0, 0 }
} ;

static const JstParamInfo noParameters[] = {
  { NULL,          0,                0, 0 }    
} ;

int main( int argc, char** argv ) {

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )

  // NOTE: this DOES NOT WORK. This code is experimental and is not compiled into the executable by default.
  //       You need to either add -D_cwcompat to the gcc opts when compiling on cygwin (into the Rantfile) or 
  //       remove the occurrences of "&& defined ( _cwcompat )" from this file.

  
  // Dynamically loading the cygwin dll is a lot more complicated than the loading of an ordinary dll. Please see
  // http://cygwin.com/faq/faq.programming.html#faq.programming.msvs-mingw
  // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/cygwin/how-cygtls-works.txt?rev=1.1&content-type=text/x-cvsweb-markup&cvsroot=uberbaum
  // "If you load cygwin1.dll dynamically from a non-cygwin application, it is
  // vital that the bottom CYGTLS_PADSIZE bytes of the stack are not in use
  // before you call cygwin_dll_init()."
  // See also 
  // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/testsuite/winsup.api/cygload.cc?rev=1.1&content-type=text/x-cvsweb-markup&cvsroot=uberbaum
  // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/testsuite/winsup.api/cygload.h?rev=1.2&content-type=text/x-cvsweb-markup&cvsroot=uberbaum
  void* sbase ;
  CygPadding pad ;
  
  g_pad = &pad ;
  pad.end = pad.padding + PAD_SIZE ;
    
  #if defined( __GNUC__ )
  __asm__ (
    "movl %%fs:4, %0"
    :"=r"( sbase )
    ) ;
  #else
  __asm {
    mov eax, fs:[ 4 ] 
    mov sbase, eax 
  }
  #endif
  g_pad->stackbase = sbase ;
  
  if ( (*(size_t*)(g_pad->stackbase)) - (*(size_t*)(g_pad->end)) ) {
    size_t delta = (*(size_t*)(g_pad->stackbase)) - (*(size_t*)(g_pad->end)) ; //g_pad->stackbase - g_pad->end ;
    g_pad->backup = jst_malloc( delta ) ;
    if ( !( g_pad->backup ) ) return -1 ;
    
    memcpy( g_pad->backup, g_pad->end, delta ) ; 
  }
  
  mainRval = rest_of_main( argc, argv ) ;
  
  // clean up the stack (is it necessary? we are exiting the program anyway...)
  if ( (*(size_t*)(g_pad->stackbase)) - (*(size_t*)(g_pad->end)) ) {
    size_t delta = (*(size_t*)(g_pad->stackbase)) - (*(size_t*)(g_pad->end)) ;
    memcpy( g_pad->end, g_pad->backup, delta ) ; 
    free( g_pad->backup ) ;
  }  
  
  return mainRval ;
  
}

int rest_of_main( int argc, char** argv ) {
  
#endif 

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

  JavaLauncherOptions options ;

  JavaVMOption *extraJvmOptions = NULL ;
  size_t       extraJvmOptionsCount = 0, 
               extraJvmOptionsSize  = 5 ;
  JstParamInfo* parameterInfos = NULL ;
  char *groovyConfFile  = NULL, 
       *groovyDConf     = NULL, // the -Dgroovy.conf=something to pass to the jvm
       *groovyHome      = NULL, 
       *groovyDHome     = NULL, // the -Dgroovy.home=something to pass to the jvm
       *classpath       = NULL,
       *classpath_dyn  = NULL,
       *temp ;

  void** dynReservedPointers = NULL ; // free all reserved pointers at at end of func
  size_t dreservedPtrsSize   = 0 ;
        
  // NULL terminated string arrays. Other NULL entries will be filled dynamically below.
  char *terminatingSuffixes[] = { ".groovy", ".gvy", ".gy", ".gsh", NULL },
       *extraProgramOptions[] = { "--main", "groovy.ui.GroovyMain", "--conf", NULL, "--classpath", NULL, NULL }, 
       *jars[]                = { NULL, NULL }, 
       // TODO: push handling these to launchJavaApp func
       *cpaliases[]           = { "-cp", "-classpath", "--classpath", NULL } ;

  // the argv will be copied into this - we don't modify the param, we modify a local copy
  char **args = NULL ;
  int  numArgs = argc - 1 ;
         
  int    i,
         numParamsToCheck,
         rval = -1 ; 

  jboolean error                = JNI_FALSE, 
           // this flag is used to tell us whether we reserved the memory for the conf file location string
           // or whether it was obtained from cmdline or environment var. That way we don't try to free memory
           // we did not allocate.
           displayHelp          = ( ( numArgs == 0 )                    || // FIXME - this causes additional helps to be always printed. 
                                                                           //         Add check that this is groovy executable.
                                    ( strcmp( argv[ 1 ], "-h"     ) == 0 ) || 
                                    ( strcmp( argv[ 1 ], "--help" ) == 0 )
                                  ) ? JNI_TRUE : JNI_FALSE ; 
  JstActualParam *processedActualParams = NULL ;
  // _jst_debug is a global debug flag
  if ( getenv( "__JLAUNCHER_DEBUG" ) ) _jst_debug = JNI_TRUE ;

  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )
  {  
  char *scriptpath_dyn = NULL ;
  HINSTANCE cygwinDllHandle = LoadLibrary( "cygwin1.dll" ) ;

  if ( cygwinDllHandle ) {
    SetLastError( 0 ) ;
    cygwin_initfunc            = (cygwin_initfunc_type)      GetProcAddress( cygwinDllHandle, "cygwin_dll_init" ) ;
    // only proceed getting more proc addresses if the last call was successfull in order not to hide the original error
    if ( cygwin_initfunc       ) cygwin_posix2win_path      = (cygwin_conversionfunc_type)GetProcAddress( cygwinDllHandle, "cygwin_conv_to_win32_path"       ) ;
    if ( cygwin_posix2win_path ) cygwin_posix2win_path_list = (cygwin_conversionfunc_type)GetProcAddress( cygwinDllHandle, "cygwin_posix_to_win32_path_list" ) ;
    
    if ( !cygwin_initfunc || !cygwin_posix2win_path || !cygwin_posix2win_path_list ) {
      fprintf( stderr, "strange bug: could not find appropriate init and conversion functions inside cygwin1.dll\n" ) ;
      jst_printWinError( GetLastError() ) ;
      goto end ;
    }
    cygwin_initfunc() ;
  } // if cygwin1.dll is not found, just carry on. It means we're not inside cygwin shell and don't need to care about cygwin path conversions
  }
#endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
    
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  { // deduce which class name to pass as --main param. In other words, this here supports all different groovy executables. 
    // The execubale acts as a different groovy executable when it's renamed / symlinked to. 
    char *execName = jst_strdup( argv[ 0 ] ),
         *eName,
         *execNameTmp ;
    
    if ( !( execNameTmp = execName ) ) goto end ;
#if defined( _WIN32 )
    {
      size_t len = strlen( execName ) ;
      if ( ( len > 4 ) && 
           ( memcmp( execName + len - 4, ".exe", 4 ) == 0 ) ) {
        execName[ len -= 4 ] = '\0' ;
      }
      if ( len > 0 && 
           ( execName[ len - 1 ] == 'w' || execName[ len - 1 ] == 'W' ) ) {
        execName[ len - 1 ] = '\0' ;
      }
    }
#endif
    eName = strrchr( execName, JST_FILE_SEPARATOR[ 0 ] ) ;
    if ( eName ) execName = eName + 1 ;

    if ( strcmp( execName, "groovy" ) == 0 ) {
      extraProgramOptions[ 1 ] = "groovy.ui.GroovyMain" ;
      parameterInfos = (JstParamInfo*)groovyParameters ;
    } else if ( strcmp( execName, "groovyc" ) == 0 ) {
      extraProgramOptions[ 1 ] = "org.codehaus.groovy.tools.FileSystemCompiler" ;
      parameterInfos = (JstParamInfo*)groovycParameters ;
    } else {
      
      if ( numArgs == 0 ) displayHelp = JNI_FALSE ;
      
      if ( strcmp( execName, "gant" ) == 0 ) {
        extraProgramOptions[ 1 ] = "gant.Gant" ; 
        parameterInfos = (JstParamInfo*)gantParameters ;
      } else if ( strcmp( execName, "groovysh" ) == 0 ) {
        extraProgramOptions[ 1 ] = getenv( "OLDSHELL" ) ? "groovy.ui.InteractiveShell" : "org.codehaus.groovy.tools.shell.Main" ;
        parameterInfos = (JstParamInfo*)groovyshParameters ;
      } else if ( strcmp( execName, "java2groovy" ) == 0 ) {
        displayHelp = JNI_FALSE ;
        extraProgramOptions[ 1 ] = "org.codehaus.groovy.antlr.java.Java2GroovyMain" ; 
        parameterInfos = (JstParamInfo*)java2groovyParameters ;
      } else if ( ( strcmp( execName, "groovyConsole" ) == 0 ) || ( strcmp( execName, "groovyconsole" ) == 0 ) ) {
        displayHelp = JNI_FALSE ;
        parameterInfos = (JstParamInfo*)noParameters ;
        extraProgramOptions[ 1 ] = "groovy.ui.Console" ;
      } else if ( ( strcmp( execName, "graphicsPad"   ) == 0 ) || strcmp( execName, "graphicspad" ) == 0 ) {
        displayHelp = JNI_FALSE ;
        parameterInfos = (JstParamInfo*)noParameters ;
        extraProgramOptions[ 1 ] = "groovy.swing.j2d.GraphicsPad" ;
      } else { // default to being "groovy"
        if ( numArgs == 0 ) displayHelp = JNI_TRUE ;
        extraProgramOptions[ 1 ] = "groovy.ui.GroovyMain" ;
        parameterInfos = (JstParamInfo*)groovyParameters ;        
      }
    }
      
    jst_free( execNameTmp ) ;
    
    if ( !extraProgramOptions[ 1 ] ) {
      fprintf( stderr, "error: could not deduce the program to launch from exec name. You should not rename this executable / symlink.\n" ) ;
      goto end ;
    }
    
  }
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
//  processedActualParams = jst_processInputParameters( argv + 1, argc - 1, parameterInfos, terminatingSuffixes ) ;
//  if ( ! processedActualParams ) goto end ;

  
  if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, 
                           args = jst_calloc( argc, sizeof( char* ) ) ) ) goto end ;
    
  for ( i = 0 ; i < numArgs ; i++ ) args[ i ] = argv[ i + 1 ] ; // skip the program name
  // args is now a NULL terminated string array. We get the NULL termination as the memory is calloced
  
  // look up the first terminating launchee param and only search for --classpath and --conf up to that   
  numParamsToCheck = jst_findFirstLauncheeParamIndex( args, numArgs, (char**)terminatingSuffixes, parameterInfos ) ;

  // append script.name w/ full path info as a jvm sys prop (groovy requires it)
  if ( numArgs > 0 && numParamsToCheck < numArgs ) {
    char scriptNameOut[ PATH_MAX + 1 ] ;
    char *scriptNameIn = args[ numParamsToCheck ],
         *scriptNameDyn = NULL ;
#if defined( _WIN32 )
    char *lpFilePart = NULL ;
    DWORD pathLen = GetFullPathName( scriptNameIn, MAX_PATH, scriptNameOut, &lpFilePart ) ;
    
    if ( pathLen == 0 ) {
      fprintf( stderr, "error: failed to get full path name for script %s\n", scriptNameIn ) ;
      jst_printWinError( GetLastError() ) ;
      goto end ;      
    } else if ( pathLen > PATH_MAX ) {
      // FIXME - reserve a bigger buffer (size pathLen) and get the dir name into that
      fprintf( stderr, "launcher bug: full path name of %s is too long to hold in the reserved buffer (%d/%d). Please report this bug.\n", scriptNameIn, (int)pathLen, PATH_MAX ) ;
      goto end ;
    }
#else
    // TODO: how to get the path name of a file that does not necessarily exist on *nix? 
    //       realpath seems to raise an error for a nonexistent file. 
    if ( !realpath( scriptNameIn, scriptNameOut ) ) {
//      fprintf( stderr, "error: could not get full path of %s\n%s\n", scriptNameIn, strerror( errno ) ) ;
//      goto end ;
      strcpy( scriptNameOut, scriptNameIn ) ;
    }
#endif

    if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, 
                             scriptNameDyn = jst_append( NULL, NULL, "-Dscript.name=", scriptNameOut, NULL ) ) ) goto end ;
    
    if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                                (int)extraJvmOptionsCount++, 
                                                &extraJvmOptionsSize, 
                                                scriptNameDyn, 
                                                NULL ) ) ) goto end ;
    
  }
  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#  if defined ( _WIN32 ) && defined ( _cwcompat )
    // - - - - - - - - - - - - -
    // cygwin compatibility: path conversion from cygwin to win format
    // - - - - - - - - - - - - -
    if ( ( numParamsToCheck < numArgs ) && 
        ( args[ numParamsToCheck ][ 0 ]  != '-' ) ) {
      scriptpath_dyn = jst_convertCygwin2winPath( args[ numParamsToCheck ] ) ;
      if ( !scriptpath_dyn ) goto end ;
      args[ numParamsToCheck ] = scriptpath_dyn ; 
    }
#  endif  

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
  
  // look for classpath param
  // - If -cp is not given, then the value of CLASSPATH is given in groovy starter param --classpath. 
  // And if not even that is given, then groovy starter param --classpath is omitted.
  for ( i = 0 ; ( temp = cpaliases[ i ] ) ; i++ ) {
    classpath = jst_valueOfParam( args, &numArgs, &numParamsToCheck, temp, JST_DOUBLE_PARAM, JNI_TRUE, &error ) ;
    if ( error ) goto end ;
    if ( classpath ) {

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#     if defined ( _WIN32 ) && defined ( _cwcompat )
      int cpind = jst_indexOfParam( args, numParamsToCheck, cpaliases[ i ] ) ;
      // - - - - - - - - - - - - -
      // cygwin compatibility: path conversion from cygwin to win format
      // - - - - - - - - - - - - -
      size_t bufSize ;
      
      classpath_dyn = jst_convertCygwin2winPathList( classpath ) ; 
      if ( !classpath_dyn ) goto end ;
      bufSize = strlen( classpath_dyn ) + 1 ;
      if ( !( classpath_dyn = jst_append( classpath_dyn, &bufSize, classpath_dyn, JST_PATH_SEPARATOR ".", NULL ) ) ||
           !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, classpath_dyn ) ) goto end ;

      args[ cpind ] = classpath = classpath_dyn ;

#     endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

      break ;
    }
  }

  if ( !classpath ) classpath = getenv( "CLASSPATH" ) ;

  // add "." to the end of the used classpath. This is what the script launcher also does
  if ( classpath ) {
    if ( !( classpath_dyn = jst_append( NULL, NULL, classpath, JST_PATH_SEPARATOR ".", NULL ) ) ||
         !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, classpath_dyn ) ) goto end ;
    extraProgramOptions[ 5 ] = classpath_dyn ;
  } else {
    extraProgramOptions[ 5 ] = "." ;
  }


  if ( !( groovyHome = getGroovyHome() ) ) goto end ;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // find out the groovy conf file to use
  
  groovyConfFile = jst_valueOfParam( args, &numArgs, &numParamsToCheck, "--conf", JST_DOUBLE_PARAM, JNI_TRUE, &error ) ;
  if ( error ) goto end ;
  
  if ( !groovyConfFile  ) groovyConfFile = getenv( "GROOVY_CONF" ) ; 
  
  if ( !groovyConfFile && // set the default groovy conf file if it was not given as a parameter
       !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, 
                           groovyConfFile = jst_createFileName( groovyHome, "conf", GROOVY_CONF, NULL ) ) ) goto end ;
  
  
  if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, 
                           jars[ 0 ] = findGroovyStartupJar( groovyHome ) ) ) goto end ;

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // set -Dgroovy.home and -Dgroovy.starter.conf as jvm options


  extraProgramOptions[ 3 ] = groovyConfFile ;
  
  if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, 
                           groovyDConf = jst_append( NULL, NULL, "-Dgroovy.starter.conf=", groovyConfFile, NULL ) ) ) goto end ;
  
  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                              (int)extraJvmOptionsCount++, 
                                              &extraJvmOptionsSize, 
                                              groovyDConf, 
                                              NULL ) ) ) goto end ;

  if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, 
                           groovyDHome = jst_append( NULL, NULL, "-Dgroovy.home=", groovyHome, NULL ) ) ) goto end ;


  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                              (int)extraJvmOptionsCount++, 
                                              &extraJvmOptionsSize, 
                                              groovyDHome, 
                                              NULL ) ) ) goto end ;

  if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, extraJvmOptions ) ) goto end ;

  // populate the startup parameters
  // first, set the memory to 0. This is just a precaution, as NULL (0) is a sensible default value for many options.
  // TODO: remove this, it gives false sense of safety...
  // memset( &options, 0, sizeof( JavaLauncherOptions ) ) ;
  
  options.javaHome            = NULL ; // let the launcher func figure it out
  options.jvmSelectStrategy   = JST_CLIENT_FIRST ; // mimic java launcher, which also prefers client vm due to its faster startup (despite it running much slower)
  options.javaOptsEnvVar      = "JAVA_OPTS" ;
  // refactor so that the target sys prop can be defined
  options.toolsJarHandling    = JST_TOOLS_JAR_TO_SYSPROP ;
  options.javahomeHandling    = JST_ALLOW_JH_ENV_VAR_LOOKUP | JST_ALLOW_JH_PARAMETER | JST_ALLOW_PATH_LOOKUP | JST_ALLOW_REGISTRY_LOOKUP ; 
  options.classpathHandling   = JST_IGNORE_GLOBAL_CP ; 
  options.arguments           = args ;
  options.numArguments        = numArgs ;
  options.jvmOptions          = extraJvmOptions ;
  options.numJvmOptions       = (int)extraJvmOptionsCount ;
  options.extraProgramOptions = extraProgramOptions ;
  options.mainClassName       = "org/codehaus/groovy/tools/GroovyStarter" ;
  options.mainMethodName      = "main" ;
  options.jarDirs             = NULL ;
  options.jars                = jars ;
  options.paramInfos          = parameterInfos ;
  options.terminatingSuffixes = terminatingSuffixes ;

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )
  // - - - - - - - - - - - - -
  // for cygwin compatibility
  // - - - - - - - - - - - - -
  if ( cygwinDllHandle ) {
    FreeLibrary( cygwinDllHandle ) ;
    cygwinDllHandle = NULL ;
    // TODO: clean up the stack here (the cygwin stack buffer) ?
  }

#endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

  rval = jst_launchJavaApp( &options ) ;

  if ( displayHelp ) { // add to the standard groovy help message
    fprintf( stderr, "\n"
    " -jh,--javahome <path to jdk/jre> makes groovy use the given jdk/jre\n" 
    "                                 instead of the one pointed to by JAVA_HOME\n"
    " --conf <conf file>              use the given groovy conf file\n"
    "\n"
    " -cp,-classpath,--classpath <user classpath>\n" 
    "                                 the classpath to use\n"
    " -client/-server                 to use a client/server VM (aliases for these\n"
    "                                 are not supported)\n"
    "\n"
    "In addition, you can give any parameters accepted by the jvm you are using, e.g.\n"
    "-Xmx<size> (see java -help and java -X for details)\n"
    "\n"
    ) ;
  }
  
end:

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )
  // - - - - - - - - - - - - -
  // cygwin compatibility
  // - - - - - - - - - - - - -
  if ( cygwinDllHandle ) FreeLibrary( cygwinDllHandle ) ;
  if ( scriptpath_dyn  ) free( scriptpath_dyn ) ; 
#endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
  
  jst_freeAll( &dynReservedPointers ) ;
  
  return rval ;
  
}

