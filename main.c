/*
 * main.c
 * ======
 * 
 * Main program module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   infrared [options] [script] < [input.nmf] > [output.mid]
 * 
 * Options
 * -------
 * 
 *   -map [path]
 * 
 * Generates a section map file at the given path.  The section map is a
 * text file that has one record per line.  Each record begins with an
 * NMF section number as an unsigned decimal, followed by a colon,
 * followed by the delta time offset of the NMF section in the generated
 * MIDI file as an unsigned decimal.
 * 
 * Requirements
 * ------------
 * 
 * May require the <math.h> library with -lm
 * 
 * Infrared consists of the following framework modules:
 * 
 *   - art.c
 *   - blob.c
 *   - control.c
 *   - core.c
 *   - diagnostic.c
 *   - graph.c
 *   - main.c
 *   - midi.c
 *   - pointer.c
 *   - primitive.c
 *   - render.c
 *   - ruler.c
 *   - set.c
 *   - text.c
 * 
 * Infrared has the following operation modules:
 * 
 *   - op_base.c
 * 
 * Infrared requires the following external libraries:
 * 
 *   - libnmf
 *   - librfdict
 *   - libshastina
 */

#include "main.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "blob.h"
#include "control.h"
#include "core.h"
#include "diagnostic.h"
#include "graph.h"
#include "midi.h"
#include "pointer.h"
#include "primitive.h"
#include "render.h"
#include "ruler.h"
#include "set.h"
#include "text.h"

#include "op_base.h"

#include "nmf.h"
#include "rfdict.h"
#include "shastina.h"

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
 * Module registration function
 * ============================
 */

/*
 * Invoke all the operation module registration functions so that all
 * the operations are registered with this main module.
 */
static void registerModules(void) {
  op_base_register();
}

/*
 * Constants
 * =========
 */

/*
 * The initial and maximum allocation of the operator registration
 * table.
 */
#define OP_INIT_CAP (32)
#define OP_MAX_CAP (16384)

/*
 * Type declarations
 * =================
 */

/*
 * Record structure for the operator table, which each store a callback
 * and the custom data to pass to that callback.
 */
typedef struct {
  main_fp_op fp;
  void *pCustom;
} OP_REC;

/*
 * Local data
 * ==========
 */

/*
 * The executable module name.
 */
static const char *pModule = NULL;

/*
 * The newline flag, which is used for the print and newline public
 * functions.
 * 
 * This is set initially, cleared after printing anything with the
 * main_print() function, and set after main_newline().
 */
static int m_newline = 1;

/*
 * The operator registration table.
 * 
 * m_op_cap is the current capacity in operator records.  m_op_len is
 * the number of operator records actually in use.
 * 
 * m_op is the dynamically allocated operator table array, which may be
 * NULL only if the capacity is zero.
 * 
 * m_op_map is the mapping of operator names to their indices in the
 * operator table array.
 */
static RFDICT *m_op_map = NULL;
static int32_t m_op_cap = 0;
static int32_t m_op_len = 0;
static OP_REC *m_op = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static long srcLine(long lnum);
static void shiftString(char *pStr);
static int validName(const char *pName);

static void capOp(int32_t n);
static void compileMap(NMF_DATA *pd, const char *pPath);

static void runString(SNENTITY *pEnt, long lnum);
static void runNumeric(SNENTITY *pEnt, long lnum);
static void runScript(SNSOURCE *pSrc);

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
 * Shift all the characters in a nul-terminated string to the left, so
 * that the first character of the string is deleted.
 * 
 * Parameters:
 * 
 *   pStr - the string to modify
 */
static void shiftString(char *pStr) {
  if (pStr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  for( ; *pStr != 0; pStr++) {
    pStr[0] = pStr[1];
  }
}

/*
 * Check whether a given name is a valid operation name.
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
 * Make room in capacity for a given number of elements in the operator
 * table.
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
 */
static void capOp(int32_t n) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_op_cap < 1) {
      m_op = (OP_REC *) calloc(
                (size_t) OP_INIT_CAP, sizeof(OP_REC));
      if (m_op == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_op_cap = OP_INIT_CAP;
      m_op_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_op_len) {
      target = m_op_len + n;
    } else {
      raiseErr(__LINE__, "Operator table capacity exceeded");
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_op_cap) {
      /* Check that target within maximum capacity */
      if (target > OP_MAX_CAP) {
        raiseErr(__LINE__, "Operator table capacity exceeded");
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_op_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > OP_MAX_CAP) {
        new_cap = OP_MAX_CAP;
      }
      
      /* Expand capacity */
      m_op = (OP_REC *) realloc(m_op,
                            ((size_t) new_cap) * sizeof(OP_REC));
      if (m_op == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_op[m_op_cap]),
        0,
        ((size_t) (new_cap - m_op_cap)) * sizeof(OP_REC));
      
      m_op_cap = new_cap;
    }
  }
}

/*
 * Compile a section map to the given output file path.
 * 
 * Makes use of the pointer module and the event range in the MIDI
 * module, so this should be done just before compiling the MIDI file.
 * 
 * Parameters:
 * 
 *   pd - the parsed NMF data
 * 
 *   pPath - the path to the map file to generate
 */
static void compileMap(NMF_DATA *pd, const char *pPath) {
  FILE *fh = NULL;
  POINTER *pp = NULL;
  int32_t i = 0;
  int64_t r = 0;
  
  if ((pd == NULL) || (pPath == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  fh = fopen(pPath, "w");
  if (fh == NULL) {
    raiseErr(__LINE__, "Failed to create file: %s", pPath);
  }
  
  pp = pointer_new();
  for(i = 0; i < nmf_sections(pd); i++) {
    pointer_reset(pp);
    pointer_jump(pp, i, -1);
    pointer_moment(pp, -1, -1);
    
    r = ((int64_t) (pointer_compute(pp, -1) / 3));
    r = r - ((int64_t) midi_range_lower());
    
    if ((r < INT32_MIN) || (r > INT32_MAX)) {
      raiseErr(__LINE__, "Section offset out of range");
    }
    
    fprintf(fh, "%ld:%ld\n", (long) i, (long) r);
  }
  
  if (fclose(fh)) {
    sayWarn(__LINE__, "Failed to close file");
  }
  fh = NULL;
}

/*
 * Interpret a Shastina string entity.
 * 
 * Parameters:
 * 
 *   pEnt - the string entity to interpret
 * 
 *   lnum - the script line number of the string entity
 */
static void runString(SNENTITY *pEnt, long lnum) {
  
  BLOB *pBlob = NULL;
  TEXT *pText = NULL;
  char *pc = NULL;
  
  /* Check parameters */
  if (pEnt == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (pEnt->status != SNENTITY_STRING) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Handle the different string types */
  if (pEnt->str_type == SNSTRING_CURLY) {
    /* Curly string -- check no string prefix */
    if (strlen(pEnt->pKey) > 0) {
      raiseErr(__LINE__,
        "String prefixes not supported on script line %ld",
        srcLine(lnum));
    }
   
    /* Parse string text as base-16 and push blob onto stack */
    pBlob = blob_fromHex(pEnt->pValue, lnum);
    core_push_b(pBlob, lnum);
    
  } else if (pEnt->str_type == SNSTRING_QUOTED) {
    /* Quoted string -- check no string prefix */
    if (strlen(pEnt->pKey) > 0) {
      raiseErr(__LINE__,
        "String prefixes not supported on script line %ld",
        srcLine(lnum));
    }
    
    /* Go through string literal, check each character in range, and
     * replace escape sequences with the escaped literals */
    for(pc = pEnt->pValue; *pc != 0; pc++) {
      /* Check character in range */
      if ((*pc < 0x20) || (*pc > 0x7e)) {
        raiseErr(__LINE__,
          "String literal has bad characters on script line %ld",
          srcLine(lnum));
      }
      
      /* Handle escape sequences */
      if (*pc == '\\') {
        /* Make sure next character is either another backslash or a
         * double quote */
        if ((pc[1] != '\\') && (pc[1] != '"')) {
          raiseErr(__LINE__,
            "String literal has invalid escapes on script line %ld",
            srcLine(lnum));
        }
        
        /* Shift the string to the left and leave the pointer as it is
         * so we will skip the escaped character */
        shiftString(pc);
      }
    }
    
    /* Create a text literal and push it onto stack */
    pText = text_literal(pEnt->pValue, lnum);
    core_push_t(pText, lnum);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * Interpret a Shastina numeric entity.
 * 
 * Parameters:
 * 
 *   pEnt - the string entity to interpret
 * 
 *   lnum - the script line number of the numeric entity
 */
static void runNumeric(SNENTITY *pEnt, long lnum) {
  
  size_t slen = 0;
  int suf = 0;
  int is_neg = 0;
  int c = 0;
  int32_t iv = 0;
  char *pc = NULL;
  POINTER *pp = NULL;
  
  /* Check parameters */
  if (pEnt == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (pEnt->status != SNENTITY_NUMERIC) {
    raiseErr(__LINE__, NULL);
  }
  
  /* If numeric ends with a lowercase letter, store this suffix and drop
   * from the literal */
  slen = strlen(pEnt->pKey);
  if (slen > 0) {
    if (((pEnt->pKey)[slen - 1] >= 'a') &&
        ((pEnt->pKey)[slen - 1] <= 'z')) {
      suf = (pEnt->pKey)[slen - 1];
      (pEnt->pKey)[slen - 1] = (char) 0;
    }
  }
  
  /* Start numeric parsing at start of literal */
  pc = pEnt->pKey;
  
  /* If first character is a sign, update is_neg flag and move beyond
   * it */
  is_neg = 0;
  if (*pc == '+') {
    is_neg = 0;
    pc++;
    
  } else if (*pc == '-') {
    is_neg = 1;
    pc++;
  }
  
  /* Make sure remaining numeric string is not empty */
  if (*pc == 0) {
    raiseErr(__LINE__, "Invalid numeric literal on script line %ld",
      srcLine(lnum));
  }
  
  /* Parse integer value */
  iv = 0;
  for( ; *pc != 0; pc++) {
    c = *pc;
    if ((c < '0') || (c > '9')) {
      raiseErr(__LINE__, "Invalid numeric literal on script line %ld",
        srcLine(lnum));
    }
    c = c - '0';
    
    if (iv <= PRIMITIVE_INT_MAX / 10) {
      iv *= 10;
    } else {
      raiseErr(__LINE__,
        "Numeric literal out of range on script line %ld",
        srcLine(lnum));
    }
    
    if (iv <= PRIMITIVE_INT_MAX - c) {
      iv += ((int32_t) c);
    } else {
      raiseErr(__LINE__,
        "Numeric literal out of range on script line %ld",
        srcLine(lnum));
    }
  }
  
  /* Apply sign */
  if (is_neg) {
    iv = 0 - iv;
  }
  
  /* Perform appropriate action depending on suffix */
  if (suf == 0) {
    /* No suffix, so just push the integer literal */
    core_push_i(iv, lnum);
    
  } else {
    /* We have a suffix, so first we need to pop a pointer off the
     * stack */
    pp = core_pop_p(lnum);
    
    /* If the suffix is anything other than "s", make sure that this is
     * not a header pointer */
    if (suf != 's') {
      if (pointer_isHeader(pp)) {
        raiseErr(__LINE__,
          "Can't adjust header pointer on script line %ld",
          srcLine(lnum));
      }
    }
    
    /* Adjust the pointer based on suffix */
    if (suf == 's') {
      pointer_jump(pp, iv, lnum);
      
    } else if (suf == 'q') {
      pointer_seek(pp, iv, lnum);
      
    } else if (suf == 'r') {
      pointer_advance(pp, iv, lnum);
      
    } else if (suf == 'g') {
      pointer_grace(
        pp,
        iv,
        core_rstack_current(lnum),
        lnum);
      
    } else if (suf == 't') {
      pointer_tilt(pp, iv, lnum);
      
    } else if (suf == 'm') {
      pointer_moment(pp, iv, lnum);
      
    } else {
      raiseErr(__LINE__,
        "Unsupported numeric suffix on script line %ld",
        srcLine(lnum));
    }
    
    /* Push updated pointer back on stack */
    core_push_p(pp, lnum);
  }
}

/*
 * Run an Infrared script using the given source file.
 * 
 * The pointer system must be initialized before calling this function,
 * and all operation modules should already be registered with this main
 * module.
 * 
 * This function will shut down the core module, but not other modules.
 * 
 * Parameters:
 * 
 *   pSrc - the Shastina source for the Infrared script to run
 */
static void runScript(SNSOURCE *pSrc) {
  
  SNPARSER *pp = NULL;
  SNENTITY ent;
  long retval = 0;
  
  /* Initialize structures */
  memset(&ent, 0, sizeof(SNENTITY));
  
  /* Check parameters */
  if (pSrc == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Get a parser */
  pp = snparser_alloc();
  
  /* Parse the header */
  snparser_read(pp, &ent, pSrc);
  if (ent.status != SNENTITY_BEGIN_META) {
    raiseErr(__LINE__, "Failed to read Infrared script header");
  }
  
  snparser_read(pp, &ent, pSrc);
  if (ent.status != SNENTITY_META_TOKEN) {
    raiseErr(__LINE__, "Failed to read Infrared script header");
  }
  if (strcmp(ent.pKey, "infrared") != 0) {
    raiseErr(__LINE__, "Failed to read Infrared script header");
  }
  
  snparser_read(pp, &ent, pSrc);
  if (ent.status != SNENTITY_END_META) {
    if (ent.status > 0) {
      raiseErr(__LINE__, "Unsupported Infrared script version");
    } else {
      raiseErr(__LINE__, "Failed to read Infrared script header");
    }
  }
  
  /* Interpret all the script entities */
  for(snparser_read(pp, &ent, pSrc);
      ent.status > 0;
      snparser_read(pp, &ent, pSrc)) {
    
    if (ent.status == SNENTITY_STRING) {
      runString(&ent, snparser_count(pp));
      
    } else if (ent.status == SNENTITY_NUMERIC) {
      runNumeric(&ent, snparser_count(pp));
      
    } else if (ent.status == SNENTITY_VARIABLE) {
      core_declare(0, ent.pKey, snparser_count(pp));
      
    } else if (ent.status == SNENTITY_CONSTANT) {
      core_declare(1, ent.pKey, snparser_count(pp));
      
    } else if (ent.status == SNENTITY_ASSIGN) {
      core_assign(ent.pKey, snparser_count(pp));
      
    } else if (ent.status == SNENTITY_GET) {
      core_get(ent.pKey, snparser_count(pp));
      
    } else if (ent.status == SNENTITY_BEGIN_GROUP) {
      core_begin_group(snparser_count(pp));
      
    } else if (ent.status == SNENTITY_END_GROUP) {
      core_end_group(snparser_count(pp));
      
    } else if (ent.status == SNENTITY_ARRAY) {
      if (ent.count > PRIMITIVE_INT_MAX) {
        raiseErr(__LINE__, "Array count too high on script line %ld",
          srcLine(snparser_count(pp)));
      }
      core_push_i((int32_t) ent.count, snparser_count(pp));
      
    } else if (ent.status == SNENTITY_OPERATION) {
      if (!validName(ent.pKey)) {
        raiseErr(__LINE__, "Invalid operation '%s' on script line %ld",
          srcLine(snparser_count(pp)));
      }
      if (m_op_map == NULL) {
        m_op_map = rfdict_alloc(1);
      }
      retval = rfdict_get(m_op_map, ent.pKey, -1);
      if (retval < 0) {
        raiseErr(__LINE__, "Invalid operation '%s' on script line %ld",
          srcLine(snparser_count(pp)));
      }
      ((m_op[retval]).fp)((m_op[retval]).pCustom, snparser_count(pp));
    
    } else {
      raiseErr(__LINE__, "Unsupported Shastina entity type on line %ld",
        srcLine(snparser_count(pp)));
    }
  }
  if (ent.status < 0) {
    raiseErr(__LINE__, "Shastina parsing error on line %ld: %s",
      srcLine(snparser_count(pp)), snerror_str(ent.status));
  }
  
  /* Shutdown core module and verify that in valid state */
  core_shutdown();
  
  /* Release parser */
  snparser_free(pp);
  pp = NULL;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * main_op function.
 */
void main_op(const char *pKey, main_fp_op fp, void *pCustom) {
  if ((pKey == NULL) || (fp == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if (!validName(pKey)) {
    raiseErr(__LINE__, "Invalid operation name registered");
  }
  
  if (m_op_map == NULL) {
    m_op_map = rfdict_alloc(1);
  }
  
  capOp(1);
  if (!rfdict_insert(m_op_map, pKey, (long) m_op_len)) {
    raiseErr(__LINE__, "Duplicate operation name registration: %s",
      pKey);
  }
  
  (m_op[m_op_len]).fp = fp;
  (m_op[m_op_len]).pCustom = pCustom;
  m_op_len++;
}

/*
 * main_print function.
 */
void main_print(CORE_VARIANT *pv, long lnum) {
  
  if (pv == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (m_newline) {
    m_newline = 0;
    fprintf(stderr, "%s: [Script line %ld] ", pModule, srcLine(lnum));
  }
  
  if (pv->tcode == CORE_T_NULL) {
    fprintf(stderr, "<null>");
    
  } else if (pv->tcode == CORE_T_INTEGER) {
    fprintf(stderr, "%ld", (long) (pv->v).iv);
    
  } else if (pv->tcode == CORE_T_TEXT) {
    fprintf(stderr, "%s", text_ptr((pv->v).pText));
    
  } else if (pv->tcode == CORE_T_BLOB) {
    blob_print((pv->v).pBlob, stderr);
    
  } else if (pv->tcode == CORE_T_GRAPH) {
    graph_print((pv->v).pGraph, stderr);
    
  } else if (pv->tcode == CORE_T_SET) {
    set_print((pv->v).pSet, stderr);
    
  } else if (pv->tcode == CORE_T_ART) {
    art_print((pv->v).pArt, stderr);
    
  } else if (pv->tcode == CORE_T_RULER) {
    ruler_print((pv->v).pRuler, stderr);
    
  } else if (pv->tcode == CORE_T_POINTER) {
    pointer_print((pv->v).pPointer, stderr);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * main_newline function.
 */
void main_newline(void) {
  fprintf(stderr, "\n");
  m_newline = 1;
}

/*
 * main_stop function.
 */
void main_stop(long lnum) {
  if (!m_newline) {
    main_newline();
  }
  
  fprintf(stderr,
    "\n%s: [Stopped on script line %ld]\n", pModule, srcLine(lnum));
  exit(EXIT_FAILURE);
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int i = 0;
  const char *pMapPath = NULL;
  const char *pScriptPath = NULL;
  NMF_DATA *pNMF = NULL;
  SNSOURCE *pSrc = NULL;
  FILE *hScript = NULL;
  
  /* Initialize diagnostics */
  diagnostic_startup(argc, argv, "infrared");
  
  /* Store the executable module name */
  if (argc > 0) {
    pModule = argv[0];
  } else {
    pModule = "infrared";
  }
  
  /* Register all operation modules */
  registerModules();
  
  /* If no program arguments, show help screen and end unsuccessfully */
  if (argc < 2) {
    fprintf(stderr, "Syntax:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  infrared [options] [script] < [nmf] > [midi]\n");
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
  }
  
  /* Interpret any options after the executable module name but before
   * the last parameter */
  for(i = 1; i <= argc - 2; i++) {
    if (strcmp(argv[i], "-map") == 0) {
      if (i >= argc - 2) {
        raiseErr(__LINE__, "Program argument syntax error");
      }
      if (pMapPath != NULL) {
        raiseErr(__LINE__, "Redefinition of -map program option");
      }
      pMapPath = argv[i + 1];
      i++;
      
    } else {
      raiseErr(__LINE__, "Unrecognized program option: %s", argv[i]);
    }
  }
  
  /* Last parameter is the script path */
  pScriptPath = argv[argc - 1];
  
  /* Parse the input NMF */
  pNMF = nmf_parse(stdin);
  if (pNMF == NULL) {
    raiseErr(__LINE__, "Failed to parse NMF input");
  }
  
  /* Initialize the pointer system */
  pointer_init(pNMF);
  
  /* Open the script file and wrap in a Shastina source */
  hScript = fopen(pScriptPath, "rb");
  if (hScript == NULL) {
    raiseErr(__LINE__, "Failed to open script file: %s", pScriptPath);
  }
  pSrc = snsource_stream(hScript, SNSTREAM_OWNER | SNSTREAM_RANDOM);
  
  /* Run the Infrared script */
  runScript(pSrc);
  
  /* If there is a script message that is missing a newline, add it */
  if (!m_newline) {
    m_newline = 1;
    fprintf(stderr, "\n");
  }
  
  /* Close the Shastina source */
  snsource_free(pSrc);
  pSrc = NULL;
  
  /* Render NMF to MIDI */
  render_nmf(pNMF);
  
  /* Add automatic controller tracking to MIDI */
  control_track();
  
  /* Generate map file if requested */
  if (pMapPath != NULL) {
    compileMap(pNMF, pMapPath);
  }
  
  /* Compile MIDI to output */
  midi_compile(stdout);
  
  /* Shut down modules */
  art_shutdown();
  blob_shutdown();
  core_shutdown();
  graph_shutdown();
  pointer_shutdown();
  ruler_shutdown();
  set_shutdown();
  text_shutdown();
  
  /* Free NMF object */
  nmf_free(pNMF);
  pNMF = NULL;
  
  /* If we got here, return successfully */
  return EXIT_SUCCESS;
}
