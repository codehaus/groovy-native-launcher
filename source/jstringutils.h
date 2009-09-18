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

#include "jni.h"

#ifndef JST_JSTRINGUTILS_H_
#  define JST_JSTRINGUTILS_H_

#if defined( __cplusplus )
  extern "C" {
#endif

/** Returns false on error. */
jboolean addStringToJStringArray( JNIEnv* env, char *strToAdd, jobjectArray jstrArr, jint ind ) ;

/** @param strings must be NULL terminated. May be NULL.
 * @param indx start at this index
 * @return != 0 on error */
int addStringsToJavaStringArray( JNIEnv* env, jobjectArray jstrings, char** strings, jint indx ) ;


#if defined( __cplusplus )
  } // end extern "C"
#endif

#endif /* JST_JSTRINGUTILS_H_ */
