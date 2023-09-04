#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

/*
 * core.h
 * ======
 * 
 * Core state and ruler stack interpretation module of Infrared.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - art.c
 *   - blob.c
 *   - diagnostic.c
 *   - graph.c
 *   - pointer.c
 *   - ruler.c
 *   - set.c
 *   - text.c
 * 
 * Requires the following external libraries:
 * 
 *   - librfdict
 */

#include <stddef.h>
#include <stdint.h>

#include "art.h"
#include "blob.h"
#include "graph.h"
#include "pointer.h"
#include "ruler.h"
#include "set.h"
#include "text.h"

/*
 * Constants
 * =========
 */

/*
 * The different data types that can be stored in a CORE_VARIANT.
 * 
 * The NULL type is only intended for marking variants that are not in
 * use.  The Infrared interpreter does not actually have a NULL type.
 * 
 * The MIN and MAX show the full valid range of values, excluding the
 * NULL type.
 */
#define CORE_T_NULL     (0)
#define CORE_T_INTEGER  (1)
#define CORE_T_TEXT     (2)
#define CORE_T_BLOB     (3)
#define CORE_T_GRAPH    (4)
#define CORE_T_SET      (5)
#define CORE_T_ART      (6)
#define CORE_T_RULER    (7)
#define CORE_T_POINTER  (8)

#define CORE_T_MIN      (1)
#define CORE_T_MAX      (8)

/*
 * Data type declarations
 * ======================
 */

/*
 * Structure that can store any of the Infrared interpreter's data
 * types.
 */
typedef struct {
  
  /*
   * The type code of the data type stored in this variant.
   * 
   * This is one of the CORE_T constant values.
   */
  uint8_t tcode;
  
  /*
   * The actual value stored in the variant.
   * 
   * The specific field to use depends on the tcode.  The NULL type does
   * not use any fields.
   */
  union {
    int32_t   iv;
    TEXT    * pText;
    BLOB    * pBlob;
    GRAPH   * pGraph;
    SET     * pSet;
    ART     * pArt;
    RULER   * pRuler;
    POINTER * pPointer;
  } v;
  
} CORE_VARIANT;

/*
 * Public functions
 * ================
 */

/*
 * Push a value of any type (except NULL) onto the interpreter stack.
 * 
 * A copy is made of the variant structure on the stack.  A stack
 * overflow error occurs if a maximum stack height limit is breached.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pv - the value to push
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void core_push(const CORE_VARIANT *pv, long lnum);

/*
 * Pop a value of any type (except NULL) off the interpreter stack.
 * 
 * A copy of the popped value is written into the passed variant
 * structure.  A stack underflow error occurs if no elements are visible
 * on the stack.
 * 
 * This function respects grouping and will not pop elements off the
 * stack that are hidden by Shastina groups.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pv - the structure to store a copy of the value
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void core_pop(CORE_VARIANT *pv, long lnum);

/*
 * Begin a group on the interpreter stack.
 * 
 * This hides all elements currently on the stack.  Groups may be
 * nested.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void core_begin_group(long lnum);

/*
 * End a group on the interpreter stack.
 * 
 * The stack must have exactly one visible element on it or an error
 * occurs.  All elements that were hidden by the group are restored.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void core_end_group(long lnum);

/*
 * Declare a constant or a variable.
 * 
 * is_const determines whether a constant or a variable is declared.
 * pKey is the name of the constant or variable to declare.  The initial
 * value will then be popped off the top of the stack by this function.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   is_const - non-zero to declare a constant, zero to declare a
 *   variable
 * 
 *   pKey - the name of the constant or variable to declare
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void core_declare(int is_const, const char *pKey, long lnum);

/*
 * Retrieve the value of a declared constant or variable and push it on
 * top of the interpreter stack.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pKey - the name of the constant or variable to get
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void core_get(const char *pKey, long lnum);

/*
 * Assign a new value to a declared variable.
 * 
 * The new value is popped off the top of the interpreter stack.  Only
 * variables may be used with this function, not constants.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pKey - the name of the variable to assign to
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void core_assign(const char *pKey, long lnum);

/*
 * Push a ruler onto the ruler stack.
 * 
 * The ruler stack is separate from the main interpreter stack.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pr - the ruler to push
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void core_rstack_push(RULER *pr, long lnum);

/*
 * Pop a ruler from the top of the ruler stack.
 * 
 * The ruler stack is separate from the main interpreter stack.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the ruler that was popped off the ruler stack
 */
RULER *core_rstack_pop(long lnum);

/*
 * Get the "current" ruler from the ruler stack.
 * 
 * If the ruler stack is not empty, the current ruler is whichever ruler
 * is on top of the ruler stack.  If the ruler stack is empty, the
 * current ruler is a default ruler.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the current ruler
 */
RULER *core_rstack_current(long lnum);

/*
 * Shut down the core interpreter state and check that it is in a valid
 * end-state.
 * 
 * There may not be any open groups, and the stack must be empty.  Else,
 * an error occurs.
 * 
 * No other functions of this module may be called after invoking this
 * function, except for the shutdown function itself, which has no
 * further effect.
 */
void core_shutdown(void);

/*
 * Wrapper around core_push() that pushes an integer.
 */
void core_push_i(int32_t iv, long lnum);

/*
 * Wrapper around core_push() that pushes a text object.
 */
void core_push_t(TEXT *pText, long lnum);

/*
 * Wrapper around core_push() that pushes a blob object.
 */
void core_push_b(BLOB *pBlob, long lnum);

/*
 * Wrapper around core_push() that pushes a graph object.
 */
void core_push_g(GRAPH *pGraph, long lnum);

/*
 * Wrapper around core_push() that pushes a set object.
 */
void core_push_s(SET *pSet, long lnum);

/*
 * Wrapper around core_push() that pushes an articulation object.
 */
void core_push_a(ART *pArt, long lnum);

/*
 * Wrapper around core_push() that pushes a ruler object.
 */
void core_push_r(RULER *pRuler, long lnum);

/*
 * Wrapper around core_push() that pushes a pointer object.
 */
void core_push_p(POINTER *pPointer, long lnum);

/*
 * Wrapper around core_pop() that pops an integer off the stack.
 * 
 * This function checks that there is an element of that type on top of
 * the stack.
 */
int32_t core_pop_i(long lnum);

/*
 * Wrapper around core_pop() that pops a text object off the stack.
 * 
 * This function checks that there is an element of that type on top of
 * the stack.
 */
TEXT *core_pop_t(long lnum);

/*
 * Wrapper around core_pop() that pops a blob off the stack.
 * 
 * This function checks that there is an element of that type on top of
 * the stack.
 */
BLOB *core_pop_b(long lnum);

/*
 * Wrapper around core_pop() that pops a graph off the stack.
 * 
 * This function checks that there is an element of that type on top of
 * the stack.
 */
GRAPH *core_pop_g(long lnum);

/*
 * Wrapper around core_pop() that pops a set off the stack.
 * 
 * This function checks that there is an element of that type on top of
 * the stack.
 */
SET *core_pop_s(long lnum);

/*
 * Wrapper around core_pop() that pops an articulation off the stack.
 * 
 * This function checks that there is an element of that type on top of
 * the stack.
 */
ART *core_pop_a(long lnum);

/*
 * Wrapper around core_pop() that pops a ruler off the stack.
 * 
 * This function checks that there is an element of that type on top of
 * the stack.
 */
RULER *core_pop_r(long lnum);

/*
 * Wrapper around core_pop() that pops a pointer off the stack.
 * 
 * This function checks that there is an element of that type on top of
 * the stack.
 */
POINTER *core_pop_p(long lnum);

#endif
