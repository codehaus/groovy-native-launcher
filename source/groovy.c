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
#include <assert.h>
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

#  if defined ( _cwcompat )
#    include "jst_cygwin_compatibility.h"
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

#  endif

#endif


#include <jni.h>

#include "jvmstarter.h"
  
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
  if ( gconfFile ) {
    rval = jst_fileExists( gconfFile ) ? 1 : 0 ;
    free( gconfFile ) ;
  }

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
      if ( _jst_debug ) {
          fprintf( stderr, "debug: groovy home located based on executable location: %s\n", _ghome ) ;
      }
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
  } 

  end : 
  
  if ( appHome ) free( appHome ) ;
  return _ghome ;
}


// the parameters accepted by groovy (note that -cp / -classpath / --classpath & --conf 
// are handled separately below
static const JstParamInfo groovyParameters[] = {
  { "-D",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "--define",    JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "-c",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "--encoding",  JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-h",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "--help",      JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "-d",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--debug",     JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-e",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "-i",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-l",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-n",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-p",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-v",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--version",   JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-jh",         JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_CONVERT },
  { "--javahome",  JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_CONVERT },
  { "-cp",         JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_CONVERT },
  { "-classpath",  JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_CONVERT },
  { "--classpath", JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_CONVERT },
  { "--conf",      JST_DOUBLE_PARAM, JST_IGNORE | JST_CYGWIN_CONVERT },
  { "-client",     JST_SINGLE_PARAM, JST_IGNORE },
  { "-server",     JST_SINGLE_PARAM, JST_IGNORE },
  { NULL,          0,                0 }
} ;

static const JstParamInfo groovycParameters[] = {
  { "--encoding",  JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-F",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "-J",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "-d",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "-e",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--exception", JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--version",   JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-h",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "--help",      JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "-j",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--jointCompilation", JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-v",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--version",   JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { NULL,          0,                0 }
} ;

static const JstParamInfo gantParameters[] = {
  { "-c",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--usecache",  JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-n",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--dry-run",   JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-D",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "-P",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-T",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "--targets",   JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-V",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--version",   JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-d",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--cachedir",  JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-f",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--gantfile",  JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-h",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "--help",      JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "-l",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "--gantlib",   JST_DOUBLE_PARAM, JST_TO_LAUNCHEE },
  { "-p",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--projecthelp", JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-q",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--quiet",     JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-s",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--silent",    JST_SINGLE_PARAM, JST_TO_LAUNCHEE },  
  { "-v",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--verbose",   JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { NULL,          0,                0 }
} ;

static const JstParamInfo groovyshParameters[] = {
  { "-C",          JST_PREFIX_PARAM, JST_TO_LAUNCHEE },
  { "--color",     JST_PREFIX_PARAM, JST_TO_LAUNCHEE },
  { "-D",          JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "--define",    JST_DOUBLE_PARAM, JST_TO_LAUNCHEE }, 
  { "-T",          JST_PREFIX_PARAM, JST_TO_LAUNCHEE }, 
  { "--terminal",  JST_PREFIX_PARAM, JST_TO_LAUNCHEE }, 
  { "-V",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE }, 
  { "--version",   JST_SINGLE_PARAM, JST_TO_LAUNCHEE }, 
  { "-d",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--debug",     JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-h",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "--help",      JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { "-q",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--quiet",     JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "-v",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { "--verbose",   JST_SINGLE_PARAM, JST_TO_LAUNCHEE },
  { NULL,          0,                0 }
} ;

static const JstParamInfo java2groovyParameters[] = {
  { "-h",          JST_SINGLE_PARAM, JST_TO_LAUNCHEE | JST_TERMINATING },
  { NULL,          0,                0 }
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
      mainClassName = "groovy.swing.j2d.GraphicsPad" ;
    } else { // default to being "groovy"
      if ( numArgs == 0 ) *displayHelpOut = JNI_TRUE ;
      mainClassName = "groovy.ui.GroovyMain" ;
      *parameterInfosOut = (JstParamInfo*)groovyParameters ;
    }
    
  }
    
  jst_free( execNameTmp ) ;
  
  return mainClassName ;
  
}


int main( int argc, char** argv ) {

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
//= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  
// cygwin compatibility code begin

#if defined ( _WIN32 ) && defined ( _cwcompat )


  // NOTE: this DOES NOT WORK. This code is experimental and is not compiled into the executable by default.
  //       You need to add -D_cwcompat to the compiler opts when compiling (into the Rantfile) 

  
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
  
  assert( sizeof( size_t ) == 4 * sizeof( byte ) ) ;
  
  if ( (*(size_t*)(g_pad->stackbase)) - (*(size_t*)(g_pad->end)) ) {
    size_t delta = (*(size_t*)(g_pad->stackbase)) - (*(size_t*)(g_pad->end)) ; //g_pad->stackbase - g_pad->end ;
    g_pad->backup = jst_malloc( delta ) ;
    if ( !( g_pad->backup ) ) return -1 ;
    
    // FIXME - crashes here
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
       *classpath_dyn   = NULL, 
       *javaHome        = NULL ;

  void** dynReservedPointers = NULL ; // free all reserved pointers at at end of func
  size_t dreservedPtrsSize   = 0 ;
        
  // NULL terminated string arrays. Other NULL entries will be filled dynamically below.
  char *terminatingSuffixes[] = { ".groovy", ".gvy", ".gy", ".gsh", NULL },
       *extraProgramOptions[] = { "--main", "groovy.ui.GroovyMain", "--conf", NULL, "--classpath", NULL, NULL }, 
       *jars[]                = { NULL, NULL } ;

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
  if ( !( extraProgramOptions[ 1 ] ) ) goto end ;
  
  processedActualParams = jst_processInputParameters( argv + 1, argc - 1, parameterInfos, terminatingSuffixes, JNI_TRUE ) ;
  if ( !processedActualParams ) goto end ;

  
  if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, 
                           processedActualParams ) ) goto end ;
      
  if ( numArgs > 0 ) {
    char scriptNameOut[ PATH_MAX + 1 ] ;
    char *scriptNameIn = jst_getParameterAfterTermination( processedActualParams, 0 ), 
         *scriptNameDyn = NULL ;
    if ( scriptNameIn ) {
#if defined( _WIN32 )
      char *lpFilePart = NULL ;
      DWORD pathLen = GetFullPathName( scriptNameIn, PATH_MAX, scriptNameOut, &lpFilePart ) ;
      
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
      //       Anyhow, this behavior is inconsistent between groovy script launchers - windows
      //       version
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
  }
  
  classpath = jst_getParameterValue( processedActualParams, "-cp" ) ;
  if ( !classpath ) {
    classpath = jst_getParameterValue( processedActualParams, "-classpath" ) ;
    if ( !classpath ) {
      classpath = jst_getParameterValue( processedActualParams, "--classpath" ) ;
      if ( !classpath ) classpath = getenv( "CLASSPATH" ) ;
    }
  }

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
  
  groovyConfFile = jst_getParameterValue( processedActualParams, "--conf" ) ; 
  
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

  javaHome = jst_getParameterValue( processedActualParams, "-jh" ) ;
  if ( !javaHome ) javaHome = jst_getParameterValue( processedActualParams, "--javahome" ) ;
  
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
  // refactor so that the target sys prop can be defined
  options.toolsJarHandling    = JST_TOOLS_JAR_TO_SYSPROP ;
  options.javahomeHandling    = JST_ALLOW_JH_ENV_VAR_LOOKUP | JST_ALLOW_JH_PARAMETER | JST_ALLOW_PATH_LOOKUP | JST_ALLOW_REGISTRY_LOOKUP ; 
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
  options.paramInfos          = parameterInfos ;
  options.terminatingSuffixes = terminatingSuffixes ;

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
    " -client/-server                 to use a client/server VM (aliases for these\n"
    "                                 are not supported)\n"
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

