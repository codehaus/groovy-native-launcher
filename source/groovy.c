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

#include "jni.h"
#include "jvmstarter.h"

#define GROOVY_MAIN_CLASS "org/codehaus/groovy/tools/GroovyStarter"
// the name of the method can be something else, but it needs to be 
// static void and take a String[] as param
#define GROOVY_MAIN_METHOD "main"

#define GROOVY_START_JAR "groovy-starter.jar"
#define GROOVY_CONF "groovy-starter.conf"

#define GROOVY_START_CLASS "groovy.ui.GroovyMain"


int main(int argc, char** argv) {

  JavaLauncherOptions options;

  JavaVMOption extraJvmOptions[10];
  int          extraJvmOptionsCount = 0;
  
  ParamInfo paramInfos[20]; // big enough, make larger if necessary
  int       paramInfoCount = 0;

  char *groovyLaunchJar = NULL, 
       *groovyConfFile  = NULL, 
       *groovyDConf     = NULL,
       *groovyHome      = NULL, 
       *groovyDHome     = NULL;
        
  char *terminatingSuffixes[] = {".groovy", NULL},
       *extraProgramOptions[] = {"--main", GROOVY_START_CLASS, "--conf", NULL, NULL}, 
       *jars[]                = {NULL, NULL};

  size_t len;
  int    rval = -1; 

  
  // the parameters accepted by groovy

  paramInfos[paramInfoCount].name = "-cp";
  paramInfos[paramInfoCount].type = DOUBLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "-classpath";
  paramInfos[paramInfoCount].type = DOUBLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "--classpath";
  paramInfos[paramInfoCount].type = DOUBLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "-c";
  paramInfos[paramInfoCount].type = DOUBLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "--encoding";
  paramInfos[paramInfoCount].type = DOUBLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "-h";
  paramInfos[paramInfoCount].type = SINGLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 1;

  paramInfos[paramInfoCount].name = "--help";
  paramInfos[paramInfoCount].type = SINGLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 1;

  paramInfos[paramInfoCount].name = "-d";
  paramInfos[paramInfoCount].type = SINGLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "--debug";
  paramInfos[paramInfoCount].type = SINGLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "-e";
  paramInfos[paramInfoCount].type = DOUBLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 1;

  paramInfos[paramInfoCount].name = "-i";
  paramInfos[paramInfoCount].type = DOUBLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "-l";
  paramInfos[paramInfoCount].type = DOUBLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "-n";
  paramInfos[paramInfoCount].type = SINGLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "-p";
  paramInfos[paramInfoCount].type = SINGLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "-v";
  paramInfos[paramInfoCount].type = SINGLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  paramInfos[paramInfoCount].name = "--version";
  paramInfos[paramInfoCount].type = SINGLE_PARAM;
  paramInfos[paramInfoCount++].terminating = 0;

  // add groovy's lib dir as the sole dir from where to append all jars

  groovyHome = getenv("GROOVY_HOME");
  if(!groovyHome || (len = strlen(groovyHome)) == 0) {
    fprintf(stderr, "GROOVY_HOME is not set\n");
    goto end;
  }
  
                           // len + nul char + "lib" + 2 file seps + file name len
  if(!(groovyLaunchJar = malloc(len + 1 + 3 + 2 * strlen(FILE_SEPARATOR) + strlen(GROOVY_START_JAR)))
    ) {
    fprintf(stderr, "error: out of memory when allocating space for directory name\n");
    goto end;
  }

  groovyLaunchJar[0] = 0;
  strcat(groovyLaunchJar, groovyHome);
  strcat(groovyLaunchJar, FILE_SEPARATOR);
  strcat(groovyLaunchJar, "lib");
  strcat(groovyLaunchJar, FILE_SEPARATOR);
  strcat(groovyLaunchJar, GROOVY_START_JAR);

  jars[0] = groovyLaunchJar;

  if(!(groovyConfFile = malloc(len + 1 + 4 + 2 * strlen(FILE_SEPARATOR) + strlen(GROOVY_CONF)))
    ) {
    fprintf(stderr, "error: out of memory when allocating space for directory name\n");
    goto end;
  }

  
  // set -Dgroovy.home and -Dgroovy.starter.conf as jvm options
  
  groovyConfFile[0] = 0;
  strcat(groovyConfFile, groovyHome);
  strcat(groovyConfFile, FILE_SEPARATOR);
  strcat(groovyConfFile, "conf");
  strcat(groovyConfFile, FILE_SEPARATOR);
  strcat(groovyConfFile, GROOVY_CONF);
  extraProgramOptions[3] = groovyConfFile;

  if(!(groovyDConf = malloc(22 + strlen(groovyConfFile) + 1))
    || !(groovyDHome = malloc(15 + strlen(groovyHome))) ) {
    fprintf(stderr, "error: out of memory when allocating space for params\n");
    goto end;
  }
  groovyDConf[0] = 0;
  strcat(groovyDConf, "-Dgroovy.starter.conf=");
  strcat(groovyDConf, groovyConfFile);

  extraJvmOptions[extraJvmOptionsCount].optionString = groovyDConf; 
  extraJvmOptions[extraJvmOptionsCount++].extraInfo = NULL;

  groovyDHome[0] = 0;
  strcat(groovyDHome, "-Dgroovy.home=");
  strcat(groovyDHome, groovyHome);

  extraJvmOptions[extraJvmOptionsCount].optionString = groovyDHome; 
  extraJvmOptions[extraJvmOptionsCount++].extraInfo = NULL;

  
  // populate the startup parameters

  options.java_home           = NULL; // let the launcher func figure it out
  options.toolsJarHandling    = TOOLS_JAR_TO_SYSPROP;
  options.javahomeHandling    = ALLOW_JH_ENV_VAR_LOOKUP|ALLOW_JH_PARAMETER; 
  options.classpathHandling   = IGNORE_GLOBAL_CP|CP_PARAM_TO_APP; 
  options.arguments           = argv + 1;
  options.numArguments        = argc - 1;
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
    if(groovyConfFile)  free(groovyConfFile);
    if(groovyDConf)     free(groovyDConf);
    if(groovyDHome)     free(groovyDHome);
    return rval;
}
