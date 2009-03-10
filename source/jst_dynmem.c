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
//  $Revision$
//  $Date$

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

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
#include "jst_stringutils.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// the only difference to originals is that these print an error msg if they fail
extern void* jst_malloc( size_t size ) {
  void* rval = malloc( size ) ;
  if ( !rval ) fprintf( stderr, "error %d in malloc: %s", errno, strerror( errno ) ) ;
  return rval ;
}

extern void* jst_calloc( size_t nelem, size_t elsize ) {
  void* rval = calloc( nelem, elsize ) ;
  if ( !rval ) fprintf( stderr, "error %d in calloc: %s", errno, strerror( errno ) ) ;
  return rval ;
}

extern void* jst_realloc( void* ptr, size_t size ) {
  void* rval = realloc( ptr, size ) ;
  if ( !rval ) fprintf( stderr, "error %d in realloc: %s", errno, strerror( errno ) ) ;
  return rval ;
}

extern char* jst_strdup( const char* s ) {
  char* result ;
  if ( !s ) return NULL ;
  result = strdup( s ) ;
  if ( !result ) fprintf( stderr, "error: out of memory when copying string \"%s\"\n", s ) ;

  return result ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#define DYNAMIC_ARRAY_LENGTH_INCREMENT 5

extern void* jst_appendArrayItem( void* array, int indx, size_t* arlen, void* item, int item_size_in_bytes ) {
  // Note that ansi-c has no data type byte. However, by definition sizeof( char ) == 1, i.e. it is one byte in size.

  // allocate array if requested
  if ( !array ) {
    if  ( *arlen < (size_t)( indx + 1 ) ) {
      *arlen = (size_t)( indx + DYNAMIC_ARRAY_LENGTH_INCREMENT ) ;
    }
    if ( ! ( array = jst_calloc( *arlen, item_size_in_bytes ) ) ) return NULL ;
  } else if ( ((size_t)indx) >= *arlen ) { // ensure there is enough space
    size_t previousSize = *arlen ;

    if ( !( array = jst_realloc( array, ( *arlen += DYNAMIC_ARRAY_LENGTH_INCREMENT ) * item_size_in_bytes ) ) ) return NULL ;

    memset( ((char*)array) + previousSize * item_size_in_bytes, 0, ( *arlen - previousSize ) * item_size_in_bytes ) ;
  }

  // append the new item.
  if ( item ) {
    memcpy( ((char*)array) + indx * item_size_in_bytes, item, item_size_in_bytes ) ;
  } else {
    memset( ((char*)array) + indx * item_size_in_bytes, 0,    item_size_in_bytes ) ;
  }

  return array ;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern char** jst_packStringArray( char** nullTerminatedStringArray ) {
  size_t totalByteSize = sizeof( char* ) ; // space for the terminating NULL
  char   *s,
         **rval,
         *temp ;
  int    i = 0 ;

  while ( ( s = nullTerminatedStringArray[ i++ ] ) ) {
    totalByteSize += sizeof( char* ) + strlen( s ) + 1 ;
  }

  if ( !( rval = jst_malloc( totalByteSize ) ) ) return NULL ;

  // make temp point to the position after the char*, i.e. where the contents of the strings starts
  // NOTE: i now equals the string count + 1, i.e. the number of char* to be stored (including the terminating NULL)
  temp = (char*)( rval + i ) ;

  for ( i = 0 ; ( s = nullTerminatedStringArray[ i ] ) ; i++ ) {
    rval[ i ] = temp ;
    while ( ( *temp++ = *s++ ) ) ;
  }
  rval[ i ] = NULL ;

  return rval ;
}

extern char* jst_concat_overwrite( char* target, ... ) {

  va_list args ;
  char *s,
       *current = target ;

  target[ 0 ] = '\0' ;

  va_start( args, target ) ;

  while ( ( s = va_arg( args, char* ) ) ) {
    while ( *s ) *current++ = *s++ ;
  }

  *current = '\0' ;

  va_end( args ) ;

  return target ;

}

extern char* jst_append( char* target, size_t* bufsize, ... ) {
  va_list args ;
  size_t targetlen = ( target ? strlen( target ) : 0 ),
         numArgs   = 10 ; // the size of the buffer we are storing the param strings into. Enlarged as necessary.
  size_t totalSize = targetlen + 1 ; // 1 for the terminating nul char
  char   *s,
         *t,
         **stringsToAppend = NULL ;
  int    i = 0 ;

  errno = 0 ;
  stringsToAppend = jst_malloc( numArgs * sizeof( char* ) ) ;
  if ( !stringsToAppend ) goto end ;

  va_start( args, bufsize ) ;
  while ( ( s = va_arg( args, char* ) ) ) {
    if ( s[ 0 ] ) { // skip empty strings
      if ( !( stringsToAppend = jst_appendArrayItem( stringsToAppend, i++, &numArgs, &s, sizeof( char* ) ) ) ) goto end ;
      totalSize += strlen( s ) ;
    }
  }
  // s = NULL ; // s is now NULL, no need to assign
  if ( !( stringsToAppend = jst_appendArrayItem( stringsToAppend, i, &numArgs, &s, sizeof( char* ) ) ) ) goto end ;

  if ( !target || *bufsize < totalSize ) {
    // if taget == NULL and bufsize is given, it means we should reserve the given amount of space
    if ( !target && bufsize && ( *bufsize > totalSize ) ) totalSize = *bufsize ;
    target = target ? jst_realloc( target, totalSize * sizeof( char ) ) :
                      jst_malloc( totalSize * sizeof( char ) ) ;
    if ( !target ) goto end ;
    if ( bufsize ) *bufsize = totalSize ; // in case target == NULL, bufsize may also be NULL
  }

  s = target + targetlen ;
  for ( i = 0 ; ( t = stringsToAppend[ i ] ) ; i++ ) {
    //size_t len = strlen( t ) ;
    //memcpy( s, t, len ) ;
    //s += len ;
    // or shorter:
    while ( *t ) *s++ = *t++ ;
  }

  *s = '\0' ;


  end:

  va_end( args ) ;

  if ( errno ) target = NULL ;
  if ( stringsToAppend ) free( stringsToAppend ) ;
  return target ;

}

/** @param strings array must be null terminated */
extern int jst_totalLenghtOfStringsInArray( char** strings ) {

  int length = 0 ;

  if ( !strings ) return 0 ;

  while ( *strings )
    length += (int)strlen( *strings++ ) ;

  return length ;
}

/** Appends the given pointer to the given null terminated pointer array.
 * given pointer to array may point to NULL, in which case a new array is created.
 * Returns NULL on error. */
extern void** jst_appendPointer( void*** pointerToNullTerminatedPointerArray, size_t* arrSize, void* item ) {

  if ( !item ) return NULL ;

  if ( !*pointerToNullTerminatedPointerArray ) {
    if ( !*arrSize ) {
      *arrSize = DYNAMIC_ARRAY_LENGTH_INCREMENT ;
    }

    if ( !( *pointerToNullTerminatedPointerArray = jst_calloc( *arrSize, sizeof( void* ) ) ) ) return NULL ;

    **pointerToNullTerminatedPointerArray = item ;


  } else {

    size_t count = 0 ;

    while ( (*pointerToNullTerminatedPointerArray)[ count ] ) count++ ;

    if ( *arrSize <= count + 1 ) { // +1 for terminating null
      *arrSize = count + DYNAMIC_ARRAY_LENGTH_INCREMENT ;
      if ( !( *pointerToNullTerminatedPointerArray = jst_realloc( *pointerToNullTerminatedPointerArray, *arrSize * sizeof( void* ) ) ) ) return NULL ;
    }

    (*pointerToNullTerminatedPointerArray)[ count ]     = item ;
    (*pointerToNullTerminatedPointerArray)[ count + 1 ] = NULL ;
  }

  return *pointerToNullTerminatedPointerArray ;

}

extern void jst_freeAll( void*** pointerToNullTerminatedPointerArray ) {

  void* item ;
  int   indx = 0 ;

  if ( !*pointerToNullTerminatedPointerArray ) return ;

  while ( ( item = (*pointerToNullTerminatedPointerArray)[ indx++ ] ) ) {
    free( item ) ;
  }

  free( *pointerToNullTerminatedPointerArray ) ;

  *pointerToNullTerminatedPointerArray = NULL ;

}

extern void* jst_removePointer( void** nullTerminatedPointerArray, void* itemToBeRemoved ) {

  while ( *nullTerminatedPointerArray && *nullTerminatedPointerArray != itemToBeRemoved ) nullTerminatedPointerArray++ ;

  if ( !*nullTerminatedPointerArray ) return NULL ;

  while ( *nullTerminatedPointerArray ) {
    *nullTerminatedPointerArray = *( nullTerminatedPointerArray + 1 ) ;
    nullTerminatedPointerArray++ ;
  }

  return itemToBeRemoved ;

}

extern int jst_removeAndFreePointer( void** nullTerminatedPointerArray, void** pointerToItemToBeRemoved ) {

  int rval = jst_removePointer( nullTerminatedPointerArray, *pointerToItemToBeRemoved ) ? JNI_TRUE : JNI_FALSE ;

  free( *pointerToItemToBeRemoved ) ;

  *pointerToItemToBeRemoved = NULL ;

  return rval ;

}

/** Concatenates the strings in the given null terminated str array to a single string, which must be freed by the caller. Returns null on error. */
extern char* jst_concatenateStrArray( char** nullTerminatedStringArray ) {
  size_t totalSize ;
  char   *s,
         *t,
         *rval ;
  int i = 0 ;

  totalSize = jst_totalLenghtOfStringsInArray( nullTerminatedStringArray ) + 1 ; // + 1 for the terminating nul char

  if ( !( rval = jst_malloc( totalSize ) ) ) return NULL ;

  t = rval ;
  for ( i = 0 ; ( s = nullTerminatedStringArray[ i++ ] ) ; ) {
    while ( *s ) *t++ = *s++ ;
  }
  *t = '\0' ;

  return rval ;
}

extern int jst_arrayContainsString( const char** nullTerminatedArray, const char* searchString, SearchMode mode ) {
  int         i   ;
  const char* str ;

  if ( nullTerminatedArray ) {
    switch ( mode ) {
      case PREFIX_SEARCH :
        for ( i = 0 ; ( str = nullTerminatedArray[ i ] ) ; i++ ) {
          if ( jst_startsWith( str, searchString ) ) return i ;
        }
        break ;
      case SUFFIX_SEARCH :
        for ( i = 0 ; ( str = nullTerminatedArray[ i ] ) ; i++ ) {
          if ( jst_endsWith( str, searchString ) ) return i ;
        }
        break ;
      case EXACT_SEARCH :
        for ( i = 0 ; ( str = nullTerminatedArray[ i ] ) ; i++ ) {
          if ( strcmp( str, searchString ) == 0 ) return i ;
        }
        break ;
    }
  }

  return -1 ;
}


extern int jst_pointerArrayLen( void** nullTerminatedPointerArray ) {

  int count = 0 ;

  if ( !nullTerminatedPointerArray ) return 0 ;

  while ( nullTerminatedPointerArray[ count ] ) count++ ;

  return count ;

}

/** returns true if the memory pointed to by the given pointer is zero for "size" bytes. */
static int regionIsZero( unsigned char* ptr, size_t size ) {

  for( ; size-- ; ptr++ )
    if ( *ptr )
      return 0 ;

  return 1 ;
}

/** Returns the size of any array whose end is marked by the first element has as its first
 * part a NULL pointer. */
extern int jst_nullTerminatedArrayLen( void* array, size_t elementSizeInBytes ) {
  int len = 0 ;
  unsigned char* ptr = array ;

  if ( !array ) return 0 ;

  while ( regionIsZero( ptr, elementSizeInBytes ) ) {
    len++ ;
    ptr += elementSizeInBytes ;
  }

  return len ;

}

