//  A library for easy creation of a native launcher for Java applications.
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

#include "jvmstarter.h"
#include <string.h>
#include <stdlib.h>

extern JstActualParam* jst_processInputParameters( char** args, int numArgs, JstParamInfo *paramInfos, char** terminatingSuffixes ) {

  // TODO: cygwin conversions of param values when requested
  //       + for all input params after termination?
  
  int    i, j,
         usedSize   = ( numArgs + 1 ) * sizeof( JstActualParam ),
         actualSize = usedSize ; // enable this once cygwin coversion is in place (takes dyn reserved memory) + 100 ; 
  size_t len ;
  JstActualParam* processedParams = jst_calloc( actualSize, 1 ) ;

  if ( !processedParams ) return NULL ;

  for ( i = 0 ; i < numArgs ; i++ ) {
    char *arg = args[ i ] ;
    int found = JNI_FALSE ;
    
    if ( ( arg[ 0 ] == 0 ) ||  // empty strs are always considered to be terminating args to the launchee
         jst_arrayContainsString( terminatingSuffixes, arg, SUFFIX_SEARCH ) ) {
      goto end ;
    }

    for ( j = 0 ; paramInfos[ j ].name && !found ; j++ ) {
      switch ( paramInfos[ j ].type ) {
        case JST_SINGLE_PARAM :
          if ( strcmp( paramInfos[ j ].name, arg ) == 0 ) {
            found = JNI_TRUE ;
            processedParams[ i ].param = processedParams[ i ].value = arg ;
            processedParams[ i ].handling = paramInfos[ j ].handling ;
          }            
          break ;
        case JST_DOUBLE_PARAM : 
          if ( strcmp( paramInfos[ j ].name, arg ) == 0 ) {
            found = JNI_TRUE ;
            processedParams[ i ].param = processedParams[ i ].value = arg ;
            processedParams[ i ].handling = paramInfos[ j ].handling ;
            if ( ++i >= numArgs ) {
              fprintf( stderr, "error: illegal use of %s (requires a value)\n", arg ) ;
              free( processedParams ) ;
              return NULL ;
            }
            processedParams[ i ].param = processedParams[ i ].value = args[ i ] ;
            processedParams[ i ].handling = paramInfos[ j ].handling ;
          }
          break ;
        case JST_PREFIX_PARAM :
          len = strlen( paramInfos[ j ].name ) ;
          if ( memcmp( paramInfos[ j ].name, arg, len ) == 0 ) {
            found = JNI_TRUE ;
            processedParams[ i ].param = arg ;
            if ( ( usedSize + len + 1 ) > actualSize ) {
              processedParams = jst_realloc( processedParams, actualSize += ( 100 + len ) ) ;
              if ( !processedParams ) return NULL ;
            }
                          
            processedParams[ i ].value = arg + len ;
            processedParams[ i ].handling = paramInfos[ j ].handling ;
          }
          break ;
      } // switch
      
      if ( found ) {
        processedParams[ i ].paramDefinition = paramInfos + j ;
        if ( paramInfos[ j ].terminating ) {
          goto end ;
        }
      } 
    } // for j
    
    if ( !found ) {
      if ( arg[ 0 ] == '-' ) {
        processedParams[ i ].param = processedParams[ i ].value = arg ;
        processedParams[ i ].handling = JST_UNRECOGNIZED ;
      } else {
        goto end ;
      }
    }
  } // for i
  
  end:
  for ( ; i < numArgs ; i++ ) {
    processedParams[ i ].param = processedParams[ i ].value = args[ i ] ;
    processedParams[ i ].handling = JST_TO_LAUNCHEE ;
  }
  
  if ( usedSize < actualSize ) {
    processedParams = jst_realloc( processedParams, usedSize ) ;
  }
  
  return processedParams ;
  
}


extern int jst_contains( char** args, int* numargs, const char* option, const jboolean removeIfFound ) {
  int i       = 0, 
      foundAt = -1 ;
  
  for ( ; i < *numargs ; i++ ) {
    if ( strcmp( option, args[ i ] ) == 0 ) {
      foundAt = i ;
      break ;
    }
  }
  if ( foundAt != -1 ) return -1 ;
  if ( removeIfFound ) {
    (*numargs)-- ;
    for ( ; i < *numargs ; i++ ) {
      args[ i ] = args[ i + i ] ;
    }
  }
  return foundAt ;
}

extern int jst_indexOfParam( char** args, int numargs, char* paramToSearch ) {
  int i = 0 ;
  for ( ; i < numargs; i++ ) {
    if ( strcmp( paramToSearch, args[i] ) == 0 ) return i ;
  }
  return -1 ;  
}

extern char* jst_valueOfParam( char** args, int* numargs, int* checkUpto, const char* option, const JstParamClass paramType, const jboolean removeIfFound, jboolean* error ) {
  int i    = 0, 
      step = 1;
  size_t len;
  char* retVal = NULL;

  switch(paramType) {
    case JST_SINGLE_PARAM :
      for ( ; i < *checkUpto ; i++ ) {
        if ( strcmp( option, args[ i ] ) == 0 ) {
          retVal = args[ i ] ;
          break ;
        }
      }
      break ;
    case JST_DOUBLE_PARAM :
      step = 2 ;
      for ( ; i < *checkUpto ; i++ ) {
        if ( strcmp( option, args[ i ] ) == 0 ) {
          if ( i == ( *numargs - 1 ) ) {
            *error = JNI_TRUE ;
            fprintf( stderr, "error: %s must have a value\n", option ) ;
            return NULL ;
          }
          retVal = args[ i + 1 ] ;
          break ;
        }
      }
      break ;
    case JST_PREFIX_PARAM :
      len = strlen( option ) ;
      for ( ; i < *checkUpto; i++ ) {
        if ( memcmp( option, args[ i ], len ) == 0 ) {
          retVal = args[ i ] + len ;
          break ;
        }
      }
      break ;
  }
  
  if ( retVal && removeIfFound ) {
    for ( ; i < ( *numargs - step ) ; i++ ) {
      args[ i ] = args[ i + step ] ;
    }
    *numargs   -= step ;
    *checkUpto -= step ;
  }
  
  return retVal ;  
  
}

