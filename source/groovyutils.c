//  Groovy -- A native launcher for Groovy
//
//  Copyright (c) 2010 Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
//
//  Licensed under the Apache License, Version 2.0 (the "License") ; you may not use this file except in
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <jni.h>

#include "jst_stringutils.h"

extern int gantJarSelect( const char* dirName, const char* fileName ) {

  int    result      = JNI_FALSE ;
  size_t fileNameLen = strlen( fileName ) ;

  // we are looking for gant-[0-9]+\.+[0-9]+.*\.jar or
  // gant_groovy[0-9]+.[0-9]+- [0-9]+.[0-9]+[0-9]+.*\.jar
  // As regexes aren't available, we'll just make some trivial checks

  if ( fileNameLen >= 10 &&
       memcmp( "gant", fileName, 4 ) == 0 &&
       strcmp( ".jar", fileName + ( fileNameLen - 4 ) ) == 0 ) {

    switch ( fileName[ 4 ] ) {

      case '-' :
        result = isdigit( fileName[ 5 ] ) ;
        break ;

      case '_' :
        result = fileNameLen >= 16 &&
                 ( memcmp( "groovy", fileName + 5, 6 ) == 0 ) &&
                 isdigit( fileName[ 11 ] ) ;
        break ;
    }

  }

  return result ;
}

extern int groovyJarSelectForGant( const char* dirName, const char* fileName ) {

  int result = strcmp( "groovy-starter.jar", fileName ) == 0 ;

  if ( !result ) result = jst_startsWith( fileName, "groovy-all-" ) ;

  if ( !result ) {
    size_t fileNameLen = strlen( fileName ) ;
    // we are looking for groovy-[0-9]+\.+[0-9]+.*\.jar. As regexes
    // aren't available, we'll just check that the char following
    // groovy- is a digit
    if ( fileNameLen >= 12 ) result = isdigit( fileName[ 7 ] ) ;
  }

  return result ;

}
