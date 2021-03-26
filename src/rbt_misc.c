/***************************************************************************
 * Red-Black Tree routines, implements balanced tree data storage.
 *
 * The code base was originally written by Emin Marinian:
 * http://www.csua.berkeley.edu/~emin/index.html
 * http://alum.mit.edu/www/emin
 * License historically available here:
 * http://web.mit.edu/~emin/www.old/source_code/red_black_tree/LICENSE
 * This is available using the Wayback machine:
 * https://web.archive.org/web/20180112031521/http://web.mit.edu/~emin/www.old/source_code/red_black_tree/LICENSE
 * This is include below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that neither the name of Emin
 * Martinian nor the names of any contributors are be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

/* This file is included as part of the Theseus distribution.
 *
 * Author(s)
 * Emin Marinian
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */

#include "rbt_misc.h"
#include <assert.h>
#include <stdio.h>  // for stderr, NULL, fprintf
#include <stdlib.h>  // for exit

/***********************************************************************/
/*  FUNCTION:  void Assert(int assertion, char* error)  */
/**/
/*  INPUTS: assertion should be a predicated that the programmer */
/*  assumes to be true.  If this assumption is not true the message */
/*  error is printed and the program exits. */
/**/
/*  OUTPUT: None. */
/**/
/*  Modifies input:  none */
/**/
/*  Note:  If DEBUG_ASSERT is not defined then assertions should not */
/*         be in use as they will slow down the code.  Therefore the */
/*         compiler will complain if an assertion is used when */
/*         DEBUG_ASSERT is undefined. */
/***********************************************************************/

void Assert(int assertion, char* error) {
  if (assertion == 0) {
    fprintf(stderr, "Assertion Failed: %s\n", error);
    exit(-1);
  }
}

/***********************************************************************/
/*  FUNCTION:  SafeMalloc */
/**/
/*    INPUTS:  size is the size to malloc */
/**/
/*    OUTPUT:  returns pointer to allocated memory if succesful */
/**/
/*    EFFECT:  mallocs new memory.  If malloc fails, prints error message */
/*             and terminates program. */
/**/
/*    Modifies Input: none */
/**/
/***********************************************************************/

void* SafeMalloc(size_t size) {
  void* result;

  if ((result = malloc(size)) != NULL) { /* assignment intentional */
    return (result);
  } else {
    fprintf(stderr, "memory overflow: malloc failed in SafeMalloc.");
    fprintf(stderr, "  Exiting Program.\n");
    exit(-1);
  }
}
/*  NullFunction does nothing it is included so that it can be passed */
/*  as a function to RBTreeCreate when no other suitable function has */
/*  been defined */

void NullFunction(void* junk) {
  assert(junk != NULL);
}
