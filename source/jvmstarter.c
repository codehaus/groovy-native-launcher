//  A library for easy creation of a native launcher for Java applications.
//
//  Copyright (c) 2006 Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
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
//  Author:  Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
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

/*
 *  The Mac OS X Leopard version of jni_md.h (Java SE 5 JDK) is broken in that it tests the value of
 *  __LP64__ instead of the presence of _LP64 as happens in Sun's Java 6.0 JDK.  To prevent spurious
 *  warnings define this with a false value.
 */
#define __LP64__ 0

#endif

#include <jni.h>

#include "jvmstarter.h"
#include "jst_dynmem.h"
#include "jst_fileutils.h"
#include "jst_stringutils.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// NOTE: when compiling w/ gcc on cygwin, pass -mno-cygwin, which makes gcc define _WIN32 and handle the win headers ok

#if defined( _WIN32 )

// as appended to JAVA_HOME + JST_FILE_SEPARATOR (when a jre) or JAVA_HOME + JST_FILE_SEPARATOR + "jre" + JST_FILE_SEPARATOR (when a jdk)
#  define PATHS_TO_SERVER_JVM "bin\\server\\jvm.dll", "bin\\jrockit\\jvm.dll"
#  define PATHS_TO_CLIENT_JVM "bin\\client\\jvm.dll"

#  include <Windows.h>
#  include <direct.h>

#  define dlopen( path, mode ) LoadLibrary( path )
#  define dlsym( libraryhandle, funcname ) GetProcAddress( libraryhandle, funcname )
#  define dlclose( handle ) FreeLibrary( handle )

// for dynamically loading SetDllDirectoryA. See the explanation later in this file.
typedef BOOL (WINAPI *SetDllDirFunc)( LPCTSTR lpPathname ) ;

#define JAVA_EXECUTABLE "java.exe"

// PATH_MAX is defined when compiling w/ e.g. msys gcc, but not w/ ms cl compiler (the visual studio c compiler)
#  if !defined( PATH_MAX )
#    define PATH_MAX MAX_PATH
#  endif

#  include "jst_winreg.h"

#else

#define JAVA_EXECUTABLE "java"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#  if defined( __linux__ )
#    if defined( __i386__ )
#      define PATHS_TO_SERVER_JVM "lib/i386/server/libjvm.so"
#      define PATHS_TO_CLIENT_JVM "lib/i386/client/libjvm.so"
#    elif defined( __amd64__ )
// I only know path to server jvm on this platform:
#      define PATHS_TO_SERVER_JVM "lib/amd64/server/libjvm.so"
#      define PATHS_TO_CLIENT_JVM "lib/amd64/server/libjvm.so"
#    else
#      error "linux currently supported only on x86 and amd64. Please contact the author to have support added."
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
//  two rather than one place to search.

#    define PATHS_TO_SERVER_JVM "Libraries/libserver.dylib", "../Libraries/libserver.dylib"
#    define PATHS_TO_CLIENT_JVM "Libraries/libclient.dylib", "../Libraries/libclient.dylib"

#    define CREATE_JVM_FUNCTION_NAME "JNI_CreateJavaVM_Impl"

#  else
#    error "Either your OS and/or architecture is not currently supported. Support should be easy to add - please see the source (look for #if defined stuff) or contact the author."
#  endif

   // for getpid()
#  include <unistd.h>

// stuff for loading a dynamic library
#  include <dlfcn.h>

#  if !defined ( __APPLE__ )
#    include <link.h>
#  endif

#endif

#if !defined( CREATE_JVM_FUNCTION_NAME )
// this is what it's called on most platforms (and in the jni specification). E.g. Apple is different.
#  define CREATE_JVM_FUNCTION_NAME "JNI_CreateJavaVM"
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#define JST_DEBUG_ENV_VAR_NAME "__JLANCHER_DEBUG"
jboolean _jst_debug = JNI_FALSE ;

extern int jst_initDebugState() {
  return _jst_debug = ( getenv( JST_DEBUG_ENV_VAR_NAME ) ? 1 : 0 ) ;
}

// The pointer to the JNI_CreateJavaVM function needs to be called w/ JNICALL calling convention. Using this typedef
// takes care of that.
typedef jint ( JNICALL *JVMCreatorFunc )( JavaVM**, void**, void* ) ;

typedef struct {
  JVMCreatorFunc creatorFunc  ;
  JstDLHandle       dynLibHandle ;
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


extern char* jst_findJavaHomeFromPath() {
  // TODO: check if this is a valid java home (how? possibly by seeing that the dyn lib can be found)
  // end loop over path elements

  // we are checking whether the executable is in "bin" dir
  // TODO: this is rather elementary test (but works unless there is a java executable in
  //       a bin dir of some other app) - a better one is needed.
  char *javaBinDir = jst_findFromPath( JAVA_EXECUTABLE, validateThatFileIsInBinDir ) ;
  if ( javaBinDir ) javaBinDir = jst_pathToParentDir( javaBinDir ) ;
  return javaBinDir ;
}


static char* getJavaHomeFromParameter( JstActualParam* processedActualParams, const char* paramName ) {

  char *javaHome = jst_getParameterValue( processedActualParams, paramName ) ;

  if ( javaHome ) {
    if ( _jst_debug ) {
      fprintf( stderr, "java home %s given as command line parameter\n", javaHome ) ;
    }
    return jst_strdup( javaHome ) ;
  }
  return NULL ;
}

static char* getJavaHomeFromEnvVar() {
  char* javaHome = getenv( "JAVA_HOME" ) ;

  if ( javaHome ) {
    if ( jst_fileExists( javaHome ) ) {
      if ( _jst_debug ) fprintf( stderr, "debug: using java home obtained from env var JAVA_HOME\n" ) ;
      return jst_strdup( javaHome ) ;
    } else {
      fprintf( stderr, "warning: JAVA_HOME points to a nonexistent location\n" ) ;
    }
  }

  return NULL ;

}

#if defined( __APPLE__ )

static char* getJavaHomeFromStandardLocation() {
  char* javaHome = "/System/Library/Frameworks/JavaVM.framework" ;
  if ( !jst_fileExists( javaHome ) ) {
     fprintf( stderr, "warning: java home not found in standard location %s\n", javaHome ) ;
     javaHome = NULL ;
   } else {
     javaHome = jst_strdup( javaHome ) ;
   }
  return javaHome ;
}

#endif

/** First sees if JAVA_HOME is set and points to an existing location (the validity is not checked).
 * Next, windows registry is checked (if on windows) or if on os-x the standard location
 * /System/Library/Frameworks/JavaVM.framework is checked for existence.
 * Last, java is looked up from the PATH.
 * This is just a composite function putting together the ways to search for a java installation
 * in a way that is very coomon. If you want to use a different order, please use the more
 * specific funtions, e.g. jst_findJavaHomeFromPath().
 * Returns NULL if java home could not be figured out. Freeing the returned value is up to the caller. */
extern char* jst_findJavaHome( JstActualParam* processedActualParams ) {
  char* javaHome ;

  errno = 0 ;

  if (
  // TODO It is a little dubious that the hane of the parameter that is used to pass in
  //      java home is hard codeed here - refactor.
     ( javaHome = getJavaHomeFromParameter( processedActualParams, "-jh" ) ) || errno
  || ( javaHome = getJavaHomeFromEnvVar()    ) || errno
  || ( javaHome = jst_findJavaHomeFromPath() ) || errno
#  if defined( _WIN32 )
  || ( javaHome = jst_findJavaHomeFromWinRegistry() ) || errno
#  elif defined( __APPLE__ )
  || ( javaHome = getJavaHomeFromStandardLocation() )
#  endif
  ) {
    ; // do nothing
  }

  if ( !javaHome ) fprintf( stderr, "error: could not locate java home\n" ) ;

  return javaHome ;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#if defined( _WIN32 )

static SetDllDirFunc getDllDirSetterFunc() {

  HINSTANCE hKernel32 = GetModuleHandle( "kernel32" ) ;
  return hKernel32 ? (SetDllDirFunc)GetProcAddress( hKernel32, "SetDllDirectoryA" ) // not found on older windows (e.g. win2000)
                   : NULL ; // should never happen
}

#endif

static char* getJvmSelectStrategy( JVMSelectStrategy jvmSelectStrategy, char*** lookupDirsOut ) {

  static char* potentialPathsToServerJVM[]              = { PATHS_TO_SERVER_JVM, NULL } ;
  static char* potentialPathsToClientJVM[]              = { PATHS_TO_CLIENT_JVM, NULL } ;
  static char* potentialPathsToAnyJVMPreferringServer[] = { PATHS_TO_SERVER_JVM, PATHS_TO_CLIENT_JVM, NULL } ;
  static char* potentialPathsToAnyJVMPreferringClient[] = { PATHS_TO_CLIENT_JVM, PATHS_TO_SERVER_JVM, NULL } ;

  jboolean   preferClient = ( jvmSelectStrategy & JST_PREFER_CLIENT ) ? JNI_TRUE : JNI_FALSE,
             allowClient  = ( jvmSelectStrategy & JST_CLIENTVM )      ? JNI_TRUE : JNI_FALSE,
             allowServer  = ( jvmSelectStrategy & JST_SERVERVM )      ? JNI_TRUE : JNI_FALSE ;

  char* mode = NULL ;

  assert( allowClient || allowServer ) ;

  if ( allowServer && !allowClient ) {
    mode = "server" ;
    *lookupDirsOut = potentialPathsToServerJVM ;
  } else if ( allowClient && !allowServer ) {
    mode = "client" ;
    *lookupDirsOut = potentialPathsToClientJVM ;
  } else {
    mode = "client or server" ;
    *lookupDirsOut = preferClient ? potentialPathsToAnyJVMPreferringClient : potentialPathsToAnyJVMPreferringServer ;
  }

  return mode ;

}

#if defined( _WIN32 )

/** Sets the pathBuffer[0]=0 if dll setter func is found, to current dir otherwise. */
static SetDllDirFunc getDllDirSetterFuncOrPathToCurrentDir( char* pathBuffer, size_t bufsize ) {

  SetDllDirFunc dllDirSetterFunc = dllDirSetterFunc = getDllDirSetterFunc() ;

  *pathBuffer = '\0' ;

  errno = 0 ;

  if ( !dllDirSetterFunc ) {
    if ( !getcwd( pathBuffer, (int)bufsize ) ) {
      fprintf( stderr, "error %d: could not figure out current dir: %s\n", errno, strerror( errno ) ) ;
      *pathBuffer = '\0' ;
    }
  }

  return dllDirSetterFunc ;

}

// on some sun jdks (e.g. 1.6.0_06) the jvm.dll depends on msvcr71.dll, which is in the bin dir
// of java installation, which may not be on PATH and thus the dll is not found if it is
// not available in e.g. system dll dirs.
// For a thorough discussion on this, see http://www.duckware.com/tech/java6msvcr71.html
// Returns 0 on error
static int addJREBinDirToDllSearchPath( const char* javaHome, SetDllDirFunc dllDirSetterFunc ) {

  char* javaBinDir ;
  int   success = 1 ;

  JST_CONCATA2( javaBinDir, javaHome, JST_FILE_SEPARATOR "bin" ) ;

  if ( !javaBinDir ) return 0 ;

  SetLastError( 0 ) ;

  if ( dllDirSetterFunc ) {
    // NOTE: SetDllDirectoryA is only available on Win XP sp 1 and later. For compatibility
    //       with older windows versions, we dynamically load that function to see if it's available.
    if ( !dllDirSetterFunc( javaBinDir ) ) {
      jst_printWinError( GetLastError() ) ;
      success = JNI_FALSE ;
    }
  } else {
    chdir( javaBinDir ) ;
  }

  jst_freea( javaBinDir ) ;
  return success ;
}

#endif

static JstDLHandle openDynLib( const char* pathToLib ) {

  JstDLHandle libraryHandle ;

  errno = 0 ;
  if ( !( libraryHandle = dlopen( pathToLib, RTLD_LAZY ) ) )  {
    fprintf( stderr, "error: dynamic library %s exists but could not be loaded!\n", pathToLib ) ;
    if ( errno ) fprintf( stderr, strerror( errno ) ) ;
#if defined( _WIN32 )
    jst_printWinError( GetLastError() ) ;
#else
    {
      char* errorMsg = dlerror() ;
      if ( errorMsg ) fprintf( stderr, "%s\n", errorMsg ) ;
    }
#endif
  }

  return libraryHandle ;

}

/** returns != 0 if loaded successfully or on error. On error jvmLib == NULL */
static int loadJvmDynLib( const char* java_home, const char* dynLibFile, JstDLHandle* jvmLib
#if defined( _WIN32 )
    , SetDllDirFunc dllDirSetterFunc
#endif
) {

  int i ;

  *jvmLib = NULL ;

  for ( i = 0 ; i < 2 ; i++ ) { // try both jdk and jre style paths

    // on a jdk, we need to add jre to the path
    char* path ;
    JST_CONCATA4( path, java_home, JST_FILE_SEPARATOR, ( i == 0 ) ? "jre" JST_FILE_SEPARATOR : "", dynLibFile ) ;
    if ( !path ) {
      fprintf( stderr, "java launcher memory error\n" ) ;
      return 1 ;
    }

    if ( jst_fileExists( path ) ) {

#if defined( _WIN32 )
      if ( !addJREBinDirToDllSearchPath( java_home, dllDirSetterFunc ) ) return 1 ;
#endif

      *jvmLib = openDynLib( path ) ;

      if ( _jst_debug && *jvmLib ) fprintf( stderr, "debug: loaded jvm dynamic library %s\n", path ) ;

      break ;
    }
  }

  return *jvmLib != NULL ;

}

//static JvmCreatorFunc getJvmCreatorFunc( JstDLHandle jvmDynLibHandle ) {

//}

/** In case there are errors, the returned struct contains only NULLs. */
static JavaDynLib findJVMDynamicLibrary( char* java_home, JVMSelectStrategy jvmSelectStrategy ) {

#if defined( _WIN32 )
  static SetDllDirFunc dllDirSetterFunc = NULL ;
  char currentDir[ PATH_MAX + 1 ] ;
#endif

  char       *mode ;
  JavaDynLib rval ;
  int        i ;
  JstDLHandle   jvmLib = (JstDLHandle)0 ;
  char**     lookupDirs = NULL ;
  char*      dynLibFile ;


  rval.creatorFunc  = NULL ;
  rval.dynLibHandle = NULL ;

  mode = getJvmSelectStrategy( jvmSelectStrategy, &lookupDirs ) ;

#if defined( _WIN32 )

  dllDirSetterFunc = getDllDirSetterFuncOrPathToCurrentDir( currentDir, sizeof( currentDir ) ) ;
  if ( !dllDirSetterFunc && !*currentDir ) goto end ;

#endif

  for ( i = 0; ( dynLibFile = lookupDirs[ i ] ) ; i++ ) {

    if ( loadJvmDynLib( java_home, dynLibFile, &jvmLib
#if defined( _WIN32 )
        , dllDirSetterFunc
#endif
        ) ) break ;

  }


  if ( !jvmLib ) {
    fprintf( stderr, "error: could not find %s jvm under %s\n"
                     "       please check that it is a valid jdk / jre containing the desired type of jvm\n",
                     mode, java_home ) ;
    goto end ;
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
    fprintf( stderr, "strange bug: jvm creator function not found in jvm dynamic library\n"
                     "Please rerun with debug output enabled by setting environment variable " JST_DEBUG_ENV_VAR_NAME "\n" ) ;
  }

  end:
# if defined( _WIN32 )
  if ( dllDirSetterFunc ) {
    dllDirSetterFunc( NULL ) ;
  } else if ( currentDir[ 0 ] ) {
    chdir( currentDir ) ;
  }
# endif

  return rval ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** To be called when there is a pending exception that is the result of some
 * irrecoverable error in this startup program. Clears the exception and prints its description. */
static void clearException( JNIEnv* env ) {

  (*env)->ExceptionDescribe( env ) ;
  (*env)->ExceptionClear( env ) ;

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// TODO: move jni utilities to a separate source file (for clarity)


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

  jboolean firstEntry =
    // "-Xbootclasspath:"
    cp[ 15 ] == ':' ? !cp[ 16 ] :
    // "-Djava.class.path=" == 18 chars -> if 19th char (index 18) is not a null char, we have more than that and need to append path separator
    // "-Xbootclasspath/a:" or "-Xbootclasspath/p:" are same length
    !cp[ 18 ]
    ;

  if ( !firstEntry
      && !( cp = jst_append( cp, cpsize, JST_PATH_SEPARATOR, NULL ) ) ) return NULL ;

  return jst_append( cp, cpsize, entry, NULL ) ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


/** returns != 0 on failure. May change the target to point to a new location */
static jboolean appendJarsFromDir( JarDirSpecification* dirSpec, char** target, size_t* targetSize ) {

  char *dirName = dirSpec->name ;
  char **jarNames = jst_getFileNames( dirName, NULL, ".jar", NULL ),
       *s ;
  int i = 0 ;
  jboolean dirNameEndsWithSeparator = jst_dirNameEndsWithSeparator( dirName ),
           errorOccurred = JNI_FALSE ;

  if ( !jarNames ) return JNI_TRUE ;

  while ( ( s = jarNames[ i++ ] ) ) {
    if ( !dirSpec->filter || dirSpec->filter( dirName, s ) ) {
      if(    !( *target = appendCPEntry( *target, targetSize, dirName ) )
          || !( *target = jst_append( *target, targetSize, dirNameEndsWithSeparator ? "" : JST_FILE_SEPARATOR, s, NULL ) )
        ) {
        errorOccurred = JNI_TRUE ;
        goto end ;
      }
    }
  }

  if ( dirSpec->fetchRecursively ) {
    // FIXME - implement this behavior
    fprintf( stderr, "error: adding jars recursively from a directory not supported yet\n" ) ;
    errorOccurred = JNI_TRUE ;
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

static char* selectClasspathType( JstClasspathStrategy classpathStrategy ) {

  char* cpPrefix ;

  switch ( classpathStrategy ) {
    case JST_NORMAL_CLASSPATH :
      cpPrefix = "-Djava.class.path=" ;
      break ;
    case JST_BOOTSTRAP_CLASSPATH :
      cpPrefix = "-Xbootclasspath:" ;
      break ;
    case JST_BOOTSTRAP_CLASSPATH_A :
      cpPrefix = "-Xbootclasspath/a:" ;
      break ;
    case JST_BOOTSTRAP_CLASSPATH_P :
      cpPrefix = "-Xbootclasspath/p:" ;
      break ;
    default :
      fprintf( stderr, "error: unrecognized classpath strategy %d\n", classpathStrategy ) ;
      cpPrefix = NULL ;
      break ;
  }
  return cpPrefix ;
}

/** @param initialCP may be NULL */
static char* constructClasspath( char* initialCP, JarDirSpecification* jarDirs, char** jars, JstClasspathStrategy classpathStrategy ) {
  size_t cpsize    = 255 ; // just an initial guess for classpath length, will be expanded as necessary
  char*  classpath = NULL ;
  char*  cpPrefix  = selectClasspathType( classpathStrategy ) ;

  if ( !cpPrefix ) goto end ;

  if ( !( classpath = jst_append( NULL, &cpsize, cpPrefix, NULL ) ) ) goto end ;

  if ( initialCP ) {
    if ( !( classpath = appendCPEntry( classpath, &cpsize, initialCP ) ) ) goto end ;
  }

  // add the jars from the given dirs
  if ( jarDirs ) {
    int i ;
    for ( i = 0 ; jarDirs[ i ].name ; i++ ) {
      if ( appendJarsFromDir( &(jarDirs[ i ]), &classpath, &cpsize ) ) goto end ; // error msg already printed
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

/**
 * Adds the options in the param userOpts (separated by spaces) to the jvm options passed in.
 *
 * Known bug: always separates at ' ', does not take into account quoted values w/ spaces. Should not
 * usually be a problem, so fixing it is left till someone complains about it, i.e. actually has
 * a problem w/ it.
 * @param userOpts contains ' '  separated options for the jvm. May not be NULL or empty.
 * @return NULL on error, otherwise a pointer that must be freed by the called *after* the jvm has been started (a buffer containing all the options strings). */
static char* handleJVMOptsString( const char* userOpts, JavaVMOption** jvmOptions, int* jvmOptionsCount, size_t* jvmOptionsSize ) {

  char *s, *jvmOptsS ;
  jboolean firstTime = JNI_TRUE ;

  if ( !( jvmOptsS = jst_strdup( userOpts ) ) ) return NULL ;

  for ( ; ( s = strtok( firstTime ? jvmOptsS : NULL, " " ) ) ; firstTime = JNI_FALSE ) {

    if ( ! ( *jvmOptions = appendJvmOption( *jvmOptions,
                                            (*jvmOptionsCount)++,
                                            jvmOptionsSize,
                                            s,
                                            NULL ) ) ) {
      free( jvmOptsS ) ;
      return NULL ;
    }
  }

  return jvmOptsS ;

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

static jclass getJavaStringClass( JNIEnv* env ) {

  static jclass strClass = NULL ;

  if ( !strClass && !( strClass = (*env)->FindClass(env, "java/lang/String") ) ) {
    clearException( env ) ;
    fprintf( stderr, "error: could not find java.lang.String class\n" ) ; // should never happen
  }

  return strClass ;

}

static jobjectArray createJObjectArray( JNIEnv* env, jint size, jclass klass ) {
  jobjectArray arr = (*env)->NewObjectArray( env, size, klass, NULL ) ;

  if ( !arr ) {
    clearException( env ) ;
    fprintf( stderr, "error: could not allocate memory for java array\n" ) ;
  }

  return arr ;
}

static int countParamsPassedToMainMethod( JstActualParam* parameters, JstUnrecognizedParamStrategy unrecognizedParamStrategy ) {

  int count = 0,
      i ;

  for ( i = 0 ; parameters[ i ].param ; i++ ) {
    if ( jst_isToBePassedToLaunchee( parameters + i, unrecognizedParamStrategy ) ) {
      count++ ;
    }
  }

  return count ;

}

static int ensureJNILocalCapacity( JNIEnv* env, jint requiredCapacity ) {
  int result = (*env)->EnsureLocalCapacity( env, requiredCapacity ) ;
  if ( result ) {
    fprintf( stderr, "error: could not allocate memory in jvm local frame\n" ) ;
  }
  return result ;
}

/** @param strings must be NULL terminated. May be NULL.
 * @param indx start at this index
 * @return != 0 on error */
static int addStringsToJavaStringArray( JNIEnv* env, jobjectArray jstrings, char** strings, jint indx ) {

  int errorOccurred = 0 ;

  if ( !strings ) return 0 ;

  for ( ; *strings ; strings++ ) {
    if ( ( errorOccurred = !addStringToJStringArray( env, *strings, jstrings, indx++ ) ) ) {
      break ;
    }
  }

  return errorOccurred ;

}

/** Returns NULL on error, and sets rval output param accordingly.
 * On successfull execution rval is not touched. */
void printParameterDebugInformation( JstActualParam* parameter, JstParamInfo* paramInfo ) {


  if ( _jst_debug ) {

    JstInputParamHandling paramHandling = paramInfo ? paramInfo->handling : JST_TERMINATING_OR_AFTER ;
    JstParamClass         paramClass    = paramInfo ? paramInfo->type : 0 ;

    char* paramValue = parameter->value ;

    if ( ( paramClass    == JST_SINGLE_PARAM ) ||
         ( paramHandling == JST_TERMINATING_OR_AFTER ) ||
         ( paramClass    == JST_DOUBLE_PARAM ) ) {
      paramValue = parameter->param ;
    }

    fprintf( stderr, "  %s\n", paramValue ) ;

    if ( paramClass == JST_DOUBLE_PARAM )
      fprintf( stderr, "  %s\n", parameter->value ) ;

  }

}

static int addParametersToJStringArray( JNIEnv* env, jobjectArray launcheeJOptions, JstActualParam* parameters, JstUnrecognizedParamStrategy unrecognizedParamStrategy, jint indx ) {

  int i ;

  for ( i = 0 ; parameters[ i ].param ; i++ ) {

    if ( jst_isToBePassedToLaunchee( parameters + i, unrecognizedParamStrategy ) ) {

      int errorOccurred = 0 ;

      JstParamInfo* paramInfo  = parameters[ i ].paramDefinition ;
      JstParamClass paramClass = paramInfo ? paramInfo->type : 0 ;

      printParameterDebugInformation( parameters + i, paramInfo ) ;

      switch ( paramClass ) {
        case JST_SINGLE_PARAM :
        case JST_TERMINATING_OR_AFTER  :
          errorOccurred = !addStringToJStringArray( env, parameters[ i ].param, launcheeJOptions, indx++ ) ;
          break ;
        case JST_DOUBLE_PARAM :
          errorOccurred = !addStringToJStringArray( env, parameters[ i ].param,   launcheeJOptions, indx++ ) ||
                          !addStringToJStringArray( env, parameters[ i++ ].value, launcheeJOptions, indx++ ) ;
          break ;
        default : // prefix params + all params after termination
          assert( paramClass == JST_PREFIX_PARAM || ( !paramInfo || ( paramInfo->handling & JST_TERMINATING_OR_AFTER ) ) ) ;
          errorOccurred = !addStringToJStringArray( env, parameters[ i ].value, launcheeJOptions, indx++ ) ;
          break ;
      }

      if ( errorOccurred ) return errorOccurred ;

    }
  } // for

  return 0 ;

}

static jobjectArray createJStringArrayToHoldParamsToMain( JNIEnv* env, JstActualParam* parameters, char** extraProgramOptions, JstUnrecognizedParamStrategy unrecognizedParamStrategy ) {

  int          passedParamCount ;
  jclass       strClass ;

  passedParamCount = countParamsPassedToMainMethod( parameters, unrecognizedParamStrategy ) ;

  passedParamCount += jst_pointerArrayLen( (void**)(void*)extraProgramOptions ) ;

  if ( _jst_debug ) fprintf( stderr, "passing %d parameters to main method: \n", passedParamCount ) ;

  if ( ensureJNILocalCapacity( env, passedParamCount + 1 ) || // + 1 for the String[] to hold the params
       !( strClass = getJavaStringClass( env ) ) ) {
    return NULL ;
  }

  return createJObjectArray( env, passedParamCount, strClass ) ;

}

static jobjectArray createJMainParams( JNIEnv* env, JstActualParam* parameters, char** extraProgramOptions, JstUnrecognizedParamStrategy unrecognizedParamStrategy ) {

  jobjectArray launcheeJOptions ;

  int indx             = 0, // index in java String[] (args to main)
      errorOccurred    = 0 ;

  launcheeJOptions = createJStringArrayToHoldParamsToMain( env, parameters, extraProgramOptions, unrecognizedParamStrategy ) ;
  if ( !launcheeJOptions ) return NULL ;

  if ( !( errorOccurred = addStringsToJavaStringArray( env, launcheeJOptions, extraProgramOptions, indx ) ) ) {

    indx += jst_pointerArrayLen( (void**)(void*)extraProgramOptions ) ;

    if ( _jst_debug ) jst_printStringArray( stderr, "  %s\n", extraProgramOptions ) ;

    errorOccurred = addParametersToJStringArray( env, launcheeJOptions, parameters, unrecognizedParamStrategy, indx ) ;

  }

  if ( errorOccurred ) {
    (*env)->DeleteLocalRef( env, launcheeJOptions ) ;
    launcheeJOptions = NULL ;
  }

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
    fprintf( stderr, "error: could not find method \"%s\" in class %s\n", mainMethodName, mainClassName ) ;
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

  jclass       launcheeMainClassHandle  = NULL ;
  jmethodID    launcheeMainMethodID     = NULL ;
  jobjectArray launcheeJOptions         = NULL ;

  char      *classpath     = NULL,
            *userJvmOptsS  = NULL ;

  jboolean  unrecognizedParamsToJvm = ( launchOptions->unrecognizedParamStrategy & JST_UNRECOGNIZED_TO_JVM ) ? JNI_TRUE : JNI_FALSE ;

  JVMSelectStrategy jvmSelectStrategy = launchOptions->jvmSelectStrategy ;

  javaLib.creatorFunc  = NULL ;
  javaLib.dynLibHandle = NULL ;


  if ( !( classpath = constructClasspath( launchOptions->initialClasspath, launchOptions->jarDirs, launchOptions->jars, launchOptions->classpathStrategy ) ) ) goto end ;
  if ( !( jvmOptions = appendJvmOption( jvmOptions,
                                        jvmOptionsCount++,
                                        &jvmOptionsSize,
                                        classpath,
                                        NULL ) ) ) goto end ;


  // FIXME - extract function => move setting jvm starup options to a separate func

  // the order in which jvm options are handled is significant: if the same option is given more than once, the last one is the one
  // that stands. That's why we here set first the jvm opts set programmatically, then the ones from user env var
  // and then the ones from the command line. Thus the user can override the ones he wants on the next level,
  // i.e. the precedence is:
  // set on command line - env var - programmatically set (from most significant to least)

  // jvm options given as parameters to this func
  for ( i = 0 ; i < launchOptions->numJvmOptions ; i++ ) {
    if ( !( jvmOptions = appendJvmOption( jvmOptions,
                                          jvmOptionsCount++,
                                          &jvmOptionsSize,
                                          launchOptions->jvmOptions[ i ].optionString,
                                          launchOptions->jvmOptions[ i ].extraInfo ) ) ) goto end ;
  }

  // handle jvm options in env var JAVA_OPTS or similar
  {
    char *envVar = launchOptions->javaOptsEnvVar ;
    if ( envVar ) {
      envVar = getenv( envVar ) ;
      if ( envVar && envVar[ 0 ] ) {
        userJvmOptsS = handleJVMOptsString( envVar, &jvmOptions, &jvmOptionsCount, &jvmOptionsSize ) ;
        if ( !userJvmOptsS ) goto end ;
      }
    }
  }


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

  if ( !jst_startJvm( JNI_VERSION_1_4, jvmOptions, jvmOptionsCount, JNI_FALSE, launchOptions->javaHome, jvmSelectStrategy,
                      // output
                      &javaLib, &javavm, &env, &rval )
     ) goto end ;


  jst_free( jvmOptions ) ;
  jst_free( classpath  ) ;

  // construct a java.lang.String[] to give program args in
  // find the application main class
  // find the startup method and call it

  if ( !( launcheeJOptions = createJMainParams( env, launchOptions->parameters, launchOptions->extraProgramOptions, launchOptions->unrecognizedParamStrategy ) ) ) goto end ;


  if ( !( launcheeMainClassHandle = findMainClassAndMethod( env, launchOptions->mainClassName, launchOptions->mainMethodName, &launcheeMainMethodID ) ) ) goto end ;


  if ( _jst_debug ) {
    fprintf( stderr, "DEBUG: invoking %s.%s\n", launchOptions->mainClassName, launchOptions->mainMethodName ) ;
  }

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
  if ( userJvmOptsS     ) free( userJvmOptsS ) ;

  return rval ;

}
