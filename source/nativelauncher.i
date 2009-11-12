//
//  Copyright (c) 2006,2009 Antti Karanta (Antti dot Karanta (at) hornankuusi dot fi)
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
//  $Revision: 17553 $
//  $Date: 2009-09-07 05:31:36 +0300 (ma, 07 syys 2009) $

// This file contains swig wrapping definitions for select c functions in the native launcher so they 
// can be unit tested from python

%module nativelauncher

//%import "jni.h"
//%import "jni_md.h"

%{
#include "jvmstarter.h"
%}

// for some reason swig can't handle jni.h / jni_md.h so for now I'll define the necessary jni types here
//%apply unsigned char { jboolean } ;
typedef unsigned char jboolean ;


%include "jvmstarter.h"

%inline %{

// just to test getting swig integrated into the build
int returnFive() {
  return 5 ;
}

%}



