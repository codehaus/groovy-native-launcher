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
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>

#if defined ( __APPLE__ )
#  include <TargetConditionals.h>
#endif

#if defined ( _WIN32 )
#  include <Windows.h>
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

#  if !defined( MAX_PATH )
#    define MAX_PATH 512
#  endif

  /** Path conversion from cygwin format to win format. This func contains the general logic, and thus
   * requires a pointer to a cygwin conversion func. */
  static char* convertPath( cygwin_conversionfunc_type conversionFunc, char* path ) {
    char*  temp ;
    
    temp = malloc( MAX_PATH * sizeof( char ) ) ;
    if ( !temp ) {
      fprintf( stderr, strerror( errno ) ) ;
      return NULL ; 
    }

    if ( conversionFunc ) {
      conversionFunc( path, temp ) ;
    } else {
      strcpy( temp, path ) ;
    }
    
    temp = realloc( temp, strlen( temp ) + 1 ) ;
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


static char* createCPEntry( const char* groovyHome, const char* jarName ) {
  char *firstGroovyJarFound = NULL ;
  size_t len = strlen( groovyHome ) + 1 + 3 + 1 + strlen( jarName ) + 1 ;
  jboolean ghomeEndWithSeparator = ( groovyHome[ strlen( groovyHome ) ] == JST_FILE_SEPARATOR[ 0 ] ) ;
  
  if ( ! ( firstGroovyJarFound = calloc( len, sizeof( char ) ) ) ) {
    fprintf( stderr, strerror( errno ) ) ;
    return NULL ;
  }
  
  return jst_append( firstGroovyJarFound, &len, groovyHome,
                                                 ghomeEndWithSeparator ? "" : JST_FILE_SEPARATOR, 
                                                 "lib" JST_FILE_SEPARATOR, 
                                                 jarName, NULL ) ;  
}


/** If groovy-starter.jar exists, returns path to that. If not, the first jar whose name starts w/ "groovy-" is returned.
 * The previous is appropriate for groovy <= 1.0, the latter for groovy >= 1.1
 * Be sure startupJar_output is malloc:ed, it will be realloc:d if not big enough.
 * Returns JNI_FALSE on error. */
static jboolean findGroovyStartupJar( const char* groovyHome, char** startupJar_output, size_t* bufsize ) {
  char* firstGroovyJarFound = NULL ;
  
# if defined( _WIN32 )
// windows does not have dirent.h, it does things different from other os'es

  HANDLE          fileHandle = INVALID_HANDLE_VALUE ;
  WIN32_FIND_DATA fdata ;
  char*           jarEntrySpecifier ;
  size_t          jarEntryLen ;
  DWORD           lastError ;
  jboolean        rval = JNI_FALSE,
                  ghomeEndWithSeparator ; 
  
  // groovy startup jar is called groovy-XXXXXXX.jar. 100 == big enough
  jarEntryLen = strlen( groovyHome ) + 100 ;
  jarEntrySpecifier = calloc( jarEntryLen, sizeof( char ) ) ;
  if ( !jarEntrySpecifier ) {
    fprintf( stderr, strerror( errno ) ) ;
    goto end ;
  }
  
  ghomeEndWithSeparator = ( groovyHome[ strlen( groovyHome ) - 1 ] == JST_FILE_SEPARATOR[ 0 ] ) ; 
  // this only works w/ FindFirstFileW. If need be, use that.
//  strcpy(jarEntrySpecifier, "\\\\?\\"); // to allow long paths, see documentation of FindFirstFile
  if ( !(jarEntrySpecifier = jst_append( jarEntrySpecifier, &jarEntryLen, groovyHome, 
                                                                          ghomeEndWithSeparator ? "" : JST_FILE_SEPARATOR, 
                                                                          "lib" JST_FILE_SEPARATOR "*.jar", NULL ) ) ) goto end ;

  SetLastError( 0 ) ;
  fileHandle = FindFirstFile( jarEntrySpecifier, &fdata ) ;
  
  if ( fileHandle == INVALID_HANDLE_VALUE ) {
    fprintf( stderr, "error: opening directory %s%s failed w/ error code %u\n", groovyHome, JST_FILE_SEPARATOR "lib", (unsigned int)GetLastError() ) ;
    goto end ;
  }
  
  lastError = GetLastError() ;
  if( !lastError ) {

    do {
      char* jarName = fdata.cFileName ;
      
      // groovy-x.jar == 12 chars 
      if ( ( strlen( jarName ) >= 12 ) && ( memcmp( jarName, "groovy-", 7 ) == 0 ) && ( strcmp( ".jar", jarName + strlen( jarName ) - 4 ) == 0 ) ) {
        // if it's groovy-starter.jar, we found what we're lookin for. If not, continue looking.
        if ( strcmp( "groovy-starter.jar", jarName ) == 0 ) {
          (*startupJar_output)[ 0 ] = '\0' ;
          if ( !( *startupJar_output = jst_append( *startupJar_output, bufsize, groovyHome,
                                                                                ghomeEndWithSeparator ? "" : JST_FILE_SEPARATOR, 
                                                                                "lib" JST_FILE_SEPARATOR, jarName, NULL ) ) ) goto end ;
          rval = JNI_TRUE ;
          break ;
        }
        
        if ( !firstGroovyJarFound && 
           // we are looking for groovy-[0-9]+\.+[0-9]+.*\.jar. As tegexes 
           // aren't available, we'll just check that the char following 
           // groovy- is a digit
           isdigit( jarName[ 7 ] ) && 
          !( firstGroovyJarFound = createCPEntry( groovyHome, jarName ) ) ) goto end ;
        
      }

    } while ( FindNextFile( fileHandle, &fdata ) ) ;
    
    if ( !rval && firstGroovyJarFound ) {
      (*startupJar_output)[ 0 ] = '\0' ;
      if ( !(*startupJar_output = jst_append( *startupJar_output, bufsize, firstGroovyJarFound, NULL ) ) ) goto end ;
      rval = JNI_TRUE ;
    }
        
  }
    
  if( !lastError ) lastError = GetLastError() ;
  if( lastError && ( lastError != ERROR_NO_MORE_FILES ) ) {
    fprintf( stderr, "error: error %u occurred when finding jars %s\n", (unsigned int)lastError, jarEntrySpecifier ) ;
    rval = JNI_FALSE ;
  }
  
  
  end:
  
  if ( fileHandle != INVALID_HANDLE_VALUE ) FindClose( fileHandle     ) ;
  if ( jarEntrySpecifier                  ) free( jarEntrySpecifier   ) ;
  if ( firstGroovyJarFound                ) free( firstGroovyJarFound ) ;
  return rval ;
      
# else      

  // POSIX (with dirent.h existing, i.e. any other platform than windows)

  DIR           *dir = NULL ;
  struct dirent *entry ;
  size_t        len ;
  char          *groovyLibDir = NULL ;
  jboolean      ghomeEndsWithSeparator, 
                rval = JNI_FALSE ;

  // groovy startup jar is called groovy-XXXXXXX.jar. 100 == big enough
  len = strlen( groovyHome ) + 100  ;
  if ( !( groovyLibDir = calloc( len, sizeof( char ) ) ) ) {
    fprintf( stderr, strerror( errno ) ) ;
    goto end ;
  }
  ghomeEndsWithSeparator = ( strcmp( groovyHome + len - strlen(JST_FILE_SEPARATOR), JST_FILE_SEPARATOR) == 0 ) ;

  if ( !(groovyLibDir = jst_append( groovyLibDir, &len, groovyHome, 
                                                        ghomeEndsWithSeparator ? "" : JST_FILE_SEPARATOR, 
                                                        "lib" JST_FILE_SEPARATOR, NULL ) ) ) goto end ;

  dir = opendir( groovyLibDir ) ;
  
  if ( !dir ) {
    fprintf( stderr, "error: could not read groovy lib directory %s\n%s", groovyLibDir, strerror( errno ) ) ;
    return JNI_FALSE ;
  }

  while ( ( entry = readdir( dir ) ) ) {
    char *jarName = entry->d_name ;
    len = strlen( jarName ) ;
    // groovy-x.jar == 12 chars 
    if ( len >= 12 && ( memcmp( jarName, "groovy-", 7 ) == 0 ) && ( strcmp( ".jar", jarName + len - 4 ) == 0 ) ) { 

      if ( strcmp( "groovy-starter.jar", jarName ) == 0 ) {
        (*startupJar_output)[ 0 ] = '\0' ;

        if ( !( *startupJar_output = jst_append( *startupJar_output, bufsize, groovyHome, 
                                                                              ghomeEndsWithSeparator ? "" : JST_FILE_SEPARATOR, 
                                                                              "lib" JST_FILE_SEPARATOR, jarName, NULL ) ) ) goto end ;
        rval = JNI_TRUE ;
        break ;
      }

      if ( !firstGroovyJarFound    && 
           // we are looking for groovy-[0-9]+\.+[0-9]+.*\.jar. As tegexes 
           // aren't available, we'll just check that the char following 
           // groovy- is a digit
           isdigit( jarName[ 7 ] ) && 
           !( firstGroovyJarFound = createCPEntry( groovyHome, jarName ) ) ) goto end ;
      
    }
  }

  if ( !rval && firstGroovyJarFound ) {
    (*startupJar_output)[ 0 ] = '\0' ;
    if ( !(*startupJar_output = jst_append( *startupJar_output, bufsize, firstGroovyJarFound, NULL ) ) ) goto end ;
    rval = JNI_TRUE ;
  }

  end:
  
  if ( firstGroovyJarFound ) free( firstGroovyJarFound ) ;
  if ( !rval ) {
    fprintf( stderr, strerror( errno ) ) ;
  }
  closedir( dir ) ;
  return rval ;
  
#  endif
  
}

/** returns null on error, otherwise pointer to groovy home. 
 * First tries to see if the current executable is located in a groovy installation's bin directory. If not, groovy
 * home is looked up. If neither succeed, an error msg is printed.
 * Do NOT modify the returned string, make a copy. */
char* getGroovyHome() {
  static char *_ghome = NULL ;
  char *appHome, 
       *gconfFile = NULL ;
  size_t len, curSize ;
  
  if ( _ghome ) return _ghome ;
  
  appHome = jst_getExecutableHome() ;
  if ( !appHome ) return NULL ;
  
  len = strlen( appHome ) ;
  // "bin" == 3 chars
  if ( len > ( 3 + 2 * strlen( JST_FILE_SEPARATOR ) ) ) {
    int gconfExists ; 
    
    _ghome = malloc( ( len + 1 ) * sizeof( char ) ) ;
    if ( !_ghome ) {
      fprintf( stderr, strerror( errno ) ) ;
      return NULL ;
    }
    strcpy( _ghome, appHome ) ;
    _ghome[ len - 2 * strlen( JST_FILE_SEPARATOR ) - 3 ] = '\0' ; // cut of the "bin" (where the executable lives) from the given path
    // check that conf/groovy-starter.conf ( == 24 chars) exists ( + nul char )
    curSize = ( len + 25 ) * sizeof( char ) ;
    gconfFile = jst_append( NULL, &curSize, _ghome, JST_FILE_SEPARATOR "conf" JST_FILE_SEPARATOR "groovy-starter.conf", NULL ) ; 
    if ( !gconfFile ) {
      free( _ghome ) ;
      _ghome = NULL ;
      goto end ;
    }
  
    gconfExists = jst_fileExists( gconfFile ) ;

    if ( gconfExists ) {
      _ghome = realloc( _ghome, len + 1 ) ; // shrink the buffer as there is extra space
      assert( _ghome ) ;
    } else {
      free( _ghome ) ;
      _ghome = NULL ;
    }
  }
  
  if ( !_ghome ) {
    _ghome = getenv( "GROOVY_HOME" ) ;
    if ( !_ghome ) {
      fprintf( stderr, ( len == 0 ) ? "error: could not find groovy installation - please set GROOVY_HOME environment variable to point to it\n" :
                                   "error: could not find groovy installation - either the binary must reside in groovy installation's bin dir or GROOVY_HOME must be set\n" ) ;
    } else if ( _groovy_launcher_debug ) {
      fprintf( stderr, ( len == 0 ) ? "warning: figuring out groovy installation directory based on the executable location is not supported on your platform, "
                                   "using GROOVY_HOME environment variable\n"
                                 : "warning: the groovy executable is not located in groovy installation's bin directory, resorting to using GROOVY_HOME\n" ) ;
    }
  }

  end : 
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
    g_pad->backup = malloc( delta ) ;
    if ( !( g_pad->backup ) ) {
      fprintf( stderr, strerror( errno ) ) ;
      return -1 ;
    }
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
           groovyConfOverridden = JNI_FALSE,
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
      fprintf( stderr, "strange bug: could not find appropriate init and conversion functions inside cygwin1.dll, error %u\n", (unsigned int)GetLastError() ) ;
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
  
  if ( !( args = malloc( numArgs * sizeof( char* ) ) ) ) {
    fprintf( stderr, strerror( errno ) ) ;
    goto end ;
  }
  for( i = 0 ; i < numArgs ; i++ ) args[ i ] = argv[ i + 1 ] ; // skip the program name
  
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
      
      classpath_dyn = malloc( MAX_PATH ) ;
      if ( !classpath_dyn ) {
        fprintf( stderr, strerror( errno ) ) ;
        goto end ;
      }
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
  
  if ( groovyConfFile ) groovyConfOverridden = JNI_TRUE ;
  else if ( ( groovyConfFile = getenv( "GROOVY_CONF" ) ) ) groovyConfOverridden = JNI_TRUE ; 
 
  
  groovyHome = getGroovyHome() ;
  if ( !groovyHome ) goto end ;
  
  groovyLaunchJar = calloc( len = strlen( groovyHome ) + 50, sizeof( char ) ) ;  
  if ( !findGroovyStartupJar( groovyHome, &groovyLaunchJar, &len ) ) goto end ; 
  jars[ 0 ] = groovyLaunchJar ;
    
  len = strlen( groovyHome ) ;
          // ghome path len + nul + "conf" + 2 file seps + file name len
  if ( !groovyConfOverridden ) groovyConfFile = malloc( len + 1 + 4 + 2 * strlen( JST_FILE_SEPARATOR ) + strlen( GROOVY_CONF ) ) ;
  if ( !groovyLaunchJar || ( !groovyConfOverridden && !groovyConfFile ) ) {
    fprintf( stderr, strerror( errno ) ) ;
    goto end ;
  }

  // set -Dgroovy.home and -Dgroovy.starter.conf as jvm options

  if ( !groovyConfOverridden ) { // set the default groovy conf file if it was not given as a parameter
    strcpy( groovyConfFile, groovyHome ) ;
    strcat( groovyConfFile, JST_FILE_SEPARATOR "conf" JST_FILE_SEPARATOR GROOVY_CONF ) ;
  }
  extraProgramOptions[ 3 ] = groovyConfFile ;
  
  // "-Dgroovy.starter.conf=" => 22 + 1 for nul char
  groovyDConf = malloc(          22 + 1 + strlen( groovyConfFile ) ) ;
  // "-Dgroovy.home=" => 14 + 1 for nul char 
  groovyDHome = malloc(  14 + 1 + strlen( groovyHome ) ) ;
  if ( !groovyDConf || !groovyDHome ) {
    fprintf( stderr, strerror( errno ) ) ;
    goto end ;
  }
  strcpy( groovyDConf, "-Dgroovy.starter.conf=" ) ;
  strcat( groovyDConf, groovyConfFile ) ;
  
  
  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                              extraJvmOptionsCount++, 
                                              &extraJvmOptionsSize, 
                                              groovyDConf, 
                                              NULL ) ) ) goto end ;

  strcpy( groovyDHome, "-Dgroovy.home=" ) ;
  strcat( groovyDHome, groovyHome ) ;

  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                              extraJvmOptionsCount++, 
                                              &extraJvmOptionsSize, 
                                              groovyDHome, 
                                              NULL ) ) ) goto end ;

  // populate the startup parameters

  options.java_home           = NULL ; // let the launcher func figure it out
  options.jvmSelectStrategy   = JST_SERVER_FIRST ;
  options.javaOptsEnvVar      = "JAVA_OPTS" ;
  options.toolsJarHandling    = JST_TOOLS_JAR_TO_SYSPROP ;
  options.javahomeHandling    = JST_ALLOW_JH_ENV_VAR_LOOKUP | JST_ALLOW_JH_PARAMETER | JST_ALLOW_PATH_LOOKUP | JST_ALLOW_REGISTRY_LOOKUP ; 
  options.classpathHandling   = JST_IGNORE_GLOBAL_CP ; 
  options.arguments           = args ;
  options.numArguments        = numArgs ;
  options.jvmOptions          = extraJvmOptions ;
  options.numJvmOptions       = extraJvmOptionsCount ;
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
  if ( classpath_dyn )  free( classpath_dyn ) ;
  if ( scriptpath_dyn ) free( scriptpath_dyn ) ; 
#endif

// cygwin compatibility end
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  

  if ( paramInfos      )  free( paramInfos ) ;
  if ( userJvmOpts     )  free( userJvmOpts ) ;
  if ( extraJvmOptions )  free( extraJvmOptions ) ;
  if ( args            )  free( args ) ;
  if ( groovyLaunchJar )  free( groovyLaunchJar ) ;
  if ( groovyConfFile && !groovyConfOverridden ) free( groovyConfFile ) ;
  if ( groovyDConf )      free( groovyDConf ) ;
  if ( groovyDHome )      free( groovyDHome ) ;
  
  return rval ;
  
}

