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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

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
#  endif
#endif


#include "jvmstarter.h"

/** Returns != 0 if the given file exists. */
extern int jst_fileExists( const char* fileName ) {
  struct stat buf ;
  int i = stat( fileName, &buf ) ;

  return ( i == 0 ) ;

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern char* jst_pathToParentDir( char* path ) {
  size_t   len                   = strlen( path ) ;
  jboolean pathEndsWithSeparator = jst_dirNameEndsWithSeparator( path ) ;
#if defined( _WIN32 )
  if ( len <= 3 ) return NULL ;
#else
  if ( len <= 1 ) return NULL ;  
#endif
  
  if ( pathEndsWithSeparator ) {
    path[ len - 1 ] = '\0' ;
  }
  *(strrchr( path, JST_FILE_SEPARATOR[ 0 ] ) + 1 ) = '\0' ;
  return path ;
  
}

extern jboolean jst_dirNameEndsWithSeparator( const char* dirName ) {
  return ( strcmp( dirName + strlen( dirName ) - strlen( JST_FILE_SEPARATOR ), JST_FILE_SEPARATOR ) == 0 ) ? JNI_TRUE : JNI_FALSE ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern char** jst_getFileNames( char* dirName, char* fileNamePrefix, char* fileNameSuffix ) {
  char     **tempResult ;
  size_t   resultSize = 30 ;
  jboolean errorOccurred = JNI_FALSE ;
  int      indx = 0 ;
  
  if ( !( tempResult = jst_calloc( resultSize, sizeof( char* ) ) ) ) return NULL ;
  
  if ( fileNamePrefix && !*fileNamePrefix ) fileNamePrefix = NULL ; // replace empty str w/ NULL
  if ( fileNameSuffix ) {
    if ( !*fileNameSuffix ) {
      fileNameSuffix = NULL ;
    } else {
      assert( fileNameSuffix[ 0 ] == '.' ) ; // file name suffix must begin with a .
    }
  }

  {

#  if defined( _WIN32 )
    // windows does not have dirent.h, it does things different from other os'es
    {
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
  
      if ( fileHandle == INVALID_HANDLE_VALUE ) {
        lastError = GetLastError() ;
        fprintf( stderr, "error: opening directory %s failed\n", dirName ) ;
        printWinError( lastError ) ;
        errorOccurred = JNI_TRUE ;
        goto end ;
      }
  
      if ( !( lastError = GetLastError() ) ) {
      
        do {
          char* temp = jst_append( NULL, NULL, fdata.cFileName, NULL ) ;
          if ( !temp || 
               !( tempResult = jst_appendArrayItem( tempResult, indx++, &resultSize, &temp, sizeof( char* ) ) ) ) {
            errorOccurred = JNI_TRUE ;
            goto end ;
          }
        } while ( FindNextFile( fileHandle, &fdata ) ) ;
        
      }
        
      if ( !lastError ) lastError = GetLastError() ;
      if ( lastError != ERROR_NO_MORE_FILES ) {
        fprintf( stderr, "error: error occurred when finding jars from %s\n", dirName ) ;
        printWinError( lastError ) ;
        errorOccurred = JNI_TRUE ;
      }
  
      end:
      
      if ( fileHandle != INVALID_HANDLE_VALUE ) FindClose( fileHandle ) ;
      if ( fileSpecifier ) free( fileSpecifier ) ;
      
    }
#  else
    {
      DIR           *dir ;
      struct dirent *entry ;
      size_t        prefixlen = fileNamePrefix ? strlen( fileNamePrefix ) : 0,
                    suffixlen = fileNameSuffix ? strlen( fileNameSuffix ) : 0 ;
      
      dir = opendir( dirName ) ;
      if ( !dir ) {
        fprintf( stderr, "error: could not read directory %s to append jar files from\n%s", dirName, strerror( errno ) ) ;
        errorOccurred = JNI_TRUE ;
        goto end ;
      }

      while( ( entry = readdir( dir ) ) ) {
        char   *fileName = entry->d_name ;
        size_t len       = strlen( fileName ) ;

        if ( len < prefixlen || len < suffixlen ||
             ( strcmp( ".", fileName ) == 0 ) || ( strcmp( "..", fileName ) == 0 ) ) continue ;
        
        if ( ( !fileNamePrefix || memcmp( fileNamePrefix, fileName, prefixlen ) == 0 ) &&
             ( !fileNameSuffix || memcmp( fileNameSuffix, fileName + len - suffixlen, suffixlen ) == 0 ) ) {
          char* temp = jst_append( NULL, NULL, fileName, NULL ) ;
          if ( !temp || 
               !( tempResult = jst_appendArrayItem( tempResult, indx++, &resultSize, &temp, sizeof( char* ) ) ) ) {
            errorOccurred = JNI_TRUE ;
            goto end ;
          }
        }
      }
      

      end:

      if ( dir ) closedir( dir ) ;
      
    }
#  endif
  }
  
  if ( tempResult ) {
    tempResult = jst_appendArrayItem( tempResult, indx, &resultSize, NULL, sizeof( char* ) ) ;
    if ( !tempResult ) errorOccurred = JNI_TRUE ;
  }
  
  {
    char** rval = NULL ;
    
    if ( tempResult ) {
      int i = 0 ;
      char *s ;
      
      if ( !errorOccurred ) rval = jst_packStringArray( tempResult ) ;
      while ( ( s = tempResult[ i++ ] ) ) free( s ) ;
      free( tempResult ) ;
    }
    
    return rval ;
  }
  
}

extern char* jst_getExecutableHome() {
  static char* _execHome = NULL ;
  
  char   *execHome = NULL ;

  size_t len ;
  
  if ( _execHome ) return _execHome ;
  
# if defined( _WIN32 )
  {
    size_t currentBufSize = 0 ;
  
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
      SetLastError( 0 ) ; 
      len = GetModuleFileNameA( NULL, execHome, (DWORD)currentBufSize ) ;
    } while ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) ;
    // this works equally well but is a bit less readable
    // } while(len == currentBufSize);
  }
  
  if ( len == 0 ) {
    fprintf( stderr, "error: finding out executable location failed\n" ) ;
    printWinError( GetLastError() ) ;
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
  
    sprintf( procSymlink,
#     if defined( __linux__ )
        // /proc/{pid}/exe is a symbolic link to the executable
        "/proc/%d/exe" 
#     elif defined( __sun__ )
        // see above
        "/proc/%d/path/a.out"
#     endif
        , (int)getpid()
    ) ;
    
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
          return NULL ;
        }
      }
    }
#  else
#    error "looking up executable location has not been implemented on your platform"
#  endif    
    
    if ( !jst_fileExists( procSymlink ) ) { // should never happen
      fprintf( stderr, "warning: symlink or file %s does not exist\n", procSymlink ) ;
      free( procSymlink ) ;
      free( execHome ) ;
      return "" ;
    }
  
    if ( !realpath( procSymlink, execHome ) ) {
      fprintf( stderr, strerror( errno ) ) ;
      free( procSymlink ) ;
      free( execHome    ) ;
      return NULL ;
    }
    
    free( procSymlink ) ;
  }
  
# endif

  // cut off the executable name
  *(strrchr( execHome, JST_FILE_SEPARATOR[ 0 ] ) + 1 ) = '\0' ;   
  len = strlen( execHome ) ;
  execHome = jst_realloc( execHome, len + 1 ) ; // should not fail as we are shrinking the buffer
  assert( execHome ) ;
  return _execHome = execHome ;
  

}
