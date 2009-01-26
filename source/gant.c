//  Groovy -- A native launcher for Groovy
//
//  Copyright (c) 2008 Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
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
//  Author:  Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
//  $Revision$
//  $Date$

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>

#if defined ( __APPLE__ )
#  include <TargetConditionals.h>

/*
 *  The Mac OS X Leopard version of jni_md.h (Java SE 5 JDK) is broken in that it tests the value of
 *  __LP64__ instead of the presence of _LP64 as happens in Sun's Java 6.0 JDK.  To prevent spurious
 *  warnings define this with a false value.
 */
#define __LP64__ 0

#elif defined ( _WIN32 )

#  include <Windows.h>

#endif

#if defined( _WIN32 )
#  define GROOVY_EXECUTABLE "groovy.exe"
#  define ANT_EXECUTABLE    "ant.exe"
#else
#  define GROOVY_EXECUTABLE "groovy"
#  define ANT_EXECUTABLE    "ant"
#endif


#include <jni.h>

#include "jvmstarter.h"
#include "jst_dynmem.h"
#include "jst_fileutils.h"

#if defined( _WIN32 ) && defined( _cwcompat )
#  include "jst_cygwin_compatibility.h"
#endif

#define GANT_CONF_FILE "gant-starter.conf"


int gantJarSelect( const char* dirName, const char* fileName ) {
  int result = JNI_FALSE ;
  size_t fileNameLen = strlen( fileName ) ;
  // we are looking for gant-[0-9]+\.+[0-9]+.*\.jar. As tegexes
  // aren't available, we'll just check that the char following
  // gant- is a digit
  if ( fileNameLen >= 9 ) result = isdigit( fileName[ 5 ] ) ;

  return result ;
}

int groovyJarSelect( const char* fileName ) {
  int result = strcmp( "groovy-starter.jar", fileName ) == 0 ;
  if ( !result ) result = memcmp( "groovy-all-", fileName, 11 ) == 0 ;
  if ( !result ) {
    size_t fileNameLen = strlen( fileName ) ;
    // we are looking for groovy-[0-9]+\.+[0-9]+.*\.jar. As tegexes
    // aren't available, we'll just check that the char following
    // groovy- is a digit
    if ( fileNameLen >= 12 ) result = isdigit( fileName[ 7 ] ) ;
  }
  return result ;
}

/**
 * @return NULL on error, otherwise dynallocated string (which caller must free). */
static char* findGantStartupJar( const char* gantHome ) {
  char *gantStartupJar = findStartupJar( gantHome, "lib", "gant-", "gant", &gantJarSelect ) ;
//  if ( !gantStartupJar ) fprintf( stderr, "error: could not locate gant startup jar\n" ) ;
  return gantStartupJar ;
}

static char* findGroovyStartupJar( const char* groovyHome, jboolean errorMsgOnFailure ) {
  char *groovyStartupJar = findStartupJar( groovyHome, "lib", "groovy-", errorMsgOnFailure ? "groovy" : NULL, &groovyJarSelect ) ;

  return groovyStartupJar ;

}

/** Checks that the given dir is a valid groovy dir.
 * If false is returned, check errno to see if there was an error (in mem allocation) */
static int isValidGantHome( const char* dir ) {
  char *gconfFile = NULL ;
  jboolean rval = JNI_FALSE ;

  errno = 0 ;

  gconfFile = jst_createFileName( dir, "conf", GANT_CONF_FILE, NULL ) ;
  if ( gconfFile ) {
    rval = jst_fileExists( gconfFile ) ? JNI_TRUE : JNI_FALSE ;
    free( gconfFile ) ;
  }

  return rval ;
}


/** returns null on error, otherwise pointer to gant home.
 * First tries to see if the current executable is located in a gant installation's bin directory. If not,
 * GANT_HOME env var is looked up. If neither succeed, an error msg is printed.
 * freeing the returned pointer must be done by the caller. */
char* getGantHome() {

  return jst_getAppHome( JST_USE_PARENT_OF_EXEC_LOCATION_AS_HOME, "GANT_HOME", &isValidGantHome ) ;

}

// ms visual c++ compiler does not support compound literals
// ( i.e. defining e.g. arrays inline, e.g. { (char*[]){ "hello", NULL }, 12 } ),
// so for sake of maximum portability the initialization of all the arrays containing
// the parameter names must be done clumsily here

static const char* gantUsecacheParam[]    = { "-c", "--usecache", NULL } ;
static const char* gantDryrunParam[]      = { "-n", "--dry-run",  NULL } ;
static const char* gantDefineParam[]      = { "-D", NULL } ;
static const char* gantClasspathParam[]   = { "-P", "--classpath", NULL } ;
static const char* gantTargetsParam[]     = { "-T", "--targets",   NULL } ;
static const char* gantVersionParam[]     = { "-V", "--version",   NULL } ;
static const char* gantCachedirParam[]    = { "-d", "--cachedir",  NULL } ;
static const char* gantGantfileParam[]    = { "-f", "--gantfile",  NULL } ;
static const char* gantHelpParam[]        = { "-h", "--help",      NULL } ;
static const char* gantGantlibParam[]     = { "-l", "--gantlib",   NULL } ;
static const char* gantProjecthelpParam[] = { "-p", "--projecthelp", NULL } ;
static const char* gantQuietParam[]       = { "-q", "--quiet",       NULL } ;
static const char* gantSilentParam[]      = { "-s", "--silent",      NULL } ;
static const char* gantVerboseParam[]     = { "-v", "--verbose",     NULL } ;

static const JstParamInfo gantParameters[] = {
  { gantUsecacheParam,    JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantDryrunParam,      JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantDefineParam,      JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { gantClasspathParam,   JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { gantTargetsParam,     JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { gantVersionParam,     JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantCachedirParam,    JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantGantfileParam,    JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantHelpParam,        JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { gantGantlibParam,     JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { gantProjecthelpParam, JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantQuietParam,       JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantSilentParam,      JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantVerboseParam,     JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { NULL,                 0,                0 }
} ;




#if defined( _WIN32 ) && defined ( _cwcompat )

  static int rest_of_main( int argc, char** argv ) ;
  /** This is a global as I'm not sure how stack manipulation required by cygwin
   * will affect the stack variables */
  static int mainRval ;
  // 2**15
#  define PAD_SIZE 32768

#if !defined( byte )
  typedef unsigned char byte ;
#endif

  typedef struct {
    void* backup ;
    void* stackbase ;
    void* end ;
    byte padding[ PAD_SIZE ] ;
  } CygPadding ;

  static CygPadding *g_pad ;

#endif



int main( int argc, char** argv ) {

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )


  // NOTE: This code is experimental and is not compiled into the executable by default.
  //       When building w/ rant, do
  //       rant clean
  //       rant cygwinc

  // Dynamically loading the cygwin dll is a lot more complicated than the loading of an ordinary dll. Please see
  // http://cygwin.com/faq/faq.programming.html#faq.programming.msvs-mingw
  // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/cygwin/how-cygtls-works.txt?rev=1.1&content-type=text/x-cvsweb-markup&cvsroot=uberbaum
  // "If you load cygwin1.dll dynamically from a non-cygwin application, it is
  // vital that the bottom CYGTLS_PADSIZE bytes of the stack are not in use
  // before you call cygwin_dll_init()."
  // See also
  // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/testsuite/winsup.api/cygload.cc?rev=1.1&content-type=text/x-cvsweb-markup&cvsroot=uberbaum
  // http://sources.redhat.com/cgi-bin/cvsweb.cgi/winsup/testsuite/winsup.api/cygload.h?rev=1.2&content-type=text/x-cvsweb-markup&cvsroot=uberbaum
  size_t delta ;
  CygPadding pad ;
  void* sbase ;

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

  delta = (size_t)g_pad->stackbase - (size_t)g_pad->end ;

  if ( delta ) {
    g_pad->backup = malloc( delta ) ;
    if( !( g_pad->backup) ) {
      fprintf( stderr, "error: out of mem when copying stack state\n" ) ;
      return -1 ;
    }
    memcpy( g_pad->backup, g_pad->end, delta ) ;
  }

  mainRval = rest_of_main( argc, argv ) ;

  // clean up the stack (is it necessary? we are exiting the program anyway...)
  if ( delta ) {
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
  char *gantConfFile  = NULL,
       *gantHome      = NULL,
       *gantDHome     = NULL, // the -Dgroovy.home=something to pass to the jvm
       *classpath     = NULL,
       *javaHome      = NULL ;

  void** dynReservedPointers = NULL ; // free all reserved pointers at at end of func
  size_t dreservedPtrsSize   = 0 ;

# define MARK_PTR_FOR_FREEING( garbagePtr ) if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, ( garbagePtr ) ) ) goto end ;

  const char *terminatingSuffixes[] = { ".gant", NULL } ;
  char *extraProgramOptions[]       = { "--main", "gant.Gant", "--conf", NULL, "--classpath", ".", NULL },
       *jars[]                      = { NULL, NULL, NULL } ;

  int  rval = -1 ;

  JstActualParam *processedActualParams ;



  // _jst_debug is a global debug flag
  if ( getenv( "__JLAUNCHER_DEBUG" ) ) _jst_debug = JNI_TRUE ;

#if defined ( _WIN32 ) && defined ( _cwcompat )
  jst_cygwinInit() ;
#endif

  processedActualParams = jst_processInputParameters( argv + 1, argc - 1, (JstParamInfo*)gantParameters, terminatingSuffixes, JST_CYGWIN_PATH_CONVERSION ) ;

  MARK_PTR_FOR_FREEING( processedActualParams )

  classpath = getenv( "CLASSPATH" ) ;

  // add "." to the end of the used classpath. This is what the script launcher also does
  if ( classpath ) {

    classpath = jst_append( NULL, NULL, classpath, JST_PATH_SEPARATOR ".", NULL ) ;
    MARK_PTR_FOR_FREEING( classpath )

    extraProgramOptions[ 5 ] = classpath ;

  }

  gantHome = getGantHome() ;
  MARK_PTR_FOR_FREEING( gantHome )

  MARK_PTR_FOR_FREEING( jars[ 0 ] = findGantStartupJar( gantHome ) )


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // find out the groovy conf file to use

  gantConfFile = getenv( "GANT_CONF" ) ;

  if ( !gantConfFile ) {
    gantConfFile = jst_createFileName( gantHome, "conf", GANT_CONF_FILE, NULL ) ;
    MARK_PTR_FOR_FREEING( gantConfFile )
  }

  extraProgramOptions[ 3 ] = gantConfFile ;

  {
    char *groovyDConf = jst_append( NULL, NULL, "-Dgroovy.starter.conf=", gantConfFile, NULL ) ;
    MARK_PTR_FOR_FREEING( groovyDConf )


    if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions,
                                                (int)extraJvmOptionsCount++,
                                                &extraJvmOptionsSize,
                                                groovyDConf,
                                                NULL ) ) ) goto end ;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  javaHome = jst_findJavaHome( processedActualParams ) ;
  MARK_PTR_FOR_FREEING( javaHome )

  {
    char* toolsJarFile = jst_createFileName( javaHome, "lib", "tools.jar", NULL ) ;

    if ( !toolsJarFile ) goto end ;
    if ( jst_fileExists( toolsJarFile ) ) {
      char* toolsJarD = jst_append( NULL, NULL, "-Dtools.jar=", toolsJarFile, NULL ) ;

      if ( !toolsJarD ||
           !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, toolsJarD ) ||
           ! ( extraJvmOptions = appendJvmOption( extraJvmOptions,
                                                  (int)extraJvmOptionsCount++,
                                                  &extraJvmOptionsSize,
                                                  toolsJarD,
                                                  NULL ) ) ) {
        free( toolsJarFile ) ;
        goto end ;
      }

    }

    free( toolsJarFile ) ;
  }


  gantDHome = jst_append( NULL, NULL, "-Dgant.home=", gantHome, NULL ) ;
  MARK_PTR_FOR_FREEING( gantDHome )

  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions,
                                              (int)extraJvmOptionsCount++,
                                              &extraJvmOptionsSize,
                                              gantDHome,
                                              NULL ) ) ) goto end ;



  {
    char *groovyHome  = NULL,
         *groovyDHome = NULL ;
    jars[ 1 ] = findGroovyStartupJar( gantHome, JNI_FALSE ) ;

    if ( !jars[ 1 ] ) {
      groovyHome = getenv( "GROOVY_HOME" ) ;
      if ( !groovyHome ) {
        groovyHome = jst_findFromPath( GROOVY_EXECUTABLE, "bin", JNI_TRUE ) ;
        if ( errno ) goto end ;
#if defined( _WIN32 )
        if ( !groovyHome ) {
          groovyHome = jst_findFromPath( "groovy.bat", "bin", JNI_TRUE ) ;
          if ( errno ) goto end ;
        }
#endif
        if ( groovyHome ) { MARK_PTR_FOR_FREEING( groovyHome ) }
      }
      if ( groovyHome ) jars[ 1 ] = findGroovyStartupJar( groovyHome, JNI_FALSE ) ;
    }

    MARK_PTR_FOR_FREEING( jars[ 1 ] )

    groovyDHome = jst_append( NULL, NULL, "-Dgroovy.home=", groovyHome ? groovyHome : gantHome, NULL ) ;
    MARK_PTR_FOR_FREEING( groovyDHome )

    if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions,
                                                (int)extraJvmOptionsCount++,
                                                &extraJvmOptionsSize,
                                                groovyDHome,
                                                NULL ) ) ) goto end ;

  }


// FIXME - these two are essentially the same (handling groovy home and gant home). Refactor out the duplicate code.
  {
    char *antHome  = getenv( "ANT_HOME" ),
         *antDHome = NULL ;
    if ( !antHome ) {
      antHome = jst_findFromPath( ANT_EXECUTABLE, "bin", JNI_TRUE ) ;
      if ( errno ) goto end ;
#if defined( _WIN32 )
      if ( !antHome ) {
        antHome = jst_findFromPath( "ant.bat", "bin", JNI_TRUE ) ;
        if ( errno ) goto end ;
      }
#endif
      if ( antHome ) { MARK_PTR_FOR_FREEING( antHome ) }
    }

    if ( !antHome ) { fprintf( stderr, "error: could not locate ant installation\n" ) ; goto end ; }

    antDHome = jst_append( NULL, NULL, "-Dant.home=", antHome, NULL ) ;
    MARK_PTR_FOR_FREEING( antDHome )

    if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions,
                                                (int)extraJvmOptionsCount++,
                                                &extraJvmOptionsSize,
                                                antDHome,
                                                NULL ) ) ) goto end ;

  }


  MARK_PTR_FOR_FREEING( extraJvmOptions )


  // populate the startup parameters

  options.javaHome            = javaHome ;
  options.jvmSelectStrategy   = JST_CLIENT_FIRST ;
  options.javaOptsEnvVar      = "JAVA_OPTS" ;
  options.initialClasspath    = NULL ;
  options.unrecognizedParamStrategy = JST_UNRECOGNIZED_TO_JVM ;
  options.parameters          = processedActualParams ;
  options.jvmOptions          = extraJvmOptions ;
  options.numJvmOptions       = (int)extraJvmOptionsCount ;
  options.extraProgramOptions = extraProgramOptions ;
  options.mainClassName       = "org/codehaus/groovy/tools/GroovyStarter" ;
  options.mainMethodName      = "main" ;
  options.jarDirs             = NULL ;
  options.jars                = jars ;
  options.classpathStrategy   = JST_NORMAL_CLASSPATH ;

#if defined ( _WIN32 ) && defined ( _cwcompat )
  jst_cygwinRelease() ;
#endif

  rval = jst_launchJavaApp( &options ) ;


end:

  jst_freeAll( &dynReservedPointers ) ;

  return rval ;

}
