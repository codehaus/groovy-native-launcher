//  A simple library for creating a native launcher for a java app
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


#if !defined( _JST_FILEUTILS_H_ )
#  define _JST_FILEUTILS_H_

#if defined( __cplusplus )
  extern "C" {
#endif


/** true iff the fileName points to an existing file or dir (or a link to either). */
int jst_fileExists( const char* fileName ) ;

/** Returns 1 if the given fileName points to a dir, 0 if a file. If the given file does not exist, the behavior is undefined, most likely
 * a crash. Use jst_fileExists to check before calling this. */
int jst_isDir( const char* fileName ) ;

/** Figures out the path to the parent dir of the given path (which may be a file or a dir). Modifies the argument so
 * that it points to the parent dir. Returns NULL (but does not modify the given string) if the given dir is the root dir.
 * For files, the dir containing the file is the parent dir.
 * Note that the given path must be absolute and canonical for this function to work ( no e.g. ..'s ) */
char* jst_pathToParentDir( char* path ) ;

/** Returns a NULL terminated string array containing the names of files in the given dir. The returned string array is dyn
 * allocated. It and all the contained strings are freed by freeing the returned pointer.
 * @param fileNamePrefix return only files whose name begins with this prefix. May be NULL or empty string.
 * @param fileNameSuffix return only files whose name ends with this suffix.   May be NULL or empty string.
 *        Suffix here means the file type identifier part, e.g. in "foo.jar" -> ".jar"
 * @param selector pointer to a function that is called to decide whether the given file will be included (returns != 0 if file is to be included).
 *        it is to return true when the file is to be included in the returned list. May be NULL
 *  */
char** jst_getFileNames( char* dirName, char* fileNamePrefix, char* fileNameSuffix, int (*selector)( const char* dirname, const char* filename ) ) ;

/** Creates a string that represents a file (or dir) name, i.e. all the elements are
 * ensured to contain a file separator between them.
 * Usage: give as many strings as you like and a NULL as the last param to terminate. */
char* jst_createFileName( const char* root, ... ) ;


/** Returns the path to the directory where the current executable lives, excluding the last path separator, e.g.
 * c:\programs\groovy\bin or /usr/loca/groovy/bin
 * Note that this means that if the program lives in the root dir in *nix, "" is returned.
 * Freeing the returned pointer is the responsibility of the caller.
 * Returns NULL on error. */
char* jst_getExecutableHome( void ) ;

/** returns the full path to the given file or directory. If the given file or dir does not exist, the
 * given param is returned. Also, if the full path is identical to the param, the param is returned.
 * Also, resolves any symlinks on platforms where applicable.
 * Freeing the returned pointer (if different from the original) must be done by the caller.
 * @param fileOrDirName if NULL, NULL is returned.
 * @return NULL on error */
char* jst_fullPathName( const char* fileOrDirName ) ;

/** Same as jst_fullPathName, but writes the result to the given buffer, expanding it if necessary (it must be
 * dynallocated). Returns the buffer or NULL on error. In case of error the given buffer has been freed. */
char* jst_fullPathNameToBuffer( const char* fileOrDirName, char* buffer, size_t *bufsize ) ;

/** Same as jst_fullPathName, but writes the full path name on top of the original, expanding the buffer if necessary. */
char* jst_overwriteWithFullPathName( char* buffer, size_t *bufsize ) ;

/**
 * @param subdir may be NULL or empty
 * @param progname if != NULL, will be used in possible error msgs. If NULL, no error msg will be printed.
 */
char* findStartupJar( const char* basedir, const char* subdir, const char* prefix, const char* progname, int (*selector)( const char* dirname, const char* filename ) ) ;

/** This is a rather crude set of choices, but suffices for now... */
typedef enum {
  JST_INGORE_EXECUTABLE_LOCATION,
  JST_USE_EXEC_LOCATION_AS_HOME,
  JST_USE_PARENT_OF_EXEC_LOCATION_AS_HOME
} JstAppHomeStrategy ;

/**
 * @param validator returns != 0 if the given dir is a valid app home. May be NULL.
 */
char* jst_getAppHome( JstAppHomeStrategy appHomeStrategy, const char* envVarName, int (*validator)( const char* dirname ) ) ;

/** Tries to find the given executable from PATH. Freeing the returned value
 * is up to the caller. NULL is returned if not found.
 * Note that the returned path to the executable is the dir where the executable resides, not containing
 * the executable name. Any symlinks and relative paths have been resolved.
 * errno != 0 on error.
 * */
char* jst_findFromPath( const char* execName, int (*validator)( const char* dirname, const char* filename ) ) ;

/** A simple func that checks whther the last char in the dir name is a file separator. */
jboolean jst_dirNameEndsWithSeparator( const char* dirName ) ;

/** Checks that the given file name has the given prefix and suffix. Give NULL to match anything. fileName may not be NULL.
 * Returns true if the given prefix and suffix match. */
int matchPrefixAndSuffixToFileName( const char* fileName, const char* prefix, const char* suffix ) ;

/** a simple validator func that can be used as an argument to some other funcs that take a corresponding
 * function pointer as a parameter */
int validateThatFileIsInBinDir( const char* dirname, const char* filename ) ;

/** Modifies the given buffer (inserts \0 char) and returns a pointer within it. Free the *original*
 * if it's dynallocated. */
char* jst_extractProgramName( char* command, jboolean removeEndingW ) ;


#if defined( __cplusplus )
  } // end extern "C"
#endif


#endif

