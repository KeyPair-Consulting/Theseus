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

#include "red_black_tree.h"
#include <assert.h>
#include <stdlib.h>  // for free
#include "rbt_misc.h"

/***********************************************************************/
/*  FUNCTION:  RBTreeCreate */
/**/
/*  INPUTS:  All the inputs are names of functions.  CompFunc takes to */
/*  void pointers to keys and returns 1 if the first arguement is */
/*  "greater than" the second.   DestFunc takes a pointer to a key and */
/*  destroys it in the appropriate manner when the node containing that */
/*  key is deleted.  InfoDestFunc is similiar to DestFunc except it */
/*  recieves a pointer to the info of a node and destroys it. */
/**/
/*  OUTPUT:  This function returns a pointer to the newly created */
/*  red-black tree. */
/**/
/*  Modifies Input: none */
/***********************************************************************/

rb_red_blk_tree* RBTreeCreate(int (*CompFunc)(const void*, const void*), void (*DestFunc)(void*), void (*InfoDestFunc)(void*)) {
  rb_red_blk_tree* newTree;
  rb_red_blk_node* temp;

  newTree = (rb_red_blk_tree*)SafeMalloc(sizeof(rb_red_blk_tree));
  newTree->Compare = CompFunc;
  newTree->DestroyKey = DestFunc;
  newTree->DestroyInfo = InfoDestFunc;

  /*  see the comment in the rb_red_blk_tree structure in red_black_tree.h */
  /*  for information on nil and root */
  temp = newTree->nil = (rb_red_blk_node*)SafeMalloc(sizeof(rb_red_blk_node));
  temp->parent = temp->left = temp->right = temp;
  temp->red = 0;
  temp->key = 0;
  temp = newTree->root = (rb_red_blk_node*)SafeMalloc(sizeof(rb_red_blk_node));
  temp->parent = temp->left = temp->right = newTree->nil;
  temp->key = 0;
  temp->red = 0;
  return (newTree);
}

/***********************************************************************/
/*  FUNCTION:  LeftRotate */
/**/
/*  INPUTS:  This takes a tree so that it can access the appropriate */
/*           root and nil pointers, and the node to rotate on. */
/**/
/*  OUTPUT:  None */
/**/
/*  Modifies Input: tree, x */
/**/
/*  EFFECTS:  Rotates as described in _Introduction_To_Algorithms by */
/*            Cormen, Leiserson, Rivest (Chapter 14).  Basically this */
/*            makes the parent of x be to the left of x, x the parent of */
/*            its parent before the rotation and fixes other pointers */
/*            accordingly. */
/***********************************************************************/

static void LeftRotate(rb_red_blk_tree* tree, rb_red_blk_node* x) {
  rb_red_blk_node* y;
  rb_red_blk_node* nil = tree->nil;

  /*  I originally wrote this function to use the sentinel for */
  /*  nil to avoid checking for nil.  However this introduces a */
  /*  very subtle bug because sometimes this function modifies */
  /*  the parent pointer of nil.  This can be a problem if a */
  /*  function which calls LeftRotate also uses the nil sentinel */
  /*  and expects the nil sentinel's parent pointer to be unchanged */
  /*  after calling this function.  For example, when RBDeleteFixUP */
  /*  calls LeftRotate it expects the parent pointer of nil to be */
  /*  unchanged. */

  y = x->right;
  x->right = y->left;

  if (y->left != nil) {
    y->left->parent = x; /* used to use sentinel here */
  }
  /* and do an unconditional assignment instead of testing for nil */

  y->parent = x->parent;

  /* instead of checking if x->parent is the root as in the book, we */
  /* count on the root sentinel to implicitly take care of this case */
  if (x == x->parent->left) {
    x->parent->left = y;
  } else {
    x->parent->right = y;
  }
  y->left = x;
  x->parent = y;

#ifdef DEBUG_ASSERT
  Assert(!tree->nil->red, "nil not red in LeftRotate");
#endif
}

/***********************************************************************/
/*  FUNCTION:  RighttRotate */
/**/
/*  INPUTS:  This takes a tree so that it can access the appropriate */
/*           root and nil pointers, and the node to rotate on. */
/**/
/*  OUTPUT:  None */
/**/
/*  Modifies Input?: tree, y */
/**/
/*  EFFECTS:  Rotates as described in _Introduction_To_Algorithms by */
/*            Cormen, Leiserson, Rivest (Chapter 14).  Basically this */
/*            makes the parent of x be to the left of x, x the parent of */
/*            its parent before the rotation and fixes other pointers */
/*            accordingly. */
/***********************************************************************/

static void RightRotate(rb_red_blk_tree* tree, rb_red_blk_node* y) {
  rb_red_blk_node* x;
  rb_red_blk_node* nil = tree->nil;

  /*  I originally wrote this function to use the sentinel for */
  /*  nil to avoid checking for nil.  However this introduces a */
  /*  very subtle bug because sometimes this function modifies */
  /*  the parent pointer of nil.  This can be a problem if a */
  /*  function which calls LeftRotate also uses the nil sentinel */
  /*  and expects the nil sentinel's parent pointer to be unchanged */
  /*  after calling this function.  For example, when RBDeleteFixUP */
  /*  calls LeftRotate it expects the parent pointer of nil to be */
  /*  unchanged. */

  x = y->left;
  y->left = x->right;

  if (nil != x->right) {
    x->right->parent = y; /*used to use sentinel here */
  }
  /* and do an unconditional assignment instead of testing for nil */

  /* instead of checking if x->parent is the root as in the book, we */
  /* count on the root sentinel to implicitly take care of this case */
  x->parent = y->parent;
  if (y == y->parent->left) {
    y->parent->left = x;
  } else {
    y->parent->right = x;
  }
  x->right = y;
  y->parent = x;

#ifdef DEBUG_ASSERT
  Assert(!tree->nil->red, "nil not red in RightRotate");
#endif
}

/***********************************************************************/
/*  FUNCTION:  TreeInsertHelp  */
/**/
/*  INPUTS:  tree is the tree to insert into and z is the node to insert */
/**/
/*  OUTPUT:  none */
/**/
/*  Modifies Input:  tree, z */
/**/
/*  EFFECTS:  Inserts z into the tree as if it were a regular binary tree */
/*            using the algorithm described in _Introduction_To_Algorithms_ */
/*            by Cormen et al.  This funciton is only intended to be called */
/*            by the RBTreeInsert function and not by the user */
/***********************************************************************/

static void TreeInsertHelp(rb_red_blk_tree* tree, rb_red_blk_node* z) {
  /*  This function should only be called by InsertRBTree (see above) */
  rb_red_blk_node* x;
  rb_red_blk_node* y;
  rb_red_blk_node* nil = tree->nil;

  z->left = z->right = nil;
  y = tree->root;
  x = tree->root->left;
  while (x != nil) {
    y = x;
    if (1 == tree->Compare(x->key, z->key)) { /* x.key > z.key */
      x = x->left;
    } else { /* x,key <= z.key */
      x = x->right;
    }
  }
  z->parent = y;
  if ((y == tree->root) || (1 == tree->Compare(y->key, z->key))) { /* y.key > z.key */
    y->left = z;
  } else {
    y->right = z;
  }

#ifdef DEBUG_ASSERT
  Assert(!tree->nil->red, "nil not red in TreeInsertHelp");
#endif
}

/*  Before calling Insert RBTree the node x should have its key set */

/***********************************************************************/
/*  FUNCTION:  RBTreeInsert */
/**/
/*  INPUTS:  tree is the red-black tree to insert a node which has a key */
/*           pointed to by key and info pointed to by info.  */
/**/
/*  OUTPUT:  This function returns a pointer to the newly inserted node */
/*           which is guarunteed to be valid until this node is deleted. */
/*           What this means is if another data structure stores this */
/*           pointer then the tree does not need to be searched when this */
/*           is to be deleted. */
/**/
/*  Modifies Input: tree */
/**/
/*  EFFECTS:  Creates a node node which contains the appropriate key and */
/*            info pointers and inserts it into the tree. */
/***********************************************************************/

rb_red_blk_node* RBTreeInsert(rb_red_blk_tree* tree, void* key, void* info) {
  rb_red_blk_node* y;
  rb_red_blk_node* x;
  rb_red_blk_node* newNode;

  x = (rb_red_blk_node*)SafeMalloc(sizeof(rb_red_blk_node));
  x->key = key;
  x->info = info;

  TreeInsertHelp(tree, x);
  newNode = x;
  x->red = 1;
  while (x->parent->red) { /* use sentinel instead of checking for root */
    if (x->parent == x->parent->parent->left) {
      y = x->parent->parent->right;
      if (y->red) {
        x->parent->red = 0;
        y->red = 0;
        x->parent->parent->red = 1;
        x = x->parent->parent;
      } else {
        if (x == x->parent->right) {
          x = x->parent;
          LeftRotate(tree, x);
        }
        x->parent->red = 0;
        x->parent->parent->red = 1;
        RightRotate(tree, x->parent->parent);
      }
    } else { /* case for x->parent == x->parent->parent->right */
      y = x->parent->parent->left;
      if (y->red) {
        x->parent->red = 0;
        y->red = 0;
        x->parent->parent->red = 1;
        x = x->parent->parent;
      } else {
        if (x == x->parent->left) {
          x = x->parent;
          RightRotate(tree, x);
        }
        x->parent->red = 0;
        x->parent->parent->red = 1;
        LeftRotate(tree, x->parent->parent);
      }
    }
  }
  tree->root->left->red = 0;
  return (newNode);
}

/***********************************************************************/
/*  FUNCTION:  TreeSuccessor  */
/**/
/*    INPUTS:  tree is the tree in question, and x is the node we want the */
/*             the successor of. */
/**/
/*    OUTPUT:  This function returns the successor of x or NULL if no */
/*             successor exists. */
/**/
/*    Modifies Input: none */
/**/
/*    Note:  uses the algorithm in _Introduction_To_Algorithms_ */
/***********************************************************************/

rb_red_blk_node* TreeSuccessor(rb_red_blk_tree* tree, rb_red_blk_node* x) {
  rb_red_blk_node* y;
  rb_red_blk_node* nil = tree->nil;
  rb_red_blk_node* root = tree->root;

  if (nil != (y = x->right)) { /* assignment to y is intentional */
    while (y->left != nil) { /* returns the minium of the right subtree of x */
      y = y->left;
    }
    return (y);
  } else {
    y = x->parent;
    while (x == y->right) { /* sentinel used instead of checking for nil */
      x = y;
      y = y->parent;
    }
    if (y == root) {
      return (nil);
    }
    return (y);
  }
}

/***********************************************************************/
/*  FUNCTION:  Treepredecessor  */
/**/
/*    INPUTS:  tree is the tree in question, and x is the node we want the */
/*             the predecessor of. */
/**/
/*    OUTPUT:  This function returns the predecessor of x or NULL if no */
/*             predecessor exists. */
/**/
/*    Modifies Input: none */
/**/
/*    Note:  uses the algorithm in _Introduction_To_Algorithms_ */
/***********************************************************************/

rb_red_blk_node* TreePredecessor(rb_red_blk_tree* tree, rb_red_blk_node* x) {
  rb_red_blk_node* y;
  rb_red_blk_node* nil = tree->nil;
  rb_red_blk_node* root = tree->root;

  if (nil != (y = x->left)) { /* assignment to y is intentional */
    while (y->right != nil) { /* returns the maximum of the left subtree of x */
      y = y->right;
    }
    return (y);
  } else {
    y = x->parent;
    while (x == y->left) {
      if (y == root) {
        return (nil);
      }
      x = y;
      y = y->parent;
    }
    return (y);
  }
}

/***********************************************************************/
/*  FUNCTION:  TreeDestHelper */
/**/
/*    INPUTS:  tree is the tree to destroy and x is the current node */
/**/
/*    OUTPUT:  none  */
/**/
/*    EFFECTS:  This function recursively destroys the nodes of the tree */
/*              postorder using the DestroyKey and DestroyInfo functions. */
/**/
/*    Modifies Input: tree, x */
/**/
/*    Note:    This function should only be called by RBTreeDestroy */
/***********************************************************************/

static void TreeDestHelper(rb_red_blk_tree* tree, rb_red_blk_node* x) {
  rb_red_blk_node* nil;

  assert(x != NULL);
  assert(tree != NULL);

  nil = tree->nil;
  if (x != nil) {
    TreeDestHelper(tree, x->left);
    x->left = NULL;
    TreeDestHelper(tree, x->right);
    x->right = NULL;
    tree->DestroyKey(x->key);
    tree->DestroyInfo(x->info);
    assert(x != NULL);
    free(x);
  }
}

/***********************************************************************/
/*  FUNCTION:  RBTreeDestroy */
/**/
/*    INPUTS:  tree is the tree to destroy */
/**/
/*    OUTPUT:  none */
/**/
/*    EFFECT:  Destroys the key and frees memory */
/**/
/*    Modifies Input: tree */
/**/
/***********************************************************************/

void RBTreeDestroy(rb_red_blk_tree* tree) {
  assert(tree != NULL);

  TreeDestHelper(tree, tree->root->left);
  tree->root->left = NULL;
  free(tree->root);
  tree->root = NULL;
  free(tree->nil);
  tree->nil = NULL;
  free(tree);
}

/***********************************************************************/
/*  FUNCTION:  RBMinQuery */
/**/
/*    INPUTS:  tree is the tree */
/**/
/*    OUTPUT:  returns the a node with the lexigraphically minimum index. */
/**/
/*    Modifies Input: none */
/**/
/***********************************************************************/

rb_red_blk_node* RBMinQuery(rb_red_blk_tree* tree) {
  rb_red_blk_node* x = tree->root->left;
  rb_red_blk_node* nil = tree->nil;
  if (x == nil) {
    return (0);
  }
  while (x->left != nil) { /*assignemnt*/
    x = x->left;
  }
  return (x);
}

/***********************************************************************/
/*  FUNCTION:  RBExactQuery */
/**/
/*    INPUTS:  tree is the tree to search and q is a pointer to the key */
/*             we are searching for */
/**/
/*    OUTPUT:  returns the a node with key equal to q.  If there are */
/*             multiple nodes with key equal to q this function returns */
/*             the one highest in the tree */
/**/
/*    Modifies Input: none */
/**/
/***********************************************************************/

rb_red_blk_node* RBExactQuery(rb_red_blk_tree* tree, void* q) {
  rb_red_blk_node* x = tree->root->left;
  rb_red_blk_node* nil = tree->nil;
  int compVal;
  if (x == nil) {
    return (0);
  }
  compVal = tree->Compare(x->key, (int*)q);
  while (0 != compVal) { /*assignemnt*/
    if (1 == compVal) { /* x->key > q */
      x = x->left;
    } else {
      x = x->right;
    }
    if (x == nil) {
      return (0);
    }
    compVal = tree->Compare(x->key, (int*)q);
  }
  return (x);
}

/***********************************************************************/
/*  FUNCTION:  RBDeleteFixUp */
/**/
/*    INPUTS:  tree is the tree to fix and x is the child of the spliced */
/*             out node in RBTreeDelete. */
/**/
/*    OUTPUT:  none */
/**/
/*    EFFECT:  Performs rotations and changes colors to restore red-black */
/*             properties after a node is deleted */
/**/
/*    Modifies Input: tree, x */
/**/
/*    The algorithm from this function is from _Introduction_To_Algorithms_ */
/***********************************************************************/

static void RBDeleteFixUp(rb_red_blk_tree* tree, rb_red_blk_node* x) {
  rb_red_blk_node* root = tree->root->left;
  rb_red_blk_node* w;

  while ((!x->red) && (root != x)) {
    if (x == x->parent->left) {
      w = x->parent->right;
      if (w->red) {
        w->red = 0;
        x->parent->red = 1;
        LeftRotate(tree, x->parent);
        w = x->parent->right;
      }
      if ((!w->right->red) && (!w->left->red)) {
        w->red = 1;
        x = x->parent;
      } else {
        if (!w->right->red) {
          w->left->red = 0;
          w->red = 1;
          RightRotate(tree, w);
          w = x->parent->right;
        }
        w->red = x->parent->red;
        x->parent->red = 0;
        w->right->red = 0;
        LeftRotate(tree, x->parent);
        x = root; /* this is to exit while loop */
      }
    } else { /* the code below is has left and right switched from above */
      w = x->parent->left;
      if (w->red) {
        w->red = 0;
        x->parent->red = 1;
        RightRotate(tree, x->parent);
        w = x->parent->left;
      }
      if ((!w->right->red) && (!w->left->red)) {
        w->red = 1;
        x = x->parent;
      } else {
        if (!w->left->red) {
          w->right->red = 0;
          w->red = 1;
          LeftRotate(tree, w);
          w = x->parent->left;
        }
        w->red = x->parent->red;
        x->parent->red = 0;
        w->left->red = 0;
        RightRotate(tree, x->parent);
        x = root; /* this is to exit while loop */
      }
    }
  }
  x->red = 0;

#ifdef DEBUG_ASSERT
  Assert(!tree->nil->red, "nil not black in RBDeleteFixUp");
#endif
}

/***********************************************************************/
/*  FUNCTION:  RBDelete */
/**/
/*    INPUTS:  tree is the tree to delete node z from */
/**/
/*    OUTPUT:  none */
/**/
/*    EFFECT:  Deletes z from tree and frees the key and info of z */
/*             using DestoryKey and DestoryInfo.  Then calls */
/*             RBDeleteFixUp to restore red-black properties */
/**/
/*    Modifies Input: tree, z */
/**/
/*    The algorithm from this function is from _Introduction_To_Algorithms_ */
/***********************************************************************/

void RBDelete(rb_red_blk_tree* tree, rb_red_blk_node* z) {
  rb_red_blk_node* y;
  rb_red_blk_node* x;
  rb_red_blk_node* nil;
  rb_red_blk_node* root;

  assert(tree != NULL);
  assert(z != NULL);

  nil = tree->nil;
  root = tree->root;

  y = ((z->left == nil) || (z->right == nil)) ? z : TreeSuccessor(tree, z);
  x = (y->left == nil) ? y->right : y->left;
  if (root == (x->parent = y->parent)) { /* assignment of y->p to x->p is intentional */
    root->left = x;
  } else {
    if (y == y->parent->left) {
      y->parent->left = x;
    } else {
      y->parent->right = x;
    }
  }
  if (y != z) { /* y should not be nil in this case */

#ifdef DEBUG_ASSERT
    Assert((y != tree->nil), "y is nil in RBDelete\n");
#endif
    /* y is the node to splice out and x is its child */

    if (!(y->red)) {
      RBDeleteFixUp(tree, x);
    }

    tree->DestroyKey(z->key);
    tree->DestroyInfo(z->info);
    y->left = z->left;
    y->right = z->right;
    y->parent = z->parent;
    y->red = z->red;
    z->left->parent = z->right->parent = y;
    if (z == z->parent->left) {
      z->parent->left = y;
    } else {
      z->parent->right = y;
    }
    assert(z != NULL);
    free(z);
    z = NULL;
  } else {
    tree->DestroyKey(y->key);
    tree->DestroyInfo(y->info);
    if (!(y->red)) {
      RBDeleteFixUp(tree, x);
    }
    assert(y != NULL);
    free(y);
    y = NULL;
  }
}
