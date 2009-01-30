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

  return ( i == 0 ) ;

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
  size_t   len                   = strlen( path ) ;
  jboolean pathEndsWithSeparator = jst_dirNameEndsWithSeparator( path ) ;

  // this implementation is not perfect, but works ok in this program's context

#if defined( _WIN32 )
  if ( ( len == 0 ) ||
       ( len == 1 && path[ 0 ] == JST_FILE_SEPARATOR[ 0 ] ) ||
       ( len == 3 && isalpha( path[ 0 ] ) && path[ 1 ] == ':' && path[ 2 ] == JST_FILE_SEPARATOR[ 0 ] )
     ) return NULL ;
#else
  if ( len <= 1 ) return NULL ;
#endif

  if ( pathEndsWithSeparator ) {
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
static jboolean addFileToList( char ***tempResult, int *indx, size_t *resultSize, char* dirName, char* fileName, int (*selector)( const char* dirname, const char* filename ), jboolean *errorOccurred ) {

  int okToInclude = JNI_TRUE ;

  if ( selector ) {
    errno = 0 ;
    okToInclude = selector( dirName, fileName ) ;
    if ( errno ) { *errorOccurred = JNI_TRUE; goto end ; }
  }

  if ( okToInclude ) {
    char* temp = jst_strdup( fileName ) ;
    if ( !temp ||
         !( *tempResult = jst_appendArrayItem( *tempResult, (*indx)++, resultSize, &temp, sizeof( char* ) ) ) ) {
      *errorOccurred = JNI_TRUE ;
      goto end ;
    }
  }

  end:
  return *errorOccurred ;
}


static void changeEmptyPrefixAndSuffixToNULL( char** fileNamePrefix, char** fileNameSuffix ) {

  if ( *fileNamePrefix && !**fileNamePrefix ) *fileNamePrefix = NULL ; // replace empty str w/ NULL
  if ( *fileNameSuffix ) {
    if ( !**fileNameSuffix ) {
      *fileNameSuffix = NULL ;
    } else {
      assert( (*fileNameSuffix)[ 0 ] == '.' ) ; // file name suffix must begin with a .
    }
  }

}

extern jboolean matchPrefixAndSuffixToFileName( char* fileName, char* prefix, char* suffix ) {

  return jst_startsWith( fileName, prefix ) && jst_endsWith( fileName, suffix ) ;

}

extern char** jst_getFileNames( char* dirName, char* fileNamePrefix, char* fileNameSuffix, int (*selector)( const char* dirname, const char* filename ) ) {
  char     **tempResult ;
  size_t   resultSize = 30 ;
  jboolean errorOccurred = JNI_FALSE ;
  int      indx = 0 ;

  if ( !( tempResult = jst_calloc( resultSize, sizeof( char* ) ) ) ) return NULL ;

  changeEmptyPrefixAndSuffixToNULL( &fileNamePrefix, &fileNameSuffix ) ;

  {

#  if defined( _WIN32 )
  // windows does not have dirent.h, it does things different from other os'es

    jboolean        dirNameEndsWithSeparator = jst_dirNameEndsWithSeparator( dirName ) ;
    HANDLE          fileHandle               = INVALID_HANDLE_VALUE ;
    WIN32_FIND_DATA fdata ;
    DWORD           lastError ;
    char*           fileSpecifier = jst_append( NULL, NULL, dirName,
                                                            dirNameEndsWithSeparator ? "" : JST_FILE_SEPARATOR,
                                                            fileNamePrefix ? fileNamePrefix : "",
                                                            "*",
                                                            fileNameSuffix ? fileNameSuffix : ".*",
                                                            NULL ) ;
    if ( !fileSpecifier ) {
      errorOccurred = JNI_TRUE ;
      goto end ;
    }

    SetLastError( 0 ) ;

    fileHandle = FindFirstFile( fileSpecifier, &fdata ) ;

    if ( checkForInvalidFileHandle( fileHandle, dirName, &errorOccurred ) ) goto end ;


    if ( !( lastError = GetLastError() ) ) {

      do {

        if ( addFileToList( &tempResult, &indx, &resultSize, dirName, fdata.cFileName, selector, &errorOccurred ) ) goto end ;

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

#  else

    DIR           *dir ;
    struct dirent *entry ;

    dir = opendir( dirName ) ;
    if ( !dir ) {
      fprintf( stderr, "error: could not open directory %s\n%s", dirName, strerror( errno ) ) ;
      errorOccurred = JNI_TRUE ;
      goto end ;
    }

    while( ( entry = readdir( dir ) ) ) {
      char   *fileName = entry->d_name ;

      if ( ( strcmp( ".", fileName ) == 0 ) || ( strcmp( "..", fileName ) == 0 ) ) continue ;

      if ( matchPrefixAndSuffixToFileName( fileName, fileNamePrefix, fileNameSuffix ) ) {

        if ( addFileToList( &tempResult, &indx, &resultSize, dirName, fileName, selector, &errorOccurred ) ) goto end ;

      }
    }


    end:

    if ( dir ) closedir( dir ) ;

#  endif
  }

  if ( tempResult ) {
    tempResult = jst_appendArrayItem( tempResult, indx, &resultSize, NULL, sizeof( char* ) ) ;
    if ( !tempResult ) errorOccurred = JNI_TRUE ;
  }

  {
    char** rval = NULL ;

    if ( tempResult ) {
      if ( !errorOccurred ) rval = jst_packStringArray( tempResult ) ;
      jst_freeAll( (void***)(void*)&tempResult ) ;
    }

    return rval ;
  }

}

extern char* jst_getExecutableHome() {

  char   *execHome = NULL ;

  size_t len ;

# if defined( _WIN32 )
  {
    size_t currentBufSize = 0 ;

    do {
      if ( currentBufSize == 0 ) {
        currentBufSize = PATH_MAX + 1 ;
        execHome = jst_malloc( currentBufSize * sizeof( char ) ) ;
      } else {
        execHome = jst_realloc( execHome, (currentBufSize += 100) * sizeof( char ) ) ;
      }

      if ( !execHome ) return NULL ;


      // reset the error state, just in case it has been left dangling somewhere and
      // GetModuleFileNameA does not reset it (its docs don't tell).
      // GetModuleFileName docs: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/getmodulefilename.asp
      SetLastError( 0 ) ;
      len = GetModuleFileNameA( NULL, execHome, (DWORD)currentBufSize ) ;
    } while ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) ;
    // this works equally well but is a bit less readable
    // } while(len == currentBufSize);
  }

  if ( len == 0 ) {
    fprintf( stderr, "error: finding out executable location failed\n" ) ;
    jst_printWinError( GetLastError() ) ;
    free( execHome ) ;
    return NULL ;
  }

# elif defined( __linux__ ) || defined( __sun__ ) || defined( __APPLE__ )
  {
    char   *procSymlink ;

    if ( !( execHome = jst_malloc( ( PATH_MAX + 1 ) * sizeof( char ) ) ) ) return NULL ;

   // getting the symlink varies by platform
#  if defined( __linux__ ) || defined( __sun__ )
    // TODO - reserve space for this from the stack, not dynamically
    procSymlink = jst_malloc( 40 * sizeof( char ) ) ; // big enough
    if ( !procSymlink ) {
      jst_free( execHome ) ;
      return NULL ;
    }

    sprintf( procSymlink, JST_SYMLINK_TO_EXECUTABLE_STR_TEMPLATE, (int)getpid() ) ;

#  elif defined( __APPLE__ )
    {
      jboolean appleError = JNI_FALSE ;
      const CFBundleRef bundle = CFBundleGetMainBundle() ;
      if ( !bundle ) {
        fprintf( stderr, "error: failed to get main bundle\n" ) ;
        jst_free( execHome ) ;
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
          free( execHome ) ;
          if ( procSymlink ) free( procSymlink ) ;
          fprintf( stderr, "error: error occurred when using bundle api to find executable home directory\n" ) ;
          return NULL ;
        }
      }
    }

#  endif

    if ( !jst_fileExists( procSymlink ) ) { // should never happen
      fprintf( stderr, "error: symlink or file %s does not exist\n", procSymlink ) ;
      free( procSymlink ) ;
      free( execHome ) ;
      return NULL ;
    }

    if ( !realpath( procSymlink, execHome ) ) {
      fprintf( stderr, strerror( errno ) ) ;
      free( procSymlink ) ;
      free( execHome    ) ;
      return NULL ;
    }

    free( procSymlink ) ;
  }

# else
#  error "looking up executable location has not been implemented on your platform"
# endif

  // cut off the executable name and the trailing file separator
  *(strrchr( execHome, JST_FILE_SEPARATOR[ 0 ] ) ) = '\0' ;
  len = strlen( execHome ) ;
  execHome = jst_realloc( execHome, len + 1 ) ; // should not fail as we are shrinking the buffer
  assert( execHome ) ;
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

extern char* jst_getAppHome( JstAppHomeStrategy appHomeStrategy, const char* envVarName, int (*validator)( const char* dirname ) ) {

  // FIXME - this func is a bit too complex - refactor

  char *appHome = NULL ;

  if ( appHomeStrategy != JST_INGORE_EXECUTABLE_LOCATION ) {
    appHome = jst_getExecutableHome() ;
    if ( !appHome ) return NULL ;

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

  if ( !appHome && envVarName ) {
    appHome = getenv( envVarName ) ;
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

      if ( _jst_debug ) fprintf( stderr, "debug: obtained app home from env var  %s, value: %s\n", envVarName, appHome ) ;

    }
  }

  if ( !appHome ) {
    int execLocAndEnvVar = ( appHomeStrategy != JST_INGORE_EXECUTABLE_LOCATION ) && envVarName ;
    fprintf( stderr, "error: could not locate app home\nplease " ) ;
    if ( execLocAndEnvVar )                                  fprintf( stderr, "either " ) ;
    if ( appHomeStrategy != JST_INGORE_EXECUTABLE_LOCATION ) fprintf( stderr, "place the executable in the appropriate directory " ) ;
    if ( execLocAndEnvVar )                                  fprintf( stderr, "or " ) ;
    if ( envVarName )                                        fprintf( stderr, "set the environment variable %s", envVarName ) ;
    fprintf( stderr, "\n" ) ;
  }

  return appHome ;
}


/** Tries to find the given executable from PATH. Freeing the returned value
 * is up to the caller. NULL is returned if not found.
 * errno != 0 on error.
 * @param lastDirOnExecPath the leaf dir that the executable is required to be in, e.g. "bin". May be NULL
 * @param removeLastDir if true and lastDirOnExecPath, the last dir is removed from the path before it is returned.
 * */
extern char* jst_findFromPath( const char* execName, const char* lastDirOnExecPath, jboolean removeLastDir ) {

  char     *path = NULL,
           *p,
           *executablePath = NULL ;
  size_t   execPathBufSize = 100 ;
  int      lastDirLen = ( lastDirOnExecPath && lastDirOnExecPath[ 0 ] ) ? (int)strlen( lastDirOnExecPath ) : -1 ;
  jboolean firstTime = JNI_TRUE,
           found     = JNI_FALSE ;

  errno = 0 ;

  p = getenv( "PATH" ) ;
  if ( !p ) goto end ; // PATH not defined. Should never happen, but is not really an error if it does.

  if ( !( path = jst_strdup( p ) ) ) goto end ;

  for ( ; ( p = strtok( firstTime ? path : NULL, JST_PATH_SEPARATOR ) ) ; firstTime = JNI_FALSE ) {
    size_t len = strlen( p ) ;
    if ( len == 0 ) continue ;

    if ( executablePath ) executablePath[ 0 ] = '\0' ;

    if ( !( executablePath = jst_append( executablePath, &execPathBufSize, p, NULL ) ) ) goto end ;

    executablePath = jst_append( executablePath, &execPathBufSize,
                           ( executablePath[ len - 1 ] != JST_FILE_SEPARATOR[ 0 ] ) ? JST_FILE_SEPARATOR : "",
                           execName, NULL ) ;

    if ( !executablePath ) goto end ;

    if ( jst_fileExists( executablePath ) ) {
      char* lastFileSep ;
      char* realFile = jst_fullPathName( executablePath ) ;

      if ( !realFile ) goto end ;

      if ( realFile != executablePath ) { // if true, what we got was either not full path or behind a symlnk
        executablePath[ 0 ] = '\0' ;
        executablePath = jst_append( executablePath, &execPathBufSize, realFile, NULL ) ;
        jst_free( realFile ) ;
        if ( !executablePath ) goto end ;
      }

      lastFileSep = strrchr( executablePath, JST_FILE_SEPARATOR[ 0 ] ) ;
      if ( lastFileSep ) {
        *lastFileSep = '\0' ;
        len = strlen( executablePath ) ;
        if ( lastDirLen != -1 ) {
          if ( ( (int)len >= lastDirLen + 1 ) &&
               ( strcmp( executablePath + len - lastDirLen, lastDirOnExecPath ) == 0 ) ) {
            if ( removeLastDir ) executablePath[ len -= lastDirLen + 1 ] = '\0' ;
            found = JNI_TRUE ;
            break ;
          }
        } else {
          found = JNI_TRUE ;
          break ;
        }
      }
    }
  }

  end:
  if ( path ) free( path ) ;
  if ( ( !found || errno ) && executablePath ) {
    jst_free( executablePath ) ;
  }

  if ( _jst_debug ) {
    if ( executablePath ) {
      fprintf( stderr, "debug: java home found on PATH: %s\n", executablePath ) ;
    } else {
      fprintf( stderr, "debug: java home not found on PATH\n" ) ;
    }
  }

  return executablePath ;

}
