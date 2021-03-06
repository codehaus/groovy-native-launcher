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


#if !defined( _JST_DYNMEM_H_ )
#  define _JST_DYNMEM_H_

#include <stdlib.h>
#if defined( _CRTDBG_MAP_ALLOC )
#  if !defined( _MSC_VER )
#    error "defining _CRTDBG_MAP_ALLOC only makes sense on ms compiler"
#  endif
#  if !defined( _DEBUG )
#    error "_CRTDBG_MAP_ALLOC should only be defined in debug compilation"
#  endif
#  include <crtdbg.h>
#endif

#if defined( __cplusplus )
  extern "C" {
#endif

/** Appends the given strings to target. size param tells the current size of target (target must have been
 * dynamically allocated, i.e. not from stack). If necessary, target is reallocated into a bigger space.
 * Returns the possibly new location of target, and modifies the size inout parameter accordingly.
 * If target is NULL, it is allocated w/ the given size (or bigger if given size does not fit all the given strings).
 * In case target is NULL and you are not interested how big the buffer became, you can give NULL as size. */
char* jst_append( char* target, size_t* size, ... ) ;

/** Concatenates the strings in the given null terminated str array to a single string, which must be freed by the caller. Returns null on error. */
char* jst_concatenateStrArray( char** nullTerminatedStringArray ) ;

/** If array is NULL, a new one will be created, size arlen. The given array will be reallocated if there is not enough space.
 * The newly allocated memory (in both cases) contains all 0s. If NULL is given as the item, zeroes are added at the given array position. */
void* jst_appendArrayItem( void* array, int indx, size_t* arlen, void* item, int item_size_in_bytes ) ;

/** Appends the given pointer to the given null terminated pointer array.
 * given pointer to array may point to NULL, in which case a new array is created.
 * Returns NULL on error.
 * @param item if NULL, the given array is not modified and NULL is returned. */
void** jst_appendPointer( void*** pointerToNullTerminatedPointerArray, size_t* arrSize, void* item ) ;

int jst_pointerArrayLen( void** nullTerminatedPointerArray ) ;

/** Returns the given item, NULL if the item was not in the array. */
void* jst_removePointer( void** nullTerminatedPointerArray, void* itemToBeRemoved ) ;

/** returns 0 if the given item was not in the given array. pointer pointed to is set to NULL. */
int jst_removeAndFreePointer( void** nullTerminatedPointerArray, void** pointerToItemToBeRemoved ) ;


/** Given a null terminated string array, makes a dynamically allocated copy of it that can be freed using a single call to free. Useful for constructing
 * string arrays returned to caller who must free them. In technical detail, the returned mem block first contains all the char*, the termiting NULL and
 * then all the strings one after another. Note that whoever uses the char** does not need to know this mem layout. */
char** jst_packStringArray( char** nullTerminatedStringArray ) ;

typedef enum { PREFIX_SEARCH, SUFFIX_SEARCH, EXACT_SEARCH } SearchMode;

/** The first param may be NULL, it is considered an empty array. Returns -1 if not found, the index where
 * found otherwise.
 * EXACT_SEARCH match exactly
 * PREFIX_SEARCH see if the given string is a prefix in any of the strings in the array
 * SUFFIX_SEARCH see if the given string is a suffix in any of the strings in the array  */
int jst_arrayContainsString( const char** nullTerminatedArray, const char* searchString, SearchMode mode ) ;

/** These wrap the corresponding memory allocation routines. The only difference is that these print an error message if
 * the call fails. */
void* jst_malloc( size_t size ) ;
void* jst_calloc( size_t nelem, size_t elsize ) ;
void* jst_realloc( void* ptr, size_t size ) ;
char* jst_strdup( const char* s ) ;


#define jst_free( x ) free( x ) ; x = NULL

#if defined( _WIN32 ) && !defined( NO_ALLOCA ) && !defined( _CRTDBG_MAP_ALLOC )

#  include <malloc.h>
// _malloca and _freea are available in visual studio 2008
// For some very strange reason _malloca is not found when _CRTDBG_MAP_ALLOC is defined, so
// we'll use normal malloc in that case
#  if defined( _MSC_VER ) && _MSC_VER >= 1500
#    define jst_malloca( size ) _malloca( size )
#    define jst_freea( ptr ) _freea( ptr )
#  else
#    define jst_malloca( size ) alloca( size )
#    define jst_freea( ptr )
#  endif

#elif ( defined( __linux__ ) || defined( __sun__ ) || defined( __APPLE__ ) ) && !defined( NO_ALLOCA )

#  include <alloca.h>
#  define jst_malloca( size ) alloca( size )
#  define jst_freea( ptr )

#else

#  if !defined( NO_ALLOCA ) && !defined( _CRTDBG_MAP_ALLOC )
#    warning "alloca aliases not defined for your platform. Setting them as aliases to jst_malloc and free."
#  endif

#  define jst_malloca( size ) jst_malloc( size )
#  define jst_freea( ptr ) free( ptr )

#endif

/** Concatenates the given strings into the given target buffer, overwriting any previous content.
 * The last parameter must be NULL (to terminate the arg list).
 * Returns target. */
char* jst_concat_overwrite( char* target, ... ) ;


#define JST_STRDUPA( target, source ) target = jst_malloca( strlen( source ) + 1 ) ; \
                                      if ( target ) { \
                                        strcpy( target, source ) ; \
                                      } else { fprintf( stderr, "java launcher memory error\n" ) ; }

#define JST_CONCATA2( target, str1, str2 ) target = jst_malloca( strlen( str1 ) + strlen( str2 ) + 1 ) ; \
                                           if ( target ) { \
                                             jst_concat_overwrite( target, str1, str2, NULL ) ; \
                                           } else { fprintf( stderr, "java launcher memory error\n" ) ; }
#define JST_CONCATA3( target, str1, str2, str3 ) target = jst_malloca( strlen( str1 ) + strlen( str2 ) + strlen( str3 ) + 1 ) ; \
                                                 if ( target ) { \
                                                   jst_concat_overwrite( target, str1, str2, str3, NULL ) ; \
                                                 } else { fprintf( stderr, "java launcher memory error\n" ) ; }
#define JST_CONCATA4( target, str1, str2, str3, str4 ) target = jst_malloca( strlen( str1 ) + strlen( str2 ) + strlen( str3 ) + strlen( str4 ) + 1 ) ; \
                                                       if ( target ) { \
                                                         jst_concat_overwrite( target, str1, str2, str3, str4, NULL ) ; \
                                                       } else { fprintf( stderr, "java launcher memory error\n" ) ; }

/** Frees all the pointers in the given array, the array itself and sets the reference to NULL */
void jst_freeAll( void*** pointerToNullTerminatedPointerArray ) ;

/** The members of this struct should not be manipulated except via the provided functions. */
typedef struct {
  size_t count ;
  size_t capacity ;
  /** number of elements by which the array size is grown if more items are inserted than currently fits. */
  size_t sizeIncrement ;
  /** NULL terminated even though size can be looked up from count field. This enables using as param to funcs that expect NULL terminated pointer array */
  void** pointers ;
} JstDynamicPointerArray ;

JstDynamicPointerArray* jst_initializeDynamicPointerArray( JstDynamicPointerArray* array, size_t initialCapacity, size_t sizeIncrement ) ;
/** returns the given item or NULL on error. */
void* jst_appendPointerToDynamicArray( JstDynamicPointerArray* array, void* item ) ;

void jst_freeDynamicArray( JstDynamicPointerArray* array, jboolean freeContents ) ;

/** Used to print debug messages from the below macro. If iserror==0 then this is a nop. */
void printMemoryErrorExitDebugMessage( const char* file, int line, int iserror ) ;

#define MARK_PTR_FOR_FREEING( dynReservedPointers, dreservedPtrsSize, garbagePtr, nullMeansError ) if ( !jst_appendPointer( &dynReservedPointers, &dreservedPtrsSize, ( garbagePtr ) ) ) { printMemoryErrorExitDebugMessage( __FILE__, __LINE__, nullMeansError ) ; goto end ; }
#define NULL_MEANS_ERROR 1
#define NULL_IS_NOT_ERROR 0


#if defined( __cplusplus )
  } // end extern "C"
#endif


#endif

