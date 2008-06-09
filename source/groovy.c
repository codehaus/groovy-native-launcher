//  Groovy -- A native launcher for Groovy
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
#elif defined ( _WIN32 )

#  include <Windows.h>

#  if !defined( PATH_MAX )
#    define PATH_MAX MAX_PATH
#  endif

#endif


#include <jni.h>

#include "jvmstarter.h"
#include "jst_dynmem.h"
#include "jst_fileutils.h"

#if defined( _WIN32 ) && defined( _cwcompat )
#  include "jst_cygwin_compatibility.h"
#endif

#define GROOVY_CONF_FILE "groovy-starter.conf"


int groovyJarSelect( const char* fileName ) {
  int result = strcmp( "groovy-starter.jar", fileName ) == 0 ;
  if ( !result ) {
    size_t fileNameLen = strlen( fileName ) ;
    // we are looking for groovy-[0-9]+\.+[0-9]+.*\.jar. As tegexes 
    // aren't available, we'll just check that the char following 
    // groovy- is a digit
    if ( fileNameLen >= 8 ) result = isdigit( fileName[ 7 ] ) ;
  }
  return result ;
}


/** If groovy-starter.jar exists, returns path to that. If not, the first jar whose name starts w/ "groovy-x" (where x is a digit) is returned.
 * The previous is appropriate for groovy <= 1.0, the latter for groovy >= 1.1
 * Returns NULL on error, otherwise dynallocated string (which caller must free). */
static char* findGroovyStartupJar( const char* groovyHome ) {
  return findStartupJar( groovyHome, "lib", "groovy-", &groovyJarSelect ) ;
}

/** Checks that the given dir is a valid groovy dir.
 * If false is returned, check errno to see if there was an error (in mem allocation) */
static int isValidGroovyHome( const char* dir ) {
  char *gconfFile = NULL ;
  jboolean rval = JNI_FALSE ;

  errno = 0 ;
  
  gconfFile = jst_createFileName( dir, "conf", GROOVY_CONF_FILE, NULL ) ; 
  if ( gconfFile ) {
    rval = jst_fileExists( gconfFile ) ? JNI_TRUE : JNI_FALSE ;
    free( gconfFile ) ;
  }

  return rval ;
}


/** returns null on error, otherwise pointer to groovy home. 
 * First tries to see if the current executable is located in a groovy installation's bin directory. If not, groovy
 * home env var is looked up. If neither succeed, an error msg is printed.
 * freeing the returned pointer must be done by the caller. */
char* getGroovyHome() {

  return getAppHome( JST_USE_PARENT_OF_EXEC_LOCATION_AS_HOME, "GROOVY_HOME", &isValidGroovyHome ) ;
   
}

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


static const char* groovyshColorParam[] = { "-C", "--color", NULL } ;
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

static const JstParamInfo noParameters[] = {
  { NULL,          0,                0 }    
} ;

 
// deduce which class name to pass as --main param. In other words, this here supports all different groovy executables.
static char* jst_figureOutMainClass( char* cmd, int numArgs, JstParamInfo** parameterInfosOut, jboolean* displayHelpOut ) {  
  // The execubale acts as a different groovy executable when it's renamed / symlinked to. 
  char *execName = jst_strdup( cmd ),
       *eName,
       *execNameTmp,
       *mainClassName = NULL ;
  
  if ( !( execNameTmp = execName ) ) return NULL ;
  
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
    mainClassName = "groovy.ui.GroovyMain" ;
    *parameterInfosOut = (JstParamInfo*)groovyParameters ;
  } else if ( strcmp( execName, "groovyc" ) == 0 ) {
    mainClassName = "org.codehaus.groovy.tools.FileSystemCompiler" ;
    *parameterInfosOut = (JstParamInfo*)groovycParameters ;
  } else {
    
    if ( numArgs == 0 ) *displayHelpOut = JNI_FALSE ;
    
    if ( strcmp( execName, "gant" ) == 0 ) {
      mainClassName = "gant.Gant" ; 
      *parameterInfosOut = (JstParamInfo*)gantParameters ;
    } else if ( strcmp( execName, "groovysh" ) == 0 ) {
      mainClassName = getenv( "OLDSHELL" ) ? "groovy.ui.InteractiveShell" : "org.codehaus.groovy.tools.shell.Main" ;
      *parameterInfosOut = (JstParamInfo*)groovyshParameters ;
    } else if ( strcmp( execName, "java2groovy" ) == 0 ) {
      *displayHelpOut = JNI_FALSE ;
      mainClassName = "org.codehaus.groovy.antlr.java.Java2GroovyMain" ; 
      *parameterInfosOut = (JstParamInfo*)java2groovyParameters ;
    } else if ( ( strcmp( execName, "groovyConsole" ) == 0 ) || ( strcmp( execName, "groovyconsole" ) == 0 ) ) {
      *displayHelpOut = JNI_FALSE ;
      *parameterInfosOut = (JstParamInfo*)noParameters ;
      mainClassName = "groovy.ui.Console" ;
    } else if ( ( strcmp( execName, "graphicsPad"   ) == 0 ) || strcmp( execName, "graphicspad" ) == 0 ) {
      *displayHelpOut = JNI_FALSE ;
      *parameterInfosOut = (JstParamInfo*)noParameters ;
      mainClassName = "groovy.swing.j2d.app.GraphicsPad" ;
    } else { // default to being "groovy"
      if ( numArgs == 0 ) *displayHelpOut = JNI_TRUE ;
      mainClassName = "groovy.ui.GroovyMain" ;
      *parameterInfosOut = (JstParamInfo*)groovyParameters ;
    }
    
  }
    
  free( execNameTmp ) ;
  
  return mainClassName ;
  
}

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
  JstParamInfo* parameterInfos = NULL ;
  char *groovyConfFile  = NULL, 
       *groovyDConf     = NULL, // the -Dgroovy.conf=something to pass to the jvm
       *groovyHome      = NULL, 
       *groovyDHome     = NULL, // the -Dgroovy.home=something to pass to the jvm
       *classpath       = NULL, 
       *javaHome        = NULL ;

  void** dynReservedPointers = NULL ; // free all reserved pointers at at end of func
  size_t dreservedPtrsSize   = 0 ;
  
# define MARK_PTR_FOR_FREEING( garbagePtr ) if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, ( garbagePtr ) ) ) goto end ;
        
  /** terminatingSuffixes contains the suffixes that, if matched, indicate that the matching param and all the rest of the params 
   * are launcheeParams, e.g. {".groovy", ".gy", NULL}. 
   * The significance of this is marginal, as any param w/ no preceding "-" is terminating. So, this is only significant
   * if the terminating option has "-" as prefix, but is not one of the enumerated options. Usually this would be
   * a file name associated w/ the app, e.g. "-foobar.groovy". As file names do not usually begin w/ "-" this is rather unimportant. */
  const char *terminatingSuffixes[] = { ".groovy", ".gvy", ".gy", ".gsh", NULL } ;
  char *extraProgramOptions[]       = { "--main", "groovy.ui.GroovyMain", "--conf", NULL, "--classpath", NULL, NULL }, 
       *jars[]                      = { NULL, NULL } ;

  int  numArgs = argc - 1 ;
         
  int  rval = -1 ; 

  JVMSelectStrategy jvmSelectStrategy ;
  
  // this flag is used to tell us whether we reserved the memory for the conf file location string
  // or whether it was obtained from cmdline or environment var. That way we don't try to free memory
  // we did not allocate.
  jboolean displayHelp          = ( ( numArgs == 0 )                       ||  
                                    ( strcmp( argv[ 1 ], "-h"     ) == 0 ) || 
                                    ( strcmp( argv[ 1 ], "--help" ) == 0 )
                                  ) ? JNI_TRUE : JNI_FALSE ; 
  
  JstActualParam *processedActualParams = NULL ;

  
  
  // _jst_debug is a global debug flag
  if ( getenv( "__JLAUNCHER_DEBUG" ) ) _jst_debug = JNI_TRUE ;
  
#if defined ( _WIN32 ) && defined ( _cwcompat )
  jst_cygwinInit() ;
#endif

  extraProgramOptions[ 1 ] = jst_figureOutMainClass( argv[ 0 ], numArgs, &parameterInfos, &displayHelp ) ;
  if ( !extraProgramOptions[ 1 ] ) goto end ;
  
  processedActualParams = jst_processInputParameters( argv + 1, argc - 1, parameterInfos, terminatingSuffixes, JST_CYGWIN_PATH_CONVERSION ) ;

  MARK_PTR_FOR_FREEING( processedActualParams )

  // set -Dscript.name system property if applicable
  if ( numArgs > 0 ) {
    char *scriptNameIn = jst_getParameterAfterTermination( processedActualParams, 0 ) ;

    if ( scriptNameIn ) {
  
      char *scriptNameTmp = jst_fullPathName( scriptNameIn ), 
           *scriptNameDyn ;
      
      if ( !scriptNameTmp ) goto end ;
      
      scriptNameDyn = jst_append( NULL, NULL, "-Dscript.name=", scriptNameTmp, NULL ) ;
      MARK_PTR_FOR_FREEING( scriptNameDyn )
      
      if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                                  (int)extraJvmOptionsCount++, 
                                                  &extraJvmOptionsSize, 
                                                  scriptNameDyn, 
                                                  NULL ) ) ) goto end ;
      
      if ( scriptNameTmp != scriptNameIn ) jst_free( scriptNameTmp ) ;
      
    }    
  }
  
  classpath = jst_getParameterValue( processedActualParams, "-cp" ) ;
  if ( !classpath ) {
    classpath = getenv( "CLASSPATH" ) ;
  }

  // add "." to the end of the used classpath. This is what the script launcher also does
  if ( classpath ) {
    
    classpath = jst_append( NULL, NULL, classpath, JST_PATH_SEPARATOR ".", NULL ) ;
    MARK_PTR_FOR_FREEING( classpath )

    extraProgramOptions[ 5 ] = classpath ;
    
  } else {
    
    extraProgramOptions[ 5 ] = "." ;
    
  }

  groovyHome = getGroovyHome() ;
  MARK_PTR_FOR_FREEING( groovyHome )

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // find out the groovy conf file to use
  
  groovyConfFile = jst_getParameterValue( processedActualParams, "--conf" ) ; 
  
  if ( !groovyConfFile  ) groovyConfFile = getenv( "GROOVY_CONF" ) ; 
  
  if ( !groovyConfFile ) {
    groovyConfFile = jst_createFileName( groovyHome, "conf", GROOVY_CONF_FILE, NULL ) ;    
    MARK_PTR_FOR_FREEING( groovyConfFile )
  }
  
  MARK_PTR_FOR_FREEING( jars[ 0 ] = findGroovyStartupJar( groovyHome ) )
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // set -Dgroovy.home and -Dgroovy.starter.conf as jvm options


  extraProgramOptions[ 3 ] = groovyConfFile ;

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
  
  groovyDConf = jst_append( NULL, NULL, "-Dgroovy.starter.conf=", groovyConfFile, NULL ) ;  
  MARK_PTR_FOR_FREEING( groovyDConf ) 
  
  
  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                              (int)extraJvmOptionsCount++, 
                                              &extraJvmOptionsSize, 
                                              groovyDConf, 
                                              NULL ) ) ) goto end ;

  groovyDHome = jst_append( NULL, NULL, "-Dgroovy.home=", groovyHome, NULL ) ;
  MARK_PTR_FOR_FREEING( groovyDHome ) 

  if ( ! ( extraJvmOptions = appendJvmOption( extraJvmOptions, 
                                              (int)extraJvmOptionsCount++, 
                                              &extraJvmOptionsSize, 
                                              groovyDHome, 
                                              NULL ) ) ) goto end ;
  
  MARK_PTR_FOR_FREEING( extraJvmOptions )
  
  
  jvmSelectStrategy = jst_getParameterValue( processedActualParams, "-client" ) ? JST_TRY_CLIENT_ONLY :
                      jst_getParameterValue( processedActualParams, "-server" ) ? JST_TRY_SERVER_ONLY :
                      // by default, mimic java launcher, which also prefers client vm due to its 
                      // faster startup (despite it running much slower)
                      JST_CLIENT_FIRST ;
  
  
  // populate the startup parameters
  // first, set the memory to 0. This is just a precaution, as NULL (0) is a sensible default value for many options.
  // TODO: remove this, it gives false sense of safety...
  // memset( &options, 0, sizeof( JavaLauncherOptions ) ) ;
  
  options.javaHome            = javaHome ; 
  options.jvmSelectStrategy   = jvmSelectStrategy ; 
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
  options.classpathStrategy   = jst_getParameterValue( processedActualParams, "--quickstart" ) ? JST_BOOTSTRAP_CLASSPATH_A : JST_NORMAL_CLASSPATH ;

#if defined ( _WIN32 ) && defined ( _cwcompat )
  jst_cygwinRelease() ;
#endif

  rval = jst_launchJavaApp( &options ) ;

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
  
  return rval ;
  
}

