/*
 * core.c
 * ======
 * 
 * Implementation of core.h
 * 
 * See header for further information.
 */

#include "core.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"

#include "rfdict.h"

/*
 * Diagnostics
 * ===========
 */

static void raiseErr(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(1, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

static void sayWarn(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(0, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

/*
 * Constants
 * =========
 */

/*
 * The initial and maximum capacities of the interpreter stack.
 */
#define STACK_INIT_CAP  (32)
#define STACK_MAX_CAP   (16384)

/*
 * The initial and maximum capacities of the grouping stack.
 */
#define GROUP_INIT_CAP  (32)
#define GROUP_MAX_CAP   (1024)

/*
 * The initial and maximum capacities of the variable and constants
 * memory bank.
 */
#define BANK_INIT_CAP   (64)
#define BANK_MAX_CAP    (16384)

/*
 * The initial and maximum capacities of the ruler stack.
 */
#define RSTACK_INIT_CAP (16)
#define RSTACK_MAX_CAP  (1024)

/*
 * Type declarations
 * =================
 */

/*
 * Structure used for tracking data in the variable and constant bank.
 * 
 * This simply contains a variant structure and a flag indicating
 * whether this is a constant or a variable.
 */
typedef struct {
  CORE_VARIANT cv;
  uint8_t is_const;
} BANK_CELL;

/*
 * Local data
 * ==========
 */

/*
 * Set to 1 if the module has been shut down.
 */
static int m_shutdown = 0;

/*
 * The interpreter stack.
 * 
 * This consists of the current capacity and current length, along with
 * a pointer to the dynamically allocated array.
 * 
 * This does not take into account elements hidden by the grouping
 * stack.
 */
static int32_t m_st_cap = 0;
static int32_t m_st_len = 0;
static CORE_VARIANT *m_st = NULL;

/*
 * The grouping stack.
 * 
 * This consists of the current capacity and current length, along with
 * a pointer to the dynamically allocated array.
 * 
 * Each open group pushes an element onto this stack indicating the
 * total number of elements hidden on the interpreter stack.
 */
static int32_t m_gs_cap = 0;
static int32_t m_gs_len = 0;
static int32_t *m_gs = NULL;

/*
 * The memory bank.
 * 
 * m_bank_map maps declared variable and constant names to their index
 * in the memory bank.
 * 
 * There is also the current capacity and current length of the memory
 * bank, along with a pointer to the dynamically allocated bank.
 */
static RFDICT *m_bank_map = NULL;
static int32_t m_bank_cap = 0;
static int32_t m_bank_len = 0;
static BANK_CELL *m_bank = NULL;

/*
 * The ruler stack.
 * 
 * This consists of the current capacity and current length, along with
 * a pointer to the dynamically allocated array.
 */
static int32_t m_rs_cap = 0;
static int32_t m_rs_len = 0;
static RULER **m_rs = NULL;

/*
 * The default ruler to use if the ruler stack is empty, or NULL if this
 * has not been allocated yet.
 */
static RULER *m_ruler_default = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static long srcLine(long lnum);
static int validName(const char *pName);

static void capStack(int32_t n, long lnum);
static void capGroup(int32_t n, long lnum);
static void capBank(int32_t n, long lnum);
static void capRStack(int32_t n, long lnum);

/*
 * If the given line number is within valid range, return it as-is.  In
 * all other cases, return -1.
 * 
 * Parameters:
 * 
 *   lnum - the candidate line number
 * 
 * Return:
 * 
 *   the line number as-is if valid, else -1
 */
static long srcLine(long lnum) {
  if ((lnum < 1) || (lnum >= LONG_MAX)) {
    lnum = -1;
  }
  return lnum;
}

/*
 * Check whether a given name is a valid variable or constant name.
 * 
 * Parameters:
 * 
 *   pName - the name to check
 * 
 * Return:
 * 
 *   non-zero if valid, zero otherwise
 */
static int validName(const char *pName) {
  
  size_t slen = 0;
  int result = 1;
  const char *pc = NULL;
  
  if (pName == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  slen = strlen(pName);
  if ((slen < 1) || (slen > 31)) {
    result = 0;
  }
  
  if (result) {
    if (((pName[0] < 'A') || (pName[0] > 'Z')) &&
        ((pName[0] < 'a') || (pName[0] > 'z'))) {
      result = 0;
    }
  }
  
  if (result) {
    for(pc = pName; *pc != 0; pc++) {
      if (((*pc < 'A') || (*pc > 'Z')) &&
          ((*pc < 'a') || (*pc > 'z')) &&
          ((*pc < '0') || (*pc > '9')) &&
          (*pc != '_')) {
        result = 0;
        break;
      }
    }
  }
  
  return result;
}

/*
 * Make room in capacity for a given number of elements in the
 * interpreter stack.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
static void capStack(int32_t n, long lnum) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_st_cap < 1) {
      m_st = (CORE_VARIANT *) calloc(
                  (size_t) STACK_INIT_CAP, sizeof(CORE_VARIANT));
      if (m_st == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_st_cap = STACK_INIT_CAP;
      m_st_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_st_len) {
      target = m_st_len + n;
    } else {
      raiseErr(__LINE__,
        "Interpreter stack overflow on script line %ld",
        srcLine(lnum));
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_st_cap) {
      /* Check that target within maximum capacity */
      if (target > STACK_MAX_CAP) {
        raiseErr(__LINE__,
          "Interpreter stack overflow on script line %ld",
          srcLine(lnum));
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_st_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > STACK_MAX_CAP) {
        new_cap = STACK_MAX_CAP;
      }
      
      /* Expand capacity */
      m_st = (CORE_VARIANT *) realloc(m_st,
                            ((size_t) new_cap) * sizeof(CORE_VARIANT));
      if (m_st == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_st[m_st_cap]),
        0,
        ((size_t) (new_cap - m_st_cap)) * sizeof(CORE_VARIANT));
      
      m_st_cap = new_cap;
    }
  }
}

/*
 * Make room in capacity for a given number of elements in the grouping
 * stack.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
static void capGroup(int32_t n, long lnum) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_gs_cap < 1) {
      m_gs = (int32_t *) calloc(
                  (size_t) GROUP_INIT_CAP, sizeof(int32_t));
      if (m_gs == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_gs_cap = GROUP_INIT_CAP;
      m_gs_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_gs_len) {
      target = m_gs_len + n;
    } else {
      raiseErr(__LINE__, "Too much group nesting on script line %ld",
        srcLine(lnum));
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_gs_cap) {
      /* Check that target within maximum capacity */
      if (target > GROUP_MAX_CAP) {
        raiseErr(__LINE__, "Too much group nesting on script line %ld",
          srcLine(lnum));
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_gs_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > GROUP_MAX_CAP) {
        new_cap = GROUP_MAX_CAP;
      }
      
      /* Expand capacity */
      m_gs = (int32_t *) realloc(m_gs,
                            ((size_t) new_cap) * sizeof(int32_t));
      if (m_gs == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_gs[m_gs_cap]),
        0,
        ((size_t) (new_cap - m_gs_cap)) * sizeof(int32_t));
      
      m_gs_cap = new_cap;
    }
  }
}

/*
 * Make room in capacity for a given number of elements in the memory
 * bank.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
static void capBank(int32_t n, long lnum) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_bank_cap < 1) {
      m_bank = (BANK_CELL *) calloc(
                  (size_t) BANK_INIT_CAP, sizeof(BANK_CELL));
      if (m_bank == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_bank_cap = BANK_INIT_CAP;
      m_bank_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_bank_len) {
      target = m_bank_len + n;
    } else {
      raiseErr(__LINE__,
        "Too many variables and constants on script line %ld",
        srcLine(lnum));
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_bank_cap) {
      /* Check that target within maximum capacity */
      if (target > BANK_MAX_CAP) {
        raiseErr(__LINE__,
          "Too many variables and constants on script line %ld",
          srcLine(lnum));
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_bank_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > BANK_MAX_CAP) {
        new_cap = BANK_MAX_CAP;
      }
      
      /* Expand capacity */
      m_bank = (BANK_CELL *) realloc(m_bank,
                            ((size_t) new_cap) * sizeof(BANK_CELL));
      if (m_bank == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_bank[m_bank_cap]),
        0,
        ((size_t) (new_cap - m_bank_cap)) * sizeof(BANK_CELL));
      
      m_bank_cap = new_cap;
    }
  }
}

/*
 * Make room in capacity for a given number of elements in the ruler
 * stack.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
static void capRStack(int32_t n, long lnum) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_rs_cap < 1) {
      m_rs = (RULER **) calloc(
                  (size_t) RSTACK_INIT_CAP, sizeof(RULER *));
      if (m_rs == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_rs_cap = RSTACK_INIT_CAP;
      m_rs_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_rs_len) {
      target = m_rs_len + n;
    } else {
      raiseErr(__LINE__, "Ruler stack overflow on script line %ld",
        srcLine(lnum));
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_rs_cap) {
      /* Check that target within maximum capacity */
      if (target > RSTACK_MAX_CAP) {
        raiseErr(__LINE__, "Ruler stack overflow on script line %ld",
          srcLine(lnum));
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_rs_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > RSTACK_MAX_CAP) {
        new_cap = RSTACK_MAX_CAP;
      }
      
      /* Expand capacity */
      m_rs = (RULER **) realloc(m_rs,
                            ((size_t) new_cap) * sizeof(RULER *));
      if (m_rs == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_rs[m_rs_cap]),
        0,
        ((size_t) (new_cap - m_rs_cap)) * sizeof(RULER *));
      
      m_rs_cap = new_cap;
    }
  }
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * core_push function.
 */
void core_push(const CORE_VARIANT *pv, long lnum) {
  
  /* Check state and parameters */
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  if (pv == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check variant */
  if ((pv->tcode < CORE_T_MIN) || (pv->tcode > CORE_T_MAX)) {
    raiseErr(__LINE__, NULL);
  }
  switch (pv->tcode) {
    case CORE_T_TEXT:
      if ((pv->v).pText == NULL) {
        raiseErr(__LINE__, NULL);
      }
      break;
    
    case CORE_T_BLOB:
      if ((pv->v).pBlob == NULL) {
        raiseErr(__LINE__, NULL);
      }
      break;
    
    case CORE_T_GRAPH:
      if ((pv->v).pGraph == NULL) {
        raiseErr(__LINE__, NULL);
      }
      break;
    
    case CORE_T_SET:
      if ((pv->v).pSet == NULL) {
        raiseErr(__LINE__, NULL);
      }
      break;
    
    case CORE_T_ART:
      if ((pv->v).pArt == NULL) {
        raiseErr(__LINE__, NULL);
      }
      break;
    
    case CORE_T_RULER:
      if ((pv->v).pRuler == NULL) {
        raiseErr(__LINE__, NULL);
      }
      break;
    
    case CORE_T_POINTER:
      if ((pv->v).pPointer == NULL) {
        raiseErr(__LINE__, NULL);
      }
      break;
  }
  
  /* Push onto stack */
  capStack(1, lnum);
  memcpy(&(m_st[m_st_len]), pv, sizeof(CORE_VARIANT));
  m_st_len++;
}

/*
 * core_pop function.
 */
void core_pop(CORE_VARIANT *pv, long lnum) {
  
  int32_t hidden = 0;
  
  /* Check state and parameters */
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  if (pv == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Determine number of hidden stack elements */
  hidden = 0;
  if (m_gs_len > 0) {
    hidden = m_gs[m_gs_len - 1];
  }
  
  /* Check at least one visible element */
  if (m_st_len <= hidden) {
    raiseErr(__LINE__, "Interpreter stack underflow on script line %ld",
      srcLine(lnum));
  }
  
  /* Pop the top element and clear it */
  memcpy(pv, &(m_st[m_st_len - 1]), sizeof(CORE_VARIANT));
  memset(&(m_st[m_st_len - 1]), 0, sizeof(CORE_VARIANT));
  m_st_len--;
}

/*
 * core_begin_group function.
 */
void core_begin_group(long lnum) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  
  capGroup(1, lnum);
  m_gs[m_gs_len] = m_st_len;
  m_gs_len++;
}

/*
 * core_end_group function.
 */
void core_end_group(long lnum) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  
  if (m_gs_len < 1) {
    raiseErr(__LINE__, "Unpaired end group on script line %ld",
      srcLine(lnum));
  }
  
  if (m_st_len != m_gs[m_gs_len - 1] + 1) {
    raiseErr(__LINE__, "Group constraint failed on script line %ld",
      srcLine(lnum));
  }
  
  m_gs_len--;
}

/*
 * core_declare function.
 */
void core_declare(int is_const, const char *pKey, long lnum) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  if (pKey == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (!validName(pKey)) {
    raiseErr(__LINE__, "Invalid var/const name '%s' on script line %ld",
      pKey, srcLine(lnum));
  }
  
  if (m_bank_map == NULL) {
    m_bank_map = rfdict_alloc(1);
  }
  
  if (rfdict_get(m_bank_map, pKey, -1) >= 0) {
    raiseErr(__LINE__, "Redefinition of '%s' on script line %ld",
      pKey, srcLine(lnum));
  }
  
  capBank(1, lnum);
  
  if (is_const) {
    (m_bank[m_bank_len]).is_const = 1;
  } else {
    (m_bank[m_bank_len]).is_const = 0;
  }
  
  core_pop(&((m_bank[m_bank_len]).cv), lnum);
  
  if (!rfdict_insert(m_bank_map, pKey, (long) m_bank_len)) {
    raiseErr(__LINE__, NULL);
  }
  m_bank_len++;
}

/*
 * core_get function.
 */
void core_get(const char *pKey, long lnum) {
  
  long i = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  if (pKey == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (!validName(pKey)) {
    raiseErr(__LINE__, "Invalid var/const name '%s' on script line %ld",
      pKey, srcLine(lnum));
  }
  
  if (m_bank_map == NULL) {
    m_bank_map = rfdict_alloc(1);
  }
  
  i = rfdict_get(m_bank_map, pKey, -1);
  if (i < 0) {
    raiseErr(__LINE__, "Var/const '%s' not defined on script line %ld",
      pKey, srcLine(lnum));
  }
  
  core_push(&((m_bank[i]).cv), lnum);
}

/*
 * core_assign function.
 */
void core_assign(const char *pKey, long lnum) {
  
  long i = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  if (pKey == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (!validName(pKey)) {
    raiseErr(__LINE__, "Invalid var/const name '%s' on script line %ld",
      pKey, srcLine(lnum));
  }
  
  if (m_bank_map == NULL) {
    m_bank_map = rfdict_alloc(1);
  }
  
  i = rfdict_get(m_bank_map, pKey, -1);
  if (i < 0) {
    raiseErr(__LINE__, "Var/const '%s' not defined on script line %ld",
      pKey, srcLine(lnum));
  }
  
  if ((m_bank[i]).is_const) {
    raiseErr(__LINE__, "Can't assign to const '%s' on script line %ld",
      pKey, srcLine(lnum));
  }
  
  core_push(&((m_bank[i]).cv), lnum);
}

/*
 * core_rstack_push function.
 */
void core_rstack_push(RULER *pr, long lnum) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  if (pr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  capRStack(1, lnum);
  
  m_rs[m_rs_len] = pr;
  m_rs_len++;
}

/*
 * core_rstack_pop function.
 */
RULER *core_rstack_pop(long lnum) {
  
  RULER *pResult = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  
  if (m_rs_len < 1) {
    raiseErr(__LINE__, "Ruler stack underflow on script line %ld",
      srcLine(lnum));
  }
  
  pResult = m_rs[m_rs_len - 1];
  m_rs_len--;
  
  return pResult;
}

/*
 * core_rstack_current function.
 */
RULER *core_rstack_current(long lnum) {
  
  RULER *pResult = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Core module is shut down");
  }
  
  if (m_ruler_default == NULL) {
    m_ruler_default = ruler_new(48, 0, lnum);
  }
  
  pResult = m_ruler_default;
  if (m_rs_len > 0) {
    pResult = m_rs[m_rs_len - 1];
  }
  
  return pResult;
}

/*
 * core_shutdown function.
 */
void core_shutdown(void) {
  if (!m_shutdown) {
    m_shutdown = 1;
    
    if (m_st_len > 0) {
      raiseErr(__LINE__,
        "Interpreter stack must be empty at end of script");
    }
    if (m_gs_len > 0) {
      raiseErr(__LINE__,
        "Open group left at end of script");
    }
    
    if (m_st_cap > 0) {
      m_st_cap = 0;
      m_st_len = 0;
      free(m_st);
      m_st = NULL;
    }
    
    if (m_gs_cap > 0) {
      m_gs_cap = 0;
      m_gs_len = 0;
      free(m_gs);
      m_gs = NULL;
    }
    
    if (m_bank_map != NULL) {
      rfdict_free(m_bank_map);
      m_bank_map = NULL;
    }
    
    if (m_bank_cap > 0) {
      m_bank_cap = 0;
      m_bank_len = 0;
      free(m_bank);
      m_bank = NULL;
    }
    
    if (m_rs_cap > 0) {
      m_rs_cap = 0;
      m_rs_len = 0;
      free(m_rs);
      m_rs = NULL;
    }
    
    m_ruler_default = NULL;
  }
}

/*
 * core_push_i function.
 */
void core_push_i(int32_t iv, long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  cv.tcode = (uint8_t) CORE_T_INTEGER;
  cv.v.iv = iv;
  
  core_push(&cv, lnum);
}

/*
 * core_push_t function.
 */
void core_push_t(TEXT *pText, long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  if (pText == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  cv.tcode = (uint8_t) CORE_T_TEXT;
  cv.v.pText = pText;
  
  core_push(&cv, lnum);
}

/*
 * core_push_b function.
 */
void core_push_b(BLOB *pBlob, long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  if (pBlob == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  cv.tcode = (uint8_t) CORE_T_BLOB;
  cv.v.pBlob = pBlob;
  
  core_push(&cv, lnum);
}

/*
 * core_push_g function.
 */
void core_push_g(GRAPH *pGraph, long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  if (pGraph == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  cv.tcode = (uint8_t) CORE_T_GRAPH;
  cv.v.pGraph = pGraph;
  
  core_push(&cv, lnum);
}

/*
 * core_push_s function.
 */
void core_push_s(SET *pSet, long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  if (pSet == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  cv.tcode = (uint8_t) CORE_T_SET;
  cv.v.pSet = pSet;
  
  core_push(&cv, lnum);
}

/*
 * core_push_a function.
 */
void core_push_a(ART *pArt, long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  if (pArt == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  cv.tcode = (uint8_t) CORE_T_ART;
  cv.v.pArt = pArt;
  
  core_push(&cv, lnum);
}

/*
 * core_push_r function.
 */
void core_push_r(RULER *pRuler, long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  if (pRuler == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  cv.tcode = (uint8_t) CORE_T_RULER;
  cv.v.pRuler = pRuler;
  
  core_push(&cv, lnum);
}

/*
 * core_push_p function.
 */
void core_push_p(POINTER *pPointer, long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  if (pPointer == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  cv.tcode = (uint8_t) CORE_T_POINTER;
  cv.v.pPointer = pPointer;
  
  core_push(&cv, lnum);
}

/*
 * core_pop_i function.
 */
int32_t core_pop_i(long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  
  if (cv.tcode != CORE_T_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on stack on script line %ld",
      srcLine(lnum));
  }
  
  return cv.v.iv;
}

/*
 * core_pop_t function.
 */
TEXT *core_pop_t(long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  
  if (cv.tcode != CORE_T_TEXT) {
    raiseErr(__LINE__,
      "Expecting text object on stack on script line %ld",
      srcLine(lnum));
  }
  
  return cv.v.pText;
}

/*
 * core_pop_b function.
 */
BLOB *core_pop_b(long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  
  if (cv.tcode != CORE_T_BLOB) {
    raiseErr(__LINE__,
      "Expecting blob object on stack on script line %ld",
      srcLine(lnum));
  }
  
  return cv.v.pBlob;
}

/*
 * core_pop_g function.
 */
GRAPH *core_pop_g(long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  
  if (cv.tcode != CORE_T_GRAPH) {
    raiseErr(__LINE__,
      "Expecting graph object on stack on script line %ld",
      srcLine(lnum));
  }
  
  return cv.v.pGraph;
}

/*
 * core_pop_s function.
 */
SET *core_pop_s(long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  
  if (cv.tcode != CORE_T_SET) {
    raiseErr(__LINE__,
      "Expecting set object on stack on script line %ld",
      srcLine(lnum));
  }
  
  return cv.v.pSet;
}

/*
 * core_pop_a function.
 */
ART *core_pop_a(long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  
  if (cv.tcode != CORE_T_ART) {
    raiseErr(__LINE__,
      "Expecting articulation object on stack on script line %ld",
      srcLine(lnum));
  }
  
  return cv.v.pArt;
}

/*
 * core_pop_r function.
 */
RULER *core_pop_r(long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  
  if (cv.tcode != CORE_T_RULER) {
    raiseErr(__LINE__,
      "Expecting ruler object on stack on script line %ld",
      srcLine(lnum));
  }
  
  return cv.v.pRuler;
}

/*
 * core_pop_p function.
 */
POINTER *core_pop_p(long lnum) {
  CORE_VARIANT cv;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  
  if (cv.tcode != CORE_T_POINTER) {
    raiseErr(__LINE__,
      "Expecting pointer object on stack on script line %ld",
      srcLine(lnum));
  }
  
  return cv.v.pPointer;
}
