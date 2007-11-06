//  A simple library for creating a native launcher for a java app
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


#if !defined( _JVMSTARTER_H_ )

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

typedef enum { 
  /** a standalone parameter, such as -v */
  JST_SINGLE_PARAM,
  /** a parameter followed by some additional info, e.g. --endcoding UTF8 */
  JST_DOUBLE_PARAM,
  /** a parameter that its value attached, e.g. param name "-D", that could be given on command line as -Dfoo */
  JST_PREFIX_PARAM
} JstParamClass;

typedef struct {
  char          *name;
  /** If != 0, the actual parameters followed by this one are passed on to the launchee. */
  short         terminating;  
  JstParamClass type;
} JstParamInfo;

typedef enum {
// classpath handling constants
/** if neiyher of these first two is given, CLASSPATH value is always appended to jvm classpath */
  JST_IGNORE_GLOBAL_CP = 1,
/** global CLASSPATH is appended to jvm startup classpath only if -cp / --classpath is not given */
  JST_IGNORE_GLOBAL_CP_IF_PARAM_GIVEN = 2,
/** this can be given w/ the following */
  JST_CP_PARAM_TO_APP = 4,
/** pass the given cp param to jvm startup classpath */
  JST_CP_PARAM_TO_JVM = 8
} ClasspathHandling ;

/** The strategy to use when selecting between using client and server jvm *if* no explicit -client/-server param. If explicit param
 * is given, then that type of jvm is used, period.
 *  is given by the user. Bitwise meaning:
 *  001 -> allow using client jvm
 *  010 -> allow using server jvm
 *  100 -> prefer client over server
 * If you're just passing this as a param, you need not worry about the bitwise meaning, it's just an implementation detail. */
typedef enum { 
  JST_CLIENT_FIRST     = 7,
  JST_SERVER_FIRST     = 6,
  JST_TRY_CLIENT_ONLY  = 1,
  JST_TRY_SERVER_ONLY  = 2
} JVMSelectStrategy ;

/** javahome handling constants.
 * In order of precedence: 
 *  - programmatically given jh 
 *  - jh user parameter 
 *  - JAVA_HOME env var 
 *  - java home deduced from java executable found on path 
 *  - win registry (ms windows only) / "/System/Library/Frameworks/JavaVM.framework" (os-x only)
 */
typedef enum {
  JST_USE_ONLY_GIVEN_JAVA_HOME = 0,
/** If given java_home == null, try to look it up from JAVA_HOME environment variable. */
  JST_ALLOW_JH_ENV_VAR_LOOKUP = 1,
/** Allow giving java home as -jh / --javahome parameter. The precedence is 
 * -jh command line parameter, java_home argument and then JAVA_HOME env var (if allowed). */
  JST_ALLOW_JH_PARAMETER = 2,
/** Check windows registry when looking for java home. */
  JST_ALLOW_REGISTRY_LOOKUP = 4,
  /** Check if java home can be found by inspecting where java executable is located on the path. */ 
  JST_ALLOW_PATH_LOOKUP     = 8
} JavaHomeHandling ;

// these can be or:d together w/ |
typedef enum {
  JST_IGNORE_TOOLS_JAR       = 0,
  JST_TOOLS_JAR_TO_CLASSPATH = 1,
  JST_TOOLS_JAR_TO_SYSPROP = 2
} ToolsJarHandling ;
 /** Note that if you tell that -cp / --classpath and/or -jh / --javahome params are handled automatically. 
  * If you do not want the user to be able to affect 
  * javahome, specify these two as double params and their processing is up to you. 
  */
typedef struct {
  /** May be null. */
  char* java_home ;
  /** what kind of jvm to use. */
  JVMSelectStrategy jvmSelectStrategy ;
  /** The name of the env var where to take extra jvm params from. May be NULL. */
  char* javaOptsEnvVar ;
  /** what to do about tools.jar */
  ToolsJarHandling toolsJarHandling ;
  /** See the constants above. */
  JavaHomeHandling javahomeHandling ; 
  /** See the constants above. */
  ClasspathHandling classpathHandling ; 
  /** The arguments the user gave. Usually just give argv + 1 */
  char** arguments; 
  /** The arguments the user gave. Usually just give argv - 1 */
  int numArguments;
  /** extra params to the jvm (in addition to those extracted from arguments above). */
  JavaVMOption* jvmOptions;
  int numJvmOptions; 
  /** parameters to the java class' main method. These are put first before the command line args. */
  char** extraProgramOptions;
  char* mainClassName;
  char* mainMethodName;
  /** The directories from which add all jars from to the startup classpath. NULL terminates the list. */
  char** jarDirs;
  char** jars;
  /** parameterInfos is an array containing info about all the possible program params. */
  JstParamInfo* paramInfos;
  int           paramInfosCount;
  /** terminatingSuffixes contains the suffixes that, if matched, indicate that the matching param and all the rest of the params 
   * are launcheeParams, e.g. {".groovy", "-e", "-v", NULL} */
  char** terminatingSuffixes;
} JavaLauncherOptions;


extern int jst_launchJavaApp(JavaLauncherOptions* options);

extern int jst_fileExists(const char* fileName);

/** Returns the path to the directory where the current executable lives, including the last path separator, e.g.
 * c:\programs\groovy\bin\ or /usr/loca/groovy/bin/ 
 * The trailing file separator is included, even though this is a little inconsistent w/ how e.g. dir names are usually stored
 * in environment variables (w/out the trailing file separator). This is because in the worst case the directory the executable
 * resides in may be the root dir, and in that case stripping the trailing file separator would be confusing. 
 * Do NOT modify the returned string, make a copy. 
 * Note: there seems to be no standard way to implement this, so it needs to be figured out for each os supported. We have not yet been
 * able to support all target oses, so do not make code that relies on this func. In case there is no support, the func 
 * returns an empty string (""). Returns NULL on error. */
extern char* jst_getExecutableHome();

/** Returns pointer to the param info array that may be relocated. NULL if error occurs. */
JstParamInfo* jst_setParameterDescription( JstParamInfo** paramInfo, int indx, size_t* size, char* name, JstParamClass type, short terminating ) ;

/** Returns the index of the given str in the given str array, -1 if not found.  
 * Modifies args, numargs and checkUpto if removeIfFound == true */
int jst_contains(char** args, int* numargs, const char* option, const jboolean removeIfFound);

/** may return argc if none of the presented params are "terminating", i.e. indicate that it and all the rest of the params
 * go to the launchee. */
int jst_findFirstLauncheeParamIndex(char** argv, int argc, char** terminatingSuffixes, JstParamInfo* paramInfos, int paramInfosCount);


/** returns null if not found. For prefix params, returns the value w/out the prefix.
 * paramType is double or prefix.
 * In case of double param w/ no value, error out param is set to true. */
char* jst_valueOfParam(char** args, int* numargs, int* checkUpto, const char* option, const JstParamClass paramType, const jboolean removeIfFound, jboolean* error);

/** returns -1 if not found */
int jst_indexOfParam( char** args, int numargs, char* paramToSearch) ;

/** Appends the given strings to target. size param tells the current size of target (target must have been
 * dynamically allocated, i.e. not from stack). If necessary, target is reallocated into a bigger space. 
 * Returns the possibly new location of target, and modifies the size inout parameter accordingly. 
 * If target is NULL, it is allocated w/ the given size (or bigger if given size does not fit all the given strings). */
char* jst_append( char* target, size_t* size, ... ) ; 

/** If array is NULL, a new one will be created, size arlen. */
void* appendArrayItem( void* array, int index, size_t* arlen, void* item, int item_size_in_bytes ) ;

/** As the previous, but specifically for jvm options. 
 * @param extraInfo JavaVMOption.extraInfo. See jni.h or jni documentation (JavaVMOption is defined in jni.h). */
JavaVMOption* appendJvmOption( JavaVMOption* opts, int index, size_t* optsSize, char* optStr, void* extraInfo ) ;

#if defined( __cplusplus )
  } // end extern "C"
#endif

#endif // ifndef _JVMSTARTER_H_

