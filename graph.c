/*
 * graph.c
 * =======
 * 
 * Implementation of graph.h
 * 
 * See header for further information.
 */

#include "graph.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"

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
 * The maximum number of nodes in a graph object's table.
 */
#define GRAPH_MAX_TABLE (16384)

/*
 * The initial and maximum capacity in entries of the constant graph
 * cache.
 */
#define CACHE_INIT_CAP (64)
#define CACHE_MAX_CAP INT32_C(1048576)

/*
 * The initial capacity of the graph accumulator register, measured in
 * nodes.
 * 
 * The maximum capacity is equal to GRAPH_MAX_TABLE.
 */
#define ACC_INIT_CAP (32)

/*
 * Region state constants, used in the REGION structure.
 */
#define STATE_EMPTY  (0)  /* No region buffered */
#define STATE_CONST  (1)  /* Constant region buffered */
#define STATE_RAMP   (2)  /* Ramp region buffered */
#define STATE_DERIVE (3)  /* Derived region buffered */

/*
 * Type declarations
 * =================
 */

/*
 * Graph node structure.
 * 
 * This associates a moment offset t with a graph value at that moment
 * v.  The graph value must be zero or greater.
 */
typedef struct {
  int32_t t;
  int32_t v;
} NODE;

/*
 * GRAPH structure.  Prototype given in header.
 */
struct GRAPH_TAG {
  
  /*
   * The next graph in the allocated graph chain, or NULL if this is the
   * last graph in the chain.
   * 
   * The graph chain allows the shutdown function to free all graphs.
   */
  GRAPH *pNext;
  
  /*
   * The number of entries in the graph table.
   * 
   * Must be in range 1 to GRAPH_MAX_TABLE inclusive.
   */
  int32_t len;
  
  /*
   * The graph table.
   * 
   * This extends beyond the end of the structure.  The number of
   * entries is given by the len field.
   * 
   * There is always at least one node entry.  Nodes each have a unique
   * moment offset, and nodes are sorted in ascending order of moment
   * offset.  Furthermore, nodes after the first node should always have
   * different graph values than the node that precedes them.
   */
  NODE table[1];
  
};

/*
 * Cache entry structure for the constant graph cache.
 * 
 * This associates a constant value v >= 0 with a graph object that has
 * that constant value at all time points.
 */
typedef struct {
  int32_t v;
  GRAPH *pg;
} CACHE;

/*
 * Region structure stores a buffered region waiting to be added to the
 * graph accumulator.
 */
typedef struct {
  
  /*
   * One of the STATE constants, indicating what is loaded in this
   * structure.
   */
  int state;
  
  /*
   * The moment offset of the buffered region.
   * 
   * Ignored if STATE_EMPTY.
   */
  int32_t t;
  
  /*
   * The three core region parameters.
   * 
   * STATE_EMPTY: ignored
   * 
   * STATE_CONST: a is constant value, b and c ignored
   * 
   * STATE_RAMP: a is start of ramp, b is ramp target, c is step
   * distance
   * 
   * STATE_DERIVE: a is numerator, b is denominator, c is constant value
   * to add after scaling
   */
  int32_t a;
  int32_t b;
  int32_t c;
  
  /*
   * For STATE_RAMP only, non-zero indicates logarithmic interpolation
   * while zero indicates linear interpolation.  Ignored otherwise.
   */
  int use_log;
  
  /*
   * For STATE_DERIVE only, indicates the minimum and maximum range
   * values to apply, with -1 for maximum indicating no maximum.
   * Ignored otherwise.
   */
  int32_t min_val;
  int32_t max_val;
  
  /*
   * For STATE_DERIVE only, the source graph that values are copied
   * from.  Ignored otherwise.
   */
  GRAPH *pg;
  
  /*
   * For STATE_DERIVE only, the moment offset in the source graph to
   * start copying values from.  Ignored otherwise.
   */
  int32_t t_src;
  
} REGION;

/*
 * Local data
 * ==========
 */

/*
 * Set to 1 if the module has been shut down.
 */
static int m_shutdown = 0;

/*
 * Pointers to the first and last graphs in the graph chain, or NULL for 
 * both if no graphs allocated.
 */
static GRAPH *m_pFirst = NULL;
static GRAPH *m_pLast = NULL;

/*
 * The constant graph cache.
 * 
 * m_cache_cap is the current capacity in cache entries.  m_cache_len is
 * the number of cache entries actually in use.
 * 
 * m_cache is the dynamically allocated cache array, which may be NULL
 * only if the capacity is zero.  Cache entries must be sorted in
 * ascending order of constant value, and each entry must have a unique
 * constant value.
 */
static int32_t m_cache_cap = 0;
static int32_t m_cache_len = 0;
static CACHE *m_cache = NULL;

/*
 * Flag indicating whether the graph accumulator is loaded with
 * something.
 */
static int m_load = 0;

/*
 * The graph accumulator register.
 * 
 * m_acc_cap is the current capacity in nodes.  m_acc_len is the number
 * of node entries actually in use.
 * 
 * m_acc is the dynamically allocated graph accumulator array, which may
 * be NULL only if the capacity is zero.  Nodes must be sorted in
 * ascending order of moment offset, each node must have a unique moment
 * offset, and nodes after the first should have different graph values
 * than the node that precedes them.
 * 
 * m_acc_t is the time offset of the last node entered into the
 * accumulator, if m_acc_len is greater than zero.  This is necessary
 * because accAppend() only actually adds nodes if they have a different
 * graph value, but it still needs to make sure entries are
 * chronological, even if they are not present in the accumulator.
 */
static int32_t m_acc_cap = 0;
static int32_t m_acc_len = 0;
static int32_t m_acc_t = 0;
static NODE *m_acc = NULL;

/*
 * The buffered region that should be added to the accumulator.
 * 
 * Only initialized if m_load is non-zero.
 * 
 * STATE_EMPTY is used when nothing is buffered.
 * 
 * Buffering is necessary because ramps and derived regions need to know
 * what region follows them (if any) before they can be computed.
 * 
 * m_buf_lnum stores the Shastina parser line number where the region
 * was buffered, for sake of error reporting.
 */
static REGION m_buf;
static long m_buf_lnum = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static long srcLine(long lnum);

static GRAPH *cache_get(int32_t v);

static void accReset(void);
static void accAppend(int32_t t, int32_t v, long lnum);
static GRAPH *accTake(long lnum);

static void resolveTrack(void *pCustom, int32_t t, int32_t v);
static void resolve(int32_t t_next, int has_next);

static int32_t graphSeek(GRAPH *pg, int32_t t);

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
 * Use the constant graph cache to get an instance of a constant graph.
 * 
 * v is the graph value that the graph should have at all time points.
 * It must be zero or greater.
 * 
 * If a constant graph for that value is already in the cache, that
 * cached instance is returned rather than constructing a new graph
 * object.
 * 
 * If no graph for the requested value is in the cache, a new constant
 * graph is constructed, added to the cache, and then returned.
 * 
 * Parameters:
 * 
 *   v - the constant graph value
 * 
 * Return:
 * 
 *   a graph object that has the constant value everywhere
 */
static GRAPH *cache_get(int32_t v) {
  
  int32_t new_cap = 0;
  int32_t lo = 0;
  int32_t hi = 0;
  int32_t mid = 0;
  int32_t pos = 0;
  int32_t p = 0;
  GRAPH *pResult = NULL;
  
  /* Check state and parameters */
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (v < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Initialize cache if not initialized */
  if (m_cache_cap < 1) {
    m_cache_cap = CACHE_INIT_CAP;
    m_cache_len = 0;
    
    m_cache = (CACHE *) calloc((size_t) m_cache_cap, sizeof(CACHE));
    if (m_cache == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
  }
  
  /* Find the existing cache entry that has the least value of the set
   * of all cache entries that have values greater than or equal to the
   * requested graph value, or -1 if no such entry exists */
  pos = -1;
  if (m_cache_len > 0) {
    /* Start with whole cache as range */
    lo = 0;
    hi = m_cache_len - 1;
    
    /* Zoom in on an entry by binary search */
    while (lo < hi) {
      /* Choose a midpoint halfway between, but at least one below the
       * upper boundary */
      mid = lo + ((hi - lo) / 2);
      if (mid >= hi) {
        mid = hi - 1;
      }
      
      /* Adjust range according to midpoint value */
      p = (m_cache[mid]).v;
      if (p > v) {
        /* Midpoint greater than requested value, so adjust upper
         * boundary of search range down to midpoint */
        hi = mid;
        
      } else if (p < v) {
        /* Midpoint less than requested value, so adjust lower boundary
         * of search range up to just beyond midpoint */
        lo = mid + 1;
        
      } else if (p == v) {
        /* We found the entry, so zoom in */
        lo = mid;
        hi = mid;
        
      } else {
        raiseErr(__LINE__, NULL);
      }
    }
    
    /* Compare requested value to entry we zoomed in on */
    p = (m_cache[hi]).v;
    if (p >= v) {
      /* Entry we found is greater than or equal to requested value, so
       * set pos to the entry */
      pos = hi;
      
    } else {
      /* Entry we found is not greater than or equal to requested value,
       * so set pos to -1 indicating no such entry */
      pos = -1;
    }
  }
  
  /* Check if we need to add a new cache entry */
  if ((pos < 0) || ((m_cache[pos]).v != v)) {
    /* We need to add a new cache entry, so first expand cache if
     * necessary */
    if (m_cache_len >= m_cache_cap) {
      if (m_cache_len >= CACHE_MAX_CAP) {
        raiseErr(__LINE__, "Constant graph cache overflow");
      }
      
      new_cap = m_cache_cap * 2;
      if (new_cap > CACHE_MAX_CAP) {
        new_cap = CACHE_MAX_CAP;
      }
      
      m_cache = (CACHE *) realloc(
                            m_cache,
                            ((size_t) new_cap) * sizeof(CACHE));
      if (m_cache == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_cache[m_cache_cap]),
        0,
        ((size_t) (new_cap - m_cache_cap)) * sizeof(CACHE));
      
      m_cache_cap = new_cap;
    }
    
    /* If pos is not the special -1 value, shift all entries from the
     * end down to and including pos one to the right */
    if (pos >= 0) {
      for(p = m_cache_len - 1; p >= pos; p--) {
        memcpy(&(m_cache[p + 1]), &(m_cache[p]), sizeof(CACHE));
      }
    }
    
    /* Set pos to the index of the cache entry we are adding and
     * increase the cache length by one */
    if (pos < 0) {
      pos = m_cache_len;
    }
    m_cache_len++;
    
    /* Store the value in the cache entry */
    (m_cache[pos]).v = v;
    
    /* Construct a new graph object for the cache entry and also for the
     * result */
    pResult = (GRAPH *) calloc(1, sizeof(GRAPH));
    if (pResult == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
    /* Initialize as a constant graph with the requested value */
    pResult->len = 1;
    ((pResult->table)[0]).t = 0;
    ((pResult->table)[0]).v = v;
    
    /* Link into graph chain */
    if (m_pLast == NULL) {
      m_pFirst = pResult;
      m_pLast = pResult;
      pResult->pNext = NULL;
    } else {
      m_pLast->pNext = pResult;
      pResult->pNext = NULL;
      m_pLast = pResult;
    }
    
    /* Cache the new graph */
    (m_cache[pos]).pg = pResult;
    
  } else if ((m_cache[pos]).v == v) {
    /* We already have a cached graph, so just return that */
    pResult = (m_cache[pos]).pg;
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Return result */
  return pResult;
}

/*
 * Reset the accumulator register back to the initial capacity and empty
 * it.  May be used at any time, including after shut down.
 */
static void accReset(void) {
  if (m_acc_cap < 1) {
    m_acc = (NODE *) calloc((size_t) ACC_INIT_CAP, sizeof(NODE));
    if (m_acc == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    m_acc_cap = ACC_INIT_CAP;
  }
  
  if (m_acc_cap != ACC_INIT_CAP) {
    m_acc = (NODE *) realloc(m_acc,
                        ((size_t) ACC_INIT_CAP) * sizeof(NODE));
    if (m_acc == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    m_acc_cap = ACC_INIT_CAP;
  }
  
  m_acc_len = 0;
  m_acc_t = 0;
}

/*
 * Append another node to the accumulator register.
 * 
 * May only be used when not shut down and when the accumulator register
 * is loaded.  Automatically invokes accReset() if accumulator has not
 * been initialized yet.
 * 
 * If the accumulator is empty, t may have any value.  Otherwise, t must
 * be greater than the time value of the last node value added to the
 * accumulator with this function.
 * 
 * v is the graph value of the node, which must be zero or greater.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   t - the moment offset of the node to add
 * 
 *   v - the graph value of the node to add
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
static void accAppend(int32_t t, int32_t v, long lnum) {
  
  int proceed = 0;
  int32_t new_cap = 0;
  
  /* Check state */
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (!m_load) {
    raiseErr(__LINE__, "Accumulator not loaded");
  }
  
  /* Initialize accumulator if necessary */
  if (m_acc_cap < 1) {
    accReset();
  }
  
  /* Check values */
  if (v < 0) {
    raiseErr(__LINE__,
      "Negative values not allowed in graphs on script line %ld",
      srcLine(lnum));
  }
  if (m_acc_len > 0) {
    if (t <= m_acc_t) {
      raiseErr(__LINE__,
        "Graph must be ascending chronological on script line %ld",
        srcLine(lnum));
    }
  }
  
  /* Update the accumulator t value */
  m_acc_t = t;
  
  /* Determine whether we need to actually add the node */
  proceed = 1;
  if (m_acc_len > 0) {
    if ((m_acc[m_acc_len - 1]).v == v) {
      /* The accumulator is not empty and the last node has the same
       * graph value as this new node, so we do not need to add it */
      proceed = 0;
    }
  }
  
  /* Add node if needed */
  if (proceed) {
    /* Expand capacity if necessary */
    if (m_acc_len >= m_acc_cap) {
      if (m_acc_len >= GRAPH_MAX_TABLE) {
        raiseErr(__LINE__, "Graph too complex on script line %ld",
          srcLine(lnum));
      }
      
      new_cap = m_acc_cap * 2;
      if (new_cap > GRAPH_MAX_TABLE) {
        new_cap = GRAPH_MAX_TABLE;
      }
      
      m_acc = (NODE *) realloc(m_acc,
                        ((size_t) new_cap) * sizeof(NODE));
      if (m_acc == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_acc[m_acc_cap]),
        0,
        ((size_t) (new_cap - m_acc_cap) * sizeof(NODE)));
      
      m_acc_cap = new_cap;
    }
    
    /* Add the node */
    (m_acc[m_acc_len]).t = t;
    (m_acc[m_acc_len]).v = v;
    m_acc_len++;
  }
}

/*
 * Use the contents of the accumulator to construct a graph object, and
 * then reset the accumulator.
 * 
 * May only be used when not shut down and when the accumulator register
 * is loaded.  Automatically invokes accReset() if accumulator has not
 * been initialized yet.
 * 
 * Graphs must have at least one node, so an error occurs if the graph
 * accumulator is empty.
 * 
 * If the graph accumulator has exactly one node, then the graph is a
 * constant graph.  In this case, cache_get() is used to construct the
 * graph object, so that graph object instances can be shared and
 * reused.
 * 
 * This function will also clear m_load back to zero to unload the
 * accumulator after resetting it.
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
 *   the constructed graph object
 */
static GRAPH *accTake(long lnum) {
  
  GRAPH *pGraph = NULL;
  
  /* Check state */
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (!m_load) {
    raiseErr(__LINE__, "Accumulator not loaded");
  }
  
  /* Initialize accumulator if necessary */
  if (m_acc_cap < 1) {
    accReset();
  }
  
  /* Handle depending on number of nodes */
  if (m_acc_len < 1) {
    /* Graph is empty, so error */
    raiseErr(__LINE__,
      "Empty graphs are not allowed on script line %ld",
      srcLine(lnum));
    
  } else if (m_acc_len == 1) {
    /* Graph has single node, so use the constant graph cache */
    pGraph = cache_get((m_acc[0]).v);
    
  } else if (m_acc_len > 1) {
    /* Graph has multiple nodes, so allocate a graph object with
     * sufficient space for a copy of the nodes */
    pGraph = (GRAPH *) calloc(1,
                (((size_t) (m_acc_len - 1)) * sizeof(NODE))
                  + sizeof(GRAPH));
    if (pGraph == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
    /* Initialize the graph and copy the nodes */
    pGraph->len = m_acc_len;
    memcpy(&((pGraph->table)[0]), &(m_acc[0]),
            ((size_t) m_acc_len) * sizeof(NODE));
    
    /* Link into graph chain */
    if (m_pLast == NULL) {
      m_pFirst = pGraph;
      m_pLast = pGraph;
      pGraph->pNext = NULL;
    } else {
      m_pLast->pNext = pGraph;
      pGraph->pNext = NULL;
      m_pLast = pGraph;
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Reset the accumulator and clear the load flag */
  accReset();
  m_load = 0;
  
  /* Return graph */
  return pGraph;
}

/*
 * Tracking algorithm callback used for resolving derived nodes.  See
 * documentation of graph_fp_track function pointer type for
 * specification.
 * 
 * The custom parameter is not used, since the static region buffer can
 * be used instead.
 */
static void resolveTrack(void *pCustom, int32_t t, int32_t v) {
  
  int64_t iv64 = 0;
  
  /* Ignore custom parameter */
  (void) pCustom;
  
  /* Check parameters */
  if (v < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check state */
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (!m_load) {
    raiseErr(__LINE__, "Accumulator not loaded");
  }
  if (m_buf.state != STATE_DERIVE) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check that reported t is at least the t_src parameter value in the
   * region, and then reduce reported t as an offset of t_src */
  if (t >= m_buf.t_src) {
    t = t - m_buf.t_src;
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Transform the value */
  iv64 = (((int64_t) v) * ((int64_t) m_buf.a)) / ((int64_t) m_buf.b);
  if (iv64 < INT32_MIN) {
    iv64 = (int64_t) INT32_MIN;
  } else if (iv64 > INT32_MAX) {
    iv64 = (int64_t) INT32_MAX;
  }
  
  iv64 += ((int64_t) m_buf.c);
  if (iv64 < INT32_MIN) {
    iv64 = (int64_t) INT32_MIN;
  } else if (iv64 > INT32_MAX) {
    iv64 = (int64_t) INT32_MAX;
  }
  
  if (iv64 < m_buf.min_val) {
    iv64 = (int64_t) m_buf.min_val;
  }
  if (m_buf.max_val >= 0) {
    if (iv64 > m_buf.max_val) {
      iv64 = (int64_t) m_buf.max_val;
    }
  }
  
  v = (int32_t) iv64;
  
  /* Append this graph value to the accumulator */
  accAppend(t, v, m_buf_lnum);
}

/*
 * Resolve any buffered regions by adding nodes into the accumulator
 * with accAppend().
 * 
 * May only be used when not shut down and when the accumulator register
 * is loaded.
 * 
 * If there is a region after the one buffered in m_buf, t_next should
 * be the moment offset of the next region and has_next should be
 * non-zero.
 * 
 * If the region buffered in m_buf is the last region, has_next should
 * be zero and t_next is ignored.
 * 
 * If the region has STATE_EMPTY, then the call does nothing.
 * Otherwise, the buffered region is resolved by appending all the
 * necessary nodes with accAppend() and the buffered region is reset
 * back to STATE_EMPTY.
 * 
 * This function will change STATE_RAMP regions to STATE_CONST if both
 * the start and target values of the ramp are the same.
 * 
 * Parameters:
 * 
 *   t_next - moment offset of next region, ignored if none
 * 
 *   has_next - non-zero if there is a next region, zero otherwise
 */
static void resolve(int32_t t_next, int has_next) {
  
  int track_has_end = 0;
  int32_t track_end_t = 0;
  int64_t iv64 = 0;
  int32_t ts = 0;
  int32_t te = 0;
  int32_t to = 0;
  int64_t t = 0;
  int mp = 0;
  double tv = 0.0;
  
  /* Check state */
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (!m_load) {
    raiseErr(__LINE__, "Accumulator not loaded");
  }
  
  /* If there is a next region and region buffer is not empty, make sure
   * start of next region is after start of current region */
  if (has_next && (m_buf.state != STATE_EMPTY)) {
    if (t_next <= m_buf.t) {
      raiseErr(__LINE__,
        "Graph regions must be chronological on script line %ld",
        srcLine(m_buf_lnum));
    }
  }
  
  /* Convert ramp to constant if possible */
  if (m_buf.state == STATE_RAMP) {
    if (m_buf.a == m_buf.b) {
      m_buf.state = STATE_CONST;
    }
  }
  
  /* Handle the different region types */
  if (m_buf.state == STATE_CONST) {
    /* Constant region, so just append the constant node */
    accAppend(m_buf.t, m_buf.a, m_buf_lnum);
    
  } else if (m_buf.state == STATE_DERIVE) {
    /* Derived region, so first figure out the end within the source
     * graph, if there is an endpoint; an overflow of the source range
     * is handled as though there were no endpoint */
    if (has_next) {
      /* There is a next node, so compute the endpoint of the source
       * track in 64-bit space */
      iv64 = (((int64_t) t_next) - ((int64_t) m_buf.t))
              + ((int64_t) m_buf.t_src);
      
      /* If this result is in 32-bit range, use it as endpoint; else,
       * no endpoint */
      if ((iv64 >= INT32_MIN) && (iv64 <= INT32_MAX)) {
        track_has_end = 1;
        track_end_t = (int32_t) iv64;
      } else {
        track_has_end = 0;
      }
      
    } else {
      /* No next node, so there is no endpoint in source track */
      track_has_end = 0;
    }
    
    /* Use tracking algorithm to add all the necessary nodes */
    graph_track(
      m_buf.pg,
      &resolveTrack,
      NULL,
      m_buf.t_src,
      track_end_t,
      0,
      track_has_end,
      0);
    
  } else if (m_buf.state == STATE_RAMP) {
    /* Ramp region, so first verify that there is a next node */
    if (!has_next) {
      raiseErr(__LINE__,
        "Ramp may not be last region in graph on script line %ld",
        srcLine(m_buf_lnum));
    }
    
    /* At the start of the region, there is always the start value of
     * the ramp */
    accAppend(m_buf.t, m_buf.a, m_buf_lnum);
    
    /* Parse start of region in subquantum offset in ts and moment part
     * in mp */
    ts = pointer_unpack(m_buf.t, &mp);
    
    /* Round the start of region offset down to a step distance boundary
     * value in 64-bit space */
    if (t < 0) {
      t = (0 - ((int64_t) ts)) / ((int64_t) m_buf.c);
      t *= (0 - ((int64_t) m_buf.c));
      if (t > ts) {
        t -= ((int64_t) m_buf.c);
      }
      
    } else if (m_buf.t >= 0) {
      t = ((int64_t) ts) / ((int64_t) m_buf.c);
      t *= ((int64_t) m_buf.c);
      
    } else {
      raiseErr(__LINE__, NULL);
    }
    
    /* Now set te to the subquantum offset of the region following the
     * ramp */
    te = pointer_unpack(t_next, NULL);
    
    /* Compute the rest of the ramp and add the changes in value to the
     * accumulator */
    for(t += ((int64_t) m_buf.c);
        t < te;
        t += ((int64_t) m_buf.c)) {
   
      /* Compute position on ramp on a normalized 0.0-1.0 scale */
      tv = (((double) t) - ((double) ts))
            / (((double) te) - ((double) ts));
      if (!(tv >= 0.0)) {
        tv = 0.0;
      } else if (!(tv <= 1.0)) {
        tv = 1.0;
      }
      
      /* Perform the requested interpolation */
      if (m_buf.use_log) {
        /* Logarithmic interpolation */
        tv = exp(
          log(((double) m_buf.a) + 1.0)
          + (
              tv * (
                    log(((double) m_buf.b) + 1.0)
                    - log(((double) m_buf.a) + 1.0)
                   )
            )
        ) - 1.0;
        
      } else {
        /* Linear interpolation */
        tv = ((double) m_buf.a)
              + ((((double) m_buf.b) - ((double) m_buf.a)) * tv);
      }
      
      /* Floor interpolation to integer and clamp to 32-bit integer
       * range */
      tv = floor(tv);
      if (!(tv >= 0.0)) {
        tv = 0.0;
      } else if (!(tv <= (double) INT32_MAX)) {
        tv = (double) INT32_MAX;
      }
      
      /* Get a moment offset from the current subquantum offset and the
       * moment part that was used at the start of the region */
      to = pointer_pack((int32_t) t, mp);

      /* Add the interpolated position to the accumuator */
      accAppend(to, (int32_t) tv, m_buf_lnum);
    }
    
  } else {
    if (m_buf.state != STATE_EMPTY) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  /* Clear the region buffer */
  memset(&m_buf, 0, sizeof(REGION));
  m_buf_lnum = 0;
}

/*
 * Find the graph node that covers the given moment offset.
 * 
 * The selected graph node is the node with latest time offset in the
 * set of nodes that have time offsets less than or equal to the given
 * offset.  If there is no such graph node, -1 is returned.
 * 
 * Parameters:
 * 
 *   pg - the graph
 * 
 *   t - the moment offset
 * 
 * Return:
 * 
 *   the index in the graph table of the node that covers the moment
 *   offset, or -1 if no graph node covers it
 */
static int32_t graphSeek(GRAPH *pg, int32_t t) {
  
  int32_t result = 0;
  int32_t lo = 0;
  int32_t hi = 0;
  int32_t mid = 0;
  int32_t m = 0;
  
  /* Check state and parameters */
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (pg == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (pg->len < 1) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Zoom in on the relevant node */
  lo = 0;
  hi = pg->len - 1;
  while (lo < hi) {
    /* Midpoint is halfway between boundaries, and always greater than
     * the lower boundary */
    mid = lo + ((hi - lo) / 2);
    if (mid <= lo) {
      mid = lo + 1;
    }
    
    /* Compare midpoint node time to given time */
    m = ((pg->table)[mid]).t;
    if (m < t) {
      /* Node is less than requested time, so new lower boundary is the
       * midpoint */
      lo = mid;
      
    } else if (m > t) {
      /* Node is greater than requested time, so new upper boundary is
       * below this node */
      hi = mid - 1;
      
    } else if (m == t) {
      /* Node exactly matches time, so zoom in */
      lo = mid;
      hi = mid;
      
    } else {
      raiseErr(__LINE__, NULL);
    }
  }
  
  /* If selected node has time less than or equal to requested time,
   * that node is the result; otherwise, -1 is the result */
  if (((pg->table)[lo]).t <= t) {
    result = lo;
  } else {
    result = -1;
  }
  
  return result;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * graph_constant function.
 */
GRAPH *graph_constant(int32_t v, long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (v < 0) {
    raiseErr(__LINE__,
      "Graph values must be zero or greater on script line %ld",
      srcLine(lnum));
  }
  return cache_get(v);
}

/*
 * graph_begin function.
 */
void graph_begin(long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (m_load) {
    raiseErr(__LINE__,
      "Graph accumulator already loaded on script line %ld",
      srcLine(lnum));
  }
  
  m_load = 1;
  accReset();
  
  memset(&m_buf, 0, sizeof(REGION));
  m_buf.state = STATE_EMPTY;
  m_buf_lnum = 0;
}

/*
 * graph_add_constant function.
 */
void graph_add_constant(POINTER *pp, int32_t v, long lnum) {
  
  int32_t t = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (!m_load) {
    raiseErr(__LINE__,
      "Graph accumulator not loaded on script line %ld",
      srcLine(lnum));
  }
  
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (v < 0) {
    raiseErr(__LINE__,
      "Graph values must be zero or greater on script line %ld",
      srcLine(lnum));
  }
  
  if (pointer_isHeader(pp)) {
    raiseErr(__LINE__,
      "Can't use header pointers in a graph on script line %ld",
      srcLine(lnum));
  }
  
  t = pointer_compute(pp, lnum);
  resolve(t, 1);
  
  m_buf_lnum = lnum;
  m_buf.state = STATE_CONST;
  m_buf.t = t;
  m_buf.a = v;
}

/*
 * graph_add_ramp function.
 */
void graph_add_ramp(
    POINTER * pp,
    int32_t   a,
    int32_t   b,
    int32_t   s,
    int       use_log,
    long      lnum) {
  
  int32_t t = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (!m_load) {
    raiseErr(__LINE__,
      "Graph accumulator not loaded on script line %ld",
      srcLine(lnum));
  }
  
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((a < 0) || (b < 0)) {
    raiseErr(__LINE__,
      "Graph values must be zero or greater on script line %ld",
      srcLine(lnum));
  }
  if (s < 1) {
    raiseErr(__LINE__,
      "Graph step distance must be at least one on script line %ld",
      srcLine(lnum));
  }
  
  if (a == b) {
    graph_add_constant(pp, a, lnum);
  
  } else {
    if (pointer_isHeader(pp)) {
      raiseErr(__LINE__,
        "Can't use header pointers in a graph on script line %ld",
        srcLine(lnum));
    }
    
    t = pointer_compute(pp, lnum);
    resolve(t, 1);
    
    m_buf_lnum = lnum;
    m_buf.state = STATE_RAMP;
    m_buf.t = t;
    m_buf.a = a;
    m_buf.b = b;
    m_buf.c = s;
    
    if (use_log) {
      m_buf.use_log = 1;
    } else {
      m_buf.use_log = 0;
    }
  }
}

/*
 * graph_add_derived function.
 */
void graph_add_derived(
    POINTER * pDerive,
    GRAPH   * pg,
    POINTER * pSource,
    int32_t   numerator,
    int32_t   denominator,
    int32_t   c,
    int32_t   min_val,
    int32_t   max_val,
    long      lnum) {
  
  int32_t t = 0;
  int32_t t_src = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (!m_load) {
    raiseErr(__LINE__,
      "Graph accumulator not loaded on script line %ld",
      srcLine(lnum));
  }
  
  if ((pDerive == NULL) || (pg == NULL) || (pSource == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if (numerator < 0) {
    raiseErr(__LINE__,
      "Graph region numerator may not be negative on script line %ld",
      srcLine(lnum));
  }
  if (denominator < 1) {
    raiseErr(__LINE__,
      "Graph region denominator must be at least 1 on script line %ld",
      srcLine(lnum));
  }
  if (min_val < 0) {
    raiseErr(__LINE__,
      "Graph region minimum may not be negative on script line %ld",
      srcLine(lnum));
  }
  if (max_val < -1) {
    raiseErr(__LINE__,
      "Invalid graph region maximum on script line %ld",
      srcLine(lnum));
  }
  
  if (pointer_isHeader(pDerive) || pointer_isHeader(pSource)) {
    raiseErr(__LINE__,
      "Can't use header pointers in a graph on script line %ld",
      srcLine(lnum));
  }
  
  t = pointer_compute(pDerive, lnum);
  resolve(t, 1);
  
  t_src = pointer_compute(pSource, lnum);
  
  m_buf_lnum = lnum;
  m_buf.state = STATE_DERIVE;
  m_buf.t = t;
  m_buf.a = numerator;
  m_buf.b = denominator;
  m_buf.c = c;
  m_buf.min_val = min_val;
  m_buf.max_val = max_val;
  m_buf.pg = pg;
  m_buf.t_src = t_src;
}

/*
 * graph_end function.
 */
GRAPH *graph_end(long lnum) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (!m_load) {
    raiseErr(__LINE__,
      "Graph accumulator not loaded on script line %ld",
      srcLine(lnum));
  }
  
  resolve(0, 0);
  return accTake(lnum);
}

/*
 * graph_shutdown function.
 */
void graph_shutdown(void) {
  GRAPH *pCur = NULL;
  GRAPH *pNext = NULL;
  
  if (!m_shutdown) {
    m_shutdown = 1;
    pCur = m_pFirst;
    while (pCur != NULL) {
      pNext = pCur->pNext;
      free(pCur);
      pCur = pNext;
    }
    m_pFirst = NULL;
    m_pLast = NULL;
    
    if (m_cache_cap > 0) {
      free(m_cache);
      m_cache = NULL;
      m_cache_cap = 0;
      m_cache_len = 0;
    }
    
    m_load = 0;
    if (m_acc_cap > 0) {
      free(m_acc);
      m_acc = NULL;
      m_acc_cap = 0;
      m_acc_len = 0;
      m_acc_t = 0;
    }
  }
}

/*
 * graph_query function.
 */
int32_t graph_query(GRAPH *pg, int32_t t) {
  
  int32_t i = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if (pg == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  i = graphSeek(pg, t);
  if (i < 0) {
    i = 0;
  }
  
  return ((pg->table)[i]).v;
}

/*
 * graph_track function.
 */
void graph_track(
    GRAPH          * pg,
    graph_fp_track   fp,
    void           * pCustom,
    int32_t          t_start,
    int32_t          t_end,
    int32_t          v_start,
    int              has_end,
    int              has_v_start) {
  
  int32_t i = 0;
  int proceed = 0;
  
  /* Check state and parameters */
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if ((pg == NULL) || (fp == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if (has_end) {
    if (t_end < t_start) {
      raiseErr(__LINE__, NULL);
    }
  }
  if (has_v_start) {
    if (v_start < 0) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  /* Figure out node index that covers starting time, or use first node
   * if no node covers starting time */
  i = graphSeek(pg, t_start);
  if (i < 0) {
    i = 0;
  }
  
  /* If has_v_start was specified and the starting node has the same
   * value as v_start, then we won't send the initial node; else, we
   * will */
  proceed = 1;
  if (has_v_start) {
    if (((pg->table)[i]).v == v_start) {
      proceed = 0;
    }
  }
  
  /* Unless suppressed in previous step, send the first node to the
   * callback with time value t_start (which might actually be before
   * the start of the node) and graph value from the node */
  if (proceed) {
    fp(pCustom, t_start, ((pg->table)[i]).v);
  }
  
  /* Iterate through the rest of the graph nodes and report any within
   * the time range to the callback */
  for(i++; i < pg->len; i++) {
    if (has_end) {
      if (((pg->table)[i]).t > t_end) {
        break;
      }
    }
    fp(pCustom, ((pg->table)[i]).t, ((pg->table)[i]).v);
  }
}

/*
 * graph_print function.
 */
void graph_print(GRAPH *pg, FILE *pOut) {
  
  int32_t i = 0;
  int is_filled = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Graph module is shut down");
  }
  if ((pg == NULL) || (pOut == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  for(i = 0; i < pg->len; i++) {
    
    if (is_filled) {
      fprintf(pOut, " ");
    }
    is_filled = 1;
    
    fprintf(pOut, "(%ld,%ld)",
      (long) ((pg->table)[i]).t,
      (long) ((pg->table)[i]).v);
  }
}
