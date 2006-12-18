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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if defined ( __APPLE__ )
#include <TargetConditionals.h>
#endif

#include <jni.h>

#include "jvmstarter.h"

#define GROOVY_MAIN_CLASS "org/codehaus/groovy/tools/GroovyStarter"
// the name of the method can be something else, but it needs to be 
// static void and take a String[] as param
#define GROOVY_MAIN_METHOD "main"

#define GROOVY_START_JAR "groovy-starter.jar"
#define GROOVY_CONF "groovy-starter.conf"

#define MAX_GROOVY_PARAM_DEFS 20
#define MAX_GROOVY_JVM_EXTRA_ARGS 3

static jboolean _groovy_launcher_debug = JNI_FALSE;

/** returns null on error, otherwise pointer to groovy home. 
 * First tries to see if the current executable is located in a groovy installation's bin directory. If not, groovy
 * home is looked up. If neither succeed, an error msg is printed.
 * Do NOT modify the returned string, make a copy. */
char* getGroovyHome() {
  static char *_ghome = NULL;
  char *appHome, *gstarterJar = NULL;
  size_t len;
       
  if(_ghome) return _ghome;
  
  appHome = jst_getExecutableHome();
  if(!appHome) return NULL;
  
  len = strlen(appHome);
  // "bin" == 3 chars
  if(len > (3 + 2 * strlen(JST_FILE_SEPARATOR))) {
    int starterJarExists; 
    
    _ghome = malloc((len + 1) * sizeof(char));
    if(!_ghome) {
      fprintf(stderr, "error: out of memory when figuring out groovy home\n");
      return NULL;
    }
    strcpy(_ghome, appHome);
    _ghome[len - 2 * strlen(JST_FILE_SEPARATOR) - 3] = '\0';
    // check that lib/groovy-starter.jar (== 22 chars) exists
    gstarterJar = malloc(len + 22);
    if(!gstarterJar) {
      fprintf(stderr, "error: out of memory when figuring out groovy home\n");
      return NULL;
    }
    strcpy(gstarterJar, _ghome);
    strcat(gstarterJar, JST_FILE_SEPARATOR "lib" JST_FILE_SEPARATOR GROOVY_START_JAR);
    starterJarExists = jst_fileExists(gstarterJar);
    free(gstarterJar);
    if(starterJarExists) {
      return _ghome = realloc(_ghome, len + 1); // shrink the buffer as there is extra space
    } else {
      free(_ghome);
    }
  }
  
  _ghome = getenv("GROOVY_HOME");
  if(!_ghome) {
    fprintf(stderr, (len == 0) ? "error: could not find groovy installation - please set GROOVY_HOME environment variable to point to it\n" :
                                 "error: could not find groovy installation - either the binary must reside in groovy installation's bin dir or GROOVY_HOME must be set\n");
  } else if(_groovy_launcher_debug) {
    fprintf(stderr, (len == 0) ? "warning: figuring out groovy installation directory based on the executable location is not supported on your platform, "
                                    "using GROOVY_HOME environment variable\n"
                               : "warning: the groovy executable is not located in groovy installation's bin directory, resorting to using GROOVY_HOME\n");
  }

  return _ghome;
}

int main(int argc, char** argv) {

  JavaLauncherOptions options;

  JavaVMOption extraJvmOptions[MAX_GROOVY_JVM_EXTRA_ARGS];
  int          extraJvmOptionsCount = 0;
  
  JstParamInfo paramInfos[MAX_GROOVY_PARAM_DEFS]; // big enough, make larger if necessary
  int       paramInfoCount = 0;

  char *groovyLaunchJar = NULL, 
       *groovyConfFile  = NULL, 
       *groovyDConf     = NULL, // the -Dgroovy.conf=something to pass to the jvm
       *groovyHome      = NULL, 
       *groovyDHome     = NULL, // the -Dgroovy.home=something to pass to the jvm
       *classpath       = NULL,
       *temp;
        
  char *terminatingSuffixes[] = {".groovy", ".gvy", ".gy", ".gsh", NULL},
       *extraProgramOptions[] = {"--main", "groovy.ui.GroovyMain", "--conf", NULL, "--classpath", NULL, NULL}, 
       *jars[]                = {NULL, NULL}, 
       *cpaliases[]           = {"-cp", "-classpath", "--classpath", NULL};

  // the argv will be copied into this - we don't modify the param, we modify a local copy
  char **args = NULL;
  int  numArgs = argc - 1;
         
  size_t len;
  int    i,
         numParamsToCheck,
         rval = -1; 

  jboolean error                  = JNI_FALSE, 
           groovyConfGivenAsParam = JNI_FALSE,
           displayHelp            = ( (numArgs == 0)                   || 
                                      (strcmp(argv[1], "-h")     == 0) || 
                                      (strcmp(argv[1], "--help") == 0)
                                    ) ? JNI_TRUE : JNI_FALSE; 
         
         
  // the parameters accepted by groovy (note that -cp / -classpath / --classpath & --conf 
  // are handled separately below
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-c",          JST_DOUBLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "--encoding",  JST_DOUBLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-h",          JST_SINGLE_PARAM, 1);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "--help",      JST_SINGLE_PARAM, 1);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-d",          JST_SINGLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "--debug",     JST_SINGLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-e",          JST_DOUBLE_PARAM, 1);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-i",          JST_DOUBLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-l",          JST_DOUBLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-n",          JST_SINGLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-p",          JST_SINGLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-v",          JST_SINGLE_PARAM, 0);
  jst_setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "--version",   JST_SINGLE_PARAM, 0);

  assert(paramInfoCount <= MAX_GROOVY_PARAM_DEFS);
  
  if( !( args = malloc( numArgs * sizeof(char*) ) ) ) {
    fprintf(stderr, "error: out of memory at startup\n");
    goto end;
  }
  for(i = 0; i < numArgs; i++) args[i] = argv[i + 1]; // skip the program name
  
  // look up the first terminating launchee param and only search for --classpath and --conf up to that   
  numParamsToCheck = jst_findFirstLauncheeParamIndex(args, numArgs, (char**)terminatingSuffixes, paramInfos, paramInfoCount);

  _groovy_launcher_debug = (jst_valueOfParam(args, &numArgs, &numParamsToCheck, "-d", JST_SINGLE_PARAM, JNI_FALSE, &error) ? JNI_TRUE : JNI_FALSE);
  
  // look for classpath param
  // - If -cp is not given, then the value of CLASSPATH is given in groovy starter param --classpath. 
  // And if not even that is given, then groovy starter param --classpath is omitted.
  for(i = 0; (temp = cpaliases[i++]); ) {
    classpath = jst_valueOfParam(args, &numArgs, &numParamsToCheck, temp, JST_DOUBLE_PARAM, JNI_TRUE, &error);
    if(error) goto end;
    if(classpath) break;
  }

  if(!classpath) classpath = getenv("CLASSPATH");

  if(classpath) {
    extraProgramOptions[5] = classpath;
  } else {
    extraProgramOptions[4] = NULL; // omit the --classpath param
  }
  
  groovyConfFile = jst_valueOfParam(args, &numArgs, &numParamsToCheck, "--conf", JST_DOUBLE_PARAM, JNI_TRUE, &error);
  if(error) goto end;
  
  
  if(groovyConfFile) groovyConfGivenAsParam = JNI_TRUE;

  groovyHome = getGroovyHome();
  if(!groovyHome) goto end;
  
  len = strlen(groovyHome);
 
           // ghome path len + nul char + "lib" + 2 file seps + file name len
  groovyLaunchJar = malloc(len + 1 + 3 + 2 * strlen(JST_FILE_SEPARATOR) + strlen(GROOVY_START_JAR));
          // ghome path len + nul + "conf" + 2 file seps + file name len
  if(!groovyConfGivenAsParam) groovyConfFile = malloc(len + 1 + 4 + 2 * strlen(JST_FILE_SEPARATOR) + strlen(GROOVY_CONF));
  if( !groovyLaunchJar || (!groovyConfGivenAsParam && !groovyConfFile) ) {
    fprintf(stderr, "error: out of memory when allocating space for dir names\n");
    goto end;
  }

  strcpy(groovyLaunchJar, groovyHome);
  strcat(groovyLaunchJar, JST_FILE_SEPARATOR "lib" JST_FILE_SEPARATOR GROOVY_START_JAR);

  jars[0] = groovyLaunchJar;
  
  // set -Dgroovy.home and -Dgroovy.starter.conf as jvm options

  if(!groovyConfGivenAsParam) { // set the default groovy conf file if it was not given as a parameter
    strcpy(groovyConfFile, groovyHome);
    strcat(groovyConfFile, JST_FILE_SEPARATOR "conf" JST_FILE_SEPARATOR GROOVY_CONF);
  }
  extraProgramOptions[3] = groovyConfFile;
  
  // "-Dgroovy.starter.conf=" => 22 + 1 for nul char
  groovyDConf = malloc(          22 + 1 + strlen(groovyConfFile) );
  // "-Dgroovy.home=" => 14 + 1 for nul char 
  groovyDHome = malloc(  14 + 1 + strlen(groovyHome) );
  if( !groovyDConf || !groovyDHome ) {
    fprintf(stderr, "error: out of memory when allocating space for groovy jvm params\n");
    goto end;
  }
  strcpy(groovyDConf, "-Dgroovy.starter.conf=");
  strcat(groovyDConf, groovyConfFile);

  extraJvmOptions[extraJvmOptionsCount].optionString = groovyDConf; 
  extraJvmOptions[extraJvmOptionsCount++].extraInfo = NULL;

  strcpy(groovyDHome, "-Dgroovy.home=");
  strcat(groovyDHome, groovyHome);

  extraJvmOptions[extraJvmOptionsCount].optionString = groovyDHome; 
  extraJvmOptions[extraJvmOptionsCount++].extraInfo = NULL;

  assert(extraJvmOptionsCount <= MAX_GROOVY_JVM_EXTRA_ARGS);
  
  // populate the startup parameters

  options.java_home           = NULL; // let the launcher func figure it out
  options.toolsJarHandling    = JST_TOOLS_JAR_TO_SYSPROP;
  options.javahomeHandling    = JST_ALLOW_JH_ENV_VAR_LOOKUP|JST_ALLOW_JH_PARAMETER; 
  options.classpathHandling   = JST_IGNORE_GLOBAL_CP; 
  options.arguments           = args;
  options.numArguments        = numArgs;
  options.jvmOptions          = extraJvmOptions;
  options.numJvmOptions       = extraJvmOptionsCount;
  options.extraProgramOptions = extraProgramOptions;
  options.mainClassName       = GROOVY_MAIN_CLASS;
  options.mainMethodName      = GROOVY_MAIN_METHOD;
  options.jarDirs             = NULL;
  options.jars                = jars;
  options.paramInfos          = paramInfos;
  options.paramInfosCount     = paramInfoCount;
  options.terminatingSuffixes = terminatingSuffixes;


  rval = jst_launchJavaApp(&options);

  if(displayHelp) { // add to the standard groovy help message
    fprintf(stderr, "\n"
    " -jh,--javahome <path to jdk/jre> makes groovy use the given jdk/jre\n" 
    "                                 instead of the one pointed to by JAVA_HOME\n"
    " --conf <conf file>              use the given groovy conf file\n"
    "\n"
    " -cp,-classpath,--classpath <user classpath>\n" 
    "                                 the classpath to use\n"
    " -client/-server                 to use a client/server VM (aliases for these\n"
    "                                 are not supported)\n"
    "\n"
    "In addition, you can give any parameters accepted by the jvm you are using, e.g.\n"
    "-Xmx<size> (see java -help and java -X for details)\n"
    "\n"
    );
  }
  
end:
  if(args)            free(args);
  if(groovyLaunchJar) free(groovyLaunchJar);
  if(groovyConfFile && !groovyConfGivenAsParam)  free(groovyConfFile);
  if(groovyDConf)     free(groovyDConf);
  if(groovyDHome)     free(groovyDHome);
  return rval;
}
