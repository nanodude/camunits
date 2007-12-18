#ifndef __fast_bayer_filter_h__
#define __fast_bayer_filter_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * SECTION:filter_fast_bayer
 * @short_description: SSE2/SSE3 optimized Bayer demosaic filter.
 *
 * An SSE2/SSE3 optimized Bayer demosaic filter.  Do _NOT_ try to use this
 * filter if your architecture doesn't support SSE2/SSE3.
 *
 * CamFastBayerFilter is a core unit, and is always available with unit
 * id "convert.fast_debayer"
 */

typedef struct _CamFastBayerFilter CamFastBayerFilter;
typedef struct _CamFastBayerFilterClass CamFastBayerFilterClass;

// boilerplate
#define CAM_TYPE_FAST_BAYER_FILTER  cam_fast_bayer_filter_get_type()
#define CAM_FAST_BAYER_FILTER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_FAST_BAYER_FILTER, CamFastBayerFilter))
#define CAM_FAST_BAYER_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_FAST_BAYER_FILTER, CamFastBayerFilterClass ))
#define CAM_IS_FAST_BAYER_FILTER(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_FAST_BAYER_FILTER ))
#define CAM_IS_FAST_BAYER_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_FAST_BAYER_FILTER))
#define CAM_FAST_BAYER_FILTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_FAST_BAYER_FILTER, CamFastBayerFilterClass))

struct _CamFastBayerFilter {
    CamUnit parent;
    CamUnitControl *bayer_tile_ctl;

    uint8_t * planes[4];
    int plane_stride;
};

struct _CamFastBayerFilterClass {
    CamUnitClass parent_class;
};

GType cam_fast_bayer_filter_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamFastBayerFilter * cam_fast_bayer_filter_new (void);

CamUnitDriver * cam_fast_bayer_filter_driver_new (void);

G_END_DECLS

#endif
