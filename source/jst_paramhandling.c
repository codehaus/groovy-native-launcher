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

#include <string.h>
#include <stdlib.h>

#include "jvmstarter.h"
#include "jst_cygwin_compatibility.h"


// TODO: presorting param infos would improve performance as then the param definition would not need to 
//       be sought w/ exhaustive search

extern JstActualParam* jst_processInputParameters( char** args, int numArgs, JstParamInfo *paramInfos, char** terminatingSuffixes, jboolean cygwinConvertParamsAfterTermination ) {

  // TODO: cygwin conversions of param values when requested
  //       + for all input params after termination (again, if requested)
  
  int    i, j ;
  size_t usedSize   = ( numArgs + 1 ) * sizeof( JstActualParam ),
         actualSize = usedSize, // enable this once cygwin coversion is in place (takes dyn reserved memory) + 100 ; 
         len ;
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
      char* value = NULL ;
      JstParamClass paramClass = paramInfos[ j ].type ;
      
      switch ( paramClass ) {
        case JST_SINGLE_PARAM :
          if ( strcmp( paramInfos[ j ].name, arg ) == 0 ) {
            found = JNI_TRUE ;
            processedParams[ i ].param = arg ;
            value = "" ;
            processedParams[ i ].handling = paramInfos[ j ].handling ;
          }            
          break ;
        case JST_DOUBLE_PARAM : 
          if ( strcmp( paramInfos[ j ].name, arg ) == 0 ) {
            JstInputParamHandling handling = paramInfos[ j ].handling ; 
            found = JNI_TRUE ;
            processedParams[ i ].param = arg ;
            processedParams[ i ].handling = handling ;
            if ( ++i >= numArgs ) {
              fprintf( stderr, "error: illegal use of %s (requires a value)\n", arg ) ;
              free( processedParams ) ;
              return NULL ;
            }
            processedParams[ i ].param = value = args[ i ] ;
            processedParams[ i ].handling = handling ;
          }
          break ;
        case JST_PREFIX_PARAM :
          len = strlen( paramInfos[ j ].name ) ;
          if ( memcmp( paramInfos[ j ].name, arg, len ) == 0 ) {
            found = JNI_TRUE ;
            processedParams[ i ].param = arg ;
            if ( (int)( usedSize + len + 1 ) > (int)actualSize ) {
              processedParams = jst_realloc( processedParams, actualSize += ( 100 + len ) ) ;
              if ( !processedParams ) return NULL ;
            }
                          
            value = arg ;
            processedParams[ i ].handling = paramInfos[ j ].handling ;
          }
          break ;
      } // switch
      
      if ( found ) {
        // TODO: cygwin conversion
        // TODO: do not transform the prefix in PREFIX param
        processedParams[ i ].value = value ;
        if ( paramClass == JST_DOUBLE_PARAM ) {
          processedParams[ i - 1 ].value = value ;
          processedParams[ i - 1 ].paramDefinition = paramInfos + j ;
        }
        processedParams[ i ].paramDefinition = paramInfos + j ;
        if ( ( processedParams[ i ].handling ) & JST_TERMINATING ) {
          goto end ;
        }
        break ;
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
  // mark all the params after the terminating one (if any) as going to the launchee
  for ( ; i < numArgs ; i++ ) {
    char* value = NULL ;
    
    processedParams[ i ].param = value = args[ i ] ;
    processedParams[ i ].handling = ( JST_TO_LAUNCHEE | JST_TERMINATING_OR_AFTER ) ;
    processedParams[ i ].paramDefinition = NULL ;
    // TODO: cygwin conversion of value if required
    processedParams[ i ].value = value ;
  }

  if ( _jst_debug ) {
    fprintf( stderr, "debug: param classifications for %d input params:\n", numArgs ) ;
    for ( i = 0 ; i < numArgs ; i++ ) {
      fprintf( stderr, "  %s -> %d\n", processedParams[ i ].param, processedParams[ i ].handling ) ;
    }
  }
  
  if ( usedSize < actualSize ) {
    processedParams = jst_realloc( processedParams, usedSize ) ;
  }
  
  return processedParams ;
  
}

extern int jst_isToBePassedToLaunchee( const JstActualParam* processedParam, JstUnrecognizedParamStrategy unrecognizedParamStrategy ) {
  return ( processedParam->handling & JST_TO_LAUNCHEE ) || 
         ( ( processedParam->handling & JST_UNRECOGNIZED ) && ( unrecognizedParamStrategy & JST_UNRECOGNIZED_TO_APP ) ) ;
}


extern char* jst_getParameterValue( const JstActualParam* processedParams, const char* paramName ) {

  JstParamInfo* paramDef ;
  char* rval = NULL ; 
  // TODO: refactor JSTParamInfo so that it contains list of all the aliases for the param name 
  //       (ATM they are presented as separate parameters, which makes reasoning about them harder)
  
  //        After that refactoring, for each actual param, check whether
  //        the given name is in the corresponding JstParamInfo's name list
  
  for ( ; !rval && processedParams->param ; processedParams++ ) {
    if ( ( processedParams->handling ) & JST_TERMINATING_OR_AFTER ) break ;
    paramDef = processedParams->paramDefinition ;
    if ( paramDef && strcmp( paramName, paramDef->name ) == 0 ) {
      rval = processedParams->value ;
      if ( paramDef->type == JST_PREFIX_PARAM ) {
        size_t len = strlen( paramDef->name ) ;
        rval += len ;
      }
    }
  }
  
  return rval ;
  
}

/** Returns the input parameter at position indx (0 based) after first terminating param. Returns NULL if index is
 * more than the number of params after termination - 1. */
extern char* jst_getParameterAfterTermination( const JstActualParam* processedParams, int indx ) {
  
  while ( processedParams->param && !( ( processedParams->handling ) & JST_TERMINATING_OR_AFTER ) ) processedParams++ ;
  
  while ( processedParams->param && indx-- ) processedParams++ ;
  
  return processedParams->param ;
  
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

