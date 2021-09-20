/*
 * infrared.c
 * ==========
 * 
 * See Infrared.md in the doc directory for further information.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nmf.h"

/* Lua headers */
#include <lua.h>
#include <lauxlib.h>

/*
 * Constants
 * ---------
 */

/*
 * Range of valid Retro instrument indices.
 * 
 * The minimum is always one.  The greatest is this value.  This should
 * match INSTR_MAXCOUNT in instr.h of Retro.
 */
#define INSTR_MAX (4096)

/*
 * Range of valid Retro layer indices.
 * 
 * The minimum is always one.  The greatest is this value.  This should
 * match LAYER_MAXCOUNT in layer.h of Retro.
 */
#define RLAYER_MAX (16384)

/*
 * Maximum number of characters in a line in the template file,
 * including the terminating nul but not including any line break
 * characters.
 */
#define TMAX_LINE (4094)

/*
 * Maximum number of characters including terminating nul and the grave
 * accent and the semicolon in a cue escape.
 */
#define TMAX_CUE (256)

/*
 * Type declarations
 * -----------------
 */

/*
 * CUE structure for storing cue data.
 */
typedef struct {
  
  /*
   * The time offset of the cue.
   */
  int32_t t;
  
  /*
   * The number of the cue within the section.
   */
  int32_t num;
  
  /*
   * The section the cue belongs to.
   */
  uint16_t sect;
  
} CUE;

/*
 * CUE_LINK structure with prototype beforehand for self-reference.
 * 
 * This is used for building a single-linked list while parsing the NMF
 * file.
 */
struct CUE_LINK_TAG;
typedef struct CUE_LINK_TAG CUE_LINK;
struct CUE_LINK_TAG {
  
  /*
   * Pointer to next cue link, or NULL if this is last link.
   */
  CUE_LINK *pNext;
  
  /*
   * The cue stored in this link.
   */
  CUE c;
};

/*
 * RETRO_LINK structure with prototype beforehand for self-reference.
 * 
 * This is used for storing the chain of generated Retro note events.
 */
struct RETRO_LINK_TAG;
typedef struct RETRO_LINK_TAG RETRO_LINK;
struct RETRO_LINK_TAG {
  
  /*
   * Pointer to next Retro event link, or NULL if this is last link.
   */
  RETRO_LINK *pNext;
  
  /*
   * Start offset of the event.
   */
  int32_t start;
  
  /*
   * Duration of the event.
   */
  int32_t dur;

  /*
   * Instrument for event.
   */
  uint16_t instr;
  
  /*
   * Retro layer for event.
   */
  uint16_t layer;
  
  /*
   * Pitch for event.
   */
  int8_t pitch;
};

/*
 * Local data
 * ----------
 */

/*
 * The name of the executing module.
 * 
 * Used for error reports.  Set at the start of the program entrypoint.
 */
static const char *pModule = NULL;

/*
 * The CR+LF flag.
 * 
 * This is set during the program entrypoint.  If it is zero, then line
 * breaks on output will be LF.  If it is non-zero, then line breaks on
 * output will be CR+LF.
 */
static int m_crlf = 0;

/*
 * The UTF-8 BOM flag.
 * 
 * Set to non-zero if there was a UTF-8 Byte Order Mark (BOM) at the
 * beginning of input that should be transferred to output.
 */
static int m_bom = 0;

/*
 * Line number, for use in error reports for template file.
 */
static int32_t m_line = 0;

/*
 * Flag indicating whether cue mode is active in the template file.
 * 
 * Non-zero if cue mode is active, zero if cue mode is off.
 */
static int m_cue_mode = 0;

/*
 * The last section that was used in a full cue, or -1 if no full cues
 * have been used yet.
 */
static int32_t m_last_sect = -1;

/*
 * The number of cues that have been defined in the NMF file.
 */
static int32_t m_cue_count = 0;

/*
 * The linked list of cues received from the NMF file, in no particular
 * order.
 */
static CUE_LINK *m_pcue_chain = NULL;

/*
 * Pointer to a sorted array of cues, created on first call to the
 * print_cue() function.
 * 
 * Also has the number of cues, if the pointer is non-NULL.
 */
static CUE *m_pcue = NULL;
static int32_t m_pcue_count = 0;

/*
 * Pointer to start and end of the Retro event list.
 */
static RETRO_LINK *m_rfirst = NULL;
static RETRO_LINK *m_rlast = NULL;

/*
 * Pointer to the Lua interpreter state object, or NULL if not
 * initialized yet.
 */
static lua_State *m_L = NULL;

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static int cue_cmp(const void *pa, const void *pb);

static void print_char(int c);
static void print_dec(int32_t v, int top);
static int print_cue(int32_t sect, int32_t cue_num);

static void process_stream(void);
static int process_cue(unsigned char *pcue);
static int process_line(unsigned char *pstr);

static int read_byte(FILE *pIn);
static int skip_bom(FILE *pIn);

static void cue_event(int32_t t, int32_t sect, int32_t cue);
static void note_event(
    int32_t start,
    int32_t dur,
    int32_t pitch,
    int32_t instr,
    int32_t layer);
static int c_callback(lua_State* L);

static int init_lua(const char *pScript);
static void free_lua(void);
static int render_note(
    int32_t rate,
    int32_t t,
    int32_t dur,
    int32_t pitch,
    int32_t art,
    int32_t sect,
    int32_t layer);

/*
 * Compare two cues.
 * 
 * This function matches the interface of the comparison function to
 * both qsort() and bsearch().  It only compares the sect and num fields
 * for purposes of ordering and ignores the t field.
 * 
 * Parameters:
 * 
 *   pa - the first cue structure pointer
 * 
 *   pb - the second cue structure pointer
 * 
 * Return:
 * 
 *   greater than zero, zero, or less than zero as the first structure
 *   is greater than, equal to, or less than the second structure
 */
static int cue_cmp(const void *pa, const void *pb) {
  
  const CUE *pca = NULL;
  const CUE *pcb = NULL;
  int result = 0;
  
  /* Check parameters */
  if ((pa == NULL) || (pb == NULL)) {
    abort();
  }
  
  /* Cast to type */
  pca = (const CUE *) pa;
  pcb = (const CUE *) pb;
  
  /* Compare first by section */
  if (pca->sect > pcb->sect) {
    result = 1;
    
  } else if (pca->sect < pcb->sect) {
    result = -1;
    
  } else if (pca->sect == pcb->sect) {
    /* Compare next by number */
    if (pca->num > pcb->num) {
      result = 1;
      
    } else if (pca->num < pcb->num) {
      result = -1;
      
    } else if (pca->num == pcb->num) {
      result = 0;
      
    } else {
      /* Shouldn't happen */
      abort();
    }
    
  } else {
    /* Shouldn't happen */
    abort();
  }
  
  /* Return result */
  return result;
}

/*
 * Print a character to output.
 * 
 * c is the unsigned character value to print, which must be in range
 * [0, 255].
 * 
 * If m_bom flag is active, this function will insert a UTF-8 BOM before
 * outputting the given character and then clear the m_bom flag.
 * 
 * If m_crlf flag is active, then each LF character will automatically
 * have a CR character prefixed.
 * 
 * I/O errors cause faults.
 * 
 * Parameters:
 * 
 *   c - the character to write
 */
static void print_char(int c) {
  
  /* Check parameter */
  if ((c < 0) || (c > 255)) {
    abort();
  }
  
  /* If BOM flag active, clear it and recursively print the BOM */
  if (m_bom) {
    m_bom = 0;
    print_char(0xef);
    print_char(0xbb);
    print_char(0xbf);
  }
  
  /* If LF given and CRLF mode active, recursively print a CR */
  if ((c == '\n') && m_crlf) {
    print_char('\r');
  }
  
  /* Print the character */
  if (putchar(c) != c) {
    abort();  /* I/O error */
  }
}

/*
 * Recursively print a decimal number.
 * 
 * Set top to non-zero only for the first call, or a recursive call
 * after printing a sign.
 * 
 * Fault if the least negative value.
 * 
 * Parameters:
 * 
 *   v - the value to print
 * 
 *   top - non-zero if top-level call, zero if recursive call
 */
static void print_dec(int32_t v, int top) {
  
  /* Check parameters */
  if (v <= INT32_MIN) {
    abort();
  }
  
  /* If value is negative, print negative sign and then recursively
   * print the positive value */
  if (v < 0) {
    print_char('-');
    print_dec(-(v), 1);
    return;
  }
  
  /* If value is zero, print zero if top-level call, and then return in
   * all cases */
  if (v == 0) {
    if (top) {
      print_char('0');
    }
    return;
  }
  
  /* Otherwise, if value less than 10, print it and return */
  if (v < 10) {
    print_char('0' + ((int) v));
    return;
  }
  
  /* Otherwise, recursive solution */
  print_dec(v / 10, 0);
  print_char('0' + ((int) (v % 10)));
}

/*
 * Look up and print a given cue.
 * 
 * This generates the sorted cue table if not already generated.
 * 
 * Error messages printed to stderr.
 * 
 * Parameters:
 * 
 *   sect - the section of the cue
 * 
 *   cue_num - the number of the cue
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int print_cue(int32_t sect, int32_t cue_num) {
  
  int status = 1;
  CUE_LINK *pl = NULL;
  int32_t i = 0;
  CUE *pc = NULL;
  CUE c;
  
  /* Check sect parameter */
  if ((sect < 0) || (sect >= NMF_MAXSECT)) {
    abort();
  }
  
  /* Initialize structures */
  memset(&c, 0, sizeof(CUE));
  
  /* Generate the sorted cue table if necessary */
  if ((m_pcue == NULL) && (m_cue_count > 0)) {
    
    /* First of all, allocate space for the table using the cue count */
    m_pcue = (CUE *) calloc((size_t) m_cue_count, sizeof(CUE));
    if (m_pcue == NULL) {
      abort();
    }
    
    /* Go through the cue link chain and copy all to the cue table */
    i = 0;
    for(pl = m_pcue_chain; pl != NULL; pl = pl->pNext) {
      memcpy(&(m_pcue[i]), &(pl->c), sizeof(CUE));
      i++;
    }
    
    /* Copy the count to the other count field */
    m_pcue_count = m_cue_count;
    
    /* If more than one cue, sort the table and check for duplicates */
    if (m_cue_count > 1) {
      qsort(m_pcue, (size_t) m_cue_count, sizeof(CUE), &cue_cmp);
      for(i = 1; i < m_cue_count; i++) {
        if (cue_cmp(&(m_pcue[i]), &(m_pcue[i - 1])) == 0) {
          status = 0;
          fprintf(stderr, "%s: Duplicate cues found!\n", pModule);
          break;
        }
      }
    }
  }
  
  /* If we have a sorted table, look up the cue */
  if (status && (m_pcue != NULL)) {
    c.sect = (uint16_t) sect;
    c.num = cue_num;
    pc = bsearch(&c,
          m_pcue, (size_t) m_pcue_count, sizeof(CUE), &cue_cmp);
  }
  
  /* Error if we didn't find a matching cue */
  if (status && (pc == NULL)) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Failed to find matching cue!\n",
              pModule, (long) m_line);
  }
  
  /* If we got here successfully, print the found cue time */
  if (status) {
    print_dec(pc->t, 1);
  }
  
  /* Return status */
  return status;
}

/*
 * Output all the Retro note events.
 */
static void process_stream(void) {
  
  RETRO_LINK *pl = NULL;
  
  /* Go through all note events */
  for(pl = m_rfirst; pl != NULL; pl = pl->pNext) {
    /* First comes the time */
    print_dec(pl->start, 1);
    
    /* Next the duration */
    print_char(' ');
    print_dec(pl->dur, 1);
    
    /* Next the pitch */
    print_char(' ');
    print_dec(pl->pitch, 1);
    
    /* Next the instrument */
    print_char(' ');
    print_dec(pl->instr, 1);
    
    /* Next the layer */
    print_char(' ');
    print_dec(pl->layer, 1);
    
    /* Finally the note operation followed by end of line */
    print_char(' ');
    print_char('n');
    print_char('\n');
  }
}

/*
 * Process a cue.
 * 
 * pcue points to a nul-terminated cue string that begins with a grave
 * accent and ends with a semicolon.  A fault may happen if this is not
 * the case.
 * 
 * This function will decode the cue, print any error messages to 
 * stderr if there is a failure, and otherwise will print to standard
 * output the offset of the cue as a decimal integer.
 * 
 * Parameters:
 * 
 *   pcue - the cue to decode
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int process_cue(unsigned char *pcue) {
  
  int status = 1;
  int32_t v = 0;
  int d = 0;
  
  int32_t sect = 0;
  int32_t cue_num = 0;
  
  /* Check parameter */
  if (pcue == NULL) {
    abort();
  }
  
  /* First character must be a grave accent */
  if (*pcue != '`') {
    abort();
  }
  
  /* Skip first character */
  pcue++;
  
  /* Current character must be a decimal digit */
  if ((*pcue < '0') || (*pcue > '9')) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Invalid cue format!\n",
              pModule, (long) m_line);
  }
  
  /* Decode current string of decimal digits */
  if (status) {
    v = 0;
    for( ; (*pcue >= '0') && (*pcue <= '9'); pcue++) {
      /* Multiply decoded number by 10, watching for overflow */
      if (v <= INT32_MAX / 10) {
        v = v * 10;
      } else {
        status = 0;
        fprintf(stderr, "%s: [Line %ld] Cue parameter overflow!\n",
                  pModule, (long) m_line);
        break;
      }
      
      /* Get current digit value */
      d = *pcue - '0';
      
      /* Add digit to result, watching for overflow */
      if (v <= INT32_MAX - d) {
        v = v + d;
      } else {
        status = 0;
        fprintf(stderr, "%s: [Line %ld] Cue parameter overflow!\n",
                  pModule, (long) m_line);
      }
    }
  }
  
  /* If we are at a dot now, we have a full cue; else, we should be at a
   * semicolon and we have a partial cue */
  if (status && (*pcue == '.')) {
    /* Full cue -- store decoded number as section */
    sect = v;
    
    /* Advance one and we should be on another decimal digit */
    pcue++;
    if ((*pcue < '0') || (*pcue > '9')) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Invalid cue format!\n",
                pModule, (long) m_line);
    }
    
    /* Decode current string of decimal digits */
    if (status) {
      v = 0;
      for( ; (*pcue >= '0') && (*pcue <= '9'); pcue++) {
        /* Multiply decoded number by 10, watching for overflow */
        if (v <= INT32_MAX / 10) {
          v = v * 10;
        } else {
          status = 0;
          fprintf(stderr, "%s: [Line %ld] Cue parameter overflow!\n",
                    pModule, (long) m_line);
          break;
        }
        
        /* Get current digit value */
        d = *pcue - '0';
        
        /* Add digit to result, watching for overflow */
        if (v <= INT32_MAX - d) {
          v = v + d;
        } else {
          status = 0;
          fprintf(stderr, "%s: [Line %ld] Cue parameter overflow!\n",
                    pModule, (long) m_line);
        }
      }
    }
    
    /* We should now be on a semicolon */
    if (status && (*pcue != ';')) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Invalid cue format!\n",
                pModule, (long) m_line);
    }
    
    /* There shouldn't be anything after the semicolon */
    if (status && (pcue[1] != 0)) {
      abort();
    }
    
    /* If we got here successfully, store recent decoded number as the
     * cue number and also store the section of this cue in the cache */
    if (status) {
      cue_num = v;
      m_last_sect = sect;
    }
    
  } else if (status && (*pcue == ';')) {
    /* Partial cue -- check that we have a cached section */
    if (m_last_sect < 0) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Can't use partial cue before any full cues!\n",
        pModule, (long) m_line);
    }
    
    /* Take section from cache and set cue number to the decoded
     * number */
    if (status) {
      sect = m_last_sect;
      cue_num = v;
    }
    
    /* Make sure nothing after semicolon */
    if (status && (pcue[1] != 0)) {
      abort();
    }
    
  } else if (status) {
    /* Invalid character follows sequence of digits (or nothing after
     * the sequence of digits) */
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Invalid cue format!\n",
              pModule, (long) m_line);
  }
  
  /* If we got here successfully, print the cue */
  if (status) {
    if (!print_cue(sect, cue_num)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Process a line of the template file.
 * 
 * pstr points to a nul-terminated line of text to process.  The line
 * must NOT include any line break characters at the end.
 * 
 * Errors are reported to stderr.
 * 
 * Parameters:
 * 
 *   pstr - the line to process
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int process_line(unsigned char *pstr) {
  
  int status = 1;
  unsigned char *pc = NULL;
  int ccount = 0;
  unsigned char cbuf[TMAX_CUE];
  
  /* Initialize buffers */
  memset(cbuf, 0, TMAX_CUE);
  
  /* Check parameter */
  if (pstr == NULL) {
    abort();
  }
  
  /* Check if first character of line is a grave accent */
  if (pstr[0] == '`') {
    /* Template command line -- only proceed if second character exists
     * and is neither space nor horizontal tab (otherwise line is a
     * comment line that is ignored) */
    if ((pstr[1] != 0) && (pstr[1] != ' ') && (pstr[1] != '\t')) {
      /* We have a non-comment template command line; first, handle
       * special case of a command escape */
      if (pstr[1] == '`') {
        /* Template escape, so print the line starting at the second
         * character to output, followed by a line break */
        for(pc = &(pstr[1]); *pc != 0; pc++) {
          print_char(*pc);
        }
        print_char('\n');
        
      } else {
        /* Template command that is neither a comment nor an escape --
         * all currently supported template commands have only a single
         * character after the grave accent, so make sure that anything
         * after the second character is either space or horizontal
         * tab */
        for(pc = &(pstr[2]); *pc != 0; pc++) {
          if ((*pc != ' ') && (*pc != '\t')) {
            status = 0;
            fprintf(stderr,
                      "%s: [Line %ld] Invalid template command!\n",
                      pModule, (long) m_line);
            break;
          }
        }
        
        /* Interpret command based on second character of line */
        if (status) {
          if (pstr[1] == 'S') {
            /* Stream command */
            process_stream();
            
          } else if (pstr[1] == 'C') {
            /* Turn cue mode on */
            m_cue_mode = 1;
            
          } else if (pstr[1] == 'c') {
            /* Turn cue mode off */
            m_cue_mode = 0;
            
          } else {
            /* Unrecognized command */
            status = 0;
            fprintf(stderr,
                      "%s: [Line %ld] Unrecognized template command!\n",
                      pModule, (long) m_line);
          }
        }
      }
    }
    
  } else {
    /* First character of line is not a grave accent -- if cue mode is
     * on, then interpret cues as printing line; if cue mode is off,
     * then print all line characters, followed by a line break */
    if (m_cue_mode) {
      /* Cue mode is on -- print the line, followed by line break, but
       * interpret cues and grave accent escapes */
      for(pc = pstr; *pc != 0; pc++) {
        /* If this is not a grave accent, print it and continue on */
        if (*pc != '`') {
          print_char(*pc);
          continue;
        }
        
        /* We have a grave accent -- if next character is also a grave
         * accent, then print a single grave accent and skip the next
         * character */
        if (pc[1] == '`') {
          print_char('`');
          pc++;
          continue;
        }
        
        /* We have an actual cue, so store it in the buffer */
        cbuf[0] = (unsigned char) '`';
        ccount = 1;
        for(pc++ ; (*pc != ';') && (*pc != '`') && (*pc != 0); pc++) {
          if (ccount < TMAX_CUE - 1) {
            cbuf[ccount] = *pc;
            ccount++;
          } else {
            status = 0;
            fprintf(stderr, "%s: [Line %ld] Cue is too long!\n",
                      pModule, (long) m_line);
            break;
          }
        }
        
        /* Check what we stopped on */
        if (status && (*pc == ';')) {
          /* We stopped on semicolon, so add that to the buffer and then
           * add the terminating nul */
          if (ccount < TMAX_CUE - 1) {
            cbuf[ccount] = (unsigned char) ';';
            ccount++;
          }
          cbuf[ccount] = (unsigned char) 0;
          
        } else if (status && (*pc == '`')) {
          /* We stopped on grave accent, so error */
          status = 0;
          fprintf(stderr, "%s: [Line %ld] Grave accent within cue!\n",
                    pModule, (long) m_line);
          
        } else if (status && (*pc == 0)) {
          /* We stopped at end of line, so missing semicolon */
          status = 0;
          fprintf(stderr, "%s: [Line %ld] Cue missing semicolon!\n",
                    pModule, (long) m_line);
          
        } else if (status) {
          /* Shouldn't happen */
          abort();
        }
        
        /* Process the cue */
        if (status) {
          if (!process_cue(cbuf)) {
            status = 0;
          }
        }
        
        /* Leave loop if error */
        if (!status) {
          break;
        }
      }
      if (status) {
        print_char('\n');
      }
    
    } else {
      /* Cue mode is off -- print the line, followed by line break */
      for(pc = pstr; *pc != 0; pc++) {
        print_char(*pc);
      }
      print_char('\n');
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Read a byte from the given file handle.
 * 
 * The return value is the unsigned byte value.  However, if a byte with
 * value zero is read from the file, it is changed to byte value 127.
 * Therefore, the range of unsigned byte values is only 1-255.
 * 
 * If the special value zero is returned, it means End Of File (EOF).
 * 
 * If the value -1 is returned, it means there was an I/O error.
 * 
 * Parameters:
 * 
 *   pIn - the file handle to read from
 * 
 * Return:
 * 
 *   an unsigned byte value in range 1-255, zero if EOF, or -1 if I/O
 *   error
 */
static int read_byte(FILE *pIn) {
  
  int c = 0;
  
  /* Check parameter */
  if (pIn == NULL) {
    abort();
  }
  
  /* Read from the file */
  c = getc(pIn);
  if (c == 0) {
    /* Change zero byte values to 127 */
    c = 127;
    
  } else if (c == EOF) {
    /* Check whether EOF means true EOF or error */
    if (feof(pIn)) {
      /* True EOF */
      c = 0;
      
    } else {
      /* Error */
      c = -1;
    }
  }
  
  /* Return result */
  return c;
}

/*
 * Rewind the given file, skip over an initial UTF-8 Byte Order Mark
 * (BOM) if present, and set m_bom according to whether a BOM was
 * present.
 * 
 * Parameters:
 * 
 *   pIn - the file handle
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int skip_bom(FILE *pIn) {
  
  int status = 1;
  int retval = 0;
  
  /* Check parameter */
  if (pIn == NULL) {
    abort();
  }
  
  /* Reset BOM flag */
  m_bom = 0;
  
  /* Rewind to start of file */
  if (fseek(pIn, 0, SEEK_SET)) {
    status = 0;
  }
  
  /* Check if first byte of BOM is present */
  if (status) {
    retval = read_byte(pIn);
    if (retval == 0xef) {
      /* Read the first byte of the BOM -- check if second byte is
       * present */
      retval = read_byte(pIn);
      if (retval == 0xbb) {
        /* Read the second byte of the BOM -- check if third byte is
         * present */
        retval = read_byte(pIn);
        if (retval == 0xbf) {
          /* Read all three bytes of the BOM, so set the bom flag */
          m_bom = 1;
          
        } else if (retval < 0) {
          /* I/O error */
          status = 0;
        }
        
      } else if (retval < 0) {
        /* I/O error */
        status = 0;
      }
      
    } else if (retval < 0) {
      /* I/O error */
      status = 0;
    }
  }
  
  /* If we were successful but didn't read a BOM, rewind to start of
   * file again; otherwise, leave position after BOM */
  if (status && (m_bom == 0)) {
    if (fseek(pIn, 0, SEEK_SET)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Called for each cue event that is encountered in the NMF file.
 * 
 * Parameters:
 * 
 *   t - the sample offset of the cue, must be greater than or equal to
 *   zero
 * 
 *   sect - the section number of the cue, must be greater than or equal
 *   to zero, and less than NMF_MAXSECT
 * 
 *   cue - the cue number, must be greater than or equal to zero
 */
static void cue_event(int32_t t, int32_t sect, int32_t cue) {
  
  CUE_LINK *pl = NULL;
  
  /* Check parameters */
  if ((t < 0) || (sect < 0) || (sect >= NMF_MAXSECT) || (cue < 0)) {
    abort();
  }
  
  /* Allocate new cue link structure */
  pl = (CUE_LINK *) malloc(sizeof(CUE_LINK));
  if (pl == NULL) {
    abort();
  }
  memset(pl, 0, sizeof(CUE_LINK));
  
  /* Set next pointer to current chain root (which might be NULL) */
  pl->pNext = m_pcue_chain;
  
  /* Store the data in the cue */
  (pl->c).t = t;
  (pl->c).num = cue;
  (pl->c).sect = (uint16_t) sect;
  
  /* Add the new cue link to the start of the chain */
  m_pcue_chain = pl;
  
  /* Increment the cue link counter */
  if (m_cue_count < INT32_MAX) {
    m_cue_count++;
  } else {
    abort();  /* counter overflow */
  }
}

/*
 * Invoked from c_callback() each time the note rendering script
 * generates a Retro note event.
 * 
 * All parameters have already been checked by the caller; a fault
 * happens if any parameter is out of range.
 * 
 * Parameters:
 * 
 *   start - the starting sample offset
 * 
 *   dur - the duration in samples
 * 
 *   pitch - the pitch
 * 
 *   instr - the instrument number
 * 
 *   layer - the Retro layer number
 */
static void note_event(
    int32_t start,
    int32_t dur,
    int32_t pitch,
    int32_t instr,
    int32_t layer) {
  
  RETRO_LINK *pl = NULL;
  
  /* Check parameters */
  if ((start < 0) || (dur < 1) ||
      (pitch < NMF_MINPITCH) || (pitch > NMF_MAXPITCH) ||
      (instr < 1) || (instr > INSTR_MAX) ||
      (layer < 1) || (layer > RLAYER_MAX)) {
    abort();
  }
  
  /* Allocate new structure for the event */
  pl = (RETRO_LINK *) malloc(sizeof(RETRO_LINK));
  if (pl == NULL) {
    abort();
  }
  memset(pl, 0, sizeof(RETRO_LINK));
  
  /* Copy parameters into structure */
  pl->start = start;
  pl->dur = dur;
  pl->instr = (uint16_t) instr;
  pl->layer = (uint16_t) layer;
  pl->pitch = (int8_t) pitch;
  
  /* Link into event list */
  if (m_rlast == NULL) {
    /* List is empty, so set this as first and only event */
    pl->pNext = NULL;
    m_rfirst = pl;
    m_rlast = pl;
    
  } else {
    /* List not empty, so append to end of list */
    pl->pNext = NULL;
    m_rlast->pNext = pl;
    m_rlast = pl;
  }
}

/*
 * C function that is called back from the Lua note rendering script.
 * 
 * This matches the lua_CFunction function pointer type.
 * 
 * From Lua, this callback requires five parameters:
 * 
 *   (1) [Integer] Starting sample offset
 *   (2) [Integer] Duration in samples
 *   (3) [Integer] Pitch number
 *   (4) [Integer] Instrument number
 *   (5) [Integer] Layer number
 * 
 * The integer types may be anything that Lua can convert into an
 * integer using lua_tointegerx().
 * 
 * The callback does not return anything to Lua, except if there is an
 * error, in which case it returns an error message string.
 */
static int c_callback(lua_State* L) {
  
  lua_Integer p_start = 0;
  lua_Integer p_dur = 0;
  lua_Integer p_pitch = 0;
  lua_Integer p_instr = 0;
  lua_Integer p_layer = 0;
  
  int32_t i_start = 0;
  int32_t i_dur = 0;
  int32_t i_pitch = 0;
  int32_t i_instr = 0;
  int32_t i_layer = 0;
  
  int retval = 0;
  
  /* Check parameter */
  if (L == NULL) {
    abort();
  }
  
  /* We should have exactly five arguments on the Lua stack, else fail
   * with error */
  if (lua_gettop(L) != 5) {
    lua_pushliteral(L, "Wrong number of parameters");
    lua_error(L);
  }
  
  /* Get all the arguments, attempting to convert to integers */
  p_start = lua_tointegerx(L, 1, &retval);
  if (!retval) {
    lua_pushliteral(L, "Wrong parameter types");
    lua_error(L);
  }
  
  p_dur = lua_tointegerx(L, 2, &retval);
  if (!retval) {
    lua_pushliteral(L, "Wrong parameter types");
    lua_error(L);
  }
  
  p_pitch = lua_tointegerx(L, 3, &retval);
  if (!retval) {
    lua_pushliteral(L, "Wrong parameter types");
    lua_error(L);
  }
  
  p_instr = lua_tointegerx(L, 4, &retval);
  if (!retval) {
    lua_pushliteral(L, "Wrong parameter types");
    lua_error(L);
  }
  
  p_layer = lua_tointegerx(L, 5, &retval);
  if (!retval) {
    lua_pushliteral(L, "Wrong parameter types");
    lua_error(L);
  }
  
  /* Cast all arguments to 32-bit integers */
  if ((p_start >= INT32_MIN) && (p_start <= INT32_MAX)) {
    i_start = (int32_t) p_start;
  } else {
    lua_pushliteral(L, "Parameter out of range");
    lua_error(L);
  }
  
  if ((p_dur >= INT32_MIN) && (p_dur <= INT32_MAX)) {
    i_dur = (int32_t) p_dur;
  } else {
    lua_pushliteral(L, "Parameter out of range");
    lua_error(L);
  }
  
  if ((p_pitch >= INT32_MIN) && (p_pitch <= INT32_MAX)) {
    i_pitch = (int32_t) p_pitch;
  } else {
    lua_pushliteral(L, "Parameter out of range");
    lua_error(L);
  }
  
  if ((p_instr >= INT32_MIN) && (p_instr <= INT32_MAX)) {
    i_instr = (int32_t) p_instr;
  } else {
    lua_pushliteral(L, "Parameter out of range");
    lua_error(L);
  }
  
  if ((p_layer >= INT32_MIN) && (p_layer <= INT32_MAX)) {
    i_layer = (int32_t) p_layer;
  } else {
    lua_pushliteral(L, "Parameter out of range");
    lua_error(L);
  }
  
  /* Check the ranges of each parameter */
  if (i_start < 0) {
    lua_pushliteral(L, "start parameter out of range");
    lua_error(L);
  }
  if (i_dur < 1) {
    lua_pushliteral(L, "dur parameter out of range");
    lua_error(L);
  }
  if ((i_pitch < NMF_MINPITCH) || (i_pitch > NMF_MAXPITCH)) {
    lua_pushliteral(L, "pitch parameter out of range");
    lua_error(L);
  }
  if ((i_instr < 1) || (i_instr > INSTR_MAX)) {
    lua_pushliteral(L, "instr parameter out of range");
    lua_error(L);
  }
  if ((i_layer < 1) || (i_layer > RLAYER_MAX)) {
    lua_pushliteral(L, "layer parameter out of range");
    lua_error(L);
  }
  
  /* Now we can call through to the note_event function */
  note_event(i_start, i_dur, i_pitch, i_instr, i_layer);
  
  /* If we made it here then the call was successful; return zero
   * because we have no result to return */
  return 0;
}

/*
 * Initialize the Lua interpreter, if not already initialized.
 * 
 * Interpreter should eventually be freed with free_lua().  Call does
 * nothing but return successfully if interpreter already allocated.
 * 
 * Errors print messages to stderr.
 * 
 * Parameters:
 * 
 *   pScript - the path to the note rendering script to load
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int init_lua(const char *pScript) {
  
  int status = 1;
  
  /* Check parameter */
  if (pScript == NULL) {
    abort();
  }
  
  /* Only proceed if not already allocated */
  if (m_L == NULL) {
    
    /* Allocate new Lua state */
    if (status) {
      m_L = luaL_newstate();
      if (m_L == NULL) {
        status = 0;
        fprintf(stderr, "%s: Failed to allocate Lua state!\n", pModule);
      }
    }
    
    /* Register c_callback as a function "retro_event" in Lua */
    if (status) {
      /* Push the C function onto the stack; no closure variables are
       * required */
      lua_pushcclosure(m_L, &c_callback, 0);
      
      /* Pop the C function from the interpreter stack and store it in
       * the global name "retro_event" */
      lua_setglobal(m_L, "retro_event");
    }
    
    /* Load the script file */
    if (status) {
      if (luaL_loadfile(m_L, pScript)) {
        status = 0;
        fprintf(stderr, "%s: Failed to load Lua script!\n", pModule);
      }
    }
    
    /* The compiled script file is now a function object on top of the
     * Lua stack; invoke it so all functions are registered and any
     * startup code is run */
    if (status) {
      if (lua_pcall(m_L, 0, 0, 0)) {
        status = 0;
        fprintf(stderr, "%s: Failed to run Lua script!\n", pModule);
      }
    }
    
    /* Make sure we have room for the function object, seven parameters,
     * and possibly one error message return value on the Lua stack */
    if (status) {
      if (!lua_checkstack(m_L, 9)) {
        status = 0;
        fprintf(stderr, "%s: Failed to grow the Lua stack!\n", pModule);
      }
    }
    
    /* If there was an error, free the Lua state if allocated */
    if (!status) {
      if (m_L != NULL) {
        lua_close(m_L);
        m_L = NULL;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Free the Lua interpreter, if allocated.
 * 
 * Call does nothing if interpreter is not currently allocated.
 */
static void free_lua(void) {
  
  /* Only proceed if interpreter allocated */
  if (m_L != NULL) {
    lua_close(m_L);
    m_L = NULL;
  }
}

/*
 * Render an NMF note by passing it through to the Lua note rendering
 * script.
 * 
 * You must initialize Lua first with init_lua().  Otherwise, a fault
 * occurs when you call this function.
 * 
 * The NMF data is passed to the Lua script with this function.  The
 * Lua script may then make use of the c_callback() to declare Retro
 * note events, which are passed to note_event().  See NoteRendering.md
 * in the doc directory for further information.
 * 
 * Errors are reported to stderr by this function.
 * 
 * Parameters:
 * 
 *   rate - the sampling rate, either 44100 or 48000
 * 
 *   t - the starting sample offset of the note, which must be zero or
 *   greater
 * 
 *   dur - the duration of the note, which must be greater than zero
 * 
 *   pitch - the pitch of the note, in [NMF_MINPITCH, NMF_MAXPITCH]
 * 
 *   art - the articulation, in [0, NMF_MAXART]
 * 
 *   sect - the section, in [0, NMF_MAXSECT - 1]
 * 
 *   layer - the NMF layer, in [1, UINT16_MAX + 1]
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int render_note(
    int32_t rate,
    int32_t t,
    int32_t dur,
    int32_t pitch,
    int32_t art,
    int32_t sect,
    int32_t layer) {
  
  int status = 1;
  
  /* Check state */
  if (m_L == NULL) {
    abort();
  }
  
  /* Check parameters */
  if ((rate != 44100) && (rate != 48000)) {
    abort();
  }
  if ((t < 0) || (dur < 1)) {
    abort();
  }
  if ((pitch < NMF_MINPITCH) || (pitch > NMF_MAXPITCH)) {
    abort();
  }
  if ((art < 0) || (art > NMF_MAXART)) {
    abort();
  }
  if ((sect < 0) || (sect >= NMF_MAXSECT)) {
    abort();
  }
  if ((layer < 1) || (layer > ((int32_t) UINT16_MAX) + 1)) {
    abort();
  }
  
  /* Push the Lua "note" function onto the interpreter stack */
  if (lua_getglobal(m_L, "note") != LUA_TFUNCTION) {
    status = 0;
    fprintf(stderr, "%s: Failed to find defined Lua note function!\n",
              pModule);
  }
  
  /* Push all the arguments onto the interpreter stack */
  if (status) {
    lua_pushinteger(m_L, rate);
    lua_pushinteger(m_L, t);
    lua_pushinteger(m_L, dur);
    lua_pushinteger(m_L, pitch);
    lua_pushinteger(m_L, art);
    lua_pushinteger(m_L, sect);
    lua_pushinteger(m_L, layer);
  }
  
  /* Call the function */
  if (status) {
    if (lua_pcall(m_L, 7, 0, 0)) {
      status = 0;
      fprintf(stderr, "%s: Failed to call Lua note function!\n",
                pModule);
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Program entrypoint
 * ------------------
 */

int main(int argc, char *argv[]) {
  
  int status = 1;
  int i = 0;
  int basis = 0;
  
  int arg_crlf = 0;
  const char *arg_pTemplatePath = NULL;
  const char *arg_pNMFPath = NULL;
  const char *arg_pScriptPath = NULL;
  
  NMF_DATA *pNMF = NULL;
  int32_t rate = 0;
  int32_t x = 0;
  int32_t count = 0;
  int32_t cue = 0;
  NMF_NOTE n;
  
  FILE *pIn = NULL;
  unsigned char *pbuf = NULL;
  int32_t rcount = 0;
  int c = 0;
  
  /* Initialize structures */
  memset(&n, 0, sizeof(NMF_NOTE));
  
  /* Set module name */
  pModule = NULL;
  if ((argc > 0) && (argv != NULL)) {
    pModule = argv[0];
  }
  if (pModule == NULL) {
    pModule = "infrared";
  }
  
  /* Check arguments */
  if (argc > 0) {
    if (argv == NULL) {
      abort();
    }
    for(i = 0; i < argc; i++) {
      if (argv[i] == NULL) {
        abort();
      }
    }
  }
  
  /* Must have three or four arguments beyond module name */
  if ((argc < 4) || (argc > 5)) {
    status = 0;
    fprintf(stderr, "%s: Wrong number of arguments!\n", pModule);
  }
  
  /* If four arguments beyond module name, first must be "-crlf"; also,
   * store the arguments in either case */
  if (status && (argc == 5)) {
    /* Check the -crlf argument and set flag */
    if (strcmp(argv[1], "-crlf") == 0) {
      arg_crlf = 1;
    } else {
      status = 0;
      fprintf(stderr, "%s: Argument syntax error!\n", pModule);
    }
    
    /* Store the rest of the arguments */
    if (status) {
      arg_pTemplatePath = argv[2];
      arg_pNMFPath = argv[3];
      arg_pScriptPath = argv[4];
    }
    
  } else if (status && (argc == 4)) {
    /* Store the arguments */
    arg_crlf = 0;
    arg_pTemplatePath = argv[1];
    arg_pNMFPath = argv[2];
    arg_pScriptPath = argv[3];
    
  } else if (status) {
    /* Shouldn't happen */
    abort();
  }
  
  /* Read and parse the NMF file into memory */
  if (status) {
    pNMF = nmf_parse_path(arg_pNMFPath);
    if (pNMF == NULL) {
      status = 0;
      fprintf(stderr, "%s: Failed to load NMF data!\n", pModule);
    }
  }
  
  /* Read the basis and determine sample rate, making sure it is a fixed
   * quantum basis */
  if (status) {
    basis = nmf_basis(pNMF);
    if (basis == NMF_BASIS_44100) {
      rate = 44100;
    } else if (basis == NMF_BASIS_48000) {
      rate = 48000;
    } else {
      status = 0;
      fprintf(stderr, "%s: NMF file must have a fixed quantum basis!\n",
                pModule);
    }
  }
  
  /* Initialize Lua interpreter and load note rendering script; error
   * messages are reported by the function */
  if (status) {
    if (!init_lua(arg_pScriptPath)) {
      status = 0;
    }
  }
  
  /* Go through all NMF notes, make sure there are no grace notes with
   * negative duration, decode any cue events and route to cue_event(),
   * and render all notes with the Lua note rendering script */
  if (status) {
    /* Get note count */
    count = nmf_notes(pNMF);
    
    /* Go through all NMF notes */
    for(x = 0; x < count; x++) {
      /* Get current NMF note */
      nmf_get(pNMF, x, &n);
      
      /* Check note type by duration */
      if (n.dur < 0) {
        /* Grace note, which is not allowed by infrared, so raise
         * error */
        status = 0;
        fprintf(stderr, "%s: No grace notes allowed in NMF data!\n",
                  pModule);
      
      } else if (n.dur == 0) {
        /* Cue note if pitch is zero; otherwise, ignore event */
        if (n.pitch == 0) {
          /* Decode the cue number */
          cue = ((int32_t) n.layer_i);
          cue = cue | (((int32_t) n.art) << 16);
          
          /* Report the cue */
          cue_event(n.t, n.sect, cue);
        }
      
      } else if (n.dur > 0) {
        /* Regular note, so render with the Lua script */
        if (!render_note(
                rate,
                n.t,
                n.dur,
                n.pitch,
                n.art,
                n.sect,
                ((int32_t) n.layer_i) + 1)) {
          status = 0;
        }
        
      } else {
        /* Shouldn't happen */
        abort();
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Release Lua interpreter if allocated */
  free_lua();
  
  /* Release NMF data if necessary */
  nmf_free(pNMF);
  pNMF = NULL;
  
  /* We have now rendered all notes to Retro note events passed through
   * the note_event() function, and recorded all cues through the
   * cue_event() function; next step is to open the template file for
   * reading */
  if (status) {
    pIn = fopen(arg_pTemplatePath, "rb");
    if (pIn == NULL) {
      status = 0;
      fprintf(stderr, "%s: Failed to open template file!\n", pModule);
    }
  }
  
  /* Skip over BOM if present and record whether it was present */
  if (status) {
    if (!skip_bom(pIn)) {
      status = 0;
      fprintf(stderr, "%s: I/O error reading template file!\n",
                pModule);
    }
  }
  
  /* Allocate line buffer */
  if (status) {
    pbuf = (unsigned char *) malloc(TMAX_LINE);
    if (pbuf == NULL) {
      abort();
    }
    memset(pbuf, 0, TMAX_LINE);
  }
  
  /* Process each line of the template file */
  while (status) {
  
    /* Increment line count unless already at maximum */
    if (m_line < INT32_MAX) {
      m_line++;
    }
  
    /* Keep adding characters to buffer until we encounter CR, LF, EOF,
     * I/O error, or buffer overflows */
    rcount = 0;
    for(c = read_byte(pIn);
        (c > 0) && (c != '\r') && (c != '\n');
        c = read_byte(pIn)) {
      if (rcount < TMAX_LINE - 1) {
        pbuf[rcount] = (unsigned char) c;
        rcount++;
      } else {
        status = 0;
        fprintf(stderr, "%s: [Line %ld] Line too long!\n",
                  pModule, (long) m_line);
        break;
      }
    }
    
    /* Check for I/O error */
    if (status && (c < 0)) {
      status = 0;
      fprintf(stderr, "%s: I/O error while reading template file!\n",
                pModule);
    }
    
    /* If we stopped on CR, read next character, which must be LF */
    if (status && (c == '\r')) {
      c = read_byte(pIn);
      if (c < 0) {
        status = 0;
        fprintf(stderr, "%s: I/O error while reading template file!\n",
                  pModule);
      } else if (c != '\n') {
        status = 0;
        fprintf(stderr, "%s: [Line %ld] CR must be followed by LF!\n",
                  pModule);
      }
    }
    
    /* If we are successful, add the terminating nul to the buffer and
     * call to process line */
    if (status) {
      pbuf[rcount] = (unsigned char) 0;
      if (!process_line(pbuf)) {
        status = 0;
      }
    }
    
    /* If we stopped on EOF, leave the loop */
    if (status && (c == 0)) {
      break;
    }
  }
  
  /* Free line buffer if allocated */
  if (pbuf != NULL) {
    free(pbuf);
    pbuf = NULL;
  }
  
  /* Close template file if open */
  if (pIn != NULL) {
    fclose(pIn);
    pIn = NULL;
  }
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
