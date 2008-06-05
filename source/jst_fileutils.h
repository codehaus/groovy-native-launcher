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

    

int jst_fileExists( const char* fileName ) ;

/** Returns 1 if the given fileName points to a dir, 0 if a file. If the given file does not exist, the behavior is undefined, most likely
 * a crash. */
int jst_isDir( const char* fileName ) ;

/** Figures out the path to the parent dir of the given path (which may be a file or a dir). Modifies the argument so
 * that it points to the parent dir. Returns null (but does not modify the given string) if the given dir is the root dir.
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
char** jst_getFileNames( char* dirName, char* fileNamePrefix, char* fileNameSuffix, int (*selector)( const char* filename ) ) ;

/** Creates a string that represents a file (or dir) name, i.e. all the elements are
 * ensured to contain a file separator between them. 
 * Usage: give as many strings as you like and a NULL as the last param to terminate. */
char* jst_createFileName( const char* root, ... ) ;


/** Returns the path to the directory where the current executable lives, excluding the last path separator, e.g.
 * c:\programs\groovy\bin or /usr/loca/groovy/bin 
 * Note that this means that if the program lives in the root dir in *nix, "" is returned.
 * Freeing the returned pointer is the responsibility of the caller. 
 * Returns NULL on error. */
char* jst_getExecutableHome() ;

/** returns the full path to the given file or directory. If the given file or dir does not exist, the
 * given param is returned. Also, if the full path is identical to the param, the param is returned.
 * Also, resolves any symlinks on platforms where applicable.
 * Freeing the returned pointer (if different from the original) must be done by the caller. 
 * @param fileOrDirName if NULL, NULL is returned. 
 * @return NULL on error */
char* jst_fullPathName( const char* fileOrDirName ) ;

/**
 * @param subdir may be NULL or empty
 */
char* findStartupJar( const char* basedir, const char* subdir, const char* prefix, int (*selector)( const char* filename ) ) ;

    
#if defined( __cplusplus )
  } // end extern "C"
#endif


#endif

