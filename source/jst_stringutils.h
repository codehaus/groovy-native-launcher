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

#ifndef JST_STRINGUTILS_H_
#define JST_STRINGUTILS_H_

#if defined( __cplusplus )
  extern "C" {
#endif

/** returns != 0 iff str starts with the given prefix or prefix == NULL.
 * Empty prefix always matches. */
int jst_startsWith( const char* str, const char* prefix ) ;

/** returns != 0 iff str ends with the given suffix or suffix == NULL.
 * Empty suffix always matches. */
int jst_endsWith( const char* str, const char* suffix ) ;

/** prints each of the strings in strings to the given file using the given formatstring,
 * which defaults to just printing the string. strings may be NULL.
 */
void jst_printStringArray( FILE* file, char* formatstring, char** strings ) ;

#if defined( __cplusplus )
  } // end extern "C"
#endif


#endif /* JST_STRINGUTILS_H_ */
