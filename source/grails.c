//  A native launcher for Grails
//
//  Copyright (c) 2006 Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
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
//  $Revision: 15105 $
//  $Date: 2009-01-19 16:07:30 +0200 (ma, 19 tammi 2009) $

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


#include <jni.h>

#include "jvmstarter.h"
#include "jst_dynmem.h"
#include "jst_fileutils.h"
#include "jst_stringutils.h"

#if defined( _WIN32 ) && defined( _cwcompat )
#  include "jst_cygwin_compatibility.h"
#endif

#define GROOVY_CONF_FILE "groovy-starter.conf"


static int grailsJarSelect( const char* dirname, const char* fileName ) {
  int result = jst_startsWith( fileName, "groovy-all-" ) ;
  if ( !result ) {
    result = jst_startsWith( fileName, "grails-bootstrap-" ) || // grails 1.1.x-
             jst_startsWith( fileName, "grails-cli-" ) ;  // grails 1.0.x
  }
  return result ;
}

/** Checks that the given dir is a valid groovy dir.
 * If false is returned, check errno to see if there was an error (in mem allocation) */
static int isValidGrailsHome( const char* dir ) {
  char *gconfFile = NULL ;
  jboolean rval = JNI_FALSE ;

  errno = 0 ;
// FIXME - check this logic
  gconfFile = jst_createFileName( dir, "conf", GROOVY_CONF_FILE, NULL ) ;
  if ( gconfFile ) {
    rval = jst_fileExists( gconfFile ) ? JNI_TRUE : JNI_FALSE ;
    free( gconfFile ) ;
  }

  if ( _jst_debug ) fprintf( stderr, "debug: %s is %sa valid grails installation\n", dir, rval ? "" : "not " ) ;

  return rval ;
}


/** returns null on error, otherwise pointer to grails home.
 * First tries to see if the current executable is located in a grails installation's bin directory. If not, grails
 * home env var is looked up. If neither succeed, an error msg is printed.
 * freeing the returned pointer must be done by the caller. */
static char* getGrailsHome() {

  return jst_getAppHome( JST_USE_PARENT_OF_EXEC_LOCATION_AS_HOME, "GRAILS_HOME", &isValidGrailsHome ) ;

}

// ms visual c++ compiler does not support compound literals
// ( i.e. defining e.g. arrays inline, e.g. { (char*[]){ "hello", NULL }, 12 } ),
// so for sake of maximum portability the initialization of all the arrays containing
// the parameter names must be done clumsily here

static const char* grailsJavahomeParam[]   = { "-jh", "--javahome", NULL } ;
static const char* grailsClientParam[]     = { "-client", NULL } ;
static const char* grailsServerParam[]     = { "-server", NULL } ;

// the parameters accepted by grails
static const JstParamInfo grailsParameters[] = {
  // native launcher supported extra params
  { grailsJavahomeParam,   JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_PATH_CONVERT },
  { grailsClientParam,     JST_SINGLE_PARAM, JST_IGNORE },
  { grailsServerParam,     JST_SINGLE_PARAM, JST_IGNORE },
  { NULL,          0,                0 }
} ;


static const JstParamInfo noParameters[] = {
  { NULL,          0,                0 }
} ;


int startGrails( int argc, char** argv ) {

  JavaLauncherOptions options ;

  JstJvmOptions extraJvmOptions ;

  char *grailsHome      = NULL,
       *javaHome        = NULL ;

  void** dynReservedPointers = NULL ; // free all reserved pointers at at end of func
  size_t dreservedPtrsSize   = 0 ;

// FIXME - go through this source file and replace with the new definition from jst_dynmem.c
#if defined( MARK_PTR_FOR_FREEING )
#  undef MARK_PTR_FOR_FREEING
#endif
# define MARK_PTR_FOR_FREEING( garbagePtr ) if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, ( garbagePtr ) ) ) goto end ;

  /** terminatingSuffixes contains the suffixes that, if matched, indicate that the matching param and all the rest of the params
   * are launcheeParams, e.g. {".groovy", ".gy", NULL}.
   * The significance of this is marginal, as any param w/ no preceding "-" is terminating. So, this is only significant
   * if the terminating option has "-" as prefix, but is not one of the enumerated options. Usually this would be
   * a file name associated w/ the app, e.g. "-foobar.groovy". As file names do not usually begin w/ "-" this is rather unimportant. */
  const char *terminatingSuffixes[] = { NULL } ;
  char *extraProgramOptions[]       = { NULL } ;

  int  rval = -1 ;

  JVMSelectStrategy jvmSelectStrategy = JST_CLIENT_FIRST ;

  JstActualParam *processedActualParams = NULL ;

  JarDirSpecification jardirs[ 3 ] ;


  jst_initDebugState() ;

  memset( &extraJvmOptions, 0, sizeof( extraJvmOptions ) ) ;

#if defined ( _WIN32 ) && defined ( _cwcompat )
  jst_cygwinInit() ;
#endif

  processedActualParams = jst_processInputParameters( argv + 1, argc - 1, (JstParamInfo*)grailsParameters, terminatingSuffixes, JST_CYGWIN_PATH_CONVERSION ) ;

  MARK_PTR_FOR_FREEING( processedActualParams )



#if defined( GRAILS_HOME )
  grailsHome = JST_STRINGIZER( GROOVY_HOME ) ;
  if ( _jst_debug ) fprintf( stderr, "debug: using grails home set at compile time: %s\n", grailsHome ) ;
#else
  grailsHome = getGrailsHome() ;
  MARK_PTR_FOR_FREEING( grailsHome )
#endif


#if defined( JAVA_HOME )
  javaHome = JST_STRINGIZER( JAVA_HOME ) ;
  if ( _jst_debug ) fprintf( stderr, "debug: using java home set at compile time: %s\n", javaHome ) ;
#else
  javaHome = jst_findJavaHome() ;
  MARK_PTR_FOR_FREEING( javaHome )
#endif

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

  {
    char *grailsDHome = jst_append( NULL, NULL, "-Dgrails.home=", grailsHome, NULL ) ;
    MARK_PTR_FOR_FREEING( grailsDHome )
    if ( !appendJvmOption( &extraJvmOptions, grailsDHome, NULL ) ) goto end ;
  }


  MARK_PTR_FOR_FREEING( extraJvmOptions.options )

  // TODO: extract setting the jar lookup into a separate func?
  {
    char* jardir = jst_createFileName( grailsHome, "lib", NULL ) ;
    if ( !jardir ) goto end ;
    jardirs[ 0 ].name = jardir ;
    MARK_PTR_FOR_FREEING( jardir )
  }
  jardirs[ 0 ].fetchRecursively = JNI_FALSE ;
  jardirs[ 0 ].filter = &grailsJarSelect ;

  {
    char* jardir = jst_createFileName( grailsHome, "dist", NULL ) ;
    if ( !jardir ) goto end ;
    jardirs[ 1 ].name = jardir ;
    MARK_PTR_FOR_FREEING( jardir )
  }
  jardirs[ 1 ].fetchRecursively = JNI_FALSE ;
  // TODO: make separate grails jar select and groovy jar select
  jardirs[ 1 ].filter = &grailsJarSelect ;

  jardirs[ 2 ].name = NULL ;


  {
    char* javaOptsFromEnvVar = getenv( "JAVA_OPTS" ) ;
    if ( javaOptsFromEnvVar ) {
      MARK_PTR_FOR_FREEING( javaOptsFromEnvVar = jst_strdup( javaOptsFromEnvVar ) )
      if ( !handleJVMOptsString( javaOptsFromEnvVar, &extraJvmOptions, &jvmSelectStrategy ) ) goto end ;
    }
  }


  // populate the startup parameters
  // first, set the memory to 0. This is just a precaution, as NULL (0) is a sensible default value for many options.
  // TODO: remove this, it gives false sense of safety...
  // memset( &options, 0, sizeof( JavaLauncherOptions ) ) ;

  options.javaHome            = javaHome ;
  options.jvmSelectStrategy   = jvmSelectStrategy ;
  options.initialClasspath    = NULL ;
  options.unrecognizedParamStrategy = JST_UNRECOGNIZED_TO_JVM ;
  options.parameters          = processedActualParams ;
  options.jvmOptions          = &extraJvmOptions ;
  options.extraProgramOptions = extraProgramOptions ;
  options.mainClassName       = "org/codehaus/groovy/grails/cli/support/GrailsStarter" ;
  options.mainMethodName      = "main" ;
  options.jarDirs             = jardirs ;
  options.jars                = NULL ;
  options.classpathStrategy   = jst_getParameterValue( processedActualParams, "--quickstart" ) ? JST_BOOTSTRAP_CLASSPATH_A : JST_NORMAL_CLASSPATH ;
  options.pointersToFreeBeforeRunningMainMethod = &dynReservedPointers ;


  rval = jst_launchJavaApp( &options ) ;

#if defined ( _WIN32 ) && defined ( _cwcompat )
  // see groovy.c for discussion on why this is removed
  // jst_cygwinRelease() ;
#endif

end:

  jst_freeAll( &dynReservedPointers ) ;

  return rval ;

}

#if defined ( _WIN32 ) && defined ( _cwcompat )

#include "jst_cygwin_compatibility.h"

int main( int argc, char** argv ) {
  return runCygwinCompatibly( argc, argv, startGrails ) ;
}
#else
int main( int argc, char** argv ) {
  return startGrails( argc, argv ) ;
}
#endif
