//  A library for easy creation of a native launcher for Java applications.
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

#include "applejnifix.h"
#include <jni.h>
#include "jniutils.h"
#include <string.h>

static jmethodID getStringConstructorB( JNIEnv* env ) {

  static jmethodID _stringConstructor = NULL ;

  if ( !_stringConstructor ) {
    jclass stringClass = getJavaStringClass( env ) ;
    if ( !stringClass ) return NULL ;
    _stringConstructor = (*env)->GetMethodID( env, stringClass, "<init>", "([B)V" ) ;
    if ( !_stringConstructor ) {
      clearException( env ) ;
    }
  }
  return _stringConstructor ;
}




static jstring createJStringFromPlatformEncodedCString( JNIEnv* env, const char* stringInPlatformDefaultEncoding ) {
  jstring jstr = NULL ;
  size_t  len  = strlen( stringInPlatformDefaultEncoding ) ;
  jbyteArray bytes ;
  jmethodID stringConstructorB = getStringConstructorB( env ) ;
  jclass stringClass = getJavaStringClass( env ) ;

  if ( !stringConstructorB || !stringClass ) {
    clearException( env ) ;
    return NULL ;
  }

  bytes = (*env)->NewByteArray( env, len ) ;
  if ( bytes ) {

    (*env)->SetByteArrayRegion( env, bytes, 0, len, (jbyte*)stringInPlatformDefaultEncoding ) ;

    jstr = (*env)->NewObject( env, stringClass, stringConstructorB, bytes ) ;

    if ( !jstr ) {
      fprintf( stderr, "error: could not convert %s to java string\n", stringInPlatformDefaultEncoding ) ;
      clearException( env ) ;
    }

    (*env)->DeleteLocalRef( env, bytes ) ;
  } else {
    clearException( env ) ;
  }

  return jstr ;

}


extern jboolean addStringToJStringArray( JNIEnv* env, char *strToAdd, jobjectArray jstrArr, jint ind ) {
  jboolean success = JNI_FALSE ;
  jstring  arg     = createJStringFromPlatformEncodedCString( env, strToAdd ) ;

  if ( !arg ) return JNI_FALSE ;

  (*env)->SetObjectArrayElement( env, jstrArr, ind, arg ) ;
  if ( (*env)->ExceptionCheck( env ) ) {
    fprintf( stderr, "error: error when writing %dth element %s to Java String[]\n", (int)ind, strToAdd ) ;
    clearException( env ) ;
  } else {
    success = JNI_TRUE ;
  }

  (*env)->DeleteLocalRef( env, arg ) ;

  return success ;
}

/** @param strings must be NULL terminated. May be NULL.
 * @param indx start at this index
 * @return != 0 on error */
extern int addStringsToJavaStringArray( JNIEnv* env, jobjectArray jstrings, char** strings, jint indx ) {

  int errorOccurred = 0 ;

  if ( !strings ) return 0 ;

  for ( ; *strings ; strings++ ) {
    if ( ( errorOccurred = !addStringToJStringArray( env, *strings, jstrings, indx++ ) ) ) {
      break ;
    }
  }

  return errorOccurred ;

}
