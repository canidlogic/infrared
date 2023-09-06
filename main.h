#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

/*
 * main.h
 * ======
 * 
 * Public exports from the main Infrared program module.
 * 
 * See main.c for further information.
 */

#include <stddef.h>
#include <stdint.h>

#include "core.h"

/*
 * Function pointer types
 * ======================
 */

/*
 * Callback function pointer type for main_op().
 * 
 * This callback function is invoked to perform an interpreter
 * operation.  The function implementation should use the core module to
 * pop input parameters and push output parameters to the interpreter
 * stack.
 * 
 * pCustom can be used to pass through an argument set during operation
 * registration.  It is optional and might be NULL.
 * 
 * The given lnum is from the Shastina parser, and it is intended for
 * error reports and diagnostic messages if necessary.  This is the line
 * number the operation occurred on.
 * 
 * Parameters:
 * 
 *   pCustom - passed through from main_op() caller
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
typedef void (*main_fp_op)(void *pCustom, long lnum);

/*
 * Public functions
 * ================
 */

/*
 * Register an operation with the script interpreter.
 * 
 * This function may only be used before script interpretation begins.
 * Within the main.c implementation file, the function registerModules()
 * will be invoked near the beginning before script interpretation
 * begins, and that function will invoke the registration functions of
 * each operation module.  Those registration functions should then use
 * this main_op() function to register each of their operations.
 * 
 * pKey is the case-sensitive name of the operation.  The name must be
 * between 1 and 31 characters, each character must be an ASCII
 * alphanumeric or underscore, and the first character must be
 * alphabetic.  An error occurs if the given name is already registered.
 * 
 * fp is the pointer to the function that will be called to implement
 * the operation.  pCustom is the custom parameter that will be passed
 * through.  It may be NULL.
 * 
 * Parameters:
 * 
 *   pKey - the name of the operation to register
 * 
 *   fp - the function to implement the operation
 * 
 *   pCustom - the custom parameter to pass through, or NULL
 */
void main_op(const char *pKey, main_fp_op fp, void *pCustom);

/*
 * Print the value contained within a given variant to the diagnostic
 * console on standard error.
 * 
 * This function may only be used during script interpretation.  It is
 * intended for diagnostic operations.
 * 
 * If this is the very first print invocation, or it is the first print
 * invocation after a main_newline() call, then a header line will be
 * inserted that looks like this:
 * 
 *   ./infrared: [Script line 25]
 * 
 * This header is followed by a space and then the value is printed.  If
 * this is not the first print invocation and not the first print
 * invocation after a new line, then the value is printed without any
 * header and without any space.
 * 
 * Printing text values will print the literal text.  Printing integer
 * values print the value as a signed decimal integer.  Printing a null
 * value is allowed and results in "<null>" being printed.  All other
 * values use the special print value defined by their modules.
 * 
 * If there is any call to main_print() during interpretation that is
 * not followed somewhere by main_newline(), then main_newline() will
 * automatically be called at the end of script interpretation.
 * 
 * The given line number is the script line number the message request
 * originated from.  It is only used when printing a header.  Values
 * that are out range will be replaced with -1.
 * 
 * Parameters:
 * 
 *   pv - the variant to print
 * 
 *   lnum - the Shastina line number where the print occurred
 */
void main_print(CORE_VARIANT *pv, long lnum);

/*
 * Print a line break on the diagnostic console on standard error.
 * 
 * This function may only be used during script interpretation.  It is
 * intended for diagnostic operations.
 */
void main_newline(void);

/*
 * Stop the program on an error.
 * 
 * This function may only be used during script interpretation.  It is
 * intended for diagnostic operations that want to stop the script in
 * the middle of execution for debugging purposes.
 * 
 * If there has been a call to main_print() that has not been followed
 * somewhere by a main_newline(), then main_newline() will automatically
 * be called here.
 * 
 * This function will print a diagnostic message with the following
 * format:
 * 
 *   ./infrared: [Stopped on script line 25]
 * 
 * The given line number is the script line number the stop request
 * originated from.  Values that are out range will be replaced with -1.
 *
 * Parameters:
 * 
 *   lnum - the Shastina line number where the print occurred
 */
void main_stop(long lnum);

#endif
