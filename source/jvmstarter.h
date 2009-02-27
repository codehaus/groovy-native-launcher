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
#define _JVMSTARTER_H_

#include <jni.h>

#if defined( __cplusplus )
  extern "C" {
#endif


#if defined(_WIN32)

#  include <Windows.h>

   typedef HINSTANCE JstDLHandle ;

#  define JST_FILE_SEPARATOR "\\"
#  define JST_PATH_SEPARATOR ";"

#else

   typedef void* JstDLHandle ;

#  define JST_FILE_SEPARATOR "/"
#  define JST_PATH_SEPARATOR ":"

#endif

#define __JST_STRINGIZER_HELPER__( arg ) #arg

/** This macro yields its parameter in double quotes, e.g. JST_STRINGIZER( /home/antti => "/home/antti" */
#define JST_STRINGIZER( str ) __JST_STRINGIZER_HELPER__( str )

/** Call this function in the beginning of main to initialize the global flag telling whether to
 * print out debug information to stderr.
 * @return != 0 if debug state enabled */
int jst_initDebugState() ;

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

  /** a NULL terminated string array with all the aliases for a certain param. */
  const char **names ;
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
  // respectively. For single params this is "" (as they have no real separate value except for being present).
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
  JST_CLIENTVM      = 1,
  JST_SERVERVM      = 2,
  JST_PREFER_CLIENT = 4,
  JST_CLIENT_FIRST  = JST_CLIENTVM | JST_SERVERVM | JST_PREFER_CLIENT,
  JST_SERVER_FIRST  = JST_CLIENTVM | JST_SERVERVM,
} JVMSelectStrategy ;


/** These may be or:d together. */
typedef enum {
  JST_IGNORE_UNRECOGNIZED = 0,
  JST_UNRECOGNIZED_TO_JVM = 1,
  JST_UNRECOGNIZED_TO_APP = 2
} JstUnrecognizedParamStrategy ;

/** On some applications putting jars in bootstrap classpath instead of classpath boosts startup performance.
 * See e.g. http://archive.jruby.codehaus.org/dev/481A4453.3060307%40sun.com */
typedef enum {
  JST_NORMAL_CLASSPATH = 0,
  JST_BOOTSTRAP_CLASSPATH = 1,
  JST_BOOTSTRAP_CLASSPATH_A = 3,
  JST_BOOTSTRAP_CLASSPATH_P = 5,
} JstClasspathStrategy ;

typedef struct {
  /** relative to apphome */
  char *name ;
  // FIXME - not supported yet
  jboolean fetchRecursively ;
  /** May be null. The dirname parameter is there so one can differentiate between folders when fetching recursively.  */
  int (*filter)( const char* dirname, const char* filename ) ;
} JarDirSpecification ;

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
  /** The directories from which add all jars from to the startup classpath. NULL (in the name field) terminates the list. */
  JarDirSpecification* jarDirs ;
  char** jars ;
  /** What classpath to put the startup jars and initial classpath into. ATM it is only possible to put everything into
   * one of the possible classpaths, but finer grain of control may be provided in future implementation if
   * it is deemed necessary. */
  JstClasspathStrategy classpathStrategy ;
} JavaLauncherOptions ;


// TODO: work in progress

typedef struct {
  char* javahome ;
  JavaVM* javavm ;
  JNIEnv* env ;
  JstDLHandle jvmDynlibHandle ;
} JstJvm ;

// TODO: creation and destroying functions
//       a type containing class and main method handle
//       a func to invoke the main method
//       a func to convert given strings to Java String[]. Or maybe just pass in c strings to the above func.


#if defined( _WIN32 )
// to have type DWORD in the func signature below we need this header
//#include "Windows.h"
// DWORD is defined as unsigned long, so we'll just use that
/** Prints an error message for the given windows error code. */
void jst_printWinError( unsigned long errcode ) ;

#endif

int jst_launchJavaApp( JavaLauncherOptions* options ) ;




// input parameter handling


/** returns an array of JstActualParam, the last one of which contains NULL for field param.
 * All the memory allocated can be freed by freeing the returned pointer.
 * Return NULL on error.
 * @param cygwinConvertParamsAfterTermination if cygwin compatibility is set in the build & cygwin1.dll is found and loaded,
 *                                            the terminating param (the param which w/ all the following params is passed to launchee)
 *                                            and all the following params are cygwin path converted with the given conversion type. */
JstActualParam* jst_processInputParameters( char** args, int numArgs, JstParamInfo *paramInfos, const char** terminatingSuffixes, CygwinConversionType cygwinConvertParamsAfterTermination ) ;

/** For single params, returns "" if the param is present, NULL otherwise. Note that you do not
 * need to try out all the aliases for a param - e.g. if -jh and --javahome stand for the same param,
 * you will get the value passed for the param even if you ask just for "-jh".
 * Note also that you can only query values for recognized params w/ this function, i.e.
 * values for params that have been defined in the JstParamInfo[] given to jst_processInputParameters func. */
char* jst_getParameterValue( const JstActualParam* processedParams, const char* paramName ) ;

char* jst_getParameterAfterTermination( const JstActualParam* processedParams, int indx ) ;

/** returns 0 iff the answer is no. */
int jst_isToBePassedToLaunchee( const JstActualParam* processedParam, JstUnrecognizedParamStrategy unrecognizedParamStrategy ) ;





/** Tries to find Java home by looking where java command is located on PATH. Freeing the returned value
 * is up to the caller.
 * errno != 0 on error. */
char* jst_findJavaHomeFromPath() ;

/** First sees if JAVA_HOME is set and points to an existing location (the validity is not checked).
 * Next, windows registry is checked (if on windows) or if on os-x the standard location
 * /System/Library/Frameworks/JavaVM.framework is checked for existence.
 * Last, java is looked up from the PATH.
 * This is just a composite function putting together the ways to search for a java installation
 * in a way that is very coomon. If you want to use a different order, please use the more
 * specific funtions, e.g. jst_findJavaHomeFromPath().
 * Returns NULL if java home could not be figured out. Freeing the returned value is up to the caller. */
char* jst_findJavaHome( JstActualParam* processedActualParams ) ;


/** As appendArrayItem, but specifically for jvm options.
 * @param extraInfo JavaVMOption.extraInfo. See jni.h or jni documentation (JavaVMOption is defined in jni.h). */
JavaVMOption* appendJvmOption( JavaVMOption* opts, int indx, size_t* optsSize, char* optStr, void* extraInfo ) ;

#if defined( __cplusplus )
  } // end extern "C"
#endif

#endif // ifndef _JVMSTARTER_H_
