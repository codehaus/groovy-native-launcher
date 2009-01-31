//  A library for easy creation of a native launcher for Java applications.
//
//  Copyright (c) 2009 Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
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

#include <string.h>

extern int jst_startsWith( const char* str, const char* prefix ) {
  size_t prefixLen ;

  if ( !prefix || !*prefix ) return 1 ;
  prefixLen = strlen( prefix ) ;
  return memcmp( str, prefix, prefixLen ) == 0 ;
}

extern int jst_endsWith( const char* str, const char* suffix ) {
  size_t suffixLen,
         strLength ;

  if ( !suffix || !*suffix ) return 1 ;
  suffixLen = strlen( suffix ) ;
  strLength = strlen( str ) ;
  return strLength >= suffixLen && memcmp( str + strLength - suffixLen, suffix, suffixLen ) == 0 ;
}

