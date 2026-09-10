#include <drm_fourcc.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

//#define DRM_DEBUG

//----------------------------------------------------------------------------
//  DRM
//----------------------------------------------------------------------------

struct _Drm_Render_ {
    int fd_drm;
    drmModeModeInfo mode;
    drmModeCrtc *saved_crtc;
    // drmEventContext ev;
    int bpp;
    uint32_t connector_id, crtc_id, video_plane;
    uint32_t mmWidth, mmHeight; // Size in mm
};
typedef struct _Drm_Render_ VideoRender;

VideoRender *render;

int DRMRefresh = 50; 

//----------------------------------------------------------------------------
//  Helper functions
//----------------------------------------------------------------------------
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
struct type_name {
    unsigned int type;
    const char *name;
};

static const char *util_lookup_type_name(unsigned int type, const struct type_name *table, unsigned int count) {
    unsigned int i;

    for (i = 0; i < count; i++)
        if (table[i].type == type)
            return table[i].name;

    return NULL;
}

static const struct type_name connector_type_names[] = {
    {DRM_MODE_CONNECTOR_Unknown, "unknown"},
    {DRM_MODE_CONNECTOR_VGA, "VGA"},
    {DRM_MODE_CONNECTOR_DVII, "DVI-I"},
    {DRM_MODE_CONNECTOR_DVID, "DVI-D"},
    {DRM_MODE_CONNECTOR_DVIA, "DVI-A"},
    {DRM_MODE_CONNECTOR_Composite, "composite"},
    {DRM_MODE_CONNECTOR_SVIDEO, "s-video"},
    {DRM_MODE_CONNECTOR_LVDS, "LVDS"},
    {DRM_MODE_CONNECTOR_Component, "component"},
    {DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN"},
    {DRM_MODE_CONNECTOR_DisplayPort, "DP"},
    {DRM_MODE_CONNECTOR_HDMIA, "HDMI-A"},
    {DRM_MODE_CONNECTOR_HDMIB, "HDMI-B"},
    {DRM_MODE_CONNECTOR_TV, "TV"},
    {DRM_MODE_CONNECTOR_eDP, "eDP"},
    {DRM_MODE_CONNECTOR_VIRTUAL, "Virtual"},
    {DRM_MODE_CONNECTOR_DSI, "DSI"},
    {DRM_MODE_CONNECTOR_DPI, "DPI"},
};

void VideoSetRefresh(char *r) { 
    DRMRefresh = atoi(r); 
    NeedDRM = 1;
}

const char *util_lookup_connector_type_name(unsigned int type) {
    return util_lookup_type_name(type, connector_type_names, ARRAY_SIZE(connector_type_names));
}

#if 0
static uint64_t GetPropertyValue(int fd_drm, uint32_t objectID, uint32_t objectType, const char *propName) {
    uint32_t i;
    int found = 0;
    uint64_t value = 0;
    drmModePropertyPtr Prop;
    drmModeObjectPropertiesPtr objectProps = drmModeObjectGetProperties(fd_drm, objectID, objectType);

    for (i = 0; i < objectProps->count_props; i++) {
        if ((Prop = drmModeGetProperty(fd_drm, objectProps->props[i])) == NULL)
            fprintf(stderr, "GetPropertyValue: Unable to query property.\n");

        if (strcmp(propName, Prop->name) == 0) {
            value = objectProps->prop_values[i];
            found = 1;
        }

        drmModeFreeProperty(Prop);

        if (found)
            break;
    }

    drmModeFreeObjectProperties(objectProps);

#ifdef DRM_DEBUG
    if (!found)
        fprintf(stderr, "GetPropertyValue: Unable to find value for property \'%s\'.\n", propName);
#endif
    return value;
}
static uint32_t GetPropertyID(int fd_drm, uint32_t objectID, uint32_t objectType, const char *propName) {
    uint32_t i;
    int found = 0;
    uint32_t value = -1;
    drmModePropertyPtr Prop;
    drmModeObjectPropertiesPtr objectProps = drmModeObjectGetProperties(fd_drm, objectID, objectType);

    for (i = 0; i < objectProps->count_props; i++) {
        if ((Prop = drmModeGetProperty(fd_drm, objectProps->props[i])) == NULL)
            fprintf(stderr, "GetPropertyValue: Unable to query property.\n");

        if (strcmp(propName, Prop->name) == 0) {
            value = objectProps->props[i];
            found = 1;
        }
        drmModeFreeProperty(Prop);
        if (found)
            break;
    }
    drmModeFreeObjectProperties(objectProps);

#ifdef DRM_DEBUG
    if (!found)
        Debug(3, "GetPropertyValue: Unable to find ID for property \'%s\'.\n", propName);
#endif
    return value;
}
#endif

static int SetPropertyRequest(drmModeAtomicReqPtr ModeReq, int fd_drm, uint32_t objectID, uint32_t objectType,
                              const char *propName, uint64_t value) {
    uint32_t i;
    uint64_t id = 0;
    drmModePropertyPtr Prop;
    drmModeObjectPropertiesPtr objectProps = drmModeObjectGetProperties(fd_drm, objectID, objectType);

    for (i = 0; i < objectProps->count_props; i++) {
        if ((Prop = drmModeGetProperty(fd_drm, objectProps->props[i])) == NULL)
            printf("SetPropertyRequest: Unable to query property.\n");

        if (strcmp(propName, Prop->name) == 0) {
            id = Prop->prop_id;
            drmModeFreeProperty(Prop);
            break;
        }

        drmModeFreeProperty(Prop);
    }

    drmModeFreeObjectProperties(objectProps);

    if (id == 0)
        printf("SetPropertyRequest: Unable to find value for property \'%s\'.\n", propName);

    return drmModeAtomicAddProperty(ModeReq, objectID, id, value);
}

void set_video_mode(int width, int height) {
    drmModeConnector *connector;
    drmModeModeInfo *mode;
    int ii;
    printf("Set video mode %d &%d\n",width,height);
    if (height != 1080 && height != 2160)
        return;
    connector = drmModeGetConnector(render->fd_drm, render->connector_id);
    for (ii = 0; ii < connector->count_modes; ii++) {
        mode = &connector->modes[ii];
        printf("Mode %d %dx%d Rate %d\n", ii, mode->hdisplay, mode->vdisplay, mode->vrefresh);
        if (width == mode->hdisplay && height == mode->vdisplay && mode->vrefresh == DRMRefresh &&
            render->mode.hdisplay != width && render->mode.vdisplay != height &&
            !(mode->flags & DRM_MODE_FLAG_INTERLACE)) {
            memcpy(&render->mode, mode, sizeof(drmModeModeInfo));
            VideoWindowWidth = mode->hdisplay;
            VideoWindowHeight = mode->vdisplay;
            Debug(3, "Set new mode %d:%d\n", mode->hdisplay, mode->vdisplay);
            break;
        }
    }
}

static int FindDevice(VideoRender *render) {
    drmVersion *version;
    drmModeRes *resources;
    drmModeConnector *connector;
    drmModeEncoder *encoder = 0;
    drmModeModeInfo *mode;
    drmModePlane *plane;
    drmModePlaneRes *plane_res;
    
    uint32_t j;
    uint64_t has_dumb;
    uint64_t has_prime;
    int i, ii = 0;
    char connectorstr[10];
    int found = 0;
    render->fd_drm = open("/dev/dri/card0", O_RDWR);
    if (render->fd_drm < 0) {
        Debug(3, "FindDevice: cannot open /dev/dri/card0: %m\n");
        return -errno;
    }

    int ret = drmSetMaster(render->fd_drm);

    if (ret < 0) {
        drm_magic_t magic;

        ret = drmGetMagic(render->fd_drm, &magic);
        if (ret < 0) {
            Debug(3, "drm:%s - failed to get drm magic: %s\n", __FUNCTION__, strerror(errno));
            return -1;
        }

        ret = drmAuthMagic(render->fd_drm, magic);
        if (ret < 0) {
            Debug(3, "drm:%s - failed to authorize drm magic: %s\n", __FUNCTION__, strerror(errno));
            return -1;
        }
    }

    version = drmGetVersion(render->fd_drm);
    Debug(3, "FindDevice: open /dev/dri/card0:  %s\n", version->name);

    // check capability
    if (drmGetCap(render->fd_drm, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || has_dumb == 0)
        Debug(3, "FindDevice: drmGetCap DRM_CAP_DUMB_BUFFER failed or doesn't have dumb buffer\n");

    if (drmSetClientCap(render->fd_drm, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0)
        Debug(3, "FindDevice: DRM_CLIENT_CAP_UNIVERSAL_PLANES not available.\n");

    if (drmSetClientCap(render->fd_drm, DRM_CLIENT_CAP_ATOMIC, 1) != 0)
        Debug(3, "FindDevice: DRM_CLIENT_CAP_ATOMIC not available.\n");

    if (drmGetCap(render->fd_drm, DRM_CAP_PRIME, &has_prime) < 0)
        Debug(3, "FindDevice: DRM_CAP_PRIME not available.\n");

    if (drmGetCap(render->fd_drm, DRM_PRIME_CAP_EXPORT, &has_prime) < 0)
        Debug(3, "FindDevice: DRM_PRIME_CAP_EXPORT not available.\n");

    if (drmGetCap(render->fd_drm, DRM_PRIME_CAP_IMPORT, &has_prime) < 0)
        Debug(3, "FindDevice: DRM_PRIME_CAP_IMPORT not available.\n");

    if ((resources = drmModeGetResources(render->fd_drm)) == NULL) {
        Debug(3, "FindDevice: cannot retrieve DRM resources (%d): %m\n", errno);
        return -errno;
    }

#ifdef DEBUG
    Debug(3, "[FindDevice] DRM have %i connectors, %i crtcs, %i encoders\n", resources->count_connectors,
          resources->count_crtcs, resources->count_encoders);
#endif

    // find all available connectors
    for (i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(render->fd_drm, resources->connectors[i]);
        if (!connector) {
            fprintf(stderr, "FindDevice: cannot retrieve DRM connector (%d): %m\n", errno);
            return -errno;
        }

        sprintf(connectorstr, "%s-%u", util_lookup_connector_type_name(connector->connector_type),
                connector->connector_type_id);
        printf("Connector >%s< is %sconnected\n", connectorstr,
               connector->connection == DRM_MODE_CONNECTED ? "" : "not ");
        Debug(3,"Connector >%s< is %sconnected\n", connectorstr,
               connector->connection == DRM_MODE_CONNECTED ? "" : "not ");
        

        if (/*connector->connection == DRM_MODE_CONNECTED && */ connector->count_modes > 0) {
            float aspect = (float)connector->mmWidth / (float)connector->mmHeight;
            if ((aspect > 1.70) && (aspect < 1.85)) {
                render->mmHeight = 90;
                render->mmWidth = 160;
            } else {
                render->mmHeight = connector->mmHeight;
                render->mmWidth = connector->mmWidth;
            }
            render->connector_id = connector->connector_id;
            // FIXME: use default encoder/crtc pair
            if ((encoder = drmModeGetEncoder(render->fd_drm, connector->encoder_id)) == NULL) {
                Debug(3, "FindDevice: cannot retrieve encoder (%d): %m\n", errno);
                return -errno;
            }
            render->crtc_id = encoder->crtc_id;

            memcpy(&render->mode, &connector->modes[0], sizeof(drmModeModeInfo)); // set fallback
            // search Modes for Connector
            for (ii = 0; ii < connector->count_modes; ii++) {
                mode = &connector->modes[ii];

                printf("Mode %d %dx%d Rate %d\n", ii, mode->hdisplay, mode->vdisplay, mode->vrefresh);
                Debug(3,"Mode %d %dx%d Rate %d\n", ii, mode->hdisplay, mode->vdisplay, mode->vrefresh);
                if (VideoWindowWidth && VideoWindowHeight) { // preset by command line
                    if (VideoWindowWidth == mode->hdisplay && VideoWindowHeight == mode->vdisplay &&
                        mode->vrefresh == DRMRefresh && !(mode->flags & DRM_MODE_FLAG_INTERLACE)) {
                        memcpy(&render->mode, mode, sizeof(drmModeModeInfo));
                        break;
                    }
                } else {
                    if (!(mode->flags & DRM_MODE_FLAG_INTERLACE)) {
                        memcpy(&render->mode, mode, sizeof(drmModeModeInfo));
                        VideoWindowWidth = mode->hdisplay;
                        VideoWindowHeight = mode->vdisplay;
                        break;
                    }
                }
            }
            found = 1;
            i = resources->count_connectors; // uuuuhh
        }

        if (found) {
            VideoWindowWidth = render->mode.hdisplay;
            VideoWindowHeight = render->mode.vdisplay;
        
            printf("Use Mode %d %dx%d Rate %d\n", ii, render->mode.hdisplay, render->mode.vdisplay,
                   render->mode.vrefresh);
            Debug(3,"Use Mode %d %dx%d Rate %d\n", ii, render->mode.hdisplay, render->mode.vdisplay,
                   render->mode.vrefresh);
        }
        drmModeFreeConnector(connector);
    }
    if (!found) {
        Debug(3, "Requested Connector not found or not connected\n");
        printf("Requested Connector not found or not connected\n");
        return -1;
    }
#if 0
    // find first plane
    if ((plane_res = drmModeGetPlaneResources(render->fd_drm)) == NULL)
        Debug(3, "FindDevice: cannot retrieve PlaneResources (%d): %m\n", errno);

    for (j = 0; j < plane_res->count_planes; j++) {
        plane = drmModeGetPlane(render->fd_drm, plane_res->planes[j]);

        if (plane == NULL)
            Debug(3, "FindDevice: cannot query DRM-KMS plane %d\n", j);

        for (i = 0; i < resources->count_crtcs; i++) {
            if (plane->possible_crtcs & (1 << i))
                break;
        }

        uint64_t type = GetPropertyValue(render->fd_drm, plane_res->planes[j], DRM_MODE_OBJECT_PLANE, "type");
        uint64_t zpos = 0;

#ifdef DRM_DEBUG // If more then 2 crtcs this must rewriten!!!
        printf("[FindDevice] Plane id %i crtc_id %i possible_crtcs %i possible CRTC %i type %s\n", plane->plane_id,
               plane->crtc_id, plane->possible_crtcs, resources->crtcs[i],
               (type == DRM_PLANE_TYPE_PRIMARY)   ? "primary plane"
               : (type == DRM_PLANE_TYPE_OVERLAY) ? "overlay plane"
               : (type == DRM_PLANE_TYPE_CURSOR)  ? "cursor plane"
                                                  : "No plane type");
#endif

        // test pixel format and plane caps
        for (k = 0; k < plane->count_formats; k++) {
            if (encoder->possible_crtcs & plane->possible_crtcs) {
                switch (plane->formats[k]) {
                    case DRM_FORMAT_XRGB2101010:
                        if (!render->video_plane) {
                            render->video_plane = plane->plane_id;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        drmModeFreePlane(plane);

    }

    drmModeFreePlaneResources(plane_res);
#endif
    drmModeFreeEncoder(encoder);
    drmModeFreeResources(resources);

#ifdef DRM_DEBUG
    printf("[FindDevice] DRM setup CRTC: %i video_plane: %i \n", render->crtc_id, render->video_plane);
#endif

    // save actual modesetting
    render->saved_crtc = drmModeGetCrtc(render->fd_drm, render->crtc_id);

    return 0;
}

///
/// Initialize video output module.
///
void VideoInitDrm() {

    if (!(render = calloc(1, sizeof(*render)))) {
       Debug(3,"video/DRM: out of memory\n");
        return;
    }

    if (FindDevice(render)) {
        Debug(3,"VideoInit: FindDevice() failed\n");
        return;
    }

    drmModeAtomicReqPtr ModeReq;
    const uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
    uint32_t modeID = 0;

    if (drmModeCreatePropertyBlob(render->fd_drm, &render->mode, sizeof(render->mode), &modeID) != 0) {
        Debug(3, "Failed to create mode property.\n");
        return;
    }
    if (!(ModeReq = drmModeAtomicAlloc())) {
        Debug(3, "cannot allocate atomic request (%d): %m\n", errno);
        return;
    }
    //printf("set CRTC %d of Connector %d aktiv\n", render->crtc_id, render->connector_id);
    SetPropertyRequest(ModeReq, render->fd_drm, render->crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID", modeID);
    SetPropertyRequest(ModeReq, render->fd_drm, render->connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID",
                       render->crtc_id);
    SetPropertyRequest(ModeReq, render->fd_drm, render->crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE", 1);

    if (drmModeAtomicCommit(render->fd_drm, ModeReq, flags, NULL) != 0)
        Debug(3, "cannot set atomic mode (%d): %m\n", errno);

    if (drmModeDestroyPropertyBlob(render->fd_drm, modeID) != 0)
        Debug(3, "cannot destroy property blob (%d): %m\n", errno);

    drmModeAtomicFree(ModeReq);
    //drmDropMaster(render->fd_drm);
    //close(render->fd_drm);
    //free(render);
}

static void drm_clean_up() {
   
    Debug(3,"drm cleanup %p",render);
    if (!render)
        return;
    
    if (render->saved_crtc) {
        drmModeSetCrtc(render->fd_drm, render->saved_crtc->crtc_id, render->saved_crtc->buffer_id, render->saved_crtc->x,
                   render->saved_crtc->y, &render->connector_id, 1, &render->saved_crtc->mode);
        drmModeFreeCrtc(render->saved_crtc);
    }
    drmDropMaster(render->fd_drm);
    close(render->fd_drm);
    free(render);
    render = NULL;
    NeedDRM = 1;  // activate drm for attach
}

#if 0

    // DRM Master-Rechte anfordern (wichtig für Legacy KMS Modosetting)
    if (drmSetMaster(drm_fd) < 0) {
        printf("Fehler beim Setzen von DRM Master \n");
        close(drm_fd);
        return false;
    }

    // Passende Ressourcen (Connector, CRTC) finden
    drmModeRes *resources = drmModeGetResources(drm_fd);
    
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(drm_fd, resources->connectors[i]);
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            connector_id = conn->connector_id;
            mode = conn->modes[0]; // Nutze die erste (oft native) Auflösung
            drmModeFreeConnector(conn);
            break;
        }
        drmModeFreeConnector(conn);
    }

    if (connector_id == 0) {
        fprintf(stderr, "Keinen aktiven Bildschirm gefunden.\n");
        return false;
    }

    // Aktive CRTC ermitteln
    int crtc_index = -1;
    crtc_id = find_active_crtc(drm_fd, &crtc_index);
    if (crtc_id == 0 || crtc_index == -1) {
        fprintf(stderr, "Keine aktive CRTC/Bildschirm gefunden.\n");
        close(drm_fd);
        return false;
    }
    printf("[DRM] Nutze aktive CRTC ID: %u (Index: %d)\n", crtc_id, crtc_index);

    drmModeFreeResources(resources);

    // Set atomic client caps so the kernel exposes atomic planes/properties
    drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmSetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1);

       // Nach einer geeigneten Video Plane suchen
    drmModePlaneRes *plane_res = drmModeGetPlaneResources(drm_fd);
    if (!plane_res) {
        perror("Kann Plane-Ressourcen nicht auslesen");
        close(drm_fd);
        return false;
    }

    video_plane_id = 0;
    
    for (uint32_t i = 0; i < plane_res->count_planes; i++) {
        uint32_t p_id = plane_res->planes[i];
        drmModePlane *plane = drmModeGetPlane(drm_fd, p_id);
        if (!plane) continue;

        // Kriterium 1: Passt die Plane zur aktuellen CRTC?
        if (!(plane->possible_crtcs & (1 << crtc_index))) {
            printf("Plane Nr %d passt nicht zum CRTC\n",p_id);
            drmModeFreePlane(plane);
            continue;
        }

        // Kriterium 2: Prüfen, ob das Format unterstützt wird
        bool format_ok = false;
        for (uint32_t j = 0; j < plane->count_formats; j++) {
            if (plane->formats[j] == target_format) {
                format_ok = true;
                break;
            }
        }
        if (!format_ok) {
            drmModeFreePlane(plane);
            continue;
        }

        // Kriterium 3: Über Properties prüfen, ob es sich um eine OVERLAY Plane handelt
        uint64_t plane_type = -1;
        if (get_plane_property_value(drm_fd, p_id, "type", &plane_type)) {
            // DRM_PLANE_TYPE_OVERLAY hat den numerischen Wert 0
            if (plane_type == DRM_PLANE_TYPE_PRIMARY) {
                video_plane_id = p_id;
                drmModeFreePlane(plane);
                break; // Gefunden!
            }
        }
        drmModeFreePlane(plane);
    }
#if 0
        // Retrieve properties attached to this plane
        drmModeObjectProperties *props = drmModeObjectGetProperties(drm_fd, video_plane_id, DRM_MODE_OBJECT_PLANE);
        if (!props) {
            fprintf(stderr, "Failed to get plane properties\n");
            return false;
        }

        // Discover the "pixel blend mode" property ID
        uint32_t blend_mode_prop_id = find_property_id(drm_fd, props, "pixel blend mode");
        if (blend_mode_prop_id == 0) {
            printf("The driver or this plane does not support the 'pixel blend mode' property.\n");
            return false;
        }
        printf("Found 'pixel blend mode' Property ID: %d\n", blend_mode_prop_id);

        // Discover the "alpha" property ID
        uint32_t alpha_prop_id = find_property_id(drm_fd, props, "alpha");
        if (alpha_prop_id == 0) {
            printf("The driver or this plane does not support the 'alpha' property.\n");
            return false;
        }
        printf("Found 'pixel blend mode' Property ID: %d\n", alpha_prop_id);

        // Look up the specific enum value for "Pre-multiplied"
        // Standard names are: "None", "Pre-multiplied", "Coverage"
        uint64_t coverage_val = find_enum_value(drm_fd, blend_mode_prop_id, "Coverage");
        if (coverage_val == (uint64_t)-1) {
            fprintf(stderr, "The hardware doesn't support Pre-multiplied blending.\n");
            return 1;
        }
        printf("Enum value index for 'Pre-multiplied': %lu\n", coverage_val);
        // Create an atomic request to update the property
        drmModeAtomicReq *req = drmModeAtomicAlloc();
        if (!req) {
            fprintf(stderr, "Failed to allocate atomic request\n");
            return false;
        }

        // Add the property change to the atomic batch
        int ret = drmModeAtomicAddProperty(req, video_plane_id, blend_mode_prop_id, coverage_val);
        if (ret < 0) {
            fprintf(stderr, "Failed to add property to atomic request\n");
            return false;
        }

        // Add the property change to the atomic batch
        ret = drmModeAtomicAddProperty(req, video_plane_id, alpha_prop_id, 0xffff);
        if (ret < 0) {
            fprintf(stderr, "Failed to add property to atomic request\n");
            return false;
        }

        // Commit the change to the hardware
        // Use DRM_MODE_ATOMIC_ALLOW_MODESET or DRM_MODE_ATOMIC_NONBLOCK as needed
        uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET; // Use TEST_ONLY first to validate if the HW accepts it
        ret = drmModeAtomicCommit(drm_fd, req, flags, NULL);
        if (ret < 0) {
            perror("Atomic test commit failed");
        } else {
            printf("Atomic configuration validated successfully! (Ready for actual page flip commit)\n");
        }
#endif
        
    
    
    drmModeFreePlaneResources(plane_res);

    if (video_plane_id == 0) {
        printf("Keine freie Overlay-Plane gefunden, die ABGR8888 auf dieser CRTC unterstützt.\n");
        close(drm_fd);
        return false;
    }
    printf("[DRM] Passende Video/Overlay Plane gefunden! ID: %u\n", video_plane_id);


// Helper to find the numeric enum value for a specific string choice (e.g., "Pre-multiplied")
uint64_t find_enum_value(int fd, uint32_t prop_id, const char *enum_name) {
    drmModePropertyPtr prop = drmModeGetProperty(fd, prop_id);
    if (!prop) return -1;

    for (int i = 0; i < prop->count_enums; i++) {
        if (strcmp(prop->enums[i].name, enum_name) == 0) {
            uint64_t val = prop->enums[i].value;
            drmModeFreeProperty(prop);
            return val;
        }
    }
    drmModeFreeProperty(prop);
    return -1;
}

// Helper to look up the ID and value string of a DRM property
uint32_t find_property_id(int fd, drmModeObjectProperties *props, const char *name) {
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
        if (!prop) continue;

        if (strcmp(prop->name, name) == 0) {
            uint32_t prop_id = prop->prop_id;
            drmModeFreeProperty(prop);
            return prop_id;
        }
        drmModeFreeProperty(prop);
    }
    return 0;
}

// Findet den Wert einer bestimmten Property (z.B. "type") einer Plane
static bool get_plane_property_value(int drm_fd, uint32_t plane_id, const char *prop_name, uint64_t *value_out) {
    drmModeObjectProperties *props = drmModeObjectGetProperties(drm_fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props) return false;

    bool found = false;
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyRes *prop = drmModeGetProperty(drm_fd, props->props[i]);
        if (!prop) continue;

        if (strcmp(prop->name, prop_name) == 0) {
            *value_out = props->prop_values[i];
            found = true;
            drmModeFreeProperty(prop);
            break;
        }
        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);
    return found;
}

// Hilfsfunktion: Findet eine aktive CRTC, um die Plane daran zu binden
static uint32_t find_active_crtc(int drm_fd, int *crtc_index_out) {
    drmModeRes *res = drmModeGetResources(drm_fd);
    if (!res) return 0;

    uint32_t crtc_id = 0;
    for (int i = 0; i < res->count_crtcs; i++) {
        drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, res->crtcs[i]);
        if (crtc && crtc->mode_valid) {
            crtc_id = crtc->crtc_id;
            *crtc_index_out = i;
            drmModeFreeCrtc(crtc);
            break;
        }
        if (crtc) drmModeFreeCrtc(crtc);
    }

    drmModeFreeResources(res);
    return crtc_id;
}

#endif