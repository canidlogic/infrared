/*
 * irtest.c
 * ========
 * 
 * Testing program for an Infrared note rendering script.
 * 
 * Syntax
 * ------
 * 
 *   irtest [script] [rate] [t] [dur] [pitch] [art] [sect] [layer]
 * 
 * [script] is the path to the Lua script containing the note rendering
 * script.
 * 
 * [rate] is the sampling rate in Hz, either 44100 or 48000. 
 * 
 * [t] is the starting time of the note, in samples.  Must be zero or
 * greater.
 * 
 * [dur] is the duration of the note, in samples.  Must be greater than
 * zero or less than zero (for a grace note offset).
 * 
 * [pitch] is the pitch of the note.  Zero is middle C, positive values
 * step up by equal-tempered semitones (half steps), and negative values
 * step down by equal-tempered semitones.  Valid range is all the keys
 * on an 88-key piano keyboard, which is [-39, 48].
 * 
 * [art] is the one-based articulation index.  Must be in range [0, 61].
 * 
 * [sect] is the section number the note belongs to.  Must be in the
 * range [0, 65534].
 * 
 * [layer] is the one-based NMF layer within the section that the note
 * belongs to.  It must be in range [1, 65536].  Note that this refers
 * to the NMF layer, not the Retro layer.
 * 
 * Operation
 * ---------
 * 
 * The given script file will be loaded as a note rendering script.  The
 * note rendering script will be called with the given command-line
 * parameters as input.  The output of the note rendering script will be
 * reported.
 * 
 * This allows one to test how a note rendering script renders specific
 * NMF notes.
 * 
 * Compilation
 * -----------
 * 
 * Compile with lua 5.4.
 * 
 * May also require the math library (for Lua) with -lm
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Lua headers */
#include <lua.h>
#include <lauxlib.h>

/*
 * Constants
 * ---------
 */

/*
 * Range of valid pitches.
 * 
 * Covers the full range of an 88-key piano keyboard, where zero is
 * middle C.
 */
#define PITCH_MIN (-39)
#define PITCH_MAX (48)

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
 * Range of NMF articulation, section, and layers.
 * 
 * The minimum of articulation and section is zero, of layers is one.
 */
#define ART_MAX (61)
#define SECT_MAX (INT32_C(65534))
#define NLAYER_MAX (INT32_C(65536))

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static int parseInt(const char *pstr, int32_t *pv);

static int note_event(
    int32_t start,
    int32_t dur,
    int32_t pitch,
    int32_t instr,
    int32_t layer);
static int c_callback(lua_State* L);

/*
 * Parse the given string as a signed integer.
 * 
 * pstr is the string to parse.
 * 
 * pv points to the integer value to use to return the parsed numeric
 * value if the function is successful.
 * 
 * In two's complement, this function will not successfully parse the
 * least negative value.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 *   pv - pointer to the return numeric value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int parseInt(const char *pstr, int32_t *pv) {
  
  int negflag = 0;
  int32_t result = 0;
  int status = 1;
  int32_t d = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* If first character is a sign character, set negflag appropriately
   * and skip it */
  if (*pstr == '+') {
    negflag = 0;
    pstr++;
  } else if (*pstr == '-') {
    negflag = 1;
    pstr++;
  } else {
    negflag = 0;
  }
  
  /* Make sure we have at least one digit */
  if (*pstr == 0) {
    status = 0;
  }
  
  /* Parse all digits */
  if (status) {
    for( ; *pstr != 0; pstr++) {
    
      /* Make sure in range of digits */
      if ((*pstr < '0') || (*pstr > '9')) {
        status = 0;
      }
    
      /* Get numeric value of digit */
      if (status) {
        d = (int32_t) (*pstr - '0');
      }
      
      /* Multiply result by 10, watching for overflow */
      if (status) {
        if (result <= INT32_MAX / 10) {
          result = result * 10;
        } else {
          status = 0; /* overflow */
        }
      }
      
      /* Add in digit value, watching for overflow */
      if (status) {
        if (result <= INT32_MAX - d) {
          result = result + d;
        } else {
          status = 0; /* overflow */
        }
      }
    
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Invert result if negative mode */
  if (status && negflag) {
    result = -(result);
  }
  
  /* Write result if successful */
  if (status) {
    *pv = result;
  }
  
  /* Return status */
  return status;
}

/*
 * Report that a note event has been generated from the note rendering
 * script.
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
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int note_event(
    int32_t start,
    int32_t dur,
    int32_t pitch,
    int32_t instr,
    int32_t layer) {
  
  /* Report parameters */
  printf("[Note event]\n");
  printf("start: %ld\n", (long) start);
  printf("dur  : %ld\n", (long) dur);
  printf("pitch: %ld\n", (long) pitch);
  printf("instr: %ld\n", (long) instr);
  printf("layer: %ld\n", (long) layer);
  
  /* Return successfully */
  return 1;
}

/*
 * C function that is called back from the Lua function.
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
  if ((i_pitch < PITCH_MIN) || (i_pitch > PITCH_MAX)) {
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
  if (!note_event(i_start, i_dur, i_pitch, i_instr, i_layer)) {
    lua_pushliteral(L, "Note event failed");
    lua_error(L);
  }
  
  /* If we made it here then the call was successful; return zero
   * because we have no result to return */
  return 0;
}

/*
 * Program entrypoint
 * ------------------
 */

int main(int argc, char *argv[]) {
  
  const char *pModule = NULL;
  const char *pScript = NULL;
  int32_t arg_rate = 0;
  int32_t arg_t = 0;
  int32_t arg_dur = 0;
  int32_t arg_pitch = 0;
  int32_t arg_art = 0;
  int32_t arg_sect = 0;
  int32_t arg_layer = 0;
  
  int status = 1;
  int i = 0;
  lua_State *L = NULL;
  
  /* Get module name */
  if ((argc > 0) && (argv != NULL)) {
    pModule = argv[0];
  }
  if (pModule == NULL) {
    pModule = "luatest";
  }
  
  /* Check that arguments are available */
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
  
  /* Check that exactly eight arguments beyond module name */
  if (argc != 9) {
    status = 0;
    fprintf(stderr, "%s: Wrong number of arguments!\n", pModule);
  }
  
  /* Get the script path */
  if (status) {
    pScript = argv[1];
  }
  
  /* Parse all the numeric parameters */
  if (status) {
    if (!parseInt(argv[2], &arg_rate)) {
      status = 0;
      fprintf(stderr, "%s: Failed to parse rate argument!\n", pModule);
    }
  }
  if (status) {
    if (!parseInt(argv[3], &arg_t)) {
      status = 0;
      fprintf(stderr, "%s: Failed to parse t argument!\n", pModule);
    }
  }
  if (status) {
    if (!parseInt(argv[4], &arg_dur)) {
      status = 0;
      fprintf(stderr, "%s: Failed to parse dur argument!\n", pModule);
    }
  }
  if (status) {
    if (!parseInt(argv[5], &arg_pitch)) {
      status = 0;
      fprintf(stderr, "%s: Failed to parse pitch argument!\n", pModule);
    }
  }
  if (status) {
    if (!parseInt(argv[6], &arg_art)) {
      status = 0;
      fprintf(stderr, "%s: Failed to parse art argument!\n", pModule);
    }
  }
  if (status) {
    if (!parseInt(argv[7], &arg_sect)) {
      status = 0;
      fprintf(stderr, "%s: Failed to parse sect argument!\n", pModule);
    }
  }
  if (status) {
    if (!parseInt(argv[8], &arg_layer)) {
      status = 0;
      fprintf(stderr, "%s: Failed to parse layer argument!\n", pModule);
    }
  }
  
  /* Check numeric argument ranges */
  if (status && (arg_rate != 44100) && (arg_rate != 48000)) {
    status = 0;
    fprintf(stderr, "%s: rate argument out of range!\n", pModule);
  }
  if (status && (arg_t < 0)) {
    status = 0;
    fprintf(stderr, "%s: t argument out of range!\n", pModule);
  }
  if (status && (arg_dur == 0)) {
    status = 0;
    fprintf(stderr, "%s: dur argument out of range!\n", pModule);
  }
  if (status && ((arg_pitch < PITCH_MIN) ||
                  (arg_pitch > PITCH_MAX))) {
    status = 0;
    fprintf(stderr, "%s: pitch argument out of range!\n", pModule);
  }
  if (status && ((arg_art < 0) || (arg_art > ART_MAX))) {
    status = 0;
    fprintf(stderr, "%s: art argument out of range!\n", pModule);
  }
  if (status && ((arg_sect < 0) || (arg_sect > SECT_MAX))) {
    status = 0;
    fprintf(stderr, "%s: sect argument out of range!\n", pModule);
  }
  if (status && ((arg_layer < 1) || (arg_layer > NLAYER_MAX))) {
    status = 0;
    fprintf(stderr, "%s: layer argument out of range!\n", pModule);
  }
  
  /* Allocate new Lua state */
  if (status) {
    L = luaL_newstate();
    if (L == NULL) {
      status = 0;
      fprintf(stderr, "%s: Failed to allocate Lua state!\n", pModule);
    }
  }
  
  /* Register c_callback as a function "retro_event" in Lua */
  if (status) {
    /* Push the C function onto the stack; no closure variables are
     * required */
    lua_pushcclosure(L, &c_callback, 0);
    
    /* Pop the C function from the interpreter stack and store it in
     * the global name "retro_event" */
    lua_setglobal(L, "retro_event");
  }
  
  /* Load the script file */
  if (status) {
    if (luaL_loadfile(L, pScript)) {
      status = 0;
      fprintf(stderr, "%s: Failed to load script!\n", pModule);
    }
  }
  
  /* The compiled script file is now a function object on top of the Lua
   * stack; invoke it so all functions are registered */
  if (status) {
    if (lua_pcall(L, 0, 0, 0)) {
      status = 0;
      fprintf(stderr, "%s: Failed to run script!\n", pModule);
    }
  }
  
  /* Make sure we have room for the function object, seven parameters,
   * and possibly one error message return value on the Lua stack */
  if (status) {
    if (!lua_checkstack(L, 9)) {
      status = 0;
      fprintf(stderr, "%s: Failed to grow the stack!\n", pModule);
    }
  }
  
  /* Run the "note" function in the Lua script */
  if (status) {
    /* Push the Lua "note" function onto the interpreter stack */
    if (lua_getglobal(L, "note") != LUA_TFUNCTION) {
      status = 0;
      fprintf(stderr, "%s: Failed to find defined note function!\n",
                pModule);
    }
    
    /* Push all the arguments onto the interpreter stack */
    if (status) {
      lua_pushinteger(L, arg_rate);
      lua_pushinteger(L, arg_t);
      lua_pushinteger(L, arg_dur);
      lua_pushinteger(L, arg_pitch);
      lua_pushinteger(L, arg_art);
      lua_pushinteger(L, arg_sect);
      lua_pushinteger(L, arg_layer);
    }
    
    /* Call the function */
    if (status) {
      if (lua_pcall(L, 7, 0, 0)) {
        status = 0;
        fprintf(stderr, "%s: Failed to call note function!\n",
                  pModule);
      }
    }
  }
  
  /* Free Lua state if allocated */
  if (L != NULL) {
    lua_close(L);
    L = NULL;
  }
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
