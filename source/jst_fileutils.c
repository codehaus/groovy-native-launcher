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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <limits.h>
#include <ctype.h>

#if defined( _WIN32 )

#  include <Windows.h>
#  if !defined( PATH_MAX )
#    define PATH_MAX MAX_PATH
#  endif

#else

#  include <dirent.h>
#  include <unistd.h>
#  if defined( __APPLE__ )
#    include <CoreFoundation/CFBundle.h>

#    include <TargetConditionals.h>

/*
 *  The Mac OS X Leopard version of jni_md.h (Java SE 5 JDK) is broken in that it tests the value of
 *  __LP64__ instead of the presence of _LP64 as happens in Sun's Java 6.0 JDK.  To prevent spurious
 *  warnings define this with a false value.
 */
#    define __LP64__ 0

#  endif

#  if defined( __linux__ )
      // /proc/{pid}/exe is a symbolic link to the executable
#    define JST_SYMLINK_TO_EXECUTABLE_STR_TEMPLATE "/proc/%d/exe"
#  elif defined( __sun__ )
#    define JST_SYMLINK_TO_EXECUTABLE_STR_TEMPLATE "/proc/%d/path/a.out"
#  endif

#endif


#include "jvmstarter.h"
#include "jst_dynmem.h"
#include "jst_fileutils.h"
#include "jst_stringutils.h"

#if defined ( _WIN32 ) && defined ( _cwcompat )
#include "jst_cygwin_compatibility.h"
#endif

/** Returns != 0 if the given file exists. In case of error, errno != 0 */
extern int jst_fileExists( const char* fileName ) {

  struct stat buf ;
  int i ;

  errno = 0 ;

  i = stat( fileName, &buf ) ;

  assert( i != 0 || errno != ENOENT ) ; // errno == ENOENT => i != 0

  if ( errno == ENOENT ) errno = 0 ;

  return i == 0 ;

}

extern int jst_isDir( const char* fileName ) {
  struct stat buf ;
#if !defined( NDEBUG )
  int i =
#endif
  stat( fileName, &buf ) ;
  assert( i == 0 ) ;
  return ( buf.st_mode & S_IFDIR ) ? 1 : 0 ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern jboolean jst_dirNameEndsWithSeparator( const char* dirName ) {
  return ( strcmp( dirName + strlen( dirName ) - ( sizeof( JST_FILE_SEPARATOR ) - 1 ), JST_FILE_SEPARATOR ) == 0 ) ? JNI_TRUE : JNI_FALSE ;
}

extern char* jst_pathToParentDir( char* path ) {

  size_t len = strlen( path ) ;

  // this implementation is not perfect, but works ok in this program's context

#if defined( _WIN32 )
  if ( ( len == 3 ) &&
        isalpha( path[ 0 ] ) &&
        ( path[ 1 ] == ':' ) &&
        ( path[ 2 ] == JST_FILE_SEPARATOR[ 0 ] )
     ) return NULL ;
#endif

  if ( len <= 1 ) return NULL ;

  if ( jst_dirNameEndsWithSeparator( path ) ) {
    path[ len - 1 ] = '\0' ;
  }

  *(strrchr( path, JST_FILE_SEPARATOR[ 0 ] ) ) = '\0' ;

  return path ;

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#if defined( _WIN32 )

/** returns true if there is an error, changes value of errorOccurred accordingly */
static jboolean checkForInvalidFileHandle( HANDLE fileHandle, const char* dirName, jboolean* errorOccurred ) {

  if ( fileHandle == INVALID_HANDLE_VALUE ) {

    DWORD lastError = GetLastError() ;

    if ( lastError != ERROR_FILE_NOT_FOUND ) {
      fprintf( stderr, "error: opening directory %s failed\n", dirName ) ;
      jst_printWinError( lastError ) ;
      *errorOccurred = JNI_TRUE ;
    }
  }

  return *errorOccurred ;

}

#endif

/** adds filename to list, return true on error and changes the value of errorOccurred accordingly. */
static jboolean addFileToList( char ***fileNames, int *fileNameCount, size_t *resultSize, char* dirName, char* fileName, int (*selector)( const char* dirname, const char* filename ), jboolean *errorOccurred ) {

  int okToInclude = JNI_TRUE ;

  if ( selector ) {
    errno = 0 ;
    okToInclude = selector( dirName, fileName ) ;
    if ( errno ) *errorOccurred = JNI_TRUE;
  }

  if ( !*errorOccurred && okToInclude ) {
    char* temp = jst_strdup( fileName ) ;
    if ( !temp ||
         !( *fileNames = jst_appendArrayItem( *fileNames, (*fileNameCount)++, resultSize, &temp, sizeof( char* ) ) ) ) {
      *errorOccurred = JNI_TRUE ;
    }
  }

  return *errorOccurred ;
}


static void changeEmptyPrefixAndSuffixToNULL( char** fileNamePrefix, char** fileNameSuffix ) {

  if ( *fileNamePrefix && !**fileNamePrefix ) *fileNamePrefix = NULL ;

  if ( *fileNameSuffix ) {
    if ( !**fileNameSuffix ) {
      *fileNameSuffix = NULL ;
    } else {
      assert( (*fileNameSuffix)[ 0 ] == '.' ) ; // file name suffix must begin with a .
    }
  }

}

extern int matchPrefixAndSuffixToFileName( char* fileName, char* prefix, char* suffix ) {

  return jst_startsWith( fileName, prefix ) && jst_endsWith( fileName, suffix ) ;

}

#if defined( _WIN32 )

/** Inserts the names of files matching the given criteria into the given (dynallocated) string array. returns true on error */
static jboolean getFileNamesToArray( char* dirName, char* fileNamePrefix, char* fileNameSuffix, char*** fileNamesOut, size_t* fileNamesSize, int* fileNameCount, int (*selector)( const char* dirname, const char* filename ) ) {

  // windows does not have dirent.h, it does things different from other os'es

  jboolean        dirNameEndsWithSeparator = jst_dirNameEndsWithSeparator( dirName ),
                  errorOccurred            = JNI_FALSE ;
  HANDLE          fileHandle               = INVALID_HANDLE_VALUE ;
  WIN32_FIND_DATA fdata ;
  DWORD           lastError ;
  char*           fileSpecifier ;


  changeEmptyPrefixAndSuffixToNULL( &fileNamePrefix, &fileNameSuffix ) ;

  fileSpecifier = jst_append( NULL, NULL, dirName,
                                          dirNameEndsWithSeparator ? "" : JST_FILE_SEPARATOR,
                                          fileNamePrefix ? fileNamePrefix : "",
                                          "*",
                                          fileNameSuffix ? fileNameSuffix : ".*",
                                          NULL ) ;
  if ( !fileSpecifier ) {
    return JNI_TRUE ;
  }

  SetLastError( 0 ) ;

  fileHandle = FindFirstFile( fileSpecifier, &fdata ) ;

  if ( checkForInvalidFileHandle( fileHandle, dirName, &errorOccurred ) ) goto end ;


  if ( !( lastError = GetLastError() ) ) {

    do {

      if ( addFileToList( fileNamesOut, fileNameCount, fileNamesSize, dirName, fdata.cFileName, selector, &errorOccurred ) ) goto end ;

    } while ( FindNextFile( fileHandle, &fdata ) ) ;

  }

  if ( !lastError ) lastError = GetLastError() ;
  if ( lastError != ERROR_NO_MORE_FILES ) {
    fprintf( stderr, "error: error occurred reading directory %s\n", dirName ) ;
    jst_printWinError( lastError ) ;
    errorOccurred = JNI_TRUE ;
  }

  end:

  if ( fileHandle != INVALID_HANDLE_VALUE ) FindClose( fileHandle ) ;
  if ( fileSpecifier ) free( fileSpecifier ) ;

  return errorOccurred ;

}

#else

static jboolean getFileNamesToArray( char* dirName, char* fileNamePrefix, char* fileNameSuffix, char*** fileNamesOut, size_t* fileNamesSize, int* fileNameCount, int (*selector)( const char* dirname, const char* filename ) ) {

  DIR           *dir ;
  struct dirent *entry ;
  jboolean      errorOccurred = JNI_FALSE ;

  dir = opendir( dirName ) ;
  if ( !dir ) {
    fprintf( stderr, "error: could not open directory %s\n%s", dirName, strerror( errno ) ) ;
    return JNI_TRUE ;
  }

  while ( ( entry = readdir( dir ) ) ) {

    char *fileName = entry->d_name ;

    if ( ( strcmp( ".", fileName ) == 0 ) || ( strcmp( "..", fileName ) == 0 ) ) continue ;

    if ( matchPrefixAndSuffixToFileName( fileName, fileNamePrefix, fileNameSuffix ) ) {

      if ( addFileToList( fileNamesOut, fileNameCount, fileNamesSize, dirName, fileName, selector, &errorOccurred ) ) goto end ;

    }
  }


  end:

  if ( dir ) closedir( dir ) ;

  return errorOccurred ;

}

#endif

extern char** jst_getFileNames( char* dirName, char* fileNamePrefix, char* fileNameSuffix, int (*selector)( const char* dirname, const char* filename ) ) {

  char     **fileNamesTmp ;
  size_t   resultSize    = 30 ;
  jboolean errorOccurred = JNI_FALSE ;
  int      fileNameCount = 0 ;

  if ( !( fileNamesTmp = jst_calloc( resultSize, sizeof( char* ) ) ) ) return NULL ;


  errorOccurred = getFileNamesToArray( dirName, fileNamePrefix, fileNameSuffix, &fileNamesTmp, &resultSize, &fileNameCount, selector ) ;


  if ( fileNamesTmp ) {
    fileNamesTmp = jst_appendArrayItem( fileNamesTmp, fileNameCount, &resultSize, NULL, sizeof( char* ) ) ;
    if ( !fileNamesTmp ) errorOccurred = JNI_TRUE ;
  }

  {
    char** rval = NULL ;

    if ( fileNamesTmp ) {
      if ( !errorOccurred ) rval = jst_packStringArray( fileNamesTmp ) ;
      jst_freeAll( (void***)(void*)&fileNamesTmp ) ;
    }

    return rval ;
  }

}


#if defined( _WIN32 )

static char* getFullPathToExecutableFile() {

  char   *execFile      = NULL ;
  size_t currentBufSize = 0 ;
  size_t len            = 0 ;

  do {
    if ( currentBufSize == 0 ) {
      currentBufSize = PATH_MAX + 1 ;
      execFile = jst_malloc( currentBufSize * sizeof( char ) ) ;
    } else {
      execFile = jst_realloc( execFile, (currentBufSize += 100) * sizeof( char ) ) ;
    }

    if ( !execFile ) return NULL ;


    // reset the error state, just in case it has been left dangling somewhere and
    // GetModuleFileNameA does not reset it (its docs don't tell).
    // GetModuleFileName docs: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/getmodulefilename.asp
    SetLastError( 0 ) ;
    len = GetModuleFileNameA( NULL, execFile, (DWORD)currentBufSize ) ;
  } while ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) ;


  if ( len == 0 ) {
    fprintf( stderr, "error: finding out executable location failed\n" ) ;
    jst_printWinError( GetLastError() ) ;
    jst_free( execFile ) ;
  }

  return execFile ;

}

#elif defined( __linux__ ) || defined( __sun__ )

static char* getFullPathToExecutableFile() {
  char execFile[    PATH_MAX + 1 ],
       procSymlink[ PATH_MAX + 1 ] ;

  sprintf( procSymlink, JST_SYMLINK_TO_EXECUTABLE_STR_TEMPLATE, (int)getpid() ) ;

  assert( jst_fileExists( procSymlink ) ) ; // should never fail

   if ( !realpath( procSymlink, execFile ) ) {
     fprintf( stderr, "could not get realpath to file %s\n%s\n", procSymlink, strerror( errno ) ) ;
     return NULL ;
   }

   return jst_strdup( execFile ) ;
}

#elif defined( __APPLE__ )

static char* getFullPathToExecutableFile() {

  char execFile[ PATH_MAX + 1 ],
       *procSymlink = NULL ;

  jboolean appleError = JNI_FALSE ;
  const CFBundleRef bundle = CFBundleGetMainBundle() ;

  if ( !bundle ) { // should never happen (?)

    fprintf( stderr, "error: failed to get main bundle\n" ) ;
    return NULL ;

  } else {

    const CFURLRef    executableURL  = CFBundleCopyExecutableURL( bundle ) ;
    const CFStringRef executablePath = CFURLCopyFileSystemPath( executableURL , kCFURLPOSIXPathStyle ) ;
    const CFIndex     maxLength      = CFStringGetMaximumSizeOfFileSystemRepresentation( executablePath ) ;

    if ( !( procSymlink = jst_malloc( maxLength * sizeof( char ) ) ) ||
         !CFStringGetFileSystemRepresentation( executablePath, procSymlink , maxLength ) ) {
      appleError = JNI_TRUE ;
    }

    CFRelease ( executablePath ) ;
    CFRelease ( executableURL  ) ;

    if ( appleError ) {
      if ( procSymlink ) free( procSymlink ) ;
      fprintf( stderr, "error: error occurred when using bundle api to find executable home directory\n" ) ;
      return NULL ;
    }
  }

  assert( jst_fileExists( procSymlink ) ) ; // should never fail

  if ( !realpath( procSymlink, execFile ) ) {
   fprintf( stderr, "could not get realpath to file %s\n%s\n", procSymlink, strerror( errno ) ) ;
   free( procSymlink ) ;
   return NULL ;
  }

  free( procSymlink ) ;

  return jst_strdup( execFile ) ;

}

# else

#  error "looking up executable location has not been implemented on your platform"

#endif

extern char* jst_getExecutableHome() {

  char *execHome = NULL ;

  execHome = getFullPathToExecutableFile() ;

  if ( execHome ) {
    size_t len ;
    // cut off the executable name and the trailing file separator
    *(strrchr( execHome, JST_FILE_SEPARATOR[ 0 ] ) ) = '\0' ;
    len = strlen( execHome ) ;
    execHome = jst_realloc( execHome, len + 1 ) ; // should not fail as we are shrinking the buffer
    assert( execHome ) ;
  }

  return execHome ;

}

extern char* jst_createFileName( const char* root, ... ) {

  static char* filesep = JST_FILE_SEPARATOR ; // we can not pass a reference to a define, so this is read into a var

  va_list args ;
  char    **strings = NULL,
          *s,
          *previous,
          *rval = NULL ;
  size_t  stringsSize = 0 ;

  if ( !jst_appendPointer( (void***)(void*)&strings, &stringsSize, (void*)root ) ) return NULL ;

  // empty string denotes root dir. In case of empty string, add that explicitly so the following
  // loop does not get confused (it assumes skipping empty strings).
  previous = root[ 0 ] ? (char*)root : JST_FILE_SEPARATOR ;

  va_start( args, root ) ;

  while ( ( s = va_arg( args, char* ) ) ) {

    if ( s[ 0 ] ) { // skip empty strings

      if ( ( s[ 0 ] != filesep[ 0 ] ) &&
           ( previous[ strlen( previous ) - 1 ] != filesep[ 0 ] ) &&
           !jst_appendPointer( (void***)(void*)&strings, &stringsSize, filesep ) ) goto end ;

      if ( !jst_appendPointer( (void***)(void*)&strings, &stringsSize, s ) ) goto end ;
      previous = s ;
    }
  }


  rval = jst_concatenateStrArray( strings ) ;

  end:

  va_end( args ) ;

  if ( strings ) free( strings ) ;
  return rval ;

}

extern char* jst_fullPathName( const char* fileOrDirName ) {

  char fullNameOut[ PATH_MAX + 1 ] ;

  if ( !fileOrDirName || !jst_fileExists( fileOrDirName ) ) return (char*)fileOrDirName ;

#if defined( _WIN32 )
  {
    char *lpFilePart = NULL ;

    DWORD pathLen = GetFullPathName( fileOrDirName, PATH_MAX, fullNameOut, &lpFilePart ) ;

    if ( pathLen == 0 ) {
      fprintf( stderr, "error: failed to get full path name for %s\n", fileOrDirName ) ;
      jst_printWinError( GetLastError() ) ;
      return NULL ;
    } else if ( pathLen > PATH_MAX ) {
      // FIXME - reserve a bigger buffer (size pathLen) and get the dir name into that
      fprintf( stderr, "launcher bug: full path name of %s is too long to hold in the reserved buffer (%d/%d). Please report this bug.\n", fileOrDirName, (int)pathLen, PATH_MAX ) ;
      return NULL ;
    }
  }
#else
      // TODO: how to get the path name of a file that does not necessarily exist on *nix?
      //       realpath seems to raise an error for a nonexistent file.
      //       Anyhow, this behavior is inconsistent between groovy script launchers - windows
      //       version
      if ( !realpath( fileOrDirName, fullNameOut ) ) {
        fprintf( stderr, "error: could not get full path of %s\n%s\n", fileOrDirName, strerror( errno ) ) ;
        return NULL ;
      }
#endif

  if ( strcmp( fileOrDirName, fullNameOut ) == 0 ) return (char*)fileOrDirName ;

  return jst_strdup( fullNameOut ) ;

}

extern char* jst_fullPathNameToBuffer( const char* fileOrDirName, char* buffer, size_t *bufsize ) {

  char* realFile = jst_fullPathName( fileOrDirName ) ;
  int   dynallocated ;

  if ( !realFile ) {
    if ( buffer ) free( buffer ) ;
    return NULL ;
  }

  dynallocated = realFile != fileOrDirName ;

  buffer = jst_append( buffer, bufsize, realFile, NULL ) ;

  if ( dynallocated ) {
    jst_free( realFile ) ;
  }

  return buffer ;

}

extern char* jst_overwriteWithFullPathName( char* buffer, size_t *bufsize ) {

  char* realFile = jst_fullPathName( buffer ) ;
  int   dynallocated ;

  if ( !realFile ) {
    free( buffer ) ;
    return NULL ;
  }

  dynallocated = realFile != buffer ;

  if ( dynallocated ) {
    buffer[ 0 ] = '\0' ;
    buffer = jst_append( buffer, bufsize, realFile, NULL ) ;
    jst_free( realFile ) ;
  }

  return buffer ;

}



extern char* findStartupJar( const char* basedir, const char* subdir, const char* prefix, const char* progname, int (*selector)( const char* dirname, const char* filename ) ) {
  char *startupJar = NULL,
       *libDir     = NULL,
       **jarNames  = NULL ;
  int numJarsFound = 0 ;

  if ( subdir && subdir[ 0 ] ) {
    if ( !( libDir = jst_createFileName( basedir, subdir, NULL ) ) ) goto end ;
  }

  if ( !jst_fileExists( libDir ) ) {
    fprintf( stderr, "Lib dir %s does not exist\n", libDir ) ;
    goto end ;
  }

  if ( !( jarNames = jst_getFileNames( (char*)(libDir ? libDir : basedir), (char*)prefix, ".jar", selector ) ) ) goto end ;

  numJarsFound = jst_pointerArrayLen( (void**)jarNames ) ;

  end:
  switch ( numJarsFound ) {
    case 0 :
      if ( progname ) fprintf( stderr, "error: could not find %s startup jar from %s\n", progname, libDir ) ;
      break ;
    case 1 :
      startupJar = jst_createFileName( libDir ? libDir : basedir, jarNames[ 0 ], NULL ) ;
      break ;
    default :
      if ( progname ) fprintf( stderr, "error: too many %s startup jars in %s e.g. %s and %s\n", progname, libDir, jarNames[ 0 ], jarNames[ 1 ] ) ;
  }

  if ( jarNames ) free( jarNames ) ;
  if ( libDir   ) free( libDir ) ;

  return startupJar ;

}

/** returns apphome if valid by validator. On error errno will be set. */
static char* getAppHomeBasedOnExecutableLocation( JstAppHomeStrategy appHomeStrategy, int (*validator)( const char* dirname ) ) {

  char *appHome = NULL ;

  if ( appHomeStrategy != JST_INGORE_EXECUTABLE_LOCATION ) {

    appHome = jst_getExecutableHome() ;

    if ( !appHome ) {
      if ( !errno ) errno = 1 ;
      return NULL ;
    }

    if ( appHomeStrategy == JST_USE_PARENT_OF_EXEC_LOCATION_AS_HOME ) {
      if ( !jst_pathToParentDir( appHome ) ) {
        jst_free( appHome ) ;
      }
    }

    if ( appHome && validator ) {
      int isvalid ;

      errno = 0 ;
      isvalid = validator( appHome ) ;

      if ( errno ) {
        jst_free( appHome ) ;
        return NULL ;
      }

      if ( !isvalid ) {
        jst_free( appHome ) ;
      }

    }

    if ( appHome ) {
      if ( _jst_debug ) fprintf( stderr, "debug: app home located based on executable location: %s\n", appHome ) ;
      return appHome ;
    }

  }

  return appHome ;

}

/** returns the app home or NULL if the env var is set (or set to invalid location). On error errno will be set. */
static char* getAppHomeFromEnvVar( const char* envVarName, int (*validator)( const char* dirname ) ) {

  char* appHome = getenv( envVarName ) ;

  if ( appHome ) {
#if defined( _WIN32 ) && defined( _cwcompat )
    if ( CYGWIN_LOADED ) {
      char convertedPath[ PATH_MAX + 1 ] ;
      cygwin_posix2win_path( appHome, convertedPath ) ;
      appHome = jst_strdup( convertedPath ) ;
    }
#endif
    if ( validator ) {
      int isvalid ;
      errno = 0 ;
      isvalid = validator( appHome ) ;
      if ( errno ) {
#if defined( _WIN32 ) && defined( _cwcompat )
        if ( CYGWIN_LOADED ) free( appHome ) ;
#endif
        return NULL ;
      }
      if ( !isvalid ) {
#if defined( _WIN32 ) && defined( _cwcompat )
        if ( CYGWIN_LOADED ) free( appHome ) ;
#endif
        appHome = NULL ;
      }
    }

#if !( defined( _WIN32 ) && defined( _cwcompat ) )
    if ( appHome ) appHome = jst_strdup( appHome ) ;
#endif

    if ( appHome && _jst_debug ) fprintf( stderr, "debug: obtained app home from env var  %s, value: %s\n", envVarName, appHome ) ;

  }

  return appHome ;

}



static void printAppHomeFindingErrorMessage( JstAppHomeStrategy appHomeStrategy, const char* envVarName ) {

    int execLocAndEnvVar = ( appHomeStrategy != JST_INGORE_EXECUTABLE_LOCATION ) && envVarName ;
    fprintf( stderr, "error: could not locate app home\nplease " ) ;
    if ( execLocAndEnvVar )                                  fprintf( stderr, "either " ) ;
    if ( appHomeStrategy != JST_INGORE_EXECUTABLE_LOCATION ) fprintf( stderr, "place the executable in the appropriate directory " ) ;
    if ( execLocAndEnvVar )                                  fprintf( stderr, "or " ) ;
    if ( envVarName )                                        fprintf( stderr, "set the environment variable %s", envVarName ) ;
    fprintf( stderr, "\n" ) ;

}

extern char* jst_getAppHome( JstAppHomeStrategy appHomeStrategy, const char* envVarName, int (*validator)( const char* dirname ) ) {

  char *appHome = NULL ;

  errno = 0 ;

  appHome = getAppHomeBasedOnExecutableLocation( appHomeStrategy, validator ) ;

  if ( !errno && !appHome && envVarName ) {
    appHome = getAppHomeFromEnvVar( envVarName, validator ) ;
  }

  if ( !appHome ) printAppHomeFindingErrorMessage( appHomeStrategy, envVarName ) ;

  return appHome ;

}

static void printPathLookupDebugMessage( const char* pathname, const char* location, const char* file ) {
  if ( location ) {
    fprintf( stderr, "debug: %s found on %s: %s\n", file, pathname, location ) ;
  } else {
    fprintf( stderr, "debug: %s not found on %s\n", file, pathname ) ;
  }
}

/** Appends filename + a file separator if necessary to dirname and writes the result in buffer (which must be dynallocated).
 * Returns buffer, possibly reallocated. Buffer may be initially NULL. */
//static char* createPathToFile( char* buffer, size_t *bufSize, const char* dirname, const char* filename ) {
//  if ( buffer ) buffer[ 0 ] = '\0' ;
//
//  return jst_append( buffer, bufSize, dirname,
//                                      jst_dirNameEndsWithSeparator( dirname ) ? "" : JST_FILE_SEPARATOR,
//                                      filename, NULL ) ;
//}

#define CREATE_PATH_TO_FILE_A( target, dirname, filename ) target = jst_malloca( strlen( dirname ) + strlen( filename ) + 1 ) ; \
                                                          if ( target ) { \
                                                            strcpy( target, dirname ) ; \
                                                            if ( !jst_dirNameEndsWithSeparator( dirname ) ) strcat( target, JST_FILE_SEPARATOR ) ; \
                                                            strcat( target, filename ) ; \
                                                          }


/** Checks that the given file exists and if validator func is given, checks against that, too. */
static int validateFile( const char* dirname, const char* filename, int (*validator)( const char* dirname, const char* filename ) ) {

  int  isvalid ;
  char *executablePath ;

  CREATE_PATH_TO_FILE_A( executablePath, dirname, filename )

#if defined( _WIN32 )
  if ( !executablePath ) {
    if ( !errno ) errno = ENOMEM ;
    return 0 ;
  }
#endif

  isvalid = jst_fileExists( executablePath ) && ( !validator || validator( dirname, filename ) ) ;

  jst_freea( executablePath ) ;

  return isvalid ;

}


/** Returns  the path entry containing the given file AND (if given) accepted by the validator. On error errno will be set.
 * The pointer is to a part of the given path buffer. */
static char* findPathEntryContainingFile( char* path, const char* file, int (*validator)( const char* dirname, const char* filename ) ) {

  char     *dirname ;
  jboolean firstTime = JNI_TRUE ;
  int      found     = JNI_FALSE ;


  for ( ; ( dirname = strtok( firstTime ? path : NULL, JST_PATH_SEPARATOR ) ) ; firstTime = JNI_FALSE ) {

    if ( !*dirname ) continue ;

    if ( ( found = validateFile( dirname, file, validator ) ) ) break ;

  }

  return found ? dirname : NULL ;

}

/** Tries to find the given executable from PATH. Freeing the returned value
 * is up to the caller. NULL is returned if not found.
 * errno != 0 on error.
 * @param lastDirOnExecPath the leaf dir that the executable is required to be in, e.g. "bin". May be NULL
 * @param removeLastDir if true and lastDirOnExecPath, the last dir is removed from the path before it is returned.
 * */
extern char* jst_findFromPath( const char* execName, int (*validator)( const char* dirname, const char* filename ) ) {

  char *path    = NULL,
       *dirname = NULL ;

  errno = 0 ;

  {
    char* pathtmp = getenv( "PATH" ) ;
    if ( !pathtmp ) goto end ; // PATH not defined. Should never happen, but is not really an error if it does.
    JST_STRDUPA( path, pathtmp ) ;
    if ( !path ) goto end ; // error allocating memory (only in windows debug mode)
  }

  dirname = findPathEntryContainingFile( path, execName, validator ) ;

  if ( dirname ) dirname = jst_strdup( dirname ) ;

  end:
  if ( path ) { jst_freea( path ) ; }
  if ( errno && dirname ) {
    jst_free( dirname ) ;
  }

  if ( _jst_debug ) printPathLookupDebugMessage( "PATH", dirname, execName ) ;

  return dirname ;

}

extern int validateThatFileIsInBinDir( const char* dirname, const char* filename ) {
  jboolean endsWithSeparator = jst_dirNameEndsWithSeparator( dirname ) ;
  int      isvalid = 0 ;

  // ok, this is not perfect, e.g. foo/barbin will be true. However, it is sufficient for our purposes.
  if ( endsWithSeparator ) {
    size_t len = strlen( dirname ) ;
    isvalid = len >= 4 && memcmp( dirname + len - 4, "bin", 3 ) == 0 ;
  } else {
    isvalid = jst_endsWith( dirname, "bin" ) ;
  }

  return isvalid ;
}

