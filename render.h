#ifndef RENDER_H_INCLUDED
#define RENDER_H_INCLUDED

/*
 * render.h
 * ========
 * 
 * Note rendering module of Infrared.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - art.c
 *   - diagnostic.c
 *   - graph.h
 *   - midi.c
 *   - pointer.c
 *   - ruler.c
 *   - set.c
 * 
 * Requires the following external libraries:
 * 
 *   - libnmf
 */

#include <stddef.h>
#include <stdint.h>

#include "art.h"
#include "graph.h"
#include "midi.h"
#include "ruler.h"
#include "set.h"

#include "nmf.h"

/*
 * Public functions
 * ================
 */

/*
 * Add an articulation classifier to the rendering pipeline.
 * 
 * Articulations are only applied to measured notes (not grace notes),
 * determining the actual performance duration of notes.
 * 
 * The default articulation if no articulation classifiers apply has
 * a multiplier of 1/1, a bumper of 8 subquanta, and a gap of zero.
 * 
 * The declared classifier applies only if the section index of the NMF
 * note is in the pSect set, AND the layer of the NMF note is in the
 * pLayer set, AND the articulation of the NMF note is in the pArt set.
 * 
 * The order that classifiers are declared is significant!  The last
 * classifier that applies to a specific note selects the value that
 * will be chosen.  If no classifiers apply, the default value indicated
 * above will be used.
 * 
 * Parameters:
 * 
 *   pSect - the set of NMF sections this classifier applies to
 * 
 *   pLayer - the set of NMF layers this classifier applies to
 * 
 *   pArt - the set of NMF articulations this classifier applies to
 * 
 *   pVal - the value that this classifier assigns when it applies
 */
void render_classify_art(
    SET * pSect,
    SET * pLayer,
    SET * pArt,
    ART * pVal);

/*
 * Add a ruler classifier to the rendering pipeline.
 * 
 * Rulers are only applied to unmeasured notes (grace notes),
 * determining the actual performance offset and duration of notes.
 * 
 * The default ruler if no ruler classifiers apply has a slot of 48
 * subquanta and a gap of zero.
 * 
 * The declared classifier applies only if the section index of the NMF
 * note is in the pSect set, AND the layer of the NMF note is in the
 * pLayer set, AND the articulation of the NMF note is in the pArt set.
 * 
 * The order that classifiers are declared is significant!  The last
 * classifier that applies to a specific note selects the value that
 * will be chosen.  If no classifiers apply, the default value indicated
 * above will be used.
 * 
 * Parameters:
 * 
 *   pSect - the set of NMF sections this classifier applies to
 * 
 *   pLayer - the set of NMF layers this classifier applies to
 * 
 *   pArt - the set of NMF articulations this classifier applies to
 * 
 *   pVal - the value that this classifier assigns when it applies
 */
void render_classify_ruler(
    SET   * pSect,
    SET   * pLayer,
    SET   * pArt,
    RULER * pVal);

/*
 * Add a graph classifier to the rendering pipeline.
 * 
 * Determines the note-on velocity of notes by querying the graph at the
 * performance time offset of the note.  If aftertouch is enabled for
 * the note by an aftertouch classifier, polyphonic aftertouch messages
 * will be issued to track changes in graph value.
 * 
 * The default graph if no graph classifiers apply has a constant value
 * of 64 throughout its range, which is the "neutral" MIDI velocity.
 * 
 * The declared classifier applies only if the section index of the NMF
 * note is in the pSect set, AND the layer of the NMF note is in the
 * pLayer set, AND the articulation of the NMF note is in the pArt set.
 * 
 * The order that classifiers are declared is significant!  The last
 * classifier that applies to a specific note selects the value that
 * will be chosen.  If no classifiers apply, the default value indicated
 * above will be used.
 * 
 * Parameters:
 * 
 *   pSect - the set of NMF sections this classifier applies to
 * 
 *   pLayer - the set of NMF layers this classifier applies to
 * 
 *   pArt - the set of NMF articulations this classifier applies to
 * 
 *   pVal - the value that this classifier assigns when it applies
 */
void render_classify_graph(
    SET   * pSect,
    SET   * pLayer,
    SET   * pArt,
    GRAPH * pVal);

/*
 * Add a channel classifier to the rendering pipeline.
 * 
 * The given value is in range 1 to MIDI_CH_MAX (16) inclusive,
 * selecting the MIDI channel the note will be assigned to.
 * 
 * The default channel if no channel classifiers apply is 1.
 * 
 * The declared classifier applies only if the section index of the NMF
 * note is in the pSect set, AND the layer of the NMF note is in the
 * pLayer set, AND the articulation of the NMF note is in the pArt set.
 * 
 * The order that classifiers are declared is significant!  The last
 * classifier that applies to a specific note selects the value that
 * will be chosen.  If no classifiers apply, the default value indicated
 * above will be used.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pSect - the set of NMF sections this classifier applies to
 * 
 *   pLayer - the set of NMF layers this classifier applies to
 * 
 *   pArt - the set of NMF articulations this classifier applies to
 * 
 *   val - the value that this classifier assigns when it applies
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void render_classify_channel(
    SET     * pSect,
    SET     * pLayer,
    SET     * pArt,
    int32_t   val,
    long      lnum);

/*
 * Add a release velocity classifier to the rendering pipeline.
 * 
 * The given value is in range -1 to MIDI_DATA_MAX inclusive.  The
 * special value -1 indicates that instead of a Note-Off message, a
 * Note-On message with a velocity of zero should be used to stop the
 * note (which is allowed by the MIDI standard).  All other values
 * indicate the Note-Off velocity that should be used to stop the note.
 * 
 * The default release velocity if no release velocity classifiers apply
 * is -1.
 * 
 * The declared classifier applies only if the section index of the NMF
 * note is in the pSect set, AND the layer of the NMF note is in the
 * pLayer set, AND the articulation of the NMF note is in the pArt set.
 * 
 * The order that classifiers are declared is significant!  The last
 * classifier that applies to a specific note selects the value that
 * will be chosen.  If no classifiers apply, the default value indicated
 * above will be used.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pSect - the set of NMF sections this classifier applies to
 * 
 *   pLayer - the set of NMF layers this classifier applies to
 * 
 *   pArt - the set of NMF articulations this classifier applies to
 * 
 *   val - the value that this classifier assigns when it applies
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void render_classify_release(
    SET     * pSect,
    SET     * pLayer,
    SET     * pArt,
    int32_t   val,
    long      lnum);

/*
 * Add an aftertouch classifier to the rendering pipeline.
 * 
 * The given value is either zero or one.  The value of zero indicates
 * that no polyphonic aftertouch messages shall be used for this note.
 * The value of one indicates that polyphonic aftertouch messages shall
 * be used as necessary to track changes in the assigned graph.
 * 
 * The default aftertouch flag value if no aftertouch classifiers apply
 * is zero.
 * 
 * The declared classifier applies only if the section index of the NMF
 * note is in the pSect set, AND the layer of the NMF note is in the
 * pLayer set, AND the articulation of the NMF note is in the pArt set.
 * 
 * The order that classifiers are declared is significant!  The last
 * classifier that applies to a specific note selects the value that
 * will be chosen.  If no classifiers apply, the default value indicated
 * above will be used.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pSect - the set of NMF sections this classifier applies to
 * 
 *   pLayer - the set of NMF layers this classifier applies to
 * 
 *   pArt - the set of NMF articulations this classifier applies to
 * 
 *   val - the value that this classifier assigns when it applies
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void render_classify_aftertouch(
    SET     * pSect,
    SET     * pLayer,
    SET     * pArt,
    int32_t   val,
    long      lnum);

/*
 * Render all the notes in the given parsed NMF data to the Infrared
 * MIDI module.
 * 
 * Prior to using this functions, the classifier pipelines should be set
 * up using the other functions of this module.
 * 
 * This function may only be called once.  After invocation, none of the
 * functions of this module may be called.
 * 
 * Parameters:
 * 
 *   pd - the NMF data to render
 */
void render_nmf(NMF_DATA *pd);

#endif
