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

#include <Windows.h>
#include <assert.h>
#include <stdlib.h>

#include <jni.h>

#include "jst_winreg.h"
#include "jvmstarter.h"

#define KEY_NAME_MAX 256
#define VALUE_NAME_MAX 16384

/** Opens reg key for read only access. */
static DWORD jst_openRegistryKey( HKEY parent, char* subkeyName, HKEY* key_out ) {
  return RegOpenKeyEx( 
                       parent, 
                       subkeyName,
                       0,  // reserved, must be zero
                       KEY_READ,
                       key_out
                     ) ;  
}

/** Currently this assumes that the value read is a string. If not, it will raise an error. 
 * Reading other datatypes as strings may be supported in the future. */
static int jst_queryRegistryValue( HKEY key, char* valueName, char* valueBuffer, DWORD* valueBufferSize ) {
  DWORD valueType ;
  int   status ;
  
  status = RegQueryValueEx( key, valueName, NULL, &valueType, (BYTE*)valueBuffer, valueBufferSize ) ;
    
  if ( status == ERROR_SUCCESS ) {
    if ( valueType != REG_SZ && valueType != REG_MULTI_SZ ) { // the first string is read from REG_MULTI_SZ
      // TODO: support converting these to strings where possible
      char* typeName = 
        valueType == REG_BINARY              ? "REG_BINARY"              :
        valueType == REG_DWORD               ? "REG_DWORD"               :
        valueType == REG_DWORD_LITTLE_ENDIAN ? "REG_DWORD_LITTLE_ENDIAN" :
        valueType == REG_DWORD_BIG_ENDIAN    ? "REG_DWORD_BIG_ENDIAN"    :
        // TODO: use func ExpandEnvironmentStrings to expand this kind of value
        valueType == REG_EXPAND_SZ           ? "REG_EXPAND_SZ"           : 
        valueType == REG_LINK                ? "REG_LINK"                :
        valueType == REG_NONE                ? "REG_NONE"                :
        valueType == REG_QWORD               ? "REG_QWORD"               :
        valueType == REG_QWORD_LITTLE_ENDIAN ? "REG_QWORD_LITTLE_ENDIAN" :
        "unknown" ; // should never happen

      status = -1 ;
      fprintf( stderr, "error: windows registry value type %s (%u) is not supported\n", typeName, (unsigned int)valueType ) ;
    }
  }
    
  return status ;
  
}

#define VALUE_BUFFER_DEFAULT_SIZE 1024

/** The return value must be freed by the caller.
 * Note that NULL return does not necessarily indicate error - it may just mean the value is not found. 
 * Check output param statusOut for error code (will be set to 0 on success, != 0 otherwise. > 0 => win error code, < 0 => custom error code.
 */
static char* jst_queryRegistryStringValue( HKEY root, char* subkeyName, char* valueName, int* statusOut ) {
  DWORD status,
        valueBufferSize = VALUE_BUFFER_DEFAULT_SIZE ;
  HKEY key = 0 ;
  char valueBuffer[ VALUE_BUFFER_DEFAULT_SIZE ],
       *valueBufferDyn = NULL ;
  
  status = jst_openRegistryKey( root, subkeyName, &key ) ;
  assert( status || key ) ;
  if ( status ) {
    if ( status != ERROR_FILE_NOT_FOUND ) {
      jst_printWinError( status ) ;
      *statusOut = (int)status ;
    }
    goto end ;
  }
  
  // TODO: RegQueryValueEx will write the value into the buffer w/out null termination if the buffer is too small
  //       to fit that in (i.e. the buffer size is strlen( regvalue ) ). Account for this: if RegQueryValueEx
  //       tells that the number of chars written is the same as buffer size, ensure that the last char is NULL; if 
  //       not, raise an error.
  status = jst_queryRegistryValue( key, valueName, valueBuffer, &valueBufferSize ) ;

  if ( status == ERROR_MORE_DATA ) {
    valueBufferDyn = jst_malloc( valueBufferSize ) ;
    if ( !valueBufferDyn ) {
      *statusOut = -1 ;
      goto end ;
    }
    status = jst_queryRegistryValue( key, valueName, valueBufferDyn, &valueBufferSize ) ;
  }

  if ( status ) {
    if ( status != ERROR_FILE_NOT_FOUND ) {
      jst_printWinError( status ) ;
      *statusOut = (int)status ;
    }
    goto end ;
  }

  if ( !valueBufferDyn ) {
    valueBufferDyn = jst_strdup( valueBuffer ) ;
    if ( !valueBufferDyn ) {
      *statusOut = -1 ;
      goto end ;
    }
  }
  
  *statusOut = 0 ;
  
  end :
  
  if ( key ) {
    status = RegCloseKey( key ) ;
    if ( status != ERROR_SUCCESS ) jst_printWinError( status ) ;
  }
  
  return valueBufferDyn ;
}

static int jst_readJavaHomeFromRegistry( char* jdkOrJreKeyName, char** _javaHome ) {
  int  status = 0 ;
  char *currentVersion = NULL,
       *fullName       = NULL ;
  
  currentVersion = jst_queryRegistryStringValue( HKEY_LOCAL_MACHINE, jdkOrJreKeyName, "CurrentVersion", &status ) ;
  if ( status || !currentVersion ) goto end ;
  
  fullName = jst_append( NULL, NULL, jdkOrJreKeyName, "\\", currentVersion, NULL ) ;
  if ( !fullName ) { status = -1 ; goto end ; }

  *_javaHome = jst_queryRegistryStringValue( HKEY_LOCAL_MACHINE, fullName, "JavaHome", &status ) ;
  
  end :
  if ( currentVersion ) free( currentVersion ) ;
  if ( fullName       ) free( fullName ) ;
  return status ;
}

extern int jst_findJavaHomeFromWinRegistry( char** javaHomeOut ) {
  static char* _javaHome = NULL ;
  
  // all these are under key HKEY_LOCAL_MACHINE
  static char* registryEntriesToCheck[] = { "SOFTWARE\\JavaSoft\\Java Development Kit", 
                                            "SOFTWARE\\JRockit\\Java Development Kit",
                                            "SOFTWARE\\JavaSoft\\Java Runtime Environment", 
                                            "SOFTWARE\\JRockit\\Java Runtime Environment",
                                            NULL 
                                          } ;

  int error = 0 ;
  
  if ( !_javaHome ) {
    char*    jdkOrJreKeyName ;
    int      javaTypeCounter = 0 ;
        
    while ( ( jdkOrJreKeyName = registryEntriesToCheck[ javaTypeCounter++ ] ) ) {

      if ( ( error = jst_readJavaHomeFromRegistry( jdkOrJreKeyName, &_javaHome ) ) || _javaHome ) break ;
      
    }   
    
    if ( _jst_debug ) {
      if ( _javaHome ) {
        fprintf( stderr, "debug: java home found from windows registry: %s\n", _javaHome ) ;
      } else {
        fprintf( stderr, "debug: java home not found from windows registry\n" ) ;      
      }
    }
  }


  if ( !error ) *javaHomeOut = _javaHome ;
  return error ;
  
}

#endif
