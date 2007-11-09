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
#include <stdarg.h>

#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>

#if defined ( __APPLE__ )
#  include <TargetConditionals.h>
#endif

#include <jni.h>

#include "jvmstarter.h"

// NOTE: when compiling w/ gcc on cygwin, pass -mno-cygwin, which makes gcc define _WIN32 and handle the win headers ok
#if defined( _WIN32 )

// as appended to JAVA_HOME + JST_FILE_SEPARATOR (when a jre) or JAVA_HOME + JST_FILE_SEPARATOR + "jre" + JST_FILE_SEPARATOR (when a jdk) 
#  define PATHS_TO_SERVER_JVM "bin\\server\\jvm.dll", "bin\\jrockit\\jvm.dll" 
#  define PATHS_TO_CLIENT_JVM "bin\\client\\jvm.dll"

#  define CREATE_JVM_FUNCTION_NAME "JNI_CreateJavaVM"

#  include "Windows.h"

   typedef HINSTANCE DLHandle;
#  define dlopen(path, mode) LoadLibrary(path)
#  define dlsym(libraryhandle, funcname) GetProcAddress(libraryhandle, funcname)
#  define dlclose(handle) FreeLibrary(handle)
#  define dlerror() NULL

#if !defined( PATH_MAX )
#  define PATH_MAX 512
#endif
   
#else

#  if defined( __linux__ ) && defined( __i386__ )

#    define PATHS_TO_SERVER_JVM "lib/i386/server/libjvm.so"
#    define PATHS_TO_CLIENT_JVM "lib/i386/client/libjvm.so"

#    define CREATE_JVM_FUNCTION_NAME "JNI_CreateJavaVM"

#  elif defined( __sun__ ) 

#    if defined( __sparc__ ) || defined( __sparc ) || defined( __sparcv9 )

#      define PATHS_TO_SERVER_JVM "lib/sparc/server/libjvm.so", "lib/sparcv9/server/libjvm.so"
#      define PATHS_TO_CLIENT_JVM "lib/sparc/client/libjvm.so", "lib/sparc/libjvm.so"

#    elif defined( __i386__ ) || defined( __i386 )
        // these are just educated guesses, I have no access to solaris running on x86...
#      define PATHS_TO_SERVER_JVM "lib/i386/server/libjvm.so"
#      define PATHS_TO_CLIENT_JVM "lib/i386/client/libjvm.so"

#    endif

#    define CREATE_JVM_FUNCTION_NAME "JNI_CreateJavaVM"

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
#    error "Either your OS and/or architecture is not currently supported. Support should be easy to add - please see the source (look for #if defined stuff)."
#  endif

   // for getpid()
#  include <unistd.h>

#  include <dirent.h>

// stuff for loading a dynamic library
#  include <dlfcn.h>
#  if ! defined ( __APPLE__ )
#    include <link.h>
#  endif
  
   typedef void *DLHandle;

#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static jboolean _jst_debug = JNI_FALSE;

// The pointer to the JNI_CreateJavaVM function needs to be called w/ JNICALL calling convention. Using this typedef
// takes care of that.
typedef jint (JNICALL *JVMCreatorFunc)(JavaVM**,void**,void*);

typedef struct {
  JVMCreatorFunc creatorFunc;
  DLHandle       dynLibHandle;
} JavaDynLib;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// the only difference to originals is that these print an error msg if they fail
extern void* jst_malloc( size_t size ) {
  void* rval = malloc( size ) ;
  if ( !rval ) fprintf( stderr, "error %d in malloc: %s", errno, strerror( errno ) ) ;
  return rval ;
}

extern void* jst_calloc( size_t nelem, size_t elsize ) {
  void* rval = calloc( nelem, elsize ) ;
  if ( !rval ) fprintf( stderr, "error %d in calloc: %s", errno, strerror( errno ) ) ;
  return rval ;  
}

extern void* jst_realloc( void* ptr, size_t size ) {
  void* rval = realloc( ptr, size ) ;
  if ( !rval ) fprintf( stderr, "error %d in realloc: %s", errno, strerror( errno ) ) ;
  return rval ;
}

extern char* jst_getExecutableHome() {
  static char* _execHome = NULL;
  
  char   *execHome = NULL;
# if defined( __linux__ ) || defined( __sun__ )
  char   *procSymlink;
# endif

# if defined( _WIN32 )
  size_t currentBufSize = 0;
# endif
  size_t len;
  
  if(_execHome) return _execHome;
  
# if defined( _WIN32 )

  do {
    if(currentBufSize == 0) {
      currentBufSize = PATH_MAX + 1;
      execHome = jst_malloc( currentBufSize * sizeof( char ) ) ;
    } else {
      execHome = jst_realloc( execHome, (currentBufSize += 100) * sizeof( char ) ) ;
    }
    
    if ( !execHome ) return NULL ; 
    

    // reset the error state, just in case it has been left dangling somewhere and 
    // GetModuleFileNameA does not reset it (its docs don't tell). 
    // GetModuleFileName docs: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/getmodulefilename.asp
    SetLastError(0); 
    len = GetModuleFileNameA(NULL, execHome, currentBufSize);
  } while(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
  // this works equally well but is a bit less readable
  // } while(len == currentBufSize);

  if(len == 0) {
    fprintf(stderr, "error: finding out executable location failed w/ error code %d\n", (int)GetLastError());
    free(execHome);
    return NULL; 
  }
  
# elif defined( __linux__ ) || defined( __sun__ )

  // TODO - remove space for this from the stack, not dynamically
  procSymlink = jst_malloc( 40 * sizeof( char ) ) ; // big enough
  execHome    = jst_malloc( ( PATH_MAX + 1 ) * sizeof( char ) ) ;
  if( !procSymlink || !execHome ) {
    if ( procSymlink ) free( procSymlink ) ; 
    if ( execHome    ) free( execHome    ) ;
    return NULL ;
  }

  sprintf(procSymlink,
#   if defined( __linux__ )
      // /proc/{pid}/exe is a symbolic link to the executable
      "/proc/%d/exe" 
#   elif defined( __sun__ )
      // see above
      "/proc/%d/path/a.out"
#   endif
      , (int)getpid()
  );

  if( !jst_fileExists( procSymlink ) ) {
    free(procSymlink);
    free(execHome);
    return "";
  }

  if( !realpath( procSymlink, execHome ) ) {
    fprintf( stderr, strerror( errno ) ) ;
    free( procSymlink ) ;
    free( execHome    ) ;
    return NULL ;
  }
  free( procSymlink ) ;

# elif defined ( __APPLE__ )

// TODO

# endif

# if defined( _WIN32 ) || defined ( __linux__ ) || defined( __sun__ )
  // cut off the executable name
  *(strrchr(execHome, JST_FILE_SEPARATOR[0]) + 1) = '\0';   
  len = strlen(execHome);
  execHome = jst_realloc( execHome, len + 1 ) ; // should not fail as we are shrinking the buffer
  assert( execHome ) ;
  return _execHome = execHome ;
  
# else
  // FIXME - remove this once this feature is supported for __APPLE__
  return "";
# endif

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/** Tries to find Java home by looking where java command is located on PATH. */
extern char* jst_findJavaHomeFromPath() {
  static char* _javaHome = NULL ;
  char *path = NULL, *p, *javahome = NULL ;
  size_t jhlen = 100 ;
  jboolean firstTime = JNI_TRUE ;
  
  p = getenv( "PATH" ) ;
  if ( !p ) goto end ;
  
  if ( !( path = jst_append( NULL, NULL, p, NULL ) ) ) goto end ;
  
  for ( ; ( p = strtok( firstTime ? path : NULL, JST_PATH_SEPARATOR ) ) ; firstTime = JNI_FALSE ) {
    size_t len = strlen( p ) ;
    if ( len == 0 ) continue ;
    
    if ( javahome ) javahome[ 0 ] = '\0' ;
    
    if ( !( javahome = jst_append( javahome, &jhlen, p, NULL ) ) ) goto end ;
    
    javahome = jst_append( javahome, &jhlen, ( javahome[ len - 1 ] != JST_FILE_SEPARATOR[ 0 ] ) ? JST_FILE_SEPARATOR : "", "java" 
#if defined( _WIN32 )
                      ".exe"
#endif
    , NULL ) ;
    if ( !javahome ) goto end ;
    
    if ( jst_fileExists( javahome ) ) {
#if !defined( _WIN32 )
      char realFile[ PATH_MAX + 1 ] ;
      if ( !realpath( javahome, realFile ) ) {
        fprintf( stderr, strerror( errno ) ) ;
        goto end ;
      }
      javahome[ 0 ] = '\0' ;
      javahome = jst_append( javahome, &jhlen, realFile, NULL ) ;
      if ( !javahome ) goto end ;
#endif
      *( strrchr( javahome, JST_FILE_SEPARATOR[ 0 ] ) ) = '\0' ;
      len = strlen( javahome ) ;
      if ( len < 4 ) goto end ;
      // see if we are in the bin dir of java home
      if ( memcmp( javahome + len - 3, "bin", 3 ) == 0 ) {
        javahome[ len -= 4 ] = '\0' ;
        assert( len == strlen( javahome ) ) ;
        _javaHome = jst_append( NULL, NULL, javahome, NULL ) ;
        if ( !_javaHome ) goto end ; 
      }
      break ;
    }
    // check if this is a valid java home (how?)
  }
  
  end:
  if ( path     ) free( path     ) ;
  if ( javahome ) free( javahome ) ;
  return _javaHome ;
}

#if defined( _WIN32 )

static char* findJavaHomeFromWinRegistry() {
  static char* _javaHome = NULL ;
  LONG status ;
  if ( _javaHome ) return _javaHome ;
  // TODO
  return _javaHome ;
}
#endif

/** First sees if JAVA_HOME is set and points to an existing location (the validity is not checked).
 * Next, windows registry is checked (if on windows). Last, java is looked up from the PATH. */
static char* findJavaHome( JavaHomeHandling javaHomeHandling ) {
  static char* _javaHome = NULL ;
  char* javahome ;
  
  if ( _javaHome ) return _javaHome ;
  
  if ( javaHomeHandling & JST_ALLOW_JH_ENV_VAR_LOOKUP ) {
    javahome = getenv( "JAVA_HOME" ) ;
    if ( javahome ) {
      if ( jst_fileExists( javahome ) ) 
        return _javaHome = javahome ;
      else
        fprintf( stderr, "warning: JAVA_HOME points to a nonexistent location\n" ) ;
    }
  }
  
  if ( (javaHomeHandling & JST_ALLOW_PATH_LOOKUP)     && ( _javaHome = jst_findJavaHomeFromPath()    ) ) return _javaHome ;
  
#if defined( _WIN32 )
  if ( (javaHomeHandling & JST_ALLOW_REGISTRY_LOOKUP) && ( _javaHome = findJavaHomeFromWinRegistry() ) ) return _javaHome ; 
#endif

#if defined ( __APPLE__ )
  if ( !_javaHome || !_javaHome[ 0 ] ) _javaHome = "/System/Library/Frameworks/JavaVM.framework" ;
#endif

  
  if ( !_javaHome ) fprintf( stderr, "error: could not locate java home\n" ) ;
  return _javaHome ;
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern int jst_contains(char** args, int* numargs, const char* option, const jboolean removeIfFound) {
  int i       = 0, 
      foundAt = -1;
  for(; i < *numargs; i++) {
    if(strcmp(option, args[i]) == 0) {
      foundAt = i;
      break;
    }
  }
  if(foundAt != -1) return -1;
  if(removeIfFound) {
    (*numargs)--;
    for(; i < *numargs; i++) {
      args[i] = args[i + i];
    }
  }
  return foundAt;
}

extern int jst_indexOfParam( char** args, int numargs, char* paramToSearch) {
  int i = 0;
  for( ; i < numargs; i++) {
    if( strcmp( paramToSearch, args[i] ) == 0 ) return i ;
  }
  return -1;  
}

extern char* jst_valueOfParam(char** args, int* numargs, int* checkUpto, const char* option, const JstParamClass paramType, const jboolean removeIfFound, jboolean* error) {
  int i    = 0, 
      step = 1;
  size_t len;
  char* retVal = NULL;

  switch(paramType) {
    case JST_SINGLE_PARAM :
      for(;i < *checkUpto; i++) {
        if(strcmp(option, args[i]) == 0) {
          retVal = args[i];
          break;
        }
      }
      break;
    case JST_DOUBLE_PARAM :
      step = 2;
      for(; i < *checkUpto; i++) {
        if(strcmp(option, args[i]) == 0) {
          if(i == (*numargs - 1)) {
            *error = JNI_TRUE;
            fprintf(stderr, "error: %s must have a value\n", option);
            return NULL;
          }
          retVal = args[i + 1];
          break;
        }
      }
      break;
    case JST_PREFIX_PARAM :
      len = strlen(option);
      for(; i < *checkUpto; i++) {
        if(memcmp(option, args[i], len) == 0) {
          retVal = args[i] + len;
          break;
        }
      }
      break;
  }
  
  if(retVal && removeIfFound) {
    for(;i < (*numargs - step); i++) {
      args[i] = args[i + step];
    }
    *numargs   -= step;
    *checkUpto -= step;
  }
  
  return retVal;  
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** In case there are errors, the returned struct contains only NULLs. */
static JavaDynLib findJVMDynamicLibrary(char* java_home, JVMSelectStrategy jvmSelectStrategy ) {
  
  char path[ PATH_MAX + 1 ], *mode ;
  JavaDynLib rval;
  int i = 0, j = 0;
  DLHandle jvmLib = (DLHandle)0;
  char* potentialPathsToServerJVM[] = { PATHS_TO_SERVER_JVM, NULL } ;
  char* potentialPathsToClientJVM[] = { PATHS_TO_CLIENT_JVM, NULL } ;
  char* potentialPathsToAnyJVMPreferringServer[] = { PATHS_TO_SERVER_JVM, PATHS_TO_CLIENT_JVM, NULL } ;
  char* potentialPathsToAnyJVMPreferringClient[] = { PATHS_TO_CLIENT_JVM, PATHS_TO_SERVER_JVM, NULL } ;
  char** lookupDirs = NULL;
  char*  dynLibFile;
  jboolean preferClient = ( jvmSelectStrategy & 4 ) ? JNI_TRUE : JNI_FALSE,  // third bit
           allowClient  = ( jvmSelectStrategy & 1 ) ? JNI_TRUE : JNI_FALSE,  // first bit
           allowServer  = ( jvmSelectStrategy & 2 ) ? JNI_TRUE : JNI_FALSE ; // secons bit
  
  assert( allowClient || allowServer ) ;
  
  rval.creatorFunc  = NULL;
  rval.dynLibHandle = NULL;

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
      strcpy( path, java_home ) ;
      strcat( path, JST_FILE_SEPARATOR ) ;
      if ( i == 0 ) { // on a jdk, we need to add jre at this point of the path
        strcat( path, "jre" JST_FILE_SEPARATOR ) ;
      }
      strcat( path, dynLibFile ) ;
      if ( jst_fileExists( path ) ) {
        errno = 0 ;
        if ( !( jvmLib = dlopen( path, RTLD_LAZY ) ) )  {
          fprintf( stderr, "error: dynamic library %s exists but could not be loaded!\n", path ) ;
          if ( errno ) fprintf( stderr, strerror( errno ) ) ;
#         if defined( _WIN32 )
          fprintf( stderr, "win error code: %u\n", (unsigned int)GetLastError() ) ;
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

  if(rval.creatorFunc) {
    rval.dynLibHandle = jvmLib ;
  } else {
    char* errorMsg = dlerror() ;
    fprintf( stderr, "strange bug: jvm creator function not found in jvm dynamic library %s\n", path ) ;
    if ( errorMsg ) fprintf( stderr, "%s\n", errorMsg ) ; 
  }
  return rval;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/** Returns != 0 if the given file exists. */
extern int jst_fileExists(const char* fileName) {
  struct stat buf;
  int i = stat(fileName, &buf);

  return (i == 0);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern JstParamInfo* jst_setParameterDescription(JstParamInfo** paramInfo, int ind, size_t* size, char* name, JstParamClass type, short terminating) {
  JstParamInfo pinfo ;
  
  pinfo.name = name ;
  pinfo.type = type ;
  pinfo.terminating = terminating ;

  return *paramInfo = (JstParamInfo*)appendArrayItem( *paramInfo, ind, size, &pinfo, sizeof( JstParamInfo ) ) ; 
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** To be called when there is a pending exception that is the result of some
 * irrecoverable error in this startup program. Clears the exception and prints its description. */
static void clearException(JNIEnv* env) {

  (*env)->ExceptionDescribe(env);
  (*env)->ExceptionClear(env);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern void* appendArrayItem( void* array, int indx, size_t* arlen, void* item, int item_size_in_bytes ) {
  // we really need here just any data type whose storage size is one byte. 
  // Since "byte" is not ansi-c, we use char ( sizeof( char ) == 1 ).
  // Signed / unsigned does not matter here, only the storage size.
  char *temp ;

  if ( !array && ! ( array = jst_malloc( *arlen * item_size_in_bytes ) ) ) return NULL ;
  
  if ( ((size_t)indx) >= *arlen ) {
    if ( !( array = jst_realloc( array, ( *arlen += 5 ) * item_size_in_bytes ) ) ) return NULL ;
  }
  
  temp = (char*)array ;
  temp += ( indx * item_size_in_bytes ) ;

  memcpy( temp, item, item_size_in_bytes ) ;
  return array ;
}

extern char* jst_append( char* target, size_t* bufsize, ... ) {
  va_list args ;
  size_t targetlen = ( target ? strlen( target ) : 0 ),
         numArgs   = 10 ; // the size of the buffer we are storing the param strings into. Enlarged as necessary.
  size_t totalSize = targetlen + 1 ; // 1 for the terminating nul char
  char *s, *t ;
  char** stringsToAppend = NULL ;
  int i = 0 ;
  
  errno = 0 ;
  stringsToAppend = jst_malloc( numArgs * sizeof( char* ) ) ;  
  if ( !stringsToAppend ) goto end ;

  va_start( args, bufsize ) ;
  while ( ( s = va_arg( args, char* ) ) ) {
    if ( s[ 0 ] ) { // skip empty strings
      if ( !( stringsToAppend = appendArrayItem( stringsToAppend, i++, &numArgs, &s, sizeof( char* ) ) ) ) goto end ;
      totalSize += strlen( s ) ;
    }
  }
  // s = NULL ; // s is now NULL, no need to assign
  if ( !( stringsToAppend = appendArrayItem( stringsToAppend, i, &numArgs, &s, sizeof( char* ) ) ) ) goto end ;
  
  if ( !target || *bufsize < totalSize ) {
    // if taget == NULL and bufsize is given, it means we should reserve the given amount of space
    if ( !target && bufsize && ( *bufsize > totalSize ) ) totalSize = *bufsize ;
    target = target ? jst_realloc( target, totalSize * sizeof( char ) ) : 
                      jst_malloc( totalSize * sizeof( char ) ) ;
    if ( !target ) goto end ;
    if ( bufsize ) *bufsize = totalSize ; // in case target == NULL, bufsize may also be NULL
  }

  s = target + targetlen ;
  for ( i = 0 ; ( t = stringsToAppend[ i ] ) ; i++ ) {
    //size_t len = strlen( t ) ;
    //memcpy( s, t, len ) ;
    //s += len ;
    // or
    //while ( *s++ = *t++ ) ; s-- ;
    // or
    char c ;
    while ( ( c = *t++ ) ) *s++ = c ;
  }
  
  *s = '\0' ;
  
  end:
  
  va_end( args ) ;

  if ( errno ) target = NULL ;
  if ( stringsToAppend ) free( stringsToAppend ) ;
  return target ;
  
}


extern JavaVMOption* appendJvmOption( JavaVMOption* opts, int indx, size_t* optsSize, char* optStr, void* extraInfo ) {
  JavaVMOption tmp ;
  
  tmp.optionString = optStr    ;
  tmp.extraInfo    = extraInfo ;
  
  return appendArrayItem( opts, indx, optsSize, &tmp, sizeof( JavaVMOption ) ) ;
  
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

# if defined( _WIN32 )
// windows does not have dirent.h, it does things different from other os'es

  HANDLE          fileHandle = INVALID_HANDLE_VALUE;
  WIN32_FIND_DATA fdata;
  char            *jarEntrySpecifier = NULL;
  DWORD           lastError;
  jboolean        dirNameEndsWithSeparator, rval = JNI_TRUE ;
  
  dirNameEndsWithSeparator = ( strcmp( dirName + strlen( dirName ) - strlen( JST_FILE_SEPARATOR ), JST_FILE_SEPARATOR ) == 0 ) ? JNI_TRUE : JNI_FALSE ;
    
  // this only works w/ FindFirstFileW. If need be, use that.
//  strcpy(jarEntrySpecifier, "\\\\?\\"); // to allow long paths, see documentation of FindFirstFile
  jarEntrySpecifier = jst_append( NULL, NULL, dirName, dirNameEndsWithSeparator ? "" : JST_FILE_SEPARATOR, "*.jar", NULL ) ;
  if ( !jarEntrySpecifier ) return JNI_TRUE ;
  
  SetLastError( 0 ) ;
  fileHandle = FindFirstFile( jarEntrySpecifier, &fdata ) ;
  
  if ( fileHandle == INVALID_HANDLE_VALUE ) {
    fprintf( stderr, "error: opening directory %s failed w/ error code %u\n", dirName, (unsigned int)GetLastError() ) ;
    goto end ;
  }
  
  lastError = GetLastError() ;
  if ( !lastError ) {
  
    do {
      // this if and the contained ||s are used so that if any of the
      // calls fail, we jump to the end
      if(    !( *target = appendCPEntry( *target, targetSize, dirName ) )         
          ||  ( dirNameEndsWithSeparator ?  JNI_FALSE : !( *target = jst_append( *target, targetSize, JST_FILE_SEPARATOR, NULL ) ) ) 
          || !( *target = jst_append( *target, targetSize, fdata.cFileName, NULL ) )
        ) goto end ;
    } while( FindNextFile( fileHandle, &fdata ) );
    
  }
    
  if ( !lastError ) lastError = GetLastError() ;
  if ( lastError != ERROR_NO_MORE_FILES ) {
    fprintf( stderr, "error: error %u occurred when finding jars from %s\n", (unsigned int)lastError, dirName ) ;
    goto end ;
  }
  
  rval = JNI_FALSE ;
  
  end:
  if ( fileHandle != INVALID_HANDLE_VALUE ) FindClose( fileHandle ) ;
  if ( jarEntrySpecifier ) free( jarEntrySpecifier ) ;
  return rval ;
      
# else      

  DIR           *dir;
  struct dirent *entry;
  size_t        len;
  jboolean      dirNameEndsWithSeparator, 
                rval = JNI_TRUE ;

  len = strlen( dirName ) ;
  dirNameEndsWithSeparator = ( strcmp( dirName + len - strlen(JST_FILE_SEPARATOR ), JST_FILE_SEPARATOR) == 0 ) ;

  dir = opendir( dirName ) ;
  if ( !dir ) {
    fprintf( stderr, "error: could not read directory %s to append jar files from\n%s", dirName, strerror( errno ) ) ;
    return JNI_TRUE ;
  }

  while( (entry = readdir( dir ) ) ) {
    len = strlen( entry->d_name ) ;
    if(len >= 5 && ( strcmp( ".jar", (entry->d_name) + len - 4 ) == 0 ) ) {
      // this if and the contained ||s are used so that if any of the
      // calls fail, we jump to the end
      if ( !( *target = appendCPEntry( *target, targetSize, dirName ) )         
       ||   ( dirNameEndsWithSeparator ?  JNI_FALSE : !( *target = jst_append( *target, targetSize, JST_FILE_SEPARATOR, NULL ) ) ) 
       ||  !( *target = jst_append( *target, targetSize, entry->d_name, NULL ) ) ) goto end ;
    }
  }
  
  rval = JNI_FALSE ;

  end:
  if ( rval ) {
    fprintf( stderr, strerror( errno ) ) ;
  }
  closedir( dir ) ;
  return rval ;

#  endif

}



// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

typedef enum { PREFIX_SEARCH, SUFFIX_SEARCH, EXACT_SEARCH } SearchMode;

/** The first param may be NULL, it is considered an empty array. */
static jboolean arrayContainsString( char** nullTerminatedArray, const char* searchString, SearchMode mode ) {
  int    i = 0 ;
  size_t sslen, len ;
  char   *str ;

  if ( nullTerminatedArray ) {
    switch ( mode ) {
      case PREFIX_SEARCH : 
        while ( ( str = nullTerminatedArray[ i++ ] ) ) {
          len = strlen( str ) ;
          if ( memcmp( str, searchString, len ) == 0 ) return JNI_TRUE ;
        }
        break ;
      case SUFFIX_SEARCH : 
        sslen = strlen( searchString ) ;        
        while( ( str = nullTerminatedArray[ i++ ] ) ) {
          len = strlen( str ) ;
          if ( len <= sslen && memcmp( searchString + sslen - len, str, len ) == 0 ) return JNI_TRUE ;
        }
        break ;
      case EXACT_SEARCH : 
        while ( ( str = nullTerminatedArray[ i++ ] ) ) {
          if ( strcmp( str, searchString ) == 0 ) return JNI_TRUE ;
        }
        break ;
    }
  }
  return JNI_FALSE ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** Returns true on error. */
static jboolean addStringToJStringArray( JNIEnv* env, char *strToAdd, jobjectArray jstrArr, jint ind ) {
  jboolean rval = JNI_FALSE ;
  jstring  arg  = (*env)->NewStringUTF( env, strToAdd ) ;

  if(!arg) {
    fprintf(stderr, "error: could not convert %s to java string\n", strToAdd);
    clearException(env);
    return JNI_TRUE;        
  }

  (*env)->SetObjectArrayElement(env, jstrArr, ind, arg);
  if((*env)->ExceptionCheck(env)) {
    fprintf(stderr, "error: error when writing %dth element %s to Java String[]\n", (int)ind, strToAdd);
    clearException(env);
    rval = JNI_TRUE;
  }
  (*env)->DeleteLocalRef(env, arg);
  return rval;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** Info about these needs to be available to perform the parameter classification correctly. To be more precise,
 *  it is needed to find the first launchee param. */
static char* _builtinDoubleParams[] = { "-cp", "-classpath", "--classpath", "-jh", "--javahome", NULL } ;

extern int jst_findFirstLauncheeParamIndex(char** args, int numArgs, char** terminatingSuffixes, JstParamInfo* paramInfos, int paramInfosCount) {
  int    i, j;
  size_t len;
  
  for(i = 0; i < numArgs; i++) {
    char* arg = args[i];
    
    if((arg[0] == 0) || (arg[0] != '-') // empty strs and ones not beginning w/ - are considered to be terminating args to the launchee
     || arrayContainsString(terminatingSuffixes, arg, SUFFIX_SEARCH)) {
      return i;
    }

    for(j = 0; j < paramInfosCount; j++) {
      if(paramInfos[j].terminating) {
        switch(paramInfos[j].type) {
          case JST_SINGLE_PARAM : // deliberate fallthrough, no break
          case JST_DOUBLE_PARAM : 
            if(strcmp(paramInfos[j].name, arg) == 0) return i;
            break;
          case JST_PREFIX_PARAM :
            len = strlen(paramInfos[j].name);
            if((strlen(arg) >= len) && (memcmp(paramInfos[j].name, arg, len) == 0)) {
              return i;
            }
            break;
        } // switch
      } else if((paramInfos[j].type == JST_DOUBLE_PARAM)
        && (strcmp(paramInfos[j].name, arg) == 0) ) {
          i++;
        }
    } // for j
    // if we have one of the builtin double params, skip the value of the param
    if(arrayContainsString(_builtinDoubleParams, arg, EXACT_SEARCH)) i++;
  } // for i
  // not found - none of the params are launchee params
  return numArgs;
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** See the header file for information.
 */
extern int jst_launchJavaApp( JavaLauncherOptions *launchOptions ) {
  int            rval    = -1 ;
  
  JavaVM         *javavm = NULL;
  JNIEnv         *env    = NULL;
  JavaDynLib     javaLib;
  jint           result ;
  JavaVMInitArgs vm_args;
  JavaVMOption   *jvmOptions = NULL,
                 // the options assigned by the user on cmdline are given last as that way they override the ones set before
                 *userJvmOptions  = NULL ; 
  size_t         len,
                 // jvm opts from command line
                 userJvmOptionsSize  = 5, // initial size
                 // all jvm options combined
                 jvmOptionsSize = 5 ;
  int            i, j, 
                 launcheeParamBeginIndex = launchOptions->numArguments,
                 userJvmOptionsCount  = 0,
                 jvmOptionsCount      = 0 ;

  jclass       launcheeMainClassHandle  = NULL;
  jclass       strClass                 = NULL;
  jmethodID    launcheeMainMethodID     = NULL;
  jobjectArray launcheeJOptions         = NULL;
  
  char      *userClasspath = NULL, 
            *envCLASSPATH  = NULL, 
            *classpath     = NULL,
            *userJvmOptsS  = NULL ; 

  size_t    cpsize = 255 ; // just an initial guess for classpath length, will be expanded as necessary 
  jboolean  *isLauncheeOption  = NULL;
  jint      launcheeParamCount = 0;
  char      *javaHome     = NULL, 
            *toolsJarD    = NULL,
            *toolsJarFile = NULL ;
  JVMSelectStrategy jvmSelectStrategy = launchOptions->jvmSelectStrategy ;

  if( getenv( "__JLAUNCHER_DEBUG" ) ) _jst_debug = JNI_TRUE;
            
  javaLib.creatorFunc  = NULL ;
  javaLib.dynLibHandle = NULL ;  

  // calloc effectively sets all elems to JNI_FALSE.  
  if ( launchOptions->numArguments ) isLauncheeOption = jst_calloc( launchOptions->numArguments, sizeof( jboolean ) ) ;

  if ( launcheeParamBeginIndex && !isLauncheeOption ) goto end ;
  
  
  // find out the argument index after which all the params are launchee (prg being launched) params 

  launcheeParamBeginIndex = jst_findFirstLauncheeParamIndex(launchOptions->arguments, launchOptions->numArguments, launchOptions->terminatingSuffixes, launchOptions->paramInfos, launchOptions->paramInfosCount);

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - -  
  // classify the arguments as jvm or launchee params. Some are passed to neither as they are handled in this func.
  // An example is the -client / -server option that selects the type of jvm
  for(i = 0; i < launcheeParamBeginIndex; i++) {
    char* argument = launchOptions->arguments[i];
    len = strlen(argument);

    if(strcmp("-cp", argument) == 0 
    || strcmp("-classpath", argument) == 0
    || strcmp("--classpath", argument) == 0
    ) {
      if(i == (launchOptions->numArguments - 1)) { // check that this is not the last param as it requires additional info
        fprintf(stderr, "erroneous use of %s\n", argument);
        goto end;
      }
      if((launchOptions->classpathHandling) & JST_CP_PARAM_TO_APP) {
        isLauncheeOption[i]     = JNI_TRUE;
        isLauncheeOption[i + 1] = JNI_TRUE;
      } 
      if((launchOptions->classpathHandling) & JST_CP_PARAM_TO_JVM) userClasspath = argument;
      i++;
      continue;
    }

    // check the param infos for params particular to the app we are launching
    for(j = 0; j < launchOptions->paramInfosCount; j++) {
      switch(launchOptions->paramInfos[j].type) {
        case JST_SINGLE_PARAM :
          if(strcmp(argument, launchOptions->paramInfos[j].name) == 0) {
            isLauncheeOption[i] = JNI_TRUE;
            goto next_arg;
          }
          break;
        case JST_DOUBLE_PARAM :
          if(strcmp(argument, launchOptions->paramInfos[j].name) == 0) {
            isLauncheeOption[i] = JNI_TRUE;
            if(i == (launchOptions->numArguments - 1)) { // check that this is not the last param as it requires additional info
              fprintf(stderr, "erroneous use of %s\n", argument);
              goto end;
            }
            isLauncheeOption[++i] = JNI_TRUE;
            goto next_arg;
          }
          break;
        case JST_PREFIX_PARAM :
          if(memcmp(argument, launchOptions->paramInfos[j].name, len) == 0) {
            isLauncheeOption[i] = JNI_TRUE;
            goto next_arg;            
          }
          break;
      } // switch
    } // for j

    if ( strcmp( "-server", argument) == 0 ) { // jvm client or server
      jvmSelectStrategy = JST_TRY_SERVER_ONLY ;
      continue ;
    } else if ( strcmp("-client", argument) == 0 ) {
      jvmSelectStrategy = JST_TRY_CLIENT_ONLY ;
      continue ;
    } else if( ((launchOptions->javahomeHandling) & JST_ALLOW_JH_PARAMETER) &&  
               ( (strcmp("-jh", argument) == 0)
              || (strcmp("--javahome", argument) == 0) 
               )
              ) {
        if(i == (launchOptions->numArguments - 1)) { // check that this is not the last param as it requires additional info
          fprintf(stderr, "erroneous use of %s\n", argument);
          goto end;
        }
        javaHome = launchOptions->arguments[++i];
    } else { // jvm option
      // add these to a separate array and add these last. This way the ones given by the user override
      // the ones set automatically, so user can override e.g. -Dgroovy.home= and the options given in JAVA_OPTS

      if ( ! ( userJvmOptions = appendJvmOption( userJvmOptions, 
                                                 userJvmOptionsCount++, 
                                                 &userJvmOptionsSize, 
                                                 argument, 
                                                 NULL ) ) ) goto end ;
    }
// this label is needed to be able to break out of nested for and switch (by jumping here w/ goto)
next_arg: 
   ;  // at least w/ ms compiler, the tag needs a statement after it before the closing brace. Thus, an empty statement here.
  } 

  // print debug if necessary
  if( _jst_debug ) { 
    if( launchOptions->numArguments != 0 ) {
      fprintf(stderr, "DEBUG: param classication\n");
      for(i = 0; i < launchOptions->numArguments; i++) {
        fprintf(stderr, "  %s\t: %s\n", launchOptions->arguments[i], (isLauncheeOption[i] || i >= launcheeParamBeginIndex) ? "launcheeparam" : "non launchee param");  
      }
    } else {
      fprintf(stderr, "DEBUG: no parameters\n");
    }
  }
  
  // end classifying arguments
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - -  

  // handle java home
  // it is null if it was not given as a param
  if ( !javaHome ) javaHome = launchOptions->java_home ;
  if ( !javaHome ) javaHome = findJavaHome( launchOptions->javahomeHandling ) ; 

  if(!javaHome || !javaHome[0]) { // not found or an empty string
    fprintf(stderr, ((launchOptions->javahomeHandling) & JST_ALLOW_JH_ENV_VAR_LOOKUP) ? "error: JAVA_HOME not set\n" : 
                                                                                  "error: java home not provided\n");
    goto end;
  } else if( _jst_debug ) {
    fprintf(stderr, "DEBUG: using java home: %s\n", javaHome);
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - -  
  

  // count the params going to the launchee so we can construct the right size java String[] as param to the java main being invoked
  for(i = 0; i < launchOptions->numArguments; i++) {
    if(isLauncheeOption[i] || i >= launcheeParamBeginIndex) {
      isLauncheeOption[i] = JNI_TRUE;
      launcheeParamCount++;
    }
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - -  
  // construct classpath for the jvm

  // look up CLASSPATH env var if necessary  
  if( !( JST_IGNORE_GLOBAL_CP & (launchOptions->classpathHandling) ) ) { // first check if CLASSPATH is ignored altogether
    if(JST_IGNORE_GLOBAL_CP_IF_PARAM_GIVEN & (launchOptions->classpathHandling)) { // use CLASSPATH only if -cp not provided
      if(!userClasspath) envCLASSPATH = getenv("CLASSPATH");
    } else {
      envCLASSPATH = getenv("CLASSPATH");
    }
  } 
    
  if ( !( classpath = jst_append( NULL, &cpsize, "-Djava.class.path=", NULL ) ) ) goto end ;

  // add the jars from the given dirs
  if(launchOptions->jarDirs) {

    char *dirName;

    for( i = 0 ; (dirName = launchOptions->jarDirs[i++]) ; ) {
      if( appendJarsFromDir( dirName, &classpath, &cpsize ) ) goto end ; // error msg already printed
    }
    
  }

  if( userClasspath && ((launchOptions->classpathHandling) & JST_CP_PARAM_TO_JVM)) {
    if( !( classpath = appendCPEntry(classpath, &cpsize, userClasspath) ) ) goto end;
  }

  if( envCLASSPATH && !( classpath = appendCPEntry(classpath, &cpsize, envCLASSPATH) ) ) goto end;

  // add the provided single jars
  
  if(launchOptions->jars) {
    char* jarName;
    
    for( i = 0 ; (jarName = launchOptions->jars[i++]) ; ) {
      if ( !( classpath = appendCPEntry( classpath, &cpsize, jarName ) ) ) goto end ;
    }
    
  }

  // tools.jar handling
  // tools.jar is not present on a jre, so in that case we omit the -Dtools.jar= option
  len = strlen( javaHome ) + 2 * strlen( JST_FILE_SEPARATOR ) + 12 + 1 ;
  toolsJarFile = jst_append( NULL, &len, javaHome,  JST_FILE_SEPARATOR "lib" JST_FILE_SEPARATOR "tools.jar", NULL ) ;  
  if ( !toolsJarFile ) goto end ;

// #if defined ( __APPLE__ )
  // // os-x jdk seems to have tools.jar in a different place than other jdks...
  // if ( !jst_fileExists( toolsJarFile ) ) { 
    // toolsJarFile[ 0 ] = '\0' ;
    // toolsJarFile = jst_append( toolsJarFile, &len, "/System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK/Classes/classes.jar", NULL ) ;
    // if ( !toolsJarFile ) goto end ;
  // } 
// #endif // TODO: check if we are in a jre under a jdk. In that case the tools jar can be found by taking away the lib before appending lib/tools.jar
  if ( jst_fileExists( toolsJarFile ) ) {
    // add as java env property if requested
    if ( ( launchOptions->toolsJarHandling ) & JST_TOOLS_JAR_TO_SYSPROP ) {
      len       = strlen(toolsJarFile) + 12 + 1 ; // "-Dtools.jar=" == 12 chars + null char
      toolsJarD = jst_append( NULL, &len, "-Dtools.jar=", toolsJarFile, NULL ) ;
      if ( !toolsJarD ) goto end ;

      if ( !( jvmOptions = appendJvmOption( jvmOptions, 
                                            jvmOptionsCount++, 
                                            &jvmOptionsSize, 
                                            toolsJarD, 
                                            NULL ) ) ) goto end ; 
    }
    
    // add tools.jar to startup classpath if requested
    if ( ( (launchOptions->toolsJarHandling ) & JST_TOOLS_JAR_TO_CLASSPATH ) 
     && !( classpath = appendCPEntry(classpath, &cpsize, toolsJarFile) ) ) goto end;
  }
  
  free( toolsJarFile ) ;
  toolsJarFile = NULL ;
  
  
  if ( !( jvmOptions = appendJvmOption( jvmOptions, 
                                        jvmOptionsCount++, 
                                        &jvmOptionsSize, 
                                        classpath, 
                                        NULL ) ) ) goto end ; 

  // end constructing classpath
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - -  

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
  if ( launchOptions->javaOptsEnvVar ) {
    int userJvmOptCount = 0 ;
    char* userOpts = getenv( launchOptions->javaOptsEnvVar ), *s ;
    jboolean firstTime = JNI_TRUE ;
    
    if ( userOpts && userOpts[ 0 ] ) {
      userJvmOptCount = 1 ;
      for ( i = 0 ; userOpts[ i ] ; i++ ) if ( userOpts[ i ] == ' ' ) userJvmOptCount++ ;

      if ( !( userJvmOptsS = jst_append( NULL, NULL, userOpts, NULL ) ) ) goto end ;        

      while ( ( s = strtok( firstTime ? userJvmOptsS : NULL, " " ) ) ) {
        firstTime = JNI_FALSE ;
        if ( ! ( jvmOptions = appendJvmOption( jvmOptions, 
                                               jvmOptionsCount++, 
                                               &jvmOptionsSize, 
                                               s, 
                                               NULL ) ) ) goto end ;
      }
      
    }

  }
    
  // jvm options given on the command line by the user
  for ( i = 0 ; i < userJvmOptionsCount ; i++ ) {
    
    if ( !( jvmOptions = appendJvmOption( jvmOptions, 
                                          jvmOptionsCount++, 
                                          &jvmOptionsSize, 
                                          userJvmOptions[ i ].optionString, 
                                          userJvmOptions[ i ].extraInfo ) ) ) goto end ; 
  }
  
  if( _jst_debug ) {
    fprintf(stderr, "DUBUG: Starting jvm with the following %d options:\n", jvmOptionsCount ) ;
    for( i = 0 ; i < jvmOptionsCount ; i++ ) {
      fprintf( stderr, "  %s\n", jvmOptions[ i ].optionString ) ;
    }
  }

  vm_args.version            = JNI_VERSION_1_4 ;
  vm_args.options            = jvmOptions      ;
  vm_args.nOptions           = jvmOptionsCount ;
  vm_args.ignoreUnrecognized = JNI_FALSE       ;

  
  // fetch the pointer to jvm creator func and invoke it
  javaLib = findJVMDynamicLibrary( javaHome, jvmSelectStrategy ) ;
  if(!javaLib.creatorFunc) goto end; // error message already printed
  
  // the cast to void* before void** serves to remove a gcc warning
  // "dereferencing type-punned pointer will break strict-aliasing rules"
  // Found the fix from
  // http://mail.opensolaris.org/pipermail/tools-gcc/2005-August/000048.html
  result = (javaLib.creatorFunc)(&javavm, (void**)(void*)&env, &vm_args);

  if(result) {
    char* errMsg;
    switch(result) {
      case JNI_ERR        : //  (-1)  unknown error 
        errMsg = "unknown error";
        break;
      case JNI_EDETACHED  : //  (-2)  thread detached from the VM 
        errMsg = "thread detachment";
        break;
      case JNI_EVERSION   : //  (-3)  JNI version error 
        errMsg = "JNI version problems";
        break;
      case JNI_ENOMEM     : //  (-4)  not enough memory 
        errMsg = "not enough memory";
        break;
      case JNI_EEXIST     : //  (-5)  VM already created 
        errMsg = "jvm already created";
        break;
      case JNI_EINVAL     : //  (-6)  invalid arguments
        errMsg = "invalid arguments to jvm creation";
        break;
      default: // should not happen
        errMsg = "unknown exit code";
        break;
    }
    fprintf(stderr, "error: jvm creation failed with code %d: %s\n", (int)result, errMsg);
    rval = result;
    goto end;
  } 

  free(toolsJarD);
  free(jvmOptions);
  free(classpath);
  toolsJarD  = NULL;
  jvmOptions = NULL;
  classpath  = NULL;

  // construct a java.lang.String[] to give program args in
  // find the groovy main class
  // find the startup method and call it

  if(launchOptions->extraProgramOptions) {
    i = 0;
    while(launchOptions->extraProgramOptions[i]) i++;
    launcheeParamCount += i;
  }
   
  if( (result = (*env)->EnsureLocalCapacity(env, launcheeParamCount + 1)) ) { // + 1 for the String[] to hold the params
    clearException( env ) ;
    fprintf( stderr, "error: could not allocate memory to hold references for groovy parameters (how many params did you give, dude?)\n" ) ;
    rval = result ;
    goto end ;
  }

  if(! ( strClass = (*env)->FindClass(env, "java/lang/String") ) ) {
    clearException(env);
    fprintf(stderr, "error: could not find java.lang.String class\n");
    goto end;
  }


  launcheeJOptions = (*env)->NewObjectArray(env, launcheeParamCount, strClass, NULL);
  if(!launcheeJOptions) {
    clearException(env);
    fprintf( stderr, "error: could not allocate memory for java String array to hold groovy parameters (how many params did you give, dude?)\n" ) ;
    goto end;
  }

  j = 0; // index in java String[] (args to main)
  if(launchOptions->extraProgramOptions) {
    char *carg;
    i = 0;
    while( (carg = launchOptions->extraProgramOptions[i++]) ) {
      if(addStringToJStringArray(env, carg, launcheeJOptions, j++)
         ) {
        goto end; // error msg already printed
      }
    }
  }

  for(i = 0; i < launchOptions->numArguments; i++) {
    if(isLauncheeOption[i]
    && addStringToJStringArray(env, launchOptions->arguments[i], launcheeJOptions, j++)
       ) {
        goto end; // error msg already printed
    }
  }

  free( isLauncheeOption ) ;
  isLauncheeOption = NULL;



  launcheeMainClassHandle = (*env)->FindClass(env, launchOptions->mainClassName);
  if(!launcheeMainClassHandle) {
    clearException(env);
    fprintf( stderr, "error: could not find startup class %s\n", launchOptions->mainClassName ) ;
    goto end ;
  }
  launcheeMainMethodID = (*env)->GetStaticMethodID(env, launcheeMainClassHandle, launchOptions->mainMethodName, "([Ljava/lang/String;)V");
  if(!launcheeMainMethodID) {
    clearException(env);
    fprintf(stderr, "error: could not find startup method \"%s\" in class %s\n", 
                    launchOptions->mainMethodName, launchOptions->mainClassName);
    goto end ;
  }

  // finally: launch the java application!
  (*env)->CallStaticVoidMethod(env, launcheeMainClassHandle, launcheeMainMethodID, launcheeJOptions);
  // TODO: what happens if the called code calls System.exit?
  // I guess we just exit. It would be cleaner if we called a method returning an int and used that as exit status
  // The jvm side of the program could ensure that System.exit() does not end the process 
  // (by catching that throwable? there is some way)
  if((*env)->ExceptionCheck(env)) {
    // TODO: provide an option which allows the caller to indicate whether to print the stack trace
    (*env)->ExceptionClear(env);
  } else {
    rval = 0;
  }
  

end:
  // cleanup
  if(javavm) {
    if( (*javavm)->DetachCurrentThread(javavm) ) {
      fprintf(stderr, "Warning: could not detach main thread from the jvm at shutdown (please report this as a bug)\n");
    }
    (*javavm)->DestroyJavaVM(javavm);
  }
  if ( javaLib.dynLibHandle ) dlclose(javaLib.dynLibHandle);
  if ( classpath )        free( classpath ) ;
  if ( isLauncheeOption ) free( isLauncheeOption ) ;
  if ( jvmOptions )       free( jvmOptions ) ;
  if ( toolsJarFile )     free( toolsJarFile ) ;
  if ( toolsJarD )        free( toolsJarD ) ;
  if ( userJvmOptions )   free( userJvmOptions ) ;
  if ( userJvmOptsS     ) free( userJvmOptsS ) ;
  
  return rval;

}
