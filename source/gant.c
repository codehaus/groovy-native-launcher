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
#include "jst_stringutils.h"

#if defined( _WIN32 ) && defined( _cwcompat )
#  include "jst_cygwin_compatibility.h"
#endif

#define GANT_CONF_FILE "gant-starter.conf"


static int gantJarSelect( const char* dirName, const char* fileName ) {
  int result = JNI_FALSE ;
  size_t fileNameLen = strlen( fileName ) ;
  // we are looking for gant-[0-9]+\.+[0-9]+.*\.jar. As tegexes
  // aren't available, we'll just check that the char following
  // gant- is a digit
  if ( fileNameLen >= 9 ) result = isdigit( fileName[ 5 ] ) ;

  return result ;
}

static int groovyJarSelect( const char* dirName, const char* fileName ) {
  int result = strcmp( "groovy-starter.jar", fileName ) == 0 ;
  if ( !result ) result = jst_startsWith( fileName, "groovy-all-" ) ;
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




static int startGant( int argc, char** argv ) {

  JavaLauncherOptions options ;

  JstJvmOptions extraJvmOptions ;
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



  jst_initDebugState() ;

  memset( &extraJvmOptions, 0, sizeof( extraJvmOptions ) ) ;

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


    if ( !appendJvmOption( &extraJvmOptions, groovyDConf, NULL ) ) goto end ;
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
           !appendJvmOption( &extraJvmOptions, toolsJarD, NULL ) ) {
        free( toolsJarFile ) ;
        goto end ;
      }

    }

    free( toolsJarFile ) ;
  }


  gantDHome = jst_append( NULL, NULL, "-Dgant.home=", gantHome, NULL ) ;
  MARK_PTR_FOR_FREEING( gantDHome )

  if ( !appendJvmOption( &extraJvmOptions, gantDHome, NULL ) ) goto end ;



  {
    char *groovyHome  = NULL,
         *groovyDHome = NULL ;
    jars[ 1 ] = findGroovyStartupJar( gantHome, JNI_FALSE ) ;

    if ( !jars[ 1 ] ) {
      groovyHome = getenv( "GROOVY_HOME" ) ;
      if ( !groovyHome ) {
        groovyHome = jst_findFromPath( GROOVY_EXECUTABLE, validateThatFileIsInBinDir ) ;
        if ( groovyHome ) groovyHome = jst_pathToParentDir( groovyHome ) ;
        if ( errno ) goto end ;
#if defined( _WIN32 )
        if ( !groovyHome ) {
          groovyHome = jst_findFromPath( "groovy.bat", validateThatFileIsInBinDir ) ;
          if ( groovyHome ) groovyHome = jst_pathToParentDir( groovyHome ) ;
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

    if ( !appendJvmOption( &extraJvmOptions, groovyDHome, NULL ) ) goto end ;

  }


// FIXME - these two are essentially the same (handling groovy home and gant home). Refactor out the duplicate code.
  {
    char *antHome  = getenv( "ANT_HOME" ),
         *antDHome = NULL ;
    if ( !antHome ) {
      antHome = jst_findFromPath( ANT_EXECUTABLE, validateThatFileIsInBinDir ) ;
      if ( antHome ) antHome = jst_pathToParentDir( antHome ) ;
      if ( errno ) goto end ;
#if defined( _WIN32 )
      if ( !antHome ) {
        antHome = jst_findFromPath( "ant.bat", validateThatFileIsInBinDir ) ;
        if ( antHome ) antHome = jst_pathToParentDir( antHome ) ;
        if ( errno ) goto end ;
      }
#endif
      if ( antHome ) { MARK_PTR_FOR_FREEING( antHome ) }
    }

    if ( !antHome ) { fprintf( stderr, "error: could not locate ant installation\n" ) ; goto end ; }

    antDHome = jst_append( NULL, NULL, "-Dant.home=", antHome, NULL ) ;
    MARK_PTR_FOR_FREEING( antDHome )

    if ( !appendJvmOption( &extraJvmOptions, antDHome, NULL ) ) goto end ;

  }


  MARK_PTR_FOR_FREEING( extraJvmOptions.options )


  // populate the startup parameters

  options.javaHome            = javaHome ;
  options.jvmSelectStrategy   = JST_CLIENT_FIRST ;
  options.initialClasspath    = NULL ;
  options.unrecognizedParamStrategy = JST_UNRECOGNIZED_TO_JVM ;
  options.parameters          = processedActualParams ;
  options.jvmOptions          = &extraJvmOptions ;
  options.extraProgramOptions = extraProgramOptions ;
  options.mainClassName       = "org/codehaus/groovy/tools/GroovyStarter" ;
  options.mainMethodName      = "main" ;
  options.jarDirs             = NULL ;
  options.jars                = jars ;
  options.classpathStrategy   = JST_NORMAL_CLASSPATH ;
  options.pointersToFreeBeforeRunningMainMethod = &dynReservedPointers ;

#if defined ( _WIN32 ) && defined ( _cwcompat )
  // see comments in groovy.c
//  jst_cygwinRelease() ;
#endif

  rval = jst_launchJavaApp( &options ) ;


end:

  jst_freeAll( &dynReservedPointers ) ;

  return rval ;

}

#if defined ( _WIN32 ) && defined ( _cwcompat )

#include "jst_cygwin_compatibility.h"

int main( int argc, char** argv ) {
  return runCygwinCompatibly( argc, argv, startGant ) ;
}
#else
int main( int argc, char** argv ) {
  return startGant( argc, argv ) ;
}
#endif

