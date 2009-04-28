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
#include <assert.h>

#if defined ( __APPLE__ )
#  include <TargetConditionals.h>

/*
 *  The Mac OS X Leopard version of jni_md.h (Java SE 5 JDK) is broken in that it tests the value of
 *  __LP64__ instead of the presence of _LP64 as happens in Sun's Java 6.0 JDK.  To prevent spurious
 *  warnings define this with a false value.
 */
#define __LP64__ 0

#endif

#include "jvmstarter.h"
#include "jst_dynmem.h"

#if defined( _WIN32 )
#  include <Windows.h>
#  if !defined( PATH_MAX )
#    define PATH_MAX MAX_PATH
#  endif
#endif

#if defined ( _WIN32 ) && defined ( _cwcompat )

#include "jst_cygwin_compatibility.h"


#define CYGWIN_CONVERTED_VALUE_BUFFER_SIZE ( 10 * PATH_MAX )

/** a helper used below. Returns a pointer to the original string if no conversion was done, the converted value if conversion was done and NULL on error. */
static char* cygwinConvertStringAndAppendInTheEndOfGivenBufferIfNotEqualToOriginal( char* value, JstActualParam** processedParams, size_t* usedSize, size_t* actualSize, JstInputParamHandling paramHandling ) {
  char convertedValue[ CYGWIN_CONVERTED_VALUE_BUFFER_SIZE ] ;

  if ( JST_CYGWIN_PATH_CONVERT & paramHandling ) {
    cygwin_posix2win_path( value, convertedValue ) ;
  } else {
    assert( JST_CYGWIN_PATHLIST_CONVERT & paramHandling ) ;
    cygwin_posix2win_path_list( value, convertedValue ) ;
  }

  assert( strlen( convertedValue ) < CYGWIN_CONVERTED_VALUE_BUFFER_SIZE ) ;


  if ( strcmp( value, convertedValue ) ) {

    if ( _jst_debug ) fprintf( stderr, "  cygwin converted param %s to %s\n", value, convertedValue ) ;

    size_t len = strlen( convertedValue ) + 1 ;

    if ( *usedSize + len > *actualSize ) {
      *processedParams = jst_realloc( *processedParams, *actualSize += len + 200 ) ;
      if ( !*processedParams ) return NULL ;
    }

    value = ((char*)*processedParams) + *usedSize ;
    strcpy( value, convertedValue ) ;

    *usedSize = *usedSize + len ;

  }

  return value ;
}

#endif


// TODO: presorting param infos would improve performance as then the param definition would not need to
//       be sought w/ exhaustive search
//       The performance is atm O(n*m) where n = number of actual params, m = number of param definitions.
//       Thenagain, this should only be optimized if profiling says it must (for a real case),
//       as n and m are both likely to be very small.
//       It would also be possible to use a (balanced) binary tree ( O(log(N)) ) or a hash.
//       However, it is unlikely that the number of parameters is ever significant enough for this to be an
//       issue, so doing these improvements is left till it is verified that there are cases where
//       this actually makes a difference.

static void printParameterClassification( JstInputParamHandling handlingFlags ) {
  // these correspond to the bits in JstInputParamHandling
  static const char* descriptions[] = {
                                        "to launcher",
                                        "to jvm",
                                        "to launched program",
                                        "terminating or after",
                                        "cygwin path convert",
                                         "cygwin pathlist convert",
                                        "unrecognized",
                                        NULL
                                      } ;
  int i = 0, firstOnePrinted = 1 ;

  for ( ; descriptions[ i ] ; i++ ) {
    if ( ( handlingFlags >> i ) & 1 ) {
      if ( !firstOnePrinted ) fprintf( stderr, ", " ) ;
      fprintf( stderr, descriptions[ i ] ) ;
      firstOnePrinted = 0 ;
    }
  }

}


static void printParameterDebugInfo( JstActualParam* params, int numParams ) {

  int i ;

  fprintf( stderr, "debug: param classifications for %d input params:\n", numParams ) ;

  for ( i = 0 ; i < numParams ; i++ ) {

    fprintf( stderr, "  %s -> ", params[ i ].param ) ;
    printParameterClassification( params[ i ].handling ) ;
    fprintf( stderr, " (0x%02x)\n", params[ i ].handling ) ;

  }

}

// FIXME: the func below is a bit too complex - refactor

extern JstActualParam* jst_processInputParameters( char** args, int numArgs, JstParamInfo *paramInfos, const char** terminatingSuffixes, CygwinConversionType cygwinConvertParamsAfterTermination ) {

  // TODO: cygwin conversions of param values
  //       + for all input params after termination (if requested) => not a good idea, e.g. xpath params migh be transformed weird.

  int    i, j ;
  size_t usedSize   = ( numArgs + 1 ) * sizeof( JstActualParam ),
         actualSize = usedSize
#if defined( _WIN32 ) && defined( _cwcompat )
         + 50 * numArgs
#endif
         , len ;
  JstActualParam* processedParams = jst_calloc( actualSize, 1 ) ;

  if ( !processedParams ) return NULL ;

  // FIXME - this is too complex - refactor into smaller pieces

  for ( i = 0 ; i < numArgs ; i++ ) {
    char *arg = args[ i ] ;
    int found = JNI_FALSE ;

    if ( ( arg[ 0 ] == 0 ) ||  // empty strs are always considered to be terminating args to the launchee
         ( jst_arrayContainsString( terminatingSuffixes, arg, SUFFIX_SEARCH ) != -1 ) ) {
      goto end ;
    }

    for ( j = 0 ; paramInfos[ j ].names && !found ; j++ ) {
      char* value = NULL ;
      JstParamClass paramClass = paramInfos[ j ].type ;

      switch ( paramClass ) {
        case JST_SINGLE_PARAM :
          if ( jst_arrayContainsString( paramInfos[ j ].names, arg, EXACT_SEARCH ) != -1 ) {
            found = JNI_TRUE ;
            processedParams[ i ].param = arg ;
            value = "" ;
            processedParams[ i ].handling = paramInfos[ j ].handling ;
          }
          break ;
        case JST_DOUBLE_PARAM :
          if ( jst_arrayContainsString( paramInfos[ j ].names, arg, EXACT_SEARCH ) != -1 ) {
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
        {
          const char* pname ;
          int k ;
          for ( k = 0 ; !found && ( pname = paramInfos[ j ].names[ k ] ) ; k++ ) {
            len = strlen( pname ) ;
            if ( memcmp( pname, arg, len ) == 0 ) {
              found = JNI_TRUE ;
              processedParams[ i ].param = arg ;
              if ( (int)( usedSize + len + 1 ) > (int)actualSize ) {
                processedParams = jst_realloc( processedParams, actualSize += ( 100 + len ) ) ;
                if ( !processedParams ) return NULL ;
              }

              value = arg ;
              processedParams[ i ].handling = paramInfos[ j ].handling ;
            }
          }
          break ;
        }
      } // switch

      if ( found ) {

#if defined ( _WIN32 ) && defined ( _cwcompat )

        if ( CYGWIN_LOADED &&
             ( processedParams[ i ].handling & ( JST_CYGWIN_PATH_CONVERT | JST_CYGWIN_PATHLIST_CONVERT ) ) ) {
          value = cygwinConvertStringAndAppendInTheEndOfGivenBufferIfNotEqualToOriginal( value, &processedParams, &usedSize, &actualSize, processedParams[ i ].handling ) ;
          if ( !value ) return NULL ;
        }

#endif

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
        processedParams[ i ].param = arg ;
        processedParams[ i ].handling = JST_UNRECOGNIZED ;
        processedParams[ i ].value = arg ;
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
#if defined( _WIN32 ) && defined( _cwcompat )
    if ( CYGWIN_LOADED && cygwinConvertParamsAfterTermination ) {
      value = cygwinConvertStringAndAppendInTheEndOfGivenBufferIfNotEqualToOriginal( value, &processedParams, &usedSize, &actualSize, cygwinConvertParamsAfterTermination ) ;
      if ( !value ) return NULL ;
    }
#endif
    processedParams[ i ].value = value ;
  }

  if ( _jst_debug ) printParameterDebugInfo( processedParams, numArgs ) ;

  if ( usedSize < actualSize ) {
    processedParams = jst_realloc( processedParams, usedSize ) ;
    assert( processedParams ) ; // shrinking the buffer, should always succeed
  }

  return processedParams ;

}

extern int jst_isToBePassedToLaunchee( const JstActualParam* processedParam, JstUnrecognizedParamStrategy unrecognizedParamStrategy ) {
  return ( processedParam->handling & JST_TO_LAUNCHEE ) ||
         ( ( processedParam->handling & JST_UNRECOGNIZED ) && ( unrecognizedParamStrategy & JST_UNRECOGNIZED_TO_APP ) ) ;
}



// not needed ATM. Should be paired w/ a version of jst_getParameterValue that takes a single JstParamInfo*
// instead of the name of the parameter
//extern JstParamInfo* jst_findParamInfo( JstParamInfo* paraminfos, const char* paramName ) {
  // TODO
//  return NULL ;
//}


extern char* jst_getParameterValue( const JstActualParam* processedParams, const char* paramName ) {

  JstParamInfo* paramDef ;
  char* rval = NULL ;

  for ( ; !rval && processedParams->param ; processedParams++ ) {
    if ( ( processedParams->handling ) & JST_TERMINATING_OR_AFTER ) break ;
    paramDef = processedParams->paramDefinition ;
    if ( paramDef ) {
       if ( ( paramDef->handling & JST_SINGLE_PARAM || paramDef->handling & JST_DOUBLE_PARAM ) &&
          ( jst_arrayContainsString( paramDef->names, paramName, EXACT_SEARCH ) != -1 ) ) {
        rval = processedParams->value ;
      } else if ( ( paramDef->type == JST_PREFIX_PARAM ) &&
                  ( jst_arrayContainsString( paramDef->names, paramName, PREFIX_SEARCH ) != -1 ) ) {
        size_t len = strlen( paramName ) ;
        rval = ( processedParams->value ) + len ;
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
