#ifndef CAM_UNIT_H
#define CAM_UNIT_H

#include <stdarg.h>

#include <glib.h>
#include <glib-object.h>

#include "pixels.h"

#include "framebuffer.h"
#include "unit_format.h"
#include "unit_control.h"

/**
 * SECTION:unit
 * @short_description: CamUnit is the fundamental object in libcam.
 *
 * CamUnit is an abstract base class, and is the fundamental object in libcam.
 * CamUnit objects may consume images as input, produce images as output, and
 * are typically connected in sequence to form an image processing chain (see
 * also: #CamUnitChain)
 *
 * When implementing a subclass of a CamUnit, it is helpful to think of the
 * unit as falling into one of two categories: Input and Filter
 *
 * Input units are always the first unit in a chain, and do not consume images
 * as input.  Instead, an input unit generates images to be consumed by other
 * units.  Examples of input units are #CamDC1394 and #CamInputLog
 *
 * Filter units form the backbone of an image processing chain, and may
 * transform an image in some way.  Examples of filter units that modify 
 * images are #CamColorConversionFilter and #CamBayerFilter.  Other filter
 * units may simply pass the image through while doing something else (e.g.
 * #CamFilterGL)
 *
 * A CamUnit object exists in one of three states: idle, ready, or streaming.
 * See: #CamUnitStatus
 *
 * Filter units must have an associated input unit, set using the
 * #cam_unit_set_input method.  This method is typically invoked by the
 * #CamUnitChain containing the unit.
 */

#ifdef __cplusplus
extern "C" {
#endif

// ========= enums and constants

/**
 * CamUnitStatus:
 *
 * A CamUnit object is always on one of these three states.  In the
 * CAM_UNIT_STATUS_IDLE state, a unit has not reserved any system resources for
 * image acquisition or processing, is not bound to an output format, and is
 * not capable of image acquisition or processing.
 *
 * In the CAM_UNIT_STATUS_READY state, a unit has reserved the system resources
 * it needs for image acquisition/processing, but is not actually acquiring or
 * processing images.  Such units are bound to a specific output format that
 * cannot change until the unit is idle again.
 *
 * In the CAM_UNIT_STATUS_STREAMING state, a unit is fully "active", and is
 * acquiring and processing images.
 */
typedef enum {
    CAM_UNIT_STATUS_IDLE = 0,
    CAM_UNIT_STATUS_READY,
    CAM_UNIT_STATUS_STREAMING,
    CAM_UNIT_STATUS_MAX
} CamUnitStatus;

typedef enum {
    CAM_UNIT_RENDERS_GL         = (1<<2),
    CAM_UNIT_EVENT_METHOD_FD    = (1<<5),
    CAM_UNIT_EVENT_METHOD_TIMEOUT = (1<<6),
} CamUnitFlags;

/* ================ CamUnit =============== */

#define CAM_TYPE_UNIT  cam_unit_get_type()
#define CAM_UNIT(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CAM_TYPE_UNIT, CamUnit))
#define CAM_UNIT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_UNIT, CamUnitClass))
#define CAM_IS_UNIT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_UNIT))
#define CAM_IS_UNIT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE (\
            (klass), CAM_TYPE_UNIT))
#define CAM_UNIT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_UNIT, CamUnitClass))

typedef struct _CamUnit CamUnit;
typedef struct _CamUnitClass CamUnitClass;

struct _CamUnit {
    GInitiallyUnowned parent;

    /*< public >*/
    char * unit_id;

    /*< protected >*/
    CamUnit * input_unit;
    char * name;
    uint32_t flags;
    
    // the actual output format used.  borrowed pointer that points to a format
    // contained within the output_formats list.  NULL if the unit is not READY
    // or STREAMING
    const CamUnitFormat *fmt;

    /*< private >*/

    // do not modify this directly.  Instead, use cam_unit_set_status
    CamUnitStatus status;

    GHashTable *controls;
    GList *controls_list;

    GList *output_formats;

    // If the unit is initialized with cam_unit_stream_init_any_format, then
    // image formats matching these requests are preferred
    CamPixelFormat requested_pixelformat;
    int requested_width;
    int requested_height;
};

/**
 * CamUnitClass:
 * @stream_init:
 * @stream_shutdown:
 * @stream_on:
 * @stream_off:
 * @try_produce_frame: return TRUE if a frame is produced, FALSE if not.
 * @get_fileno:
 * @get_next_event_time:
 * @on_input_frame_ready:
 * @draw_gl_init:
 * @draw_gl:
 * @draw_gl_shutdown:
 * @try_set_control:
 *
 */
struct _CamUnitClass {
    GInitiallyUnownedClass parent;

    // ========== CamUnit virtual methods ============
    int (*stream_init)(CamUnit *self, const CamUnitFormat *format);
    int (*stream_on)(CamUnit *self);

    int (*stream_shutdown)(CamUnit *self);
    int (*stream_off)(CamUnit *self);

    // Input units should override these methods
    gboolean (*try_produce_frame) (CamUnit *self);
    int (*get_fileno)(CamUnit *self);
    int64_t (*get_next_event_time)(CamUnit *self);

    // Filter units should override this method
    void (*on_input_frame_ready) (CamUnit *self, const CamFrameBuffer *buf,
            const CamUnitFormat *infmt);

    // Units that can render to OpenGL should override these methods
    int (*draw_gl_init) (CamUnit * self);
    int (*draw_gl) (CamUnit *self);
    int (*draw_gl_shutdown) (CamUnit * self);

    /*< protected >*/

    // Individual units should override this method if it they need to take
    // action as soon as an option is set, or if it is possible that setting a
    // control value may fail.  
    // 
    // In this case, the method should return either 0 or 1
    //
    // A return value of
    //  FALSE : indicates the proposed value is not acceptable, and the
    //          existing control value should not change
    //   TRUE : indicates the control value should change, and the value to
    //          which it should change should be stored in actual.  If the
    //          proposed value is acceptable, then the overriding method
    //          should simply g_value_copy (proposed, actual)
    gboolean (*try_set_control)(CamUnit *self, const CamUnitControl *ctl, 
            const GValue *proposed, GValue *actual);
};

GType cam_unit_get_type(void);

// ======== CamUnit public methods =============

int cam_unit_set_input (CamUnit * self, CamUnit * src_unit);

/**
 * cam_unit_get_input:
 *
 * Returns: the input unit to this unit, or NULL if there is none
 */
CamUnit * cam_unit_get_input (CamUnit *self);

CamUnitStatus cam_unit_get_status (const CamUnit * self);
uint32_t cam_unit_get_flags (const CamUnit *self);

/**
 * cam_unit_get_name:
 *
 * Returns: a human-understable name for the unit
 */
const char * cam_unit_get_name(const CamUnit *self);

/**
 * cam_unit_get_id:
 *
 * Returns: a unit ID that uniquely identifies the unit within the computer
 */
const char * cam_unit_get_id(const CamUnit *self);

/**
 * cam_unit_get_output_format:
 *
 * This function returns the output format of the unit.
 *
 * Returns: the output format of the unit, or NULL if the unit is either not
 *          yet configured for output (e.g. IDLE) or has no output.
 */
const CamUnitFormat* cam_unit_get_output_format(CamUnit *self);

/**
 * cam_unit_get_output_formats:
 *
 * Enumerates the output formats supported by the unit.
 *
 * Returns: a GList of CamUnitFormat objects that should be released with
 * g_list_free after usage.  Do not modify the returned CamUnitFormat objects
 */
GList * cam_unit_get_output_formats(CamUnit *self);

/**
 * cam_unit_stream_init:
 * @format: the format to use in initialization
 *
 * Initializes a unit, reserves buffers and system resources, and prepares it
 * for streaming.  Must be called before cam_unit_stream_on
 *
 * Returns: 0 on success, < 0 on failure
 */
int cam_unit_stream_init (CamUnit * self, const CamUnitFormat *format);

/**
 * cam_unit_stream_init_any_format:
 *
 * convenience method to invoke cam_unit_stream_init with any format supported
 * by the unit.
 *
 * Returns: 0 on success, < 0 on failure
 */
int cam_unit_stream_init_any_format (CamUnit *self);

/**
 * cam_unit_set_preferred_format:
 * @pixelformat: the preferred pixel format, or CAM_PIXEL_FORMAT_INVALID to
 *               indicate that any pixel format is acceptable.
 * @width: the preferred image width, or 0 to indicate that any width is
 *         acceptable.
 * @height: the preferred image height, or 0 to indicate that any height is
 *          acceptable.
 *
 * Sets the preferred format when initializing the stream via
 * cam_unit_stream_init_any_format.  If a format matching the requested
 * parameters is not found, then an arbitrary format is chosen.
 *
 * Returns: 0 on success, -1 if format does not belong to the unit and is not
 * NULL
 */
int cam_unit_set_preferred_format (CamUnit *self, 
        CamPixelFormat pixelformat, int width, int height);

/**
 * cam_unit_stream_shutdown:
 *
 * Releases buffers and system resources reserved for image
 * acquisition/processing
 */
int cam_unit_stream_shutdown (CamUnit * self);

/**
 * cam_unit_stream_on:
 *
 * Starts the image acquisition/processing capabilities of the unit.  The unit
 * must be first be initialized.
 *
 * Returns: 0 on success, < 0 on failure
 */
int cam_unit_stream_on (CamUnit * self);

/**
 * cam_unit_stream_off:
 *
 * Halts the image acquisition/processing capabilities of the unit.
 */
int cam_unit_stream_off (CamUnit * self);

/**
 * cam_unit_try_produce_frame:
 * @timeout_ms: timeout (milliseconds)  If set to 0, then this method will
 * not block.  If set to a negative number, then this method blocks
 * indefinitely.
 *
 * Only meaningful for input units.  Tries for at most %timeout_ms milliseconds
 * to produce a frame.  If you use glib, attach a CamUnitChain to the glib
 * mainloop instead of calling this directly.  This method may block, but
 * should never spin.
 *
 * If the unit is misconfigured, not streaming, or otherwise not in a good
 * state, then this method may return immediately.
 *
 * Returns: TRUE if a frame was produced, FALSE if not
 */
gboolean cam_unit_try_produce_frame (CamUnit * self, int timeout_ms);

/**
 * cam_unit_get_fileno:
 *
 * Only meaningful for input units with the flag CAM_UNIT_EVENT_METHOD_FD
 *
 * Returns: a file number usable by select, poll, etc.
 */
int cam_unit_get_fileno(CamUnit *self);

/**
 * cam_unit_get_next_event_time:
 * 
 * Only meaningful for input units with the flag CAM_UNIT_EVENT_METHOD_TIMEOUT
 *
 * Returns: the next time that a call cam_unit_try_produce_frame might
 *          cause a frame to be produced
 */
int64_t cam_unit_get_next_event_time(CamUnit *self);

/**
 * cam_unit_list_controls:
 *
 * Retrieves the user controls available for this unit.  The list should be
 * released with g_list_free after usage.  Do not modify the objects in the
 * list.
 *
 * Returns: a GList of CamUnitControl objects.
 */
GList * cam_unit_list_controls(CamUnit * self);

/**
 * cam_unit_find_control:
 *
 * Searches for the CamUnitControl with the specified id.
 *
 * Returns: a pointer to the CamUnitControl, or NULL if not found.
 */
CamUnitControl* cam_unit_find_control (CamUnit *self, const char *id);

/**
 * cam_unit_set_control_int:
 *
 * Convenience function to set a control
 *
 * Returns: TRUE if the control was successfully set to the requested value,
 *          FALSE if not.
 */
gboolean cam_unit_set_control_int (CamUnit *self, const char *id, int val);

/**
 * cam_unit_set_control_float:
 *
 * Convenience function to set a control
 *
 * Returns: TRUE if the control was successfully set to the requested value,
 *          FALSE if not.
 */
gboolean cam_unit_set_control_float (CamUnit *self, const char *id, float val);

/**
 * cam_unit_set_control_enum:
 *
 * Convenience function to set a control
 *
 * Returns: TRUE if the control was successfully set to the requested value,
 *          FALSE if not.
 */
gboolean cam_unit_set_control_enum (CamUnit *self, const char *id, int index);

/**
 * cam_unit_set_control_boolean:
 *
 * Convenience function to set a control
 *
 * Returns: TRUE if the control was successfully set to the requested value,
 *          FALSE if not.
 */
gboolean cam_unit_set_control_boolean (CamUnit *self, const char *id, int val);

/**
 * cam_unit_set_control_string:
 *
 * Convenience function to set a control
 *
 * Returns: TRUE if the control was successfully set to the requested value,
 *          FALSE if not.
 */
gboolean cam_unit_set_control_string (CamUnit *self, const char *id, 
        const char *val);

/**
 * cam_unit_get_control_int:
 * @val: output parameter
 *
 * Convenience function to retrieve the value of a control.
 *
 * Returns: TRUE if the value of the value of the control was successfully read,
 *          FALSE if not
 */
gboolean cam_unit_get_control_int (CamUnit *self, const char *id, int *val);

/**
 * cam_unit_get_control_float:
 * @val: output parameter
 *
 * Convenience function to retrieve the value of a control.
 *
 * Returns: TRUE if the value of the value of the control was successfully read,
 *          FALSE if not
 */
gboolean cam_unit_get_control_float (CamUnit *self, const char *id, float *val);

/**
 * cam_unit_get_control_enum:
 * @val: output parameter
 *
 * Convenience function to retrieve the value of a control.
 *
 * Returns: TRUE if the value of the value of the control was successfully read,
 *          FALSE if not
 */
gboolean cam_unit_get_control_enum (CamUnit *self, const char *id, int *val);

/**
 * cam_unit_get_control_boolean:
 * @val: output parameter
 *
 * Convenience function to retrieve the value of a control.
 *
 * Returns: TRUE if the value of the value of the control was successfully read,
 *          FALSE if not
 */
gboolean cam_unit_get_control_boolean (CamUnit *self, const char *id, int *val);

/**
 * cam_unit_get_control_string:
 * @val: output parameter.  On a successful return, this points to a newly
 *       allocated string, and must be freed by the user when no longer needed.
 *
 * Convenience function to retrieve the value of a control.
 *
 * Returns: TRUE if the value of the value of the control was successfully read,
 *          FALSE if not
 */
gboolean cam_unit_get_control_string (CamUnit *self, const char *id, 
        char **val);

/**
 * cam_unit_draw_gl_init:
 *
 * Called once per unit when rendering with an OpenGL context
 *
 * Returns: 0 on success, -1 on failure
 */
int cam_unit_draw_gl_init (CamUnit * self);

/**
 * cam_unit_draw_gl:
 *
 * Call this method to have the unit render to an active OpenGL context
 */
int cam_unit_draw_gl (CamUnit *self);

/**
 * cam_unit_draw_gl_shutdown:
 *
 */
int cam_unit_draw_gl_shutdown (CamUnit * self);

// ========= CamUnit protected methods ========

/**
 * cam_unit_set_name:
 *
 * protected method.  Should only be called by subclasses of CamUnit.  Sets the
 * name of the unit.
 */
void cam_unit_set_name (CamUnit *self, const char *name);

/**
 * cam_unit_set_id:
 *
 * protected method.  Do not call directly.
 */
void cam_unit_set_id (CamUnit *self, const char *unit_id);

/**
 * cam_unit_add_control_enum:
 *
 * protected method.
 */
CamUnitControl* cam_unit_add_control_enum (CamUnit *self, const char *id,
        const char *name, int default_index, int enabled,
        const char **entries, const int * entries_enabled);
/**
 * cam_unit_add_control_int:
 *
 * protected method.
 */
CamUnitControl* cam_unit_add_control_int (CamUnit *self, const char *id,
        const char *name, int min, int max, int step, int default_val,
        int enabled);

/**
 * cam_unit_add_control_float:
 *
 * protected method.
 */
CamUnitControl*  cam_unit_add_control_float (CamUnit *self, const char *id,
        const char *name, float min, float max, float step, float default_val,
        int enabled);
/**
 * cam_unit_add_control_boolean:
 *
 * protected method.
 */
CamUnitControl* cam_unit_add_control_boolean (CamUnit *self, const char *id,
        const char *name, int default_val, int enabled);
/**
 * cam_unit_add_control_string:
 *
 * protected method.
 */
CamUnitControl* cam_unit_add_control_string (CamUnit *self, const char *id,
        const char *name, const char *default_val, int enabled);

/**
 * cam_unit_add_output_format_full:
 *
 * Protected method.  Subclasses of CamUnit should invoke this to configure the
 * acceptable output formats
 */
CamUnitFormat * cam_unit_add_output_format_full (CamUnit *self, 
        CamPixelFormat pfmt, const char *name, 
        int width, int height, int row_stride,
        int max_data_size);

/**
 * cam_unit_remove_output_format:
 *
 * Protected method.
 */
void cam_unit_remove_output_format (CamUnit *self, CamUnitFormat *fmt);

/**
 * cam_unit_remove_all_output_formats:
 *
 * Protected method.
 */
void cam_unit_remove_all_output_formats (CamUnit *self);

/**
 * cam_unit_set_status:
 *
 * Protected method.  Subclasses of CamUnit should invoke this to change the
 * status of the unit.  Invoking this methods emits the "status-changed"
 * signal.
 */
void cam_unit_set_status (CamUnit *self, CamUnitStatus newstatus);

/**
 * cam_unit_produce_frame:
 *
 * Protected method.  Subclasses of CamUnit shuld invoke this to signal that
 * a new frame is ready for consumption.  Invoking this method emits the
 * "frame-ready" signal.
 */
void cam_unit_produce_frame (CamUnit *self, 
        const CamFrameBuffer *buffer, const CamUnitFormat *fmt);

// ================== utility functions ==================

const char *cam_unit_status_to_str (CamUnitStatus status);

#ifdef __cplusplus
}
#endif

#endif /* CAM_UNIT_H */