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

#ifndef REDBLACKTREE_H
#define REDBLACKTREE_H

/*  CONVENTIONS:  All data structures for red-black trees have the prefix */
/*                "rb_" to prevent name conflicts. */
/*                                                                      */
/*                Function names: Each word in a function name begins with */
/*                a capital letter.  An example funcntion name is  */
/*                CreateRedTree(a,b,c). Furthermore, each function name */
/*                should begin with a capital letter to easily distinguish */
/*                them from variables. */
/*                                                                     */
/*                Variable names: Each word in a variable name begins with */
/*                a capital letter EXCEPT the first letter of the variable */
/*                name.  For example, int newLongInt.  Global variables have */
/*                names beginning with "g".  An example of a global */
/*                variable name is gNewtonsConstant. */

/* comment out the line below to remove all the debugging assertion */
/* checks from the compiled code.  */
#define DEBUG_ASSERT 1

typedef struct rb_red_blk_node {
  struct rb_red_blk_node* left;
  struct rb_red_blk_node* right;
  struct rb_red_blk_node* parent;
  void* key;
  void* info;
  int red; /* if red=0 then the node is black */
} rb_red_blk_node;

/* Compare(a,b) should return 1 if *a > *b, -1 if *a < *b, and 0 otherwise */
/* Destroy(a) takes a pointer to whatever key might be and frees it accordingly */
typedef struct rb_red_blk_tree {
  int (*Compare)(const void* a, const void* b);
  void (*DestroyKey)(void* a);
  void (*DestroyInfo)(void* a);
  /*  A sentinel is used for root and for nil.  These sentinels are */
  /*  created when RBTreeCreate is caled.  root->left should always */
  /*  point to the node which is the root of the tree.  nil points to a */
  /*  node which should always be black but has aribtrary children and */
  /*  parent and no key or info.  The point of using these sentinels is so */
  /*  that the root and nil nodes do not require special cases in the code */
  rb_red_blk_node* root;
  rb_red_blk_node* nil;
} rb_red_blk_tree;

rb_red_blk_tree* RBTreeCreate(int (*CompFunc)(const void*, const void*), void (*DestFunc)(void*), void (*InfoDestFunc)(void*));
rb_red_blk_node* RBTreeInsert(rb_red_blk_tree*, void* key, void* info);
void RBDelete(rb_red_blk_tree*, rb_red_blk_node*);
void RBTreeDestroy(rb_red_blk_tree*);
rb_red_blk_node* TreePredecessor(rb_red_blk_tree*, rb_red_blk_node*);
rb_red_blk_node* TreeSuccessor(rb_red_blk_tree*, rb_red_blk_node*);
rb_red_blk_node* RBExactQuery(rb_red_blk_tree*, void*);
rb_red_blk_node* RBMinQuery(rb_red_blk_tree* tree);

#endif
