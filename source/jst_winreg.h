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

#if defined( _WIN32 )

#  if !defined( _JST_WINREG_H_ )
#    define _JST_WINREG_H_

#    if defined( __cplusplus )
       extern "C" {
#    endif


/** errno != 0 on error. Note that not finding java home from registry is not considered an error. */
char* jst_findJavaHomeFromWinRegistry() ;

#    if defined( __cplusplus )
       }
#    endif


#  endif
  
#endif

