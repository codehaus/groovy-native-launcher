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
#endif

#if defined ( _WIN32 )
#  include <Windows.h>
#  if !defined( PATH_MAX )
#    define PATH_MAX MAX_PATH
#  endif
#else
#  include <dirent.h>
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

#define GROOVY_CONF "groovy-starter.conf"

static jboolean _groovy_launcher_debug = JNI_FALSE ;

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
  jboolean ghomeEndWithSeparator = jst_dirNameEndsWithSeparator( groovyHome ) ;

  if (
        !( groovyLibDir = jst_append( NULL, NULL, groovyHome, 
                                                  ghomeEndWithSeparator ? "" : JST_FILE_SEPARATOR, 
                                                  "lib", 
                                                  NULL ) 
         )
     ) goto end ;

  if ( !jst_fileExists( groovyLibDir ) ) {
    fprintf( stderr, "Lib dir %s does not exist\n", groovyLibDir ) ; 
    goto end ;
  }
  
  if ( !( jarNames = jst_getFileNames( groovyLibDir, "groovy-", ".jar" ) ) ) goto end ;
  
  while ( ( jarName = jarNames[ i++ ] ) ) {
    if ( strcmp( "groovy-starter.jar", jarName ) == 0 ) { // groovy < 1.1
      if ( !( startupJar = jst_append( NULL, NULL, groovyLibDir,
                                                   JST_FILE_SEPARATOR, 
                                                   jarName, 
                                                   NULL ) ) ) goto end ;
      break ;
    }
    if ( !firstGroovyJarFound && 
       // we are looking for groovy-[0-9]+\.+[0-9]+.*\.jar. As tegexes 
       // aren't available, we'll just check that the char following 
       // groovy- is a digit
       isdigit( jarName[ 7 ] ) && 
      !( firstGroovyJarFound = jst_append( NULL, NULL, groovyLibDir,
                                                       JST_FILE_SEPARATOR, 
                                                       jarName, 
                                                       NULL ) ) ) goto end ;  
     
  } // while
  
  if ( !startupJar && firstGroovyJarFound ) {
    if ( !( startupJar = jst_append( NULL, NULL, firstGroovyJarFound, NULL ) ) ) goto end ;
  }

  end:
  if ( jarNames            ) free( jarNames ) ;
  if ( groovyLibDir        ) free( groovyLibDir ) ;
  if ( firstGroovyJarFound ) free( firstGroovyJarFound ) ;
  return startupJar ;
   
}

/** returns null on error, otherwise pointer to groovy home. 
 * First tries to see if the current executable is located in a groovy installation's bin directory. If not, groovy
 * home env var is looked up. If neither succeed, an error msg is printed.
 * Do NOT modify the returned string, make a copy. */
char* getGroovyHome() {
  static char *_ghome = NULL ;
  
  char *appHome   = NULL, 
       *gconfFile = NULL ;
  
  if ( _ghome ) return _ghome ;
  
  if ( !( appHome = jst_getExecutableHome() ) ) return NULL ;
  // make a copy of exec home str
  if ( !( appHome = jst_append( NULL, NULL, appHome, NULL ) ) ) return NULL ;
  
  if ( jst_pathToParentDir( appHome ) ) {
    
    gconfFile = jst_append( NULL, NULL, appHome, JST_FILE_SEPARATOR "conf" JST_FILE_SEPARATOR "groovy-starter.conf", NULL ) ; 
    if ( !gconfFile ) goto end ;
   
    if ( jst_fileExists( gconfFile ) ) {
      _ghome = jst_append( NULL, NULL, appHome, NULL ) ;
    }
  }
  
  
  if ( !_ghome ) {
    _ghome = getenv( "GROOVY_HOME" ) ;
    if ( !_ghome ) {
      fprintf( stderr, 
#if defined( __APPLE__ )          
              "error: could not find groovy installation - please set GROOVY_HOME environment variable to point to it\n" 
#else           
              "error: could not find groovy installation - either the binary must reside in groovy installation's bin dir or GROOVY_HOME must be set\n" 
#endif          
              ) ;
    } else if ( _groovy_launcher_debug ) {
      fprintf( stderr, 
#if defined( __APPLE__ )                    
                "warning: figuring out groovy installation directory based on the executable location is not supported on your platform, "
                "using GROOVY_HOME environment variable\n"
#else          
                "warning: the groovy executable is not located in groovy installation's bin directory, resorting to using GROOVY_HOME\n" 
#endif          
              ) ;
    }
  }

  end : 
  
  if ( appHome   ) free( appHome   ) ;
  if ( gconfFile ) free( gconfFile ) ;
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
  size_t       extraJvmOptionsCount = 0, extraJvmOptionsSize = 5 ;
  char         *userJvmOpts = NULL ;
  
  JstParamInfo* paramInfos     = NULL ;
  int           paramInfoCount = 0    ;

  char *groovyLaunchJar = NULL, 
       *groovyConfFile  = NULL, 
       *groovyDConf     = NULL, // the -Dgroovy.conf=something to pass to the jvm
       *groovyHome      = NULL, 
       *groovyDHome     = NULL, // the -Dgroovy.home=something to pass to the jvm
       *classpath       = NULL,
       *temp ;
        
  // NULL terminated string arrays. Other NULL entries will be filled dynamically below.
  char *terminatingSuffixes[] = { ".groovy", ".gvy", ".gy", ".gsh", NULL },
       *extraProgramOptions[] = { "--main", "groovy.ui.GroovyMain", "--conf", NULL, "--classpath", NULL, NULL }, 
       *jars[]                = { NULL, NULL }, 
       *cpaliases[]           = { "-cp", "-classpath", "--classpath", NULL } ;

  // the argv will be copied into this - we don't modify the param, we modify a local copy
  char **args = NULL ;
  int  numArgs = argc - 1 ;
         
  size_t len, 
         numGroovyOpts = 13 ;
  int    i,
         numParamsToCheck,
         rval = -1 ; 

  jboolean error                = JNI_FALSE, 
           // this flag is used to tell us whether we reserved the memory for the conf file location string
           // or whether it was obtained from cmdline or environment var. That way we don't try to free memory
           // we did not allocate.
           displayHelp          = ( ( numArgs == 0 )                    || 
                                    ( strcmp( argv[ 1 ], "-h"     ) == 0 ) || 
                                    ( strcmp( argv[ 1 ], "--help" ) == 0 )
                                  ) ? JNI_TRUE : JNI_FALSE ; 

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )
  
  char *classpath_dyn  = NULL,
       *scriptpath_dyn = NULL ;
  HINSTANCE cygwinDllHandle = LoadLibrary( "cygwin1.dll" ) ;

  if ( cygwinDllHandle ) {
    SetLastError( 0 ) ;
    cygwin_initfunc            = (cygwin_initfunc_type)      GetProcAddress( cygwinDllHandle, "cygwin_dll_init" ) ;
    // only proceed getting more proc addresses if the last call was successfull in order not to hide the original error
    if ( cygwin_initfunc       ) cygwin_posix2win_path      = (cygwin_conversionfunc_type)GetProcAddress( cygwinDllHandle, "cygwin_conv_to_win32_path"       ) ;
    if ( cygwin_posix2win_path ) cygwin_posix2win_path_list = (cygwin_conversionfunc_type)GetProcAddress( cygwinDllHandle, "cygwin_posix_to_win32_path_list" ) ;
    
    if ( !cygwin_initfunc || !cygwin_posix2win_path || !cygwin_posix2win_path_list ) {
      fprintf( stderr, "strange bug: could not find appropriate init and conversion functions inside cygwin1.dll\n" ) ;
      printWinError( GetLastError() ) ;
      goto end ;
    }
    cygwin_initfunc() ;
  } // if cygwin1.dll is not found, just carry on. It means we're not inside cygwin shell and don't need to care about cygwin path conversions

#endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

  // the parameters accepted by groovy (note that -cp / -classpath / --classpath & --conf 
  // are handled separately below
  if ( !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-c",          JST_DOUBLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "--encoding",  JST_DOUBLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-h",          JST_SINGLE_PARAM, 1 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "--help",      JST_SINGLE_PARAM, 1 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-d",          JST_SINGLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "--debug",     JST_SINGLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-e",          JST_DOUBLE_PARAM, 1 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-i",          JST_DOUBLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-l",          JST_DOUBLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-n",          JST_SINGLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-p",          JST_SINGLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "-v",          JST_SINGLE_PARAM, 0 ) ) ||
       !( jst_setParameterDescription( &paramInfos, paramInfoCount++, &numGroovyOpts, "--version",   JST_SINGLE_PARAM, 0 ) ) ) goto end ;
  
  if ( !( args = jst_malloc( numArgs * sizeof( char* ) ) ) ) goto end ;
  
  for ( i = 0 ; i < numArgs ; i++ ) args[ i ] = argv[ i + 1 ] ; // skip the program name
  
  // look up the first terminating launchee param and only search for --classpath and --conf up to that   
  numParamsToCheck = jst_findFirstLauncheeParamIndex( args, numArgs, (char**)terminatingSuffixes, paramInfos, paramInfoCount ) ;

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


//  _groovy_launcher_debug = (jst_valueOfParam( args, &numArgs, &numParamsToCheck, "-d", JST_SINGLE_PARAM, JNI_FALSE, &error ) ? JNI_TRUE : JNI_FALSE ) ;
  _groovy_launcher_debug = ( getenv( "__JLAUNCHER_DEBUG" ) ? JNI_TRUE : JNI_FALSE ) ;
  
  // look for classpath param
  // - If -cp is not given, then the value of CLASSPATH is given in groovy starter param --classpath. 
  // And if not even that is given, then groovy starter param --classpath is omitted.
  for( i = 0 ; ( temp = cpaliases[i] ) ; i++ ) {
    classpath = jst_valueOfParam( args, &numArgs, &numParamsToCheck, temp, JST_DOUBLE_PARAM, JNI_TRUE, &error ) ;
    if ( error ) goto end ;
    if ( classpath ) {

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#     if defined ( _WIN32 ) && defined ( _cwcompat )
      // - - - - - - - - - - - - -
      // cygwin compatibility: path conversion from cygwin to win format
      // - - - - - - - - - - - - -
      int cpind = jst_indexOfParam( args, numParamsToCheck, cpaliases[ i ] ) ;
      
      classpath_dyn = jst_malloc( PATH_MAX + 1 ) ;
      if ( !classpath_dyn ) goto end ;
      
      classpath_dyn = jst_convertCygwin2winPathList( classpath ) ; 
      if ( !classpath_dyn ) goto end ;
      
      args[ cpind ] = classpath = classpath_dyn ;
#     endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

      break ;
    }
  }

  if ( !classpath ) classpath = getenv( "CLASSPATH" ) ;

  if ( classpath ) {
    extraProgramOptions[ 5 ] = classpath ;
  } else {
    extraProgramOptions[ 4 ] = NULL ; // omit the --classpath param, extraProgramOptions is a NULL terminated char**
  }
  
  groovyConfFile = jst_valueOfParam( args, &numArgs, &numParamsToCheck, "--conf", JST_DOUBLE_PARAM, JNI_TRUE, &error ) ;
  if ( error ) goto end ;
  
  if ( groovyConfFile || ( groovyConfFile = getenv( "GROOVY_CONF" ) ) ) { 
    // copy the string so we don't have to wonder whether it's dynamically allocated
    groovyConfFile = jst_append( NULL, NULL, groovyConfFile, NULL ) ;
  }
  
  groovyHome = getGroovyHome() ;
  if ( !groovyHome ) goto end ;
  
  if ( !( groovyLaunchJar = findGroovyStartupJar( groovyHome ) ) ) goto end ; 
  jars[ 0 ] = groovyLaunchJar ;
    
  len = strlen( groovyHome ) ;

  // set -Dgroovy.home and -Dgroovy.starter.conf as jvm options

  if ( !groovyConfFile && // set the default groovy conf file if it was not given as a parameter
       !( groovyConfFile = jst_append( NULL, NULL, groovyHome, JST_FILE_SEPARATOR "conf" JST_FILE_SEPARATOR GROOVY_CONF, NULL ) ) ) goto end ;

  extraProgramOptions[ 3 ] = groovyConfFile ;
  
  if ( !( groovyDConf = jst_append( NULL, NULL, "-Dgroovy.starter.conf=", groovyConfFile, NULL ) ) ) goto end ;
  
  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                              (int)extraJvmOptionsCount++, 
                                              &extraJvmOptionsSize, 
                                              groovyDConf, 
                                              NULL ) ) ) goto end ;

  if ( !( groovyDHome = jst_append( NULL, NULL, "-Dgroovy.home=", groovyHome, NULL ) ) ) goto end ;

  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                              (int)extraJvmOptionsCount++, 
                                              &extraJvmOptionsSize, 
                                              groovyDHome, 
                                              NULL ) ) ) goto end ;

  // populate the startup parameters
  // first, set the memory to 0. This is just a precaution, as NULL (0) is a sensible default value for many options.
  // TODO: remove this, it gives false sense of safety...
  // memset( &options, 0, sizeof( JavaLauncherOptions ) ) ;
  
  options.java_home           = NULL ; // let the launcher func figure it out
  options.jvmSelectStrategy   = JST_CLIENT_FIRST ; // mimic java launcher, which also prefers client vm due to its faster startup (despite it running much slower)
  options.javaOptsEnvVar      = "JAVA_OPTS" ;
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
  options.paramInfos          = paramInfos ;
  options.paramInfosCount     = paramInfoCount ;
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
  if ( classpath_dyn   ) free( classpath_dyn ) ;
  if ( scriptpath_dyn  ) free( scriptpath_dyn ) ; 
#endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

  if ( paramInfos      ) free( paramInfos ) ;
  if ( userJvmOpts     ) free( userJvmOpts ) ;
  if ( extraJvmOptions ) free( extraJvmOptions ) ;
  if ( args            ) free( args ) ;
  if ( groovyLaunchJar ) free( groovyLaunchJar ) ;
  if ( groovyConfFile  ) free( groovyConfFile ) ;
  if ( groovyDConf     ) free( groovyDConf ) ;
  if ( groovyDHome     ) free( groovyDHome ) ;
  
  return rval ;
  
}

