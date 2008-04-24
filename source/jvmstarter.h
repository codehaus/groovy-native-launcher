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
//  $Revision$
//  $Date$


#if !defined( _JVMSTARTER_H_ )

#include <jni.h>

#if defined( __cplusplus )
  extern "C" {
#endif

#define _JVMSTARTER_H_

#if defined(_WIN32)

#  define JST_FILE_SEPARATOR "\\"
#  define JST_PATH_SEPARATOR ";"

#else

#  define JST_FILE_SEPARATOR "/"
#  define JST_PATH_SEPARATOR ":"

#endif

/** set to true at startup to print debug information about what the launcher is doing to stdout */
extern jboolean _jst_debug ;    
    
typedef enum { 
  /** a standalone parameter, such as -v */
  JST_SINGLE_PARAM = 1,
  /** a parameter followed by some additional info, e.g. --endcoding UTF8 */
  JST_DOUBLE_PARAM = 2,
  /** a parameter that its value attached, e.g. param name "-D", that could be given on command line as -Dfoo */
  JST_PREFIX_PARAM = 4
} JstParamClass ;

typedef enum {
  /** Ignoring an input parameter means it is not passed to jvm nor to the launched java app. It can
   * still be used for some logic during the launch process. An example is "-server", which can 
   * be used to instruct the launcher to use the server jvm. */
  JST_IGNORE       = 1,
  // both of the next two may be set at the same time, use & operator to find out
  JST_TO_JVM       = 2,
  JST_TO_LAUNCHEE  = 4,
  JST_TERMINATING  = 8,
  JST_CYGWIN_PATH_CONVERT = 0x10, 
  JST_CYGWIN_PATHLIST_CONVERT = 0x20, 
  
  // the following can't be set as value to ParamInfo.handling (it doesn't make sense)
  
  JST_UNRECOGNIZED = 0x40,
  // this bit will be set on the parameter after which all the parameters are launchee params (and all following params)
  JST_TERMINATING_OR_AFTER = JST_TERMINATING
} JstInputParamHandling ;

typedef enum {
  JST_CYGWIN_NO_CONVERT          = 0,
  JST_CYGWIN_PATH_CONVERSION     = JST_CYGWIN_PATH_CONVERT, 
  JST_CYGWIN_PATHLIST_CONVERSION = JST_CYGWIN_PATHLIST_CONVERT  
} CygwinConversionType ;


typedef struct {
  
  char          *name ;
  JstParamClass type ;
  /** If != 0, the actual parameters followed by this one are passed on to the launchee. */
//  unsigned char terminating ;
  JstInputParamHandling handling ;
  
} JstParamInfo ;

// this is to be taken into use after some other refactorings
typedef struct {
  
  char *param ;

  // for twin (double) parameters, only the first will contain a pointer to the corresponding param info
  JstParamInfo* paramDefinition ;
  
  // this is needed so that the cygwin conversions need to be performed only once
  // This is the actual value passed in to the jvm or launchee program. E.g. the two (connected) parameters
  // --classpath /usr/local:/home/antti might have values "--classpath" and "C:\programs\cygwin\usr\local;C:\programs\cygwin\home\antti"
  // respectively. For sinle params this is "" (as they have no real separate value except for being present). 
  // For prefix params this contains the prefix. Use jst_getParameterValue to fetch the value w/out the prefix. 
  char *value ;
  JstInputParamHandling handling ; 
    
} JstActualParam ; 


/** The strategy to use when selecting between using client and server jvm *if* no explicit -client/-server param. If explicit param
 * is given, then that type of jvm is used, period.
 * Bitwise meaning:
 *  001 -> allow using client jvm
 *  010 -> allow using server jvm
 *  100 -> prefer client over server
 * If you're just passing this as a param, you need not worry about the bitwise meaning, it's just an implementation detail. */
typedef enum { 
  JST_CLIENT_FIRST     = 7,
  JST_SERVER_FIRST     = 3,
  JST_TRY_CLIENT_ONLY  = 1,
  JST_TRY_SERVER_ONLY  = 2
} JVMSelectStrategy ;


/** These may be or:d together. */
typedef enum {
  JST_IGNORE_UNRECOGNIZED = 0,
  JST_UNRECOGNIZED_TO_JVM = 1,
  JST_UNRECOGNIZED_TO_APP = 2
} JstUnrecognizedParamStrategy ;

 /** Note that if you tell that -cp / --classpath and/or -jh / --javahome params are handled automatically. 
  * If you do not want the user to be able to affect 
  * javahome, specify these two as double params and their processing is up to you. 
  */
typedef struct {
  /** May be null. */
  char* javaHome ;
  /** what kind of jvm to use. */
  JVMSelectStrategy jvmSelectStrategy ;
  // TODO: ditch this, the called may preprocess them. Provide a func to transform a space separated string into jvm options
  /** The name of the env var where to take extra jvm params from. May be NULL. */
  char* javaOptsEnvVar ;
  JstUnrecognizedParamStrategy unrecognizedParamStrategy ;
  /** Give any cp entries you want appended to the beginning of classpath here. May be NULL */
  char* initialClasspath ;
  /** Processed actual parameters. May not be NULL. Use jst_processInputParameters function to obtain this value. */
  JstActualParam* parameters ;
  /** extra params to the jvm (in addition to those extracted from arguments above). */
  JavaVMOption* jvmOptions ;
  int numJvmOptions; 
  /** parameters to the java class' main method. These are put first before the command line args. */
  char** extraProgramOptions ;
  char*  mainClassName ;
  /** Defaults to "main" */
  char*  mainMethodName ;
  /** The directories from which add all jars from to the startup classpath. NULL terminates the list. */
  char** jarDirs ;
  char** jars ;
} JavaLauncherOptions ;


#if defined( _WIN32 )
// to have type DWORD in the func signature below we need this header
//#include "Windows.h"
// DWORD is defined as unsigned long, so we'll just use that
/** Prints an error message for the given windows error code. */
void jst_printWinError( unsigned long errcode ) ;

#endif

int jst_launchJavaApp( JavaLauncherOptions* options ) ;

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
 *        Suffix here means the file type identifier part, e.g. in "foo.jar" -> ".jar" */
char** jst_getFileNames( char* dirName, char* fileNamePrefix, char* fileNameSuffix ) ;

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
 * Freeing the returned pointer (if different from the original) must be done by the caller. 
 * @param fileOrDirName if NULL, NULL is returned. 
 * @return NULL on error */
char* jst_fullPathName( const char* fileOrDirName ) ;



/** returns an array of JstActualParam, the last one of which contains NULL for field param. 
 * All the memory allocated can be freed by freeing the returned pointer.
 * Return NULL on error. 
 * @param cygwinConvertParamsAfterTermination if cygwin compatibility is set in the build & cygwin1.dll is found and loaded, 
 *                                            the terminating param (the param which w/ all the following params is passed to launchee)
 *                                            and all the following params are cygwin path converted with the given conversion type. */
JstActualParam* jst_processInputParameters( char** args, int numArgs, JstParamInfo *paramInfos, char** terminatingSuffixes, CygwinConversionType cygwinConvertParamsAfterTermination ) ;

/** For single params, returns "" if the param is present, NULL otherwise. */
char* jst_getParameterValue( const JstActualParam* processedParams, const char* paramName ) ;

char* jst_getParameterAfterTermination( const JstActualParam* processedParams, int indx ) ;

/** returns 0 iff the answer is no. */
int jst_isToBePassedToLaunchee( const JstActualParam* processedParam, JstUnrecognizedParamStrategy unrecognizedParamStrategy ) ;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// dynamic memory handling
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/** Appends the given strings to target. size param tells the current size of target (target must have been
 * dynamically allocated, i.e. not from stack). If necessary, target is reallocated into a bigger space. 
 * Returns the possibly new location of target, and modifies the size inout parameter accordingly. 
 * If target is NULL, it is allocated w/ the given size (or bigger if given size does not fit all the given strings). 
 * In case target is NULL and you are not interested how big the buffer became, you can give NULL as size. */
char* jst_append( char* target, size_t* size, ... ) ; 

/** Concatenates the strings in the given null terminated str array to a single string, which must be freed by the caller. Returns null on error. */
char* jst_concatenateStrArray( char** nullTerminatedStringArray ) ;

/** If array is NULL, a new one will be created, size arlen. The given array will be reallocated if there is not enough space.
 * The newly allocated memory (in both cases) contains all 0s. If NULL is given as the item, zeroes are added at the given array position. */
void* jst_appendArrayItem( void* array, int index, size_t* arlen, void* item, int item_size_in_bytes ) ;

/** Appends the given pointer to the given null terminated pointer array.
 * given pointer to array may point to NULL, in which case a new array is created. 
 * Returns NULL on error. 
 * @param item if NULL, the given array is not modified and NULL is returned. */
void** jst_appendPointer( void*** pointerToNullTerminatedPointerArray, size_t* arrSize, void* item ) ;

int jst_pointerArrayLen( void** nullTerminatedPointerArray ) ;

/** Returns the given item, NULL if the item was not in the array. */
void* jst_removePointer( void** nullTerminatedPointerArray, void* itemToBeRemoved ) ;

/** returns 0 if the given item was not in the given array. pointer pointed to is set to NULL. */
int jst_removeAndFreePointer( void** nullTerminatedPointerArray, void** pointerToItemToBeRemoved ) ;


/** Given a null terminated string array, makes a dynamically allocated copy of it that can be freed using a single call to free. Useful for constructing
 * string arrays returned to caller who must free them. In technical detail, the returned mem block first contains all the char*, the termiting NULL and
 * then all the strings one after another. Note that whoever uses the char** does not need to know this mem layout. */
char** jst_packStringArray( char** nullTerminatedStringArray ) ;

typedef enum { PREFIX_SEARCH, SUFFIX_SEARCH, EXACT_SEARCH } SearchMode;

/** The first param may be NULL, it is considered an empty array. */
jboolean jst_arrayContainsString( char** nullTerminatedArray, const char* searchString, SearchMode mode ) ; 

/** These wrap the corresponding memory allocation routines. The only difference is that these print an error message if
 * the call fails. */
void* jst_malloc( size_t size ) ;
void* jst_calloc( size_t nelem, size_t elsize ) ;
void* jst_realloc( void* ptr, size_t size ) ;
char* jst_strdup( const char* s ) ;


#define jst_free( x ) free( x ) ; x = NULL

/** Frees all the pointers in the given array, the array itself and sets the reference to NULL */
void jst_freeAll( void*** pointerToNullTerminatedPointerArray ) ;


/** Tries to find Java home by looking where java command is located on PATH. Freeing the returned value
 * is up to the caller. 
 * errno != 0 on error. */
char* jst_findJavaHomeFromPath() ;



/** As appendArrayItem, but specifically for jvm options. 
 * @param extraInfo JavaVMOption.extraInfo. See jni.h or jni documentation (JavaVMOption is defined in jni.h). */
JavaVMOption* appendJvmOption( JavaVMOption* opts, int index, size_t* optsSize, char* optStr, void* extraInfo ) ;

#if defined( __cplusplus )
  } // end extern "C"
#endif

#endif // ifndef _JVMSTARTER_H_
