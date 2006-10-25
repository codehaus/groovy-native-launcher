//  Groovy -- A native launcher for Groovy
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

#if defined(win)

#define FILE_SEPARATOR "\\"
#define PATH_SEPARATOR ";"


#else

#define FILE_SEPARATOR "/"
#define PATH_SEPARATOR ":"

#endif

typedef enum { 
  /** a standalone parameter, such as -v */
  SINGLE_PARAM,
  /** a parameter followed by some additional info, e.g. --endcoding UTF8 */
  DOUBLE_PARAM,
  /** a parameter that its value attached, e.g. param name "-D", that could be given on command line as -Dfoo */
  PREFIX_PARAM
} ParamClass;

typedef struct {
  char *name;
  /** If != 0, the actual parameters followed by this one are passed on to the launchee. */
  short  terminating;  
  ParamClass type;
} ParamInfo;

// classpath handling constants
/** if neiyher of these first two is given, CLASSPATH value is always appended to jvm classpath */
#define IGNORE_GLOBAL_CP 1
/** global CLASSPATH is appended to jvm startup classpath only if -cp / --classpath is not given */
#define IGNORE_GLOBAL_CP_IF_PARAM_GIVEN 2
/** this can be given w/ the following */
#define CP_PARAM_TO_APP 4
/**  */
#define CP_PARAM_TO_JVM 8


// javahome handling constants
/** If given java_home == null, try to look it up from JAVA_HOME environment variable. */
#define ALLOW_JH_ENV_VAR_LOOKUP 1
/** Allow giving java home as -jh / --javahome parameter. The precedence is 
 * -jh command line parameter, java_home argument and then JAVA_HOME env var (if allowed). */
#define ALLOW_JH_PARAMETER 2

#define IGNORE_TOOLS_JAR 0
#define TOOLS_JAR_TO_CLASSPATH 1
#define TOOLS_JAR_TO_SYSPROP 2

 /** Note that if you tell that -cp / --classpath and/or -jh / --javahome params are handled automatically. 
  * If you do not want the user to be able to affect 
  * javahome, specify these two as double params and their processing is up to you. 
  */
typedef struct {
  /** May be null. */
  char* java_home; 
  /** what to do about tools.jar */
  int toolsJarHandling;
  /** See the constants above. */
  int javahomeHandling; 
  /** See the constants above. */
  int classpathHandling; 
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
  ParamInfo* paramInfos;
  int        paramInfosCount;
  /** terminatingSuffixes contains the suffixes that, if matched, indicate that the matching param and all the rest of the params 
   * are launcheeParams, e.g. {".groovy", "-e", "-v", NULL} */
  char** terminatingSuffixes;
} JavaLauncherOptions;


extern int launchJavaApp(JavaLauncherOptions* options);

void setParameterDescription(ParamInfo* paramInfo, int index, int size, char* name, ParamClass type, short terminating);

