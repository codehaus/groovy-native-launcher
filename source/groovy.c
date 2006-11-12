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

#include <jni.h>
#include "jvmstarter.h"

#define GROOVY_MAIN_CLASS "org/codehaus/groovy/tools/GroovyStarter"
// the name of the method can be something else, but it needs to be 
// static void and take a String[] as param
#define GROOVY_MAIN_METHOD "main"

#define GROOVY_START_JAR "groovy-starter.jar"
#define GROOVY_CONF "groovy-starter.conf"

#define MAX_GROOVY_PARAM_DEFS 20


int main(int argc, char** argv) {

  JavaLauncherOptions options;

  JavaVMOption extraJvmOptions[10];
  int          extraJvmOptionsCount = 0;
  
  ParamInfo paramInfos[MAX_GROOVY_PARAM_DEFS]; // big enough, make larger if necessary
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
  jboolean error                  = JNI_FALSE, 
           groovyConfGivenAsParam = JNI_FALSE;

  size_t len;
  int    i,
         numParamsToCheck,
         rval = -1; 

  // skip the program name
  argc--;
  argv++;

  // the parameters accepted by groovy (note that -cp / -classpath / --classpath & --conf 
  // are handled separately below
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-c",          DOUBLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "--encoding",  DOUBLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-h",          SINGLE_PARAM, 1);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "--help",      SINGLE_PARAM, 1);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-d",          SINGLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "--debug",     SINGLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-e",          DOUBLE_PARAM, 1);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-i",          DOUBLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-l",          DOUBLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-n",          SINGLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-p",          SINGLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "-v",          SINGLE_PARAM, 0);
  setParameterDescription(paramInfos, paramInfoCount++, MAX_GROOVY_PARAM_DEFS, "--version",   SINGLE_PARAM, 0);

  // look up the first terminating launchee param and only search for --classpath and --conf up to that   
  numParamsToCheck = findFirstLauncheeParamIndex(argv, argc, (char**)terminatingSuffixes, paramInfos, paramInfoCount);

  // look for classpath param
  // - If -cp is not given, then the value of CLASSPATH is given in groovy starter param --classpath. 
  // And if not even that is given, then groovy starter param --classpath is omitted.
  i = 0;
  while( (temp = cpaliases[i++]) ) {
    classpath = valueOfParam(argv, &argc, &numParamsToCheck, temp, DOUBLE_PARAM, JNI_TRUE, &error);
    if(error) {
      fprintf(stderr, "error: %s option must have a value\n", temp);
      goto end;
    }
    if(classpath) break;
  }

  if(!classpath) classpath = getenv("CLASSPATH");

  if(classpath) {
    extraProgramOptions[5] = classpath;
  } else {
    extraProgramOptions[4] = NULL; // omit the --classpath param
  }
  
  groovyConfFile = valueOfParam(argv, &argc, &numParamsToCheck, "--conf", DOUBLE_PARAM, JNI_TRUE, &error);
  if(error) {
    fprintf(stderr, "error: --conf must have a value\n");
    goto end;
  }
  
  if(groovyConfFile) groovyConfGivenAsParam = JNI_TRUE;

  groovyHome = getenv("GROOVY_HOME");
  if(!groovyHome || (len = strlen(groovyHome)) == 0) {
    fprintf(stderr, "GROOVY_HOME is not set\n");
    goto end;
  }
 
           // ghome path len + nul char + "lib" + 2 file seps + file name len
  groovyLaunchJar = malloc(len + 1 + 3 + 2 * strlen(FILE_SEPARATOR) + strlen(GROOVY_START_JAR));
          // ghome path len + nul + "conf" + 2 file seps + file name len
  if(!groovyConfGivenAsParam) groovyConfFile = malloc(len + 1 + 4 + 2 * strlen(FILE_SEPARATOR) + strlen(GROOVY_CONF));
  if( !groovyLaunchJar || (!groovyConfGivenAsParam && !groovyConfFile) ) {
    fprintf(stderr, "error: out of memory when allocating space for directory names\n");
    goto end;
  }

  strcpy(groovyLaunchJar, groovyHome);
  strcat(groovyLaunchJar, FILE_SEPARATOR "lib" FILE_SEPARATOR GROOVY_START_JAR);

  jars[0] = groovyLaunchJar;
  
  // set -Dgroovy.home and -Dgroovy.starter.conf as jvm options

  if(!groovyConfGivenAsParam) { // set the default groovy conf file if it was not given as a parameter
    strcpy(groovyConfFile, groovyHome);
    strcat(groovyConfFile, FILE_SEPARATOR "conf" FILE_SEPARATOR GROOVY_CONF);
  }
  extraProgramOptions[3] = groovyConfFile;
  
  // "-Dgroovy.starter.conf=" => 22 + 1 for nul char
  groovyDConf = malloc(          22 + 1 + strlen(groovyConfFile) );
  // "-Dgroovy.home=" => 14 + 1 for nul char 
  groovyDHome = malloc(  14 + 1 + strlen(groovyHome) );
  if( !groovyDConf || !groovyDHome ) {
    fprintf(stderr, "error: out of memory when allocating space for params\n");
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

  
  // populate the startup parameters

  options.java_home           = NULL; // let the launcher func figure it out
  options.toolsJarHandling    = TOOLS_JAR_TO_SYSPROP;
  options.javahomeHandling    = ALLOW_JH_ENV_VAR_LOOKUP|ALLOW_JH_PARAMETER; 
  options.classpathHandling   = IGNORE_GLOBAL_CP; 
  options.arguments           = argv;
  options.numArguments        = argc;
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


  rval = launchJavaApp(&options);

end:
  if(groovyLaunchJar) free(groovyLaunchJar);
  if(groovyConfFile && !groovyConfGivenAsParam)  free(groovyConfFile);
  if(groovyDConf)     free(groovyDConf);
  if(groovyDHome)     free(groovyDHome);
  return rval;
}

