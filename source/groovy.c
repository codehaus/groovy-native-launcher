//  Groovy -- A native launcher for Groovy
//
//  Copyright (c) 2006,2009 Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
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

#include <assert.h>

#if defined ( _WIN32 )
#  include <Windows.h>
#  define strcasecmp stricmp
#else
#  include <strings.h>
#endif

#include "applejnifix.h"
#include <jni.h>

#include "jvmstarter.h"
#include "jst_dynmem.h"
#include "jst_fileutils.h"
#include "jst_stringutils.h"

#if defined( _WIN32 ) && defined( _cwcompat )
#  include "jst_cygwin_compatibility.h"
#endif

#define GROOVY_CONF_FILE "groovy-starter.conf"


// ms visual c++ compiler does not support compound literals
// ( i.e. defining e.g. arrays inline, e.g. { (char*[]){ "hello", NULL }, 12 } ),
// so for sake of maximum portability the initialization of all the arrays containing
// the parameter names must be done clumsily here

static const char* groovyLinebylineParam[] = { "-p", NULL } ;
static const char* groovyDefineParam[]     = { "-D", "--define",    NULL } ;
static const char* groovyAutosplitParam[]  = { "-a", "--autosplit", NULL } ;
static const char* groovyCharsetParam[]    = { "-c", "--encoding",  NULL } ;
static const char* groovyDebugParam[]      = { "-d", "--debug",     NULL } ;
static const char* groovyOnelinerParam[]   = { "-e", NULL } ;
static const char* groovyHelpParam[]       = { "-h", "--help", NULL } ;
static const char* groovyModinplaceParam[] = { "-i", NULL } ;
static const char* groovyListenParam[]     = { "-l", NULL } ;
static const char* groovyLinevarParam[]    = { "-n", NULL } ;
static const char* groovyVersionParam[]    = { "-v", "--version", NULL } ;
static const char* groovyClasspathParam[]  = { "-cp", "-classpath", "--classpath", NULL } ;
static const char* groovyJavahomeParam[]   = { "-jh", "--javahome", NULL } ;
static const char* groovyConfParam[]       = { "--conf",  NULL } ;
static const char* groovyClientParam[]     = { "-client", NULL } ;
static const char* groovyServerParam[]     = { "-server", NULL } ;
static const char* groovyQuickStartParam[] = { "--quickstart", NULL } ;

// the parameters accepted by groovy (note that -cp / -classpath / --classpath & --conf
// are handled separately below
static const JstParamInfo groovyParameters[] = {
  { groovyLinebylineParam, JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { groovyDefineParam,     JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovyAutosplitParam,  JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovyCharsetParam,    JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovyDebugParam,      JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { groovyOnelinerParam,   JST_DOUBLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { groovyHelpParam,       JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { groovyModinplaceParam, JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovyListenParam,     JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovyLinevarParam,    JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { groovyVersionParam,    JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  // native launcher supported extra params
  { groovyClasspathParam,  JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_PATHLIST_CONVERT },
  { groovyJavahomeParam,   JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_PATH_CONVERT },
  { groovyConfParam,       JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_PATH_CONVERT },
  { groovyClientParam,     JST_SINGLE_PARAM, JST_IGNORE },
  { groovyServerParam,     JST_SINGLE_PARAM, JST_IGNORE },
  { groovyQuickStartParam, JST_SINGLE_PARAM, JST_IGNORE },
  { NULL,          0,                0 }
} ;


static const char* groovycEncodingParam[]   = { "--encoding", NULL } ;
static const char* groovycFParam[]          = { "-F", NULL } ;
static const char* groovycJParam[]          = { "-J", NULL } ;
static const char* groovycClassplaceParam[] = { "-d", NULL } ;
static const char* groovycExceptionParam[]  = { "-e", "--exception", NULL } ;
static const char* groovycVersionParam[]    = { "-v", "--version", NULL } ;
static const char* groovycHelpParam[]       = { "-h", "--help", NULL } ;
static const char* groovycJointcompParam[]  = { "-j", "--jointCompilation", NULL } ;

static const JstParamInfo groovycParameters[] = {
  { groovycEncodingParam,   JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovycFParam,          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovycJParam,          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovycClassplaceParam, JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovycExceptionParam,  JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { groovycHelpParam,       JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { groovycJointcompParam,  JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { groovycVersionParam,    JST_SINGLE_PARAM, JST_TO_LAUNCHEE },

  // native launcher supported extra params
  { groovyClasspathParam,   JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_PATHLIST_CONVERT },
  { groovyJavahomeParam,    JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_PATH_CONVERT },
  { groovyConfParam,        JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_PATH_CONVERT },
  { groovyClientParam,      JST_SINGLE_PARAM, JST_IGNORE },
  { groovyServerParam,      JST_SINGLE_PARAM, JST_IGNORE },
  { NULL,                   0,                0 }
} ;


static const char* gantUsecacheParam[]    = { "-c", "--usecache", NULL } ;
static const char* gantDryrunParam[]      = { "-n", "--dry-run",  NULL } ;
static const char* gantDefineParam[]      = { "-D", NULL } ;
static const char* gantClasspathParam[]   = { "-P", "--classpath", NULL } ;
static const char* gantTargetsParam[]     = { "-T", "--targets",   NULL } ;
static const char* gantVersionParam[]     = { "-V", "--version",   NULL } ;
static const char* gantCachedirParam[]    = { "-d", "--cachedir",  NULL } ;
static const char* gantGantfileParam[]    = { "-f", "--gantfile",  NULL } ;
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
  { groovyHelpParam,      JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { gantGantlibParam,     JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { gantProjecthelpParam, JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantQuietParam,       JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantSilentParam,      JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantVerboseParam,     JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { NULL,                 0,                0 }
} ;


static const char* groovyshColorParam[]    = { "-C", "--color", NULL } ;
static const char* groovyshTerminalParam[] = { "-T", "--terminal", NULL } ;

static const JstParamInfo groovyshParameters[] = {
  { groovyshColorParam,    JST_PREFIX_PARAM, JST_TO_LAUNCHEE },
  { groovyDefineParam,     JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { groovyshTerminalParam, JST_PREFIX_PARAM, JST_TO_LAUNCHEE },
  { gantVersionParam,      JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { groovyDebugParam,      JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { groovyHelpParam,       JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { gantQuietParam,        JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { gantVerboseParam,      JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { NULL,                  0,                0 }
} ;


static const char* java2groovyHelpParam[] = { "-h", NULL } ;

static const JstParamInfo java2groovyParameters[] = {
  { java2groovyHelpParam, JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { NULL,                 0,                0 }
} ;

static const JstParamInfo groovyStarterParameters[] = {
  { groovyClasspathParam,  JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_PATHLIST_CONVERT },
  { NULL,                  0,                0 }
} ;

static const JstParamInfo noParameters[] = {
  { NULL,          0,                0 }
} ;

typedef struct {
  char* executableName ;
  char* mainClass ;
  JstParamInfo* parameterInfos ;
  JstUnrecognizedParamStrategy unrecognizedParamStrategy ;
} GroovyApp ;

static GroovyApp groovyApps[] = {
  { "groovy",        "groovy.ui.GroovyMain",                           (JstParamInfo*)groovyParameters,        JST_UNRECOGNIZED_TO_JVM },
  { "groovyc",       "org.codehaus.groovy.tools.FileSystemCompiler",   (JstParamInfo*)groovycParameters,       JST_UNRECOGNIZED_TO_JVM },
  { "gant",          "gant.Gant",                                      (JstParamInfo*)gantParameters,          JST_UNRECOGNIZED_TO_JVM },
  { "groovysh",      "org.codehaus.groovy.tools.shell.Main",           (JstParamInfo*)groovyshParameters,      JST_UNRECOGNIZED_TO_JVM },
  { "grape",         "org.codehaus.groovy.tools.GrapeMain",            (JstParamInfo*)noParameters,            JST_UNRECOGNIZED_TO_JVM },
  { "java2groovy",   "org.codehaus.groovy.antlr.java.Java2GroovyMain", (JstParamInfo*)java2groovyParameters,   JST_UNRECOGNIZED_TO_JVM },
  { "groovyconsole", "groovy.ui.Console",                              (JstParamInfo*)noParameters,            JST_UNRECOGNIZED_TO_JVM },
  { "graphicspad",   "groovy.swing.j2d.app.GraphicsPad",               (JstParamInfo*)noParameters,            JST_UNRECOGNIZED_TO_JVM },
  { "startgroovy",   NULL,                                             (JstParamInfo*)groovyStarterParameters, JST_UNRECOGNIZED_TO_APP },

  { NULL, NULL, NULL, 0 }
} ;



static int groovyJarSelect( const char* dirName, const char* fileName ) {
  int result = strcmp( "groovy-starter.jar", fileName ) == 0 ;
  if ( !result ) {
    size_t fileNameLen = strlen( fileName ) ;
    // we are looking for groovy-[0-9]+\.+[0-9]+.*\.jar. As tegexes
    // aren't available, we'll just check that the char following
    // groovy- is a digit
    // note that having && ( memcmp( "groovy-", fileName, 7 ) == 0 ) in the condition is unnecessary as that is taken care of in findGroovyStartupJar
    if ( fileNameLen >= 12 ) result = isdigit( fileName[ 7 ] ) ;
  }
  return result ;
}

/** If groovy-starter.jar exists, returns path to that. If not, the first jar whose name starts w/ "groovy-x" (where x is a digit) is returned.
 * The previous is appropriate for groovy <= 1.0, the latter for groovy >= 1.1
 * Returns NULL on error, otherwise dynallocated string (which caller must free). */
static char* findGroovyStartupJar( const char* groovyHome ) {
  return findStartupJar( groovyHome, "lib", "groovy-", "groovy", &groovyJarSelect ) ;
}

/** Checks that the given dir is a valid groovy dir.
 * If false is returned, check errno to see if there was an error (in mem allocation) */
static int isValidGroovyHome( const char* dir ) {
  char *gconfFile = NULL ;
  jboolean isValid = JNI_FALSE ;

  assert( dir ) ;

  errno = 0 ;

  gconfFile = jst_createFileName( dir, "conf", GROOVY_CONF_FILE, NULL ) ;
  if ( gconfFile ) {
    isValid = jst_fileExists( gconfFile ) ? JNI_TRUE : JNI_FALSE ;
    free( gconfFile ) ;
  }

  if ( _jst_debug ) fprintf( stderr, "debug: %s is %sa valid groovy installation\n", dir, isValid ? "" : "not " ) ;

  return isValid ;
}

/** returns null on error, otherwise pointer to groovy home.
 * First tries to see if the current executable is located in a groovy installation's bin directory. If not, groovy
 * home env var is looked up. If neither succeed, an error msg is printed.
 * freeing the returned pointer must be done by the caller. */
char* getGroovyHome() {
// TODO: add looking up "standard locations" if this fails
  return jst_getAppHome( JST_USE_PARENT_OF_EXEC_LOCATION_AS_HOME, "GROOVY_HOME", &isValidGroovyHome ) ;

}


static GroovyApp* recognizeGroovyApp( const char* cmd ) {
  GroovyApp *app = (GroovyApp*)&groovyApps[ 0 ], // default to being groovy
            *tmp ;
  char *copyOfCmd,
       *execName ;

  JST_STRDUPA( copyOfCmd, cmd ) ;
  if ( !copyOfCmd ) return NULL ;

  execName = jst_extractProgramName( copyOfCmd, JNI_TRUE ) ;

  for ( tmp = (GroovyApp*)groovyApps ; tmp->executableName ; tmp++ ) {
    if ( strcasecmp( execName, tmp->executableName ) == 0 ) {
      app = tmp ;
      break ;
    }
  }

  if ( _jst_debug ) {
    fprintf( stderr, "debug: starting groovy app %s", app->executableName ) ;
    if ( strcasecmp( app->executableName, execName ) ) fprintf( stderr, " (executable name %s)", execName ) ;
    fprintf( stderr, "\n" ) ;
  }

  if ( strcasecmp( "groovysh", app->executableName ) == 0 && getenv( "OLDSHELL" ) ) {
    app->mainClass = "groovy.ui.InteractiveShell" ;
  }

  jst_freea( copyOfCmd ) ;

  return app ;
}

static char* createScriptNameDParam( const char* scriptName ) {

  char* scriptNameD = NULL ;

  if ( scriptName ) {

    char *scriptNameTmp = jst_fullPathName( scriptName ) ;

    if ( !scriptNameTmp ) return NULL ;

    scriptNameD = jst_append( NULL, NULL, "-Dscript.name=", scriptNameTmp, NULL ) ;

    if ( scriptNameTmp != scriptName ) {
      jst_free( scriptNameTmp ) ;
    }

  }

  return scriptNameD ;
}

static void printProgramArgs( int argc, char** argv ) {
  int i = 0 ;
  fprintf( stderr, "parameters passed to the launcher:\n" ) ;
  for ( ; i < argc ; i++ ) fprintf( stderr, "\t%d : %s\n", i, argv[ i ] ) ;
}

int startGroovy( int argc, char** argv ) {

  JavaLauncherOptions options ;

  JstJvmOptions extraJvmOptions ;
  char *groovyConfFile  = NULL,
       *groovyDConf     = NULL, // the -Dgroovy.conf=something to pass to the jvm
       *groovyHome      = NULL,
       *groovyDHome     = NULL, // the -Dgroovy.home=something to pass to the jvm
       *classpath       = NULL,
       *javaHome        = NULL ;

  void** dynReservedPointers = NULL ; // free all reserved pointers at at end of func
  size_t dreservedPtrsSize   = 0 ;

  /** terminatingSuffixes contains the suffixes that, if matched, indicate that the matching param and all the rest of the params
   * are launcheeParams, e.g. {".groovy", ".gy", NULL}.
   * The significance of this is marginal, as any param w/ no preceding "-" is terminating. So, this is only significant
   * if the terminating option has "-" as prefix, but is not one of the enumerated options. Usually this would be
   * a file name associated w/ the app, e.g. "-foobar.groovy". As file names do not usually begin w/ "-" this is rather unimportant. */
  const char *terminatingSuffixes[] = { ".groovy", ".gvy", ".gy", ".gsh", NULL } ;
  char *extraProgramOptions[]       = { "--main", "groovy.ui.GroovyMain", "--conf", NULL, "--classpath", ".", NULL },
       *jars[]                      = { NULL, NULL } ;

  int  numArgs = argc - 1 ;

  int  exitCode = -1,
       numSkippedCommandLineParams = 1 ;

  JVMSelectStrategy jvmSelectStrategy = JST_CLIENT_FIRST ;

  jboolean displayHelp          = ( ( numArgs == 0 )                       ||
                                    ( strcmp( argv[ 1 ], "-h"     ) == 0 ) ||
                                    ( strcmp( argv[ 1 ], "--help" ) == 0 )
                                  ) ? JNI_TRUE : JNI_FALSE ;

  JstActualParam *processedActualParams = NULL ;

  GroovyApp* groovyApp = NULL ;

  jst_initDebugState() ;

  if ( _jst_debug ) printProgramArgs( argc, argv ) ;

  memset( &extraJvmOptions, 0, sizeof( extraJvmOptions ) ) ;

#if defined ( _WIN32 ) && defined ( _cwcompat )
  jst_cygwinInit() ;
#endif

  groovyApp = recognizeGroovyApp( argv[ 0 ] ) ;

  if ( !groovyApp ) goto end ;

  if ( strcasecmp( "startgroovy", groovyApp->executableName ) == 0 ) {
    if ( numArgs < 2 ) {
      fprintf( stderr, "error: too few parameters to startGroovy\n" ) ;
      goto end ;
    }
    groovyApp->mainClass = argv[ 2 ] ;
    numSkippedCommandLineParams += 2 ;
    numArgs -= 2 ;
  }

  extraProgramOptions[ 1 ] = groovyApp->mainClass ;


  if ( displayHelp && strcasecmp( "groovy", groovyApp->executableName ) != 0 ) displayHelp = JNI_FALSE ;

  processedActualParams = jst_processInputParameters( argv + numSkippedCommandLineParams, argc - numSkippedCommandLineParams, groovyApp->parameterInfos, terminatingSuffixes, JST_CYGWIN_PATH_CONVERSION ) ;

  MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, processedActualParams, NULL_MEANS_ERROR )

  // set -Dscript.name system property if applicable
  if ( numArgs > 0 ) {
    char* scriptName = jst_getParameterAfterTermination( processedActualParams, 0 ) ;
    if ( scriptName ) {
      char* scriptNameD = createScriptNameDParam( scriptName ) ;
      MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, scriptNameD, NULL_MEANS_ERROR )
      if ( !appendJvmOption( &extraJvmOptions, scriptNameD, NULL ) ) goto end ;
    }
  }

  classpath = jst_getParameterValue( processedActualParams, "-cp" ) ;
  if ( !classpath ) {
    classpath = getenv( "CLASSPATH" ) ;
  }

  // add "." to the end of the used classpath. This is what the script launcher also does
  if ( classpath ) {

    classpath = jst_append( NULL, NULL, classpath, JST_PATH_SEPARATOR ".", NULL ) ;
    MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, classpath, NULL_MEANS_ERROR )

    extraProgramOptions[ 5 ] = classpath ;

  }

#if defined( GROOVY_HOME )
  // TODO: for some reason this won't accept something that begins with a "/"
  groovyHome = JST_STRINGIZER( GROOVY_HOME ) ;
  if ( _jst_debug ) fprintf( stderr, "debug: using groovy home set at compile time: %s\n", groovyHome ) ;
#else
  groovyHome = getGroovyHome() ;
  MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, groovyHome, NULL_IS_NOT_ERROR )
#endif

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // find out the groovy conf file to use

  groovyConfFile = jst_getParameterValue( processedActualParams, "--conf" ) ;

  if ( !groovyConfFile  ) groovyConfFile = getenv( "GROOVY_CONF" ) ;

  if ( !groovyConfFile ) {
    groovyConfFile = jst_createFileName( groovyHome, "conf", GROOVY_CONF_FILE, NULL ) ;
    MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, groovyConfFile, NULL_MEANS_ERROR )
  }

#if defined( GROOVY_STARTUP_JAR )
  jars[ 0 ] = JST_STRINGIZER( GROOVY_STARTUP_JAR ) ;
  if ( _jst_debug ) fprintf( stderr, "debug: using groovy startup jar set at compile time: %s\n", jars[ 0 ] ) ;
#else
  MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, jars[ 0 ] = findGroovyStartupJar( groovyHome ), NULL_IS_NOT_ERROR )
#endif
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // set -Dgroovy.home and -Dgroovy.starter.conf as jvm options


  extraProgramOptions[ 3 ] = groovyConfFile ;

#if defined( JAVA_HOME )
  javaHome = JST_STRINGIZER( JAVA_HOME ) ;
  if ( _jst_debug ) fprintf( stderr, "debug: using java home set at compile time: %s\n", javaHome ) ;
#else
  errno = 0 ;
  if ( !( javaHome = getJavaHomeFromParameter( processedActualParams, "-jh" ) ) && !errno ) {
    javaHome = jst_findJavaHome() ;
  }
  MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, javaHome, NULL_IS_NOT_ERROR )
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

  groovyDConf = jst_append( NULL, NULL, "-Dgroovy.starter.conf=", groovyConfFile, NULL ) ;
  MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, groovyDConf, NULL_MEANS_ERROR )


  if ( !appendJvmOption( &extraJvmOptions, groovyDConf, NULL ) ) goto end ;

  groovyDHome = jst_append( NULL, NULL, "-Dgroovy.home=", groovyHome, NULL ) ;
  MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, groovyDHome, NULL_MEANS_ERROR )

  if ( !appendJvmOption( &extraJvmOptions, groovyDHome, NULL ) ) goto end ;

  {
    char* javaOptsFromEnvVar = getenv( "JAVA_OPTS" ) ;
    if ( javaOptsFromEnvVar ) {
      MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, javaOptsFromEnvVar = jst_strdup( javaOptsFromEnvVar ), NULL_MEANS_ERROR )
      if ( !handleJVMOptsString( javaOptsFromEnvVar, &extraJvmOptions, &jvmSelectStrategy ) ) goto end ;
    }
  }




  MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, extraJvmOptions.options, NULL_MEANS_ERROR )


  if ( jst_getParameterValue( processedActualParams, "-client" ) ) {
    jvmSelectStrategy = JST_CLIENTVM ;
  } else if ( jst_getParameterValue( processedActualParams, "-server" ) ) {
    jvmSelectStrategy = JST_SERVERVM ;
  }


  // populate the startup parameters
  // first, set the memory to 0. This is just a precaution, as NULL (0) is a sensible default value for many options.
  // TODO: remove this, it gives false sense of safety...
  // memset( &options, 0, sizeof( JavaLauncherOptions ) ) ;

  options.javaHome            = javaHome ;
  options.jvmSelectStrategy   = jvmSelectStrategy ;
  options.initialClasspath    = NULL ;
  options.unrecognizedParamStrategy = groovyApp->unrecognizedParamStrategy ;
  options.parameters          = processedActualParams ;
  options.jvmOptions          = &extraJvmOptions ;
  options.extraProgramOptions = extraProgramOptions ;
  options.mainClassName       = "org/codehaus/groovy/tools/GroovyStarter" ;
  options.mainMethodName      = "main" ;
  options.jarDirs             = NULL ;
  options.jars                = jars ;
  options.classpathStrategy   = jst_getParameterValue( processedActualParams, "--quickstart" ) ? JST_BOOTSTRAP_CLASSPATH_A : JST_NORMAL_CLASSPATH ;
  options.pointersToFreeBeforeRunningMainMethod = &dynReservedPointers ;

  exitCode = jst_launchJavaApp( &options ) ;

  // In GROOVY-3340, SimonS reports that things do not work properly if Cygwin is released before the JVM is
  // launched, but things are fine the other way around.  This ordering doesn't matter except in the Cygwin
  // case, so it seems safe to make the change as requested.
  // See http://jira.codehaus.org/browse/GROOVY-3340

  // freeing this after running the java program does not make much sense - it will be freed by
  // the os at process exit anyway. The idea of freeing this manually before invoking the java program
  // was to free resources that are no longer needed.
//#if defined ( _WIN32 ) && defined ( _cwcompat )
//  jst_cygwinRelease() ;
//#endif

  if ( displayHelp ) { // add to the standard groovy help message
    fprintf( stderr, "\n"
    " -jh,--javahome <path to jdk/jre> makes groovy use the given jdk/jre\n"
    "                                 instead of the one pointed to by JAVA_HOME\n"
    " --conf <conf file>              use the given groovy conf file\n"
    "\n"
    " -cp,-classpath,--classpath <user classpath>\n"
    "                                 the classpath to use\n"
    " -client/-server                 to use a client/server VM\n"
    "\n"
    "In addition, you can give any parameters accepted by the jvm you are using, e.g.\n"
    "-Xmx<size> (see java -help and java -X for details)\n"
    "\n"
    ) ;
  }

end:

  jst_freeAll( &dynReservedPointers ) ;

  if ( _jst_debug ) fprintf( stderr, "debug: exiting %s with code %d\n", argv[ 0 ], exitCode ) ;

  return exitCode ;

}


#if defined ( _WIN32 ) && defined ( _cwcompat )

#include "jst_cygwin_compatibility.h"

int main( int argc, char** argv ) {
  return runCygwinCompatibly( argc, argv, startGroovy ) ;
}
#else

int main( int argc, char** argv ) {

  int rval ;

#if defined( _WIN32_NOT_USED_ATM )

  char **originalArgv = argv, // for debugging
       **utf8Argv ;
  int nArgs;
  LPWSTR *szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);

  if ( !szArglist ) {
    jst_printWinError( GetLastError() ) ;
    return -1 ;
  }

  // TODO: convert to utf-8

#endif

  rval = startGroovy( argc, argv ) ;

#if defined( _WIN32_NOT_USED_ATM )
  LocalFree( szArglist ) ;
#endif

  return rval ;
}

#endif


