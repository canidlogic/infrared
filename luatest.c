/*
 * luatest.c
 * =========
 * 
 * Compile with lua.  You must have the development libraries for liblua
 * installed.  Use "pkg-config --list-all" to look for the name of the
 * liblua package.  Then, if the name is, for example, lua-5.2 you can
 * compile like this (all one line):
 * 
 *   gcc -o luatest `pkg-config --cflags lua-5.2` luatest.c
 *   `pkg-config --libs lua-5.2`
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
 * Should match the valid pitch range of Retro.  See the ttone.h header
 * in Retro.
 */
#define PITCH_MIN (-39)
#define PITCH_MAX (48)

/*
 * Range of valid instrument indices.
 * 
 * The minimum is always one.  The greatest is this value.  This should
 * match INSTR_MAXCOUNT in instr.h of Retro.
 */
#define INSTR_MAX (4096)

/*
 * Range of valid layer indices.
 * 
 * The minimum is always one.  The greatest is this value.  This should
 * match LAYER_MAXCOUNT in layer.h of Retro.
 */
#define LAYER_MAX (16384)

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static int note_event(
    int32_t start,
    int32_t dur,
    int32_t pitch,
    int32_t instr,
    int32_t layer);
static int c_callback(lua_State* L);

/*
 * @@TODO:
 */
static int note_event(
    int32_t start,
    int32_t dur,
    int32_t pitch,
    int32_t instr,
    int32_t layer) {
  /* @@TODO: */
  fprintf(stderr, "[Note event]\n");
  fprintf(stderr, "start: %ld\n", (long) start);
  fprintf(stderr, "dur  : %ld\n", (long) dur);
  fprintf(stderr, "pitch: %ld\n", (long) pitch);
  fprintf(stderr, "instr: %ld\n", (long) instr);
  fprintf(stderr, "layer: %ld\n", (long) layer);
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
  
  /* Check parameter */
  if (L == NULL) {
    abort();
  }
  
  /* We should have exactly five arguments on the Lua stack, else fail
   * with error */
  if (lua_gettop(L) != 5) {
    lua_pushstring(L, "Wrong number of parameters");
    lua_error(L);
  }
  
  /* Check the argument types */
  if ((lua_type(L, 1) != LUA_TNUMBER) ||
      (lua_type(L, 2) != LUA_TNUMBER) ||
      (lua_type(L, 3) != LUA_TNUMBER) ||
      (lua_type(L, 4) != LUA_TNUMBER) ||
      (lua_type(L, 5) != LUA_TNUMBER)) {
    lua_pushstring(L, "Wrong parameter types");
    lua_error(L);
  }
  
  /* Get all the arguments */
  p_start = lua_tointeger(L, 1);
  p_dur = lua_tointeger(L, 2);
  p_pitch = lua_tointeger(L, 3);
  p_instr = lua_tointeger(L, 4);
  p_layer = lua_tointeger(L, 5);
  
  /* Cast all arguments to 32-bit integers */
  if ((p_start >= INT32_MIN) && (p_start <= INT32_MAX)) {
    i_start = (int32_t) p_start;
  } else {
    lua_pushstring(L, "Parameter out of range");
    lua_error(L);
  }
  
  if ((p_dur >= INT32_MIN) && (p_dur <= INT32_MAX)) {
    i_dur = (int32_t) p_dur;
  } else {
    lua_pushstring(L, "Parameter out of range");
    lua_error(L);
  }
  
  if ((p_pitch >= INT32_MIN) && (p_pitch <= INT32_MAX)) {
    i_pitch = (int32_t) p_pitch;
  } else {
    lua_pushstring(L, "Parameter out of range");
    lua_error(L);
  }
  
  if ((p_instr >= INT32_MIN) && (p_instr <= INT32_MAX)) {
    i_instr = (int32_t) p_instr;
  } else {
    lua_pushstring(L, "Parameter out of range");
    lua_error(L);
  }
  
  if ((p_layer >= INT32_MIN) && (p_layer <= INT32_MAX)) {
    i_layer = (int32_t) p_layer;
  } else {
    lua_pushstring(L, "Parameter out of range");
    lua_error(L);
  }
  
  /* Check the ranges of each parameter */
  if (i_start < 0) {
    lua_pushstring(L, "start parameter out of range");
    lua_error(L);
  }
  if (i_dur < 1) {
    lua_pushstring(L, "dur parameter out of range");
    lua_error(L);
  }
  if ((i_pitch < PITCH_MIN) || (i_pitch > PITCH_MAX)) {
    lua_pushstring(L, "pitch parameter out of range");
    lua_error(L);
  }
  if ((i_instr < 1) || (i_instr > INSTR_MAX)) {
    lua_pushstring(L, "instr parameter out of range");
    lua_error(L);
  }
  if ((i_layer < 1) || (i_layer > LAYER_MAX)) {
    lua_pushstring(L, "layer parameter out of range");
    lua_error(L);
  }
  
  /* Now we can call through to the note_event function */
  if (!note_event(i_start, i_dur, i_pitch, i_instr, i_layer)) {
    lua_pushstring(L, "Note event failed");
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
  
  /* Check that exactly one argument beyond module name */
  if (argc != 2) {
    status = 0;
    fprintf(stderr, "%s: Wrong number of arguments!\n", pModule);
  }
  
  /* Get the script path */
  if (status) {
    pScript = argv[1];
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
  
  /* Make sure we have room for the function object, five parameters,
   * and possibly one error message return value on the Lua stack */
  if (status) {
    if (!lua_checkstack(L, 7)) {
      status = 0;
      fprintf(stderr, "%s: Failed to grow the stack!\n", pModule);
    }
  }
  
  /* Run the "note" function in the Lua script */
  if (status) {
    /* Push the Lua "note" function onto the interpreter stack */
    lua_getglobal(L, "note");
    
    /* @@TODO: */
    
    /* Call the function */
    if (lua_pcall(L, 0, 0, 0)) {
      status = 0;
      fprintf(stderr, "%s: Failed to call note function!\n", pModule);
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
