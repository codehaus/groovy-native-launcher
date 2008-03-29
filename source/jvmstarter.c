//  A library for easy creation of a native launcher for Java applications.
//
//  Copyright (c) 2006 Antti Karanta (Antti dot Karanta (at) iki dot fi) 
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
//  Author:  Antti Karanta (Antti dot Karanta (at) iki dot fi) 
//  $Revision$
//  $Date$

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>
#include <errno.h>

#include <limits.h>

#if defined ( __APPLE__ )
#  include <TargetConditionals.h>
#endif

#include <jni.h>

#include "jvmstarter.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// NOTE: when compiling w/ gcc on cygwin, pass -mno-cygwin, which makes gcc define _WIN32 and handle the win headers ok
#if defined( _WIN32 )

// as appended to JAVA_HOME + JST_FILE_SEPARATOR (when a jre) or JAVA_HOME + JST_FILE_SEPARATOR + "jre" + JST_FILE_SEPARATOR (when a jdk) 
#  define PATHS_TO_SERVER_JVM "bin\\server\\jvm.dll", "bin\\jrockit\\jvm.dll" 
#  define PATHS_TO_CLIENT_JVM "bin\\client\\jvm.dll"

#  include <Windows.h>

   typedef HINSTANCE DLHandle ;
#  define dlopen( path, mode ) LoadLibrary( path )
#  define dlsym( libraryhandle, funcname ) GetProcAddress( libraryhandle, funcname )
#  define dlclose( handle ) FreeLibrary( handle )

// PATH_MAX is defined when compiling w/ e.g. msys gcc, but not w/ ms cl compiler (the visual studio c compiler)
#  if !defined( PATH_MAX )
#    define PATH_MAX MAX_PATH
#  endif

#  include "jst_winreg.h"   
   
#else

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#  if defined( __linux__ )
#    if defined( __i386__ )
#      define PATHS_TO_SERVER_JVM "lib/i386/server/libjvm.so"
#      define PATHS_TO_CLIENT_JVM "lib/i386/client/libjvm.so"
#    else
#      error "linux on non-x86 hardware not currently supported. Please contact the author to have support added."
#    endif
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#  elif defined( __sun__ ) 

#    if defined( __sparc__ ) || defined( __sparc ) || defined( __sparcv9 )

#      define PATHS_TO_SERVER_JVM "lib/sparc/server/libjvm.so", "lib/sparcv9/server/libjvm.so"
#      define PATHS_TO_CLIENT_JVM "lib/sparc/client/libjvm.so", "lib/sparc/libjvm.so"

#    elif defined( __i386__ ) || defined( __i386 )
        // these are just educated guesses, I have no access to solaris running on x86...
#      define PATHS_TO_SERVER_JVM "lib/i386/server/libjvm.so"
#      define PATHS_TO_CLIENT_JVM "lib/i386/client/libjvm.so"

#    else
       // should not happen, but this does not hurt either
#      error "You are running solaris on an architecture that is currently not supported. Please contact the author to have support added."
#    endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#  elif defined ( __APPLE__ )

//  The user could use the /System/Library/Frameworks/JavaVM.framework/Home or
//  /System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK/Home as their JAVA_HOME since that is
//  the more Linux/Solaris/Unix like location (the default is /System/Library/Frameworks/JavaVM.framework).
//  The issue is that all the dynamic libraries are not in that part of the tree.  To deal with this we try
//  one rather than two places to search.

#    define PATHS_TO_SERVER_JVM "Libraries/libserver.dylib", "../Libraries/libserver.dylib"
#    define PATHS_TO_CLIENT_JVM "Libraries/libclient.dylib", "../Libraries/libclient.dylib"

#    define CREATE_JVM_FUNCTION_NAME "JNI_CreateJavaVM_Impl"

#  else   
#    error "Either your OS and/or architecture is not currently supported. Support should be easy to add - please see the source (look for #if defined stuff) or contact the author."
#  endif

   // for getpid()
#  include <unistd.h>

#  include <dirent.h>

// stuff for loading a dynamic library
#  include <dlfcn.h>
   
#  if !defined ( __APPLE__ )
#    include <link.h>
#  endif
  
   typedef void* DLHandle ;

#endif

#if !defined( CREATE_JVM_FUNCTION_NAME )
// this is what it's called on most platforms (and in the jni specification). E.g. Apple is different.
#  define CREATE_JVM_FUNCTION_NAME "JNI_CreateJavaVM"
#endif
   
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

jboolean _jst_debug = JNI_FALSE ;

// The pointer to the JNI_CreateJavaVM function needs to be called w/ JNICALL calling convention. Using this typedef
// takes care of that.
typedef jint (JNICALL *JVMCreatorFunc)(JavaVM**,void**,void*);

typedef struct {
  JVMCreatorFunc creatorFunc  ;
  DLHandle       dynLibHandle ;
} JavaDynLib ;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#if defined( _WIN32 )

// what we really want is DWORD, but unsigned long is what it really is and using it directly we avoid having to include "Windows.h" in jvmstarter.h 
extern void jst_printWinError( unsigned long errcode ) {
  
  LPVOID message ;
     
  FormatMessage(
                 FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                 NULL,
                 errcode,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPTSTR) &message,
                 0, 
                 NULL 
                ) ;

  fprintf( stderr, "error (win code %u): %s\n", (unsigned int) errcode, (char*)message ) ; 
  LocalFree( message ) ;
  
}

#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/** Tries to find Java home by looking where java command is located on PATH. 
 * Returns != 0 on error. */
extern int jst_findJavaHomeFromPath( char** javaHomeOut ) {
  static char* _javaHome = NULL ;
  int error = 0 ;

  if ( !_javaHome ) {
    char     *path = NULL, 
             *p, 
             *javahome = NULL ;
    size_t   jhlen     = 100  ;
    jboolean firstTime = JNI_TRUE ;

    p = getenv( "PATH" ) ;
    if ( !p ) goto end ; // PATH not defined. Should never happen, but is not really an error if it does.
    
    if ( !( path = jst_strdup( p ) ) ) { error = -1 ; goto end ; }
    
    for ( ; ( p = strtok( firstTime ? path : NULL, JST_PATH_SEPARATOR ) ) ; firstTime = JNI_FALSE ) {
      size_t len = strlen( p ) ;
      if ( len == 0 ) continue ;
      
      if ( javahome ) javahome[ 0 ] = '\0' ;
      
      if ( !( javahome = jst_append( javahome, &jhlen, p, NULL ) ) ) { error = -1 ; goto end ; }
      
      javahome = jst_append( javahome, &jhlen, ( javahome[ len - 1 ] != JST_FILE_SEPARATOR[ 0 ] ) ? JST_FILE_SEPARATOR : "", 
                                               "java" 
#if defined( _WIN32 )
                                               ".exe"
#endif
                                             , NULL ) ;
      if ( !javahome ) { error = -1 ; goto end ; }
    
      if ( jst_fileExists( javahome ) ) {
#if !defined( _WIN32 )
        // if not on windows (which does not have symlinks), resolve the real location of the java executable
        char realFile[ PATH_MAX + 1 ] ;
        if ( !realpath( javahome, realFile ) ) {
          fprintf( stderr, strerror( errno ) ) ;
          error = errno ;
          goto end ;
        }
        javahome[ 0 ] = '\0' ;
        javahome = jst_append( javahome, &jhlen, realFile, NULL ) ;
        if ( !javahome ) { error = -1 ; goto end ; }
#endif
        *( strrchr( javahome, JST_FILE_SEPARATOR[ 0 ] ) ) = '\0' ;
        len = strlen( javahome ) ;
        if ( len < 4 ) goto end ; // we are checking whether the executable is in "bin" dir, /bin is the shortest possibility (4 chars)
        // see if we are in the bin dir of java home
        if ( memcmp( javahome + len - 3, "bin", 3 ) == 0 ) {
          javahome[ len -= 4 ] = '\0' ;
          assert( len == strlen( javahome ) ) ;
          _javaHome = jst_strdup( javahome ) ;
          if ( !_javaHome ) { error = -1 ; goto end ; } 
        }
        break ;
      }
      // check if this is a valid java home (how?)
    }
    
    end:
    if ( path     ) free( path     ) ;
    if ( javahome ) free( javahome ) ;
    if ( _jst_debug ) {
      if ( _javaHome ) {
        fprintf( stderr, "debug: java home found on PATH: %s\n", _javaHome ) ;      
      } else {
        fprintf( stderr, "debug: java home not found on PATH\n" ) ;
      }
    }
  }
  
  *javaHomeOut = _javaHome ;
  return error ;
}

/** First sees if JAVA_HOME is set and points to an existing location (the validity is not checked).
 * Next, windows registry is checked (if on windows) or if on os-x the standard location 
 * /System/Library/Frameworks/JavaVM.framework is checked for existence. 
 * Last, java is looked up from the PATH. 
 * @return != 0 on error. */
static int jst_findJavaHome( char** javaHomeOut, JavaHomeHandling javaHomeHandling ) {
  static char* _javaHome = NULL ;

  int error = 0 ;
  
  if ( !_javaHome ) {
    char* javahome ;
  
    if ( javaHomeHandling & JST_ALLOW_JH_ENV_VAR_LOOKUP ) {
      javahome = getenv( "JAVA_HOME" ) ;
      if ( javahome ) {
        if ( jst_fileExists( javahome ) ) { 
          _javaHome = javahome ;
        } else {
          fprintf( stderr, "warning: JAVA_HOME points to a nonexistent location\n" ) ;
        }
      }
    }
    
    if ( !_javaHome && ( javaHomeHandling & JST_ALLOW_PATH_LOOKUP     ) && jst_findJavaHomeFromPath( &_javaHome        ) ) return -1 ;

    // os specific lookup mechanisms
#if defined( _WIN32 )
    if ( !_javaHome && ( javaHomeHandling & JST_ALLOW_REGISTRY_LOOKUP ) && jst_findJavaHomeFromWinRegistry( &_javaHome ) ) return -1 ; 
#elif defined ( __APPLE__ )
    if ( !_javaHome || !_javaHome[ 0 ] ) {
      javahome = "/System/Library/Frameworks/JavaVM.framework" ;
      if ( jst_fileExists( javahome ) ) {
        _javaHome = javahome ;
      } else {
        fprintf( stderr, "warning: java home not found in standard location %s\n", javahome ) ;
      }
    }
#endif
  
    if ( !_javaHome ) {
      error = -2 ;
      fprintf( stderr, "error: could not locate java home\n" ) ;
    }
    
  }
  
  *javaHomeOut = _javaHome ;
  return error ;
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


/** In case there are errors, the returned struct contains only NULLs. */
static JavaDynLib findJVMDynamicLibrary(char* java_home, JVMSelectStrategy jvmSelectStrategy ) {

  static char* potentialPathsToServerJVM[]              = { PATHS_TO_SERVER_JVM, NULL } ;
  static char* potentialPathsToClientJVM[]              = { PATHS_TO_CLIENT_JVM, NULL } ;
  static char* potentialPathsToAnyJVMPreferringServer[] = { PATHS_TO_SERVER_JVM, PATHS_TO_CLIENT_JVM, NULL } ;
  static char* potentialPathsToAnyJVMPreferringClient[] = { PATHS_TO_CLIENT_JVM, PATHS_TO_SERVER_JVM, NULL } ;
  
  char       *path = NULL, 
             *mode ;
  size_t     pathSize = PATH_MAX + 1 ;
  JavaDynLib rval ;
  int        i = 0, j = 0;
  DLHandle   jvmLib = (DLHandle)0 ;
  char**     lookupDirs = NULL ;
  char*      dynLibFile ;
  jboolean   preferClient = ( jvmSelectStrategy & 4 ) ? JNI_TRUE : JNI_FALSE,  // third bit
             allowClient  = ( jvmSelectStrategy & 1 ) ? JNI_TRUE : JNI_FALSE,  // first bit
             allowServer  = ( jvmSelectStrategy & 2 ) ? JNI_TRUE : JNI_FALSE ; // secons bit
  
  assert( allowClient || allowServer ) ;
  
  rval.creatorFunc  = NULL ;
  rval.dynLibHandle = NULL ;

  if ( allowServer && !allowClient ) {
    mode = "server" ;
    lookupDirs = potentialPathsToServerJVM ;
  } else if ( allowClient && !allowServer ) {
    mode = "client" ;
    lookupDirs = potentialPathsToClientJVM ;
  } else {
    mode = "client or server" ;
    lookupDirs = preferClient ? potentialPathsToAnyJVMPreferringClient : potentialPathsToAnyJVMPreferringServer ;
  }
  
  for ( i = 0 ; i < 2 ; i++ ) { // try both jdk and jre style paths
    for ( j = 0; ( dynLibFile = lookupDirs[ j ] ) ; j++ ) {
      if ( path ) path[ 0 ] = '\0' ;
      if ( !( path = jst_append( path, &pathSize, java_home, 
                                                  JST_FILE_SEPARATOR,
                                                  // on a jdk, we need to add jre at this point of the path
                                                  ( i == 0 ) ? "jre" JST_FILE_SEPARATOR : "",
                                                  dynLibFile, 
                                                  NULL ) ) ) goto end ;

      if ( jst_fileExists( path ) ) {
        errno = 0 ;
        if ( !( jvmLib = dlopen( path, RTLD_LAZY ) ) )  {
          fprintf( stderr, "error: dynamic library %s exists but could not be loaded!\n", path ) ;
          if ( errno ) fprintf( stderr, strerror( errno ) ) ;
#         if defined( _WIN32 )
          jst_printWinError( GetLastError() ) ;
#         else
          {
            char* errorMsg = dlerror() ;
            if ( errorMsg ) fprintf( stderr, "%s\n", errorMsg ) ;
          }
#         endif
        } 
        goto exitlookup ; // just break out of 2 nested loops
      }
    }
  }
  exitlookup:
  
  if( !jvmLib ) {
    fprintf(stderr, "error: could not find %s jvm under %s\n"
                    "       please check that it is a valid jdk / jre containing the desired type of jvm\n", 
                    mode, java_home);
    return rval;
  }

  rval.creatorFunc = (JVMCreatorFunc)dlsym( jvmLib, CREATE_JVM_FUNCTION_NAME ) ;

  if ( rval.creatorFunc ) {
    rval.dynLibHandle = jvmLib ;
  } else {
#   if defined( _WIN32 )
    jst_printWinError( GetLastError() ) ;
#   else 
    char* errorMsg = dlerror() ;
    if ( errorMsg ) fprintf( stderr, "%s\n", errorMsg ) ;
#   endif
    fprintf( stderr, "strange bug: jvm creator function not found in jvm dynamic library %s\n", path ) ;
  }
  
  end:
  if ( path ) free( path ) ;
  
  return rval ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** To be called when there is a pending exception that is the result of some
 * irrecoverable error in this startup program. Clears the exception and prints its description. */
static void clearException(JNIEnv* env) {

  (*env)->ExceptionDescribe(env);
  (*env)->ExceptionClear(env);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


extern JavaVMOption* appendJvmOption( JavaVMOption* opts, int indx, size_t* optsSize, char* optStr, void* extraInfo ) {
  JavaVMOption tmp ;
  
  tmp.optionString = optStr    ;
  tmp.extraInfo    = extraInfo ;
  
  return jst_appendArrayItem( opts, indx, optsSize, &tmp, sizeof( JavaVMOption ) ) ;
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** Appends the given entry to the jvm classpath being constructed (begins w/ "-Djava.class.path=", which the given
 * cp needs to contain before calling this func). Adds path separator
 * before the given entry, unless this is the first entry. Returns the cp buffer (which may be moved)
 * Returns 0 on error (err msg already printed). */
static char* appendCPEntry(char* cp, size_t* cpsize, const char* entry) {
  // "-Djava.class.path=" == 18 chars -> if 19th char (index 18) is not a null char, we have more than that and need to append path separator
  if ( cp[ 18 ]
      && !(cp = jst_append( cp, cpsize, JST_PATH_SEPARATOR, NULL ) ) ) return NULL ;
 
  return jst_append( cp, cpsize, entry, NULL ) ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


/** returns != 0 on failure. May change the target to point to a new location */
static jboolean appendJarsFromDir( char* dirName, char** target, size_t* targetSize ) {

  char **jarNames = jst_getFileNames( dirName, NULL, ".jar" ),
       *s ;
  int i = 0 ;
  jboolean dirNameEndsWithSeparator = ( strcmp( dirName + strlen( dirName ) - strlen( JST_FILE_SEPARATOR ), JST_FILE_SEPARATOR ) == 0 ) ? JNI_TRUE : JNI_FALSE,
           errorOccurred = JNI_FALSE ;
  
  if ( !jarNames ) return JNI_TRUE ;
  
  while ( ( s = jarNames[ i++ ] ) ) {
    if(    !( *target = appendCPEntry( *target, targetSize, dirName ) )         
        || !( *target = jst_append( *target, targetSize, dirNameEndsWithSeparator ? "" : JST_FILE_SEPARATOR, s, NULL ) )
      ) {
      errorOccurred = JNI_TRUE ;
      goto end ;    
    }
  }
  
  end:
  free( jarNames ) ;
  return errorOccurred ;
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** Returns false on error. */
static jboolean addStringToJStringArray( JNIEnv* env, char *strToAdd, jobjectArray jstrArr, jint ind ) {
  jboolean rval = JNI_FALSE ;
  jstring  arg  = (*env)->NewStringUTF( env, strToAdd ) ;

  if ( !arg ) {
    fprintf( stderr, "error: could not convert %s to java string\n", strToAdd ) ;
    clearException( env ) ;
    goto end ;        
  }

  (*env)->SetObjectArrayElement( env, jstrArr, ind, arg ) ;
  if ( (*env)->ExceptionCheck( env ) ) {
    fprintf( stderr, "error: error when writing %dth element %s to Java String[]\n", (int)ind, strToAdd ) ;
    clearException( env ) ;
    goto end ;
  }
  (*env)->DeleteLocalRef( env, arg ) ;
  rval = JNI_TRUE ; 
  
  end:
  return rval;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// Used to hold dyn allocated jvm options
typedef struct {
  JavaVMOption* jvmOptions ;
  // number of options present in the above array
  int           optionsCount ; 
  // the size of the above array (in number of JavaVMOptions that can be fit in)
  size_t        optionsSize ; 
  
} JSTJVMOptionHolder ;

/** @param initialCP may be NULL */
static char* constructClasspath( char* initialCP, char** jarDirs, char** jars ) {
  size_t    cpsize = 255 ; // just an initial guess for classpath length, will be expanded as necessary 
  char *classpath     = NULL ;
  
  if ( !( classpath = jst_append( NULL, &cpsize, "-Djava.class.path=", NULL ) ) ) goto end ;

  if ( initialCP ) {
    if ( !( classpath = appendCPEntry( classpath, &cpsize, initialCP ) ) ) goto end ;
  }

  // add the jars from the given dirs
  if ( jarDirs ) {

    while ( *jarDirs  ) {
      if ( appendJarsFromDir( *jarDirs++, &classpath, &cpsize ) ) goto end ; // error msg already printed
    }
    
  }

  // add the provided single jars
  
  if ( jars ) {
    
    while ( *jars ) {
      if ( !( classpath = appendCPEntry( classpath, &cpsize, *jars++ ) ) ) goto end ;
    }
    
  }

  end:
  
  return classpath ;
  
}

/** returns 0 on error */
static jboolean handleToolsJar( const char* javaHome, ToolsJarHandling toolsJarHandling, 
                                JavaVMOption** jvmOptions, int* jvmOptionsCount, size_t* jvmOptionsSize, char** toolsJarD ) {

  jboolean rval = JNI_FALSE ;
  // tools.jar is not present on a jre, so in that case we omit the -Dtools.jar= option
  const char* toolsJarFile = jst_append( NULL, NULL, javaHome, JST_FILE_SEPARATOR "lib" JST_FILE_SEPARATOR "tools.jar", NULL ) ;
  
  if ( !toolsJarFile ) goto end ;

  if ( jst_fileExists( toolsJarFile ) ) {
    // add as java env property if requested
    if ( toolsJarHandling & JST_TOOLS_JAR_TO_SYSPROP ) {
      *toolsJarD = jst_append( NULL, NULL, "-Dtools.jar=", toolsJarFile, NULL ) ;
      if ( !toolsJarD ) goto end ;

      if ( !( *jvmOptions = appendJvmOption( *jvmOptions, 
                                             (*jvmOptionsCount)++, 
                                             jvmOptionsSize, 
                                             *toolsJarD, 
                                             NULL ) ) ) goto end ; 
    }
    
    // add tools.jar to startup classpath if requested
    //if ( ( (launchOptions->toolsJarHandling ) & JST_TOOLS_JAR_TO_CLASSPATH ) 
    // && !( classpath = appendCPEntry(classpath, &cpsize, toolsJarFile) ) ) goto end;
  }

  free( (void*)toolsJarFile ) ;
  rval = JNI_TRUE ;
  
  end:
  return rval ;
  
}

/** returns 0 on error */
static jboolean handleJVMOptsFromEnvVar( const char* javaOptsEnvVar, JavaVMOption** jvmOptions, int* jvmOptionsCount, size_t* jvmOptionsSize,
                                         // output
                                         char** userJvmOptsS ) {
  
  if ( !javaOptsEnvVar ) return JNI_TRUE ; 

  {
    char *userOpts = getenv( javaOptsEnvVar ) ; 
    
    if ( userOpts && userOpts[ 0 ] ) {
      char* s ;
      jboolean firstTime = JNI_TRUE ;
//      int userJvmOptCount = 0 ;
//      userJvmOptCount = 1 ;
//      for ( i = 0 ; userOpts[ i ] ; i++ ) if ( userOpts[ i ] == ' ' ) userJvmOptCount++ ;
  
      if ( !( *userJvmOptsS = jst_strdup( userOpts ) ) ) return JNI_FALSE ;        
  
      while ( ( s = strtok( firstTime ? *userJvmOptsS : NULL, " " ) ) ) {
        firstTime = JNI_FALSE ;
        if ( ! ( *jvmOptions = appendJvmOption( *jvmOptions, 
                                                (*jvmOptionsCount)++, 
                                                jvmOptionsSize, 
                                                s, 
                                                NULL ) ) ) return JNI_FALSE ;
      }
      
    }
  }
  return JNI_TRUE ;
}

/** returns 0 on error */
static jboolean jst_startJvm( jint vmversion, JavaVMOption *jvmOptions, jint jvmOptionsCount, jboolean ignoreUnrecognizedJvmParams, char* javaHome, JVMSelectStrategy jvmSelectStrategy, 
                    // output
                    JavaDynLib* javaLib, JavaVM** javavm, JNIEnv** env, int* errorCode ) {
  JavaVMInitArgs vm_args ;
  int result ;
  JavaDynLib javaLibTmp ;
  // ( JNI_VERSION_1_4, jvmOptions, jvmOptionsCount, JNI_FALSE, javaHome, jvmSelectStrategy, &javaLib, &javavm, &env, &rval

  vm_args.version            = vmversion       ;
  vm_args.options            = jvmOptions      ;
  vm_args.nOptions           = jvmOptionsCount ;
  vm_args.ignoreUnrecognized = ignoreUnrecognizedJvmParams ;


  // fetch the pointer to jvm creator func and invoke it
  javaLibTmp = findJVMDynamicLibrary( javaHome, jvmSelectStrategy ) ;
  if ( !( javaLibTmp.creatorFunc ) ) { // error message already printed
    *errorCode = -1 ;
    return JNI_FALSE ;
  }

  javaLib->creatorFunc  = javaLibTmp.creatorFunc  ;
  javaLib->dynLibHandle = javaLibTmp.dynLibHandle ;
  
  // start the jvm.  
  // the cast to void* before void** serves to remove a gcc warning
  // "dereferencing type-punned pointer will break strict-aliasing rules"
  // Found the fix from
  // http://mail.opensolaris.org/pipermail/tools-gcc/2005-August/000048.html
  result = (javaLib->creatorFunc)( javavm, (void**)(void*)env, &vm_args ) ;

  if ( result ) {
    char* errMsg ;
    switch ( result ) {
      case JNI_ERR        : //  (-1)  unknown error 
        errMsg = "unknown error" ;
        break ;
      case JNI_EDETACHED  : //  (-2)  thread detached from the VM 
        errMsg = "thread detachment" ;
        break ;
      case JNI_EVERSION   : //  (-3)  JNI version error 
        errMsg = "JNI version problems" ;
        break ;
      case JNI_ENOMEM     : //  (-4)  not enough memory 
        errMsg = "not enough memory" ;
        break ;
      case JNI_EEXIST     : //  (-5)  VM already created 
        errMsg = "jvm already created" ;
        break ;
      case JNI_EINVAL     : //  (-6)  invalid arguments
        errMsg = "invalid arguments to jvm creation" ;
        break ;
      default             : // should not happen
        errMsg = "unknown exit code" ;
        break ;
    }
    fprintf( stderr, "error: jvm creation failed with code %d: %s\n", (int)result, errMsg ) ;
    *errorCode = result ;
    return JNI_FALSE ;
  }
  
  return JNI_TRUE ;
}

/** Returns NULL on error, and sets rval output param accordingly. 
 * On successfull execution rval is not touched. */
static jobjectArray createJMainParams( JNIEnv* env, JstActualParam* parameters, char** extraProgramOptions, JstUnrecognizedParamStrategy unrecognizedParamStrategy, int* rval ) {

  jobjectArray launcheeJOptions = NULL ;
  jclass strClass ;
  int i, 
      passedParamCount = 0,
      indx = 0 ; // index in java String[] (args to main) 
  jint result ;

  for ( i = 0 ; parameters[ i ].param ; i++ ) {
    if ( jst_isToBePassedToLaunchee( parameters + i, unrecognizedParamStrategy ) ) {
      passedParamCount++ ;
    }
  }

  passedParamCount += extraProgramOptions ? jst_pointerArrayLen( (void**)(void*)extraProgramOptions ) : 0 ; 
  
  if ( _jst_debug ) fprintf( stderr, "passing %d parameters to main method: \n", passedParamCount ) ;
  
  if ( ( result = (*env)->EnsureLocalCapacity( env, passedParamCount + 1 ) ) ) { // + 1 for the String[] to hold the params
    clearException( env ) ;
    fprintf( stderr, "error: could not allocate memory to hold references for program parameters (how many params did you give, dude?)\n" ) ;
    *rval = result ;
    goto end ;
  }
  
  if ( !( strClass = (*env)->FindClass(env, "java/lang/String") ) ) {
    clearException( env ) ;
    fprintf( stderr, "error: could not find java.lang.String class\n" ) ;
    *rval = -1 ;
    goto end ;
  }
  
  
  launcheeJOptions = (*env)->NewObjectArray( env, passedParamCount, strClass, NULL ) ;
  if ( !launcheeJOptions ) {
    clearException( env ) ;
    fprintf( stderr, "error: could not allocate memory for java String array to hold program parameters (how many params did you give, dude?)\n" ) ;
    *rval = -1 ;
    goto end ;
  }
  
  indx = 0 ; 
  if ( extraProgramOptions ) {
    char *carg ;

    while ( ( carg = *extraProgramOptions++ ) ) {
      if ( _jst_debug ) fprintf( stderr, "  %s\n", carg ) ;

      if ( !addStringToJStringArray( env, carg, launcheeJOptions, indx++ ) ) {
        (*env)->DeleteLocalRef( env, launcheeJOptions ) ;
        launcheeJOptions = NULL ;
        *rval = -1 ;
        goto end ;
      }
    }
  }

  for ( i = 0 ; parameters[ i ].param ; i++ ) {

    if ( jst_isToBePassedToLaunchee( parameters + i, unrecognizedParamStrategy ) ) {
      jboolean failed = JNI_FALSE ;
      JstParamInfo* paramInfo = parameters[ i ].paramDefinition ;
      // FIXME - this is mixed up - think though whatn handling info and param type are used for
      JstInputParamHandling paramHandling = paramInfo ? paramInfo->handling : JST_TERMINATING_OR_AFTER ;
      JstParamClass         paramClass    = paramInfo ? paramInfo->type     : 0 ;

      if ( _jst_debug ) {
        fprintf( stderr, "  %s\n",
            ( paramClass == JST_SINGLE_PARAM ) || ( paramHandling == JST_TERMINATING_OR_AFTER ) ||
            paramClass == JST_DOUBLE_PARAM 
            ? parameters[ i ].param : parameters[ i ].value
        ) ;
        if ( paramClass == JST_DOUBLE_PARAM ) fprintf( stderr, "  %s\n", parameters[ i ].value ) ; 
      }

      assert( ( paramHandling & JST_TERMINATING_OR_AFTER ) || !( parameters[ i ].handling & JST_TERMINATING_OR_AFTER ) ) ;
      
      switch ( paramClass ) {
        case JST_SINGLE_PARAM :
        case JST_TERMINATING_OR_AFTER  :
          failed = !addStringToJStringArray( env, parameters[ i ].param, launcheeJOptions, indx++ ) ;
          break ;
        case JST_DOUBLE_PARAM :
          failed = !addStringToJStringArray( env, parameters[ i ].param,   launcheeJOptions, indx++ ) ||
                   !addStringToJStringArray( env, parameters[ i++ ].value, launcheeJOptions, indx++ ) ;
          break ;
        case JST_PREFIX_PARAM :
          failed = !addStringToJStringArray( env, parameters[ i ].value, launcheeJOptions, indx++ ) ;
          break ;
        default : // should never happen
          fprintf( stderr, "bug: unknown parameter type %d\n", parameters[ i ].paramDefinition->type ) ;
          failed = JNI_TRUE ;
      }
      if ( failed ) {
        (*env)->DeleteLocalRef( env, launcheeJOptions ) ;
        launcheeJOptions = NULL ;
        *rval = -1 ;
        goto end ;      
      }
    }
  }
  
  end:
  return launcheeJOptions ;

}

static jclass findMainClassAndMethod( JNIEnv* env, char* mainClassName, char* mainMethodName, jmethodID* launcheeMainMethodID ) {

  jclass launcheeMainClassHandle = (*env)->FindClass( env, mainClassName ) ;
  
  if ( !launcheeMainClassHandle ) {
    clearException( env ) ;
    fprintf( stderr, "error: could not find startup class %s\n", mainClassName ) ;
    goto end ;
  }
  
  if ( !mainMethodName ) mainMethodName = "main" ;
  
  *launcheeMainMethodID = (*env)->GetStaticMethodID( env, launcheeMainClassHandle, 
                                                          mainMethodName, 
                                                          "([Ljava/lang/String;)V" ) ;
  if ( !*launcheeMainMethodID ) {
    clearException( env ) ;
    fprintf( stderr, "error: could not find startup method \"%s\" in class %s\n", mainMethodName, mainClassName ) ;
    launcheeMainClassHandle = NULL ;
  }

  end:
  return launcheeMainClassHandle ;
}

/** See the header file for information.
 */
extern int jst_launchJavaApp( JavaLauncherOptions *launchOptions ) {
  int            rval    = -1 ;
  
  JavaVM         *javavm = NULL ;
  JNIEnv         *env    = NULL ;
  JavaDynLib     javaLib ;
  // TODO: put this inside a JSTJVMOptionHolder 
  JavaVMOption   *jvmOptions = NULL ;

  size_t         jvmOptionsSize = 5 ;
  int            i,
                 jvmOptionsCount      = 0 ;

  jclass       launcheeMainClassHandle  = NULL;
  jmethodID    launcheeMainMethodID     = NULL;
  jobjectArray launcheeJOptions         = NULL;
  
  char      *classpath     = NULL,
            *userJvmOptsS  = NULL ; 

  jboolean  unrecognizedParamsToJvm = ( launchOptions->unrecognizedParamStrategy & JST_UNRECOGNIZED_TO_JVM ) ? JNI_TRUE : JNI_FALSE ;
  
  jint      launcheeParamCount = 0 ;
  char      *javaHome     = NULL, 
            *toolsJarD    = NULL,
            *toolsJarFile = NULL ;

  JVMSelectStrategy jvmSelectStrategy = launchOptions->jvmSelectStrategy ;
  
  javaLib.creatorFunc  = NULL ;
  javaLib.dynLibHandle = NULL ;  

    
  
  // TODO: split finding java home so that there are separate funcs to look it up from 
  // path
  // given reg entries
  // and these funcs return a pointer to a struct (or fill one up?) that has a *checked* path to 
  // the dyn lib file of the appropriate jvm type (client/server), room for the dyn lib handle (initially NULL)
  // jvm creator func, the jvm pointer, other?
  
  // it is null if it was not given as a param
  if ( !javaHome ) javaHome = launchOptions->javaHome ;
  if ( !javaHome && jst_findJavaHome( &javaHome, launchOptions->javahomeHandling ) ) goto end ; 

  if ( !javaHome || !javaHome[ 0 ] ) { // not found or an empty string
    fprintf( stderr, ( ( launchOptions->javahomeHandling ) & JST_ALLOW_JH_ENV_VAR_LOOKUP ) ? "error: JAVA_HOME not set\n" : 
                                                                                             "error: java home not provided\n");
    goto end ;
  } else if ( _jst_debug ) {
    fprintf( stderr, "DEBUG: using java home: %s\n", javaHome ) ;
  }


  if ( !( classpath = constructClasspath( launchOptions->initialClasspath, launchOptions->jarDirs, launchOptions->jars ) ) ) goto end ;
  if ( !( jvmOptions = appendJvmOption( jvmOptions, 
                                        jvmOptionsCount++, 
                                        &jvmOptionsSize, 
                                        classpath, 
                                        NULL ) ) ) goto end ; 

  // groovy specific, will be refactored out of here
  if ( !handleToolsJar( javaHome, launchOptions->toolsJarHandling, &jvmOptions, &jvmOptionsCount, &jvmOptionsSize, &toolsJarD ) ) goto end ;
  
  
  // the jvm options order handling is significant: if the same option is given more than once, the last one is the one
  // that stands. That's why we here set first the jvm opts set programmatically, then the ones from user env var
  // and then the ones from the command line. Thus the user can override the ones he wants on the next level
  // ones on the right override those on the left
  // autoset by the caller of this func -> ones from env var (e.g. JAVA_OPTS) -> ones from command line 
  
  // jvm options given as parameters to this func  
  for ( i = 0 ; i < launchOptions->numJvmOptions ; i++ ) {
    if ( !( jvmOptions = appendJvmOption( jvmOptions, 
                                          jvmOptionsCount++, 
                                          &jvmOptionsSize, 
                                          launchOptions->jvmOptions[ i ].optionString, 
                                          launchOptions->jvmOptions[ i ].extraInfo ) ) ) goto end ; 
  }

  // handle jvm options in env var JAVA_OPTS or similar
  if ( !handleJVMOptsFromEnvVar( launchOptions->javaOptsEnvVar, &jvmOptions, &jvmOptionsCount, &jvmOptionsSize,
                                 // output, needs to be freed
                                 &userJvmOptsS ) ) goto end ;

  
  // jvm options given on the command line by the user
  for ( i = 0 ; launchOptions->parameters[ i ].param ; i++ ) {

    if ( ( ( launchOptions->parameters[ i ].handling & JST_UNRECOGNIZED ) && unrecognizedParamsToJvm ) ||
         launchOptions->parameters[ i ].handling & JST_TO_JVM ) {
      if ( !( jvmOptions = appendJvmOption( jvmOptions, 
                                            jvmOptionsCount++, 
                                            &jvmOptionsSize, 
                                            launchOptions->parameters[ i ].param, 
                                            NULL ) ) ) goto end ; 
    }
  }
  
  if( _jst_debug ) {
    fprintf( stderr, "DUBUG: Starting jvm with the following %d options:\n", jvmOptionsCount ) ;
    for ( i = 0 ; i < jvmOptionsCount ; i++ ) {
      fprintf( stderr, "  %s\n", jvmOptions[ i ].optionString ) ;
    }
  }

  if ( !jst_startJvm( JNI_VERSION_1_4, jvmOptions, jvmOptionsCount, JNI_FALSE, javaHome, jvmSelectStrategy,
                      // output
                      &javaLib, &javavm, &env, &rval ) 
     ) goto end ;
  

  jst_free( toolsJarD  ) ;
  jst_free( jvmOptions ) ;
  jst_free( classpath  ) ;

  // construct a java.lang.String[] to give program args in
  // find the application main class
  // find the startup method and call it

  if ( launchOptions->extraProgramOptions ) {
    i = 0 ;
    while ( launchOptions->extraProgramOptions[ i++ ] ) launcheeParamCount++ ;
  }

  if ( !( launcheeJOptions = createJMainParams( env, launchOptions->parameters, launchOptions->extraProgramOptions, launchOptions->unrecognizedParamStrategy, &rval ) ) ) goto end ;
                                        

  if ( !( launcheeMainClassHandle = findMainClassAndMethod( env, launchOptions->mainClassName, launchOptions->mainMethodName, &launcheeMainMethodID ) ) ) goto end ; 


  // finally: launch the java application!
  (*env)->CallStaticVoidMethod( env, launcheeMainClassHandle, launcheeMainMethodID, launcheeJOptions ) ;

  if ( (*env)->ExceptionCheck( env ) ) {
    // TODO: provide an option which allows the caller to indicate whether to print the stack trace
    (*env)->ExceptionClear( env ) ;
  } else {
    rval = 0 ;
  }
  

end:
  // cleanup
  if ( javavm ) {
    if ( (*javavm)->DetachCurrentThread( javavm ) ) {
      fprintf( stderr, "Warning: could not detach main thread from the jvm at shutdown (please report this as a bug)\n" ) ;
    }
    (*javavm)->DestroyJavaVM( javavm ) ;
  }
  
  if ( javaLib.dynLibHandle ) dlclose( javaLib.dynLibHandle ) ;
  if ( classpath        ) free( classpath ) ;
  if ( jvmOptions       ) free( jvmOptions ) ;
  if ( toolsJarFile     ) free( toolsJarFile ) ;
  if ( toolsJarD        ) free( toolsJarD ) ;
  if ( userJvmOptsS     ) free( userJvmOptsS ) ;
  
  return rval ;

}
