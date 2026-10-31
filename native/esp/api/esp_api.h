#ifndef DC_ESP_API_H
#define DC_ESP_API_H
/*
 * API C del ESP. Pensada para el overlay (ImGui) y para JNI:
 *   dc_esp_attach()  arranca un hilo que lee memoria (no bloquea).
 *   dc_esp_poll()    convierte el último frame en rectángulos de pantalla.
 *   dc_esp_set_enabled() es el interruptor del overlay.
 *
 * Reglas:
 *   - dc_esp_poll() hace cero ioctl: se puede llamar a 60 fps en el hilo de dibujo.
 *   - Un solo lector por proceso (el driver permite un cliente a la vez).
 *   - Ninguna función requiere root: lo exige el driver al abrir.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DC_ESP_MAX_ENTITIES 512
#define DC_ESP_NAME_BYTES   64

typedef enum DcEspResult {
    DC_ESP_RESULT_OK = 0,
    DC_ESP_RESULT_ALREADY,
    DC_ESP_RESULT_NO_PID,
    DC_ESP_RESULT_NO_DRIVER,
    DC_ESP_RESULT_NO_WORLD,
    DC_ESP_RESULT_FAILED
} DcEspResult;

typedef enum DcEspState {
    DC_ESP_STATE_DETACHED = 0,
    DC_ESP_STATE_STARTING,
    DC_ESP_STATE_NO_PID,
    DC_ESP_STATE_NO_DRIVER,
    DC_ESP_STATE_NO_WORLD,
    DC_ESP_STATE_NO_CAMERA,
    DC_ESP_STATE_LIVE,
    DC_ESP_STATE_ERROR
} DcEspState;

typedef enum DcEspFlag {
    DC_ESP_FLAG_ENCRYPTED = 1u << 0,
    DC_ESP_FLAG_TEAMMATE   = 1u << 1,
    DC_ESP_FLAG_DEAD       = 1u << 2,
    DC_ESP_FLAG_NO_HEALTH  = 1u << 3
} DcEspFlag;

/* Un objetivo proyectado. x/y/w/h en píxeles, origen arriba-izquierda. */
typedef struct DcEspBox {
    uintptr_t actor;
    float     x, y, w, h;
    float     distance_m;
    float     health;
    int32_t   team;
    uint32_t  flags;
    char      name[DC_ESP_NAME_BYTES];
} DcEspBox;

typedef struct DcEspEntityView {
    uintptr_t actor;
    float     center[3];
    float     half_extents[3];
    float     rgba[4];
} DcEspEntityView;

/* Copia del frame canónico (metros) por si el motor de escena quiere dibujarlo. */
typedef struct DcEspView {
    float   camera_position[3];
    float   camera_target[3];
    float   camera_up[3];
    float   vertical_fov_radians;
    float   near_plane;
    float   far_plane;
    float   horizontal_fov_radians;
    int32_t count;
    DcEspEntityView entities[DC_ESP_MAX_ENTITIES];
} DcEspView;

/* Configuración del lector. Los valores a 0 toman el predeterminado. */
typedef struct DcEspConfig {
    const char* package;        /* NULL = "com.garena.game.df" */
    int   pid;                  /* <= 0: buscar por paquete */
    int   max_distance_m;       /* <= 0: 250 */
    int   min_health;           /* 0: sin filtro; 1: descarta HP<=0 */
    int   skip_teammates;       /* 0/1 (predeterminado 1) */
    int   poll_interval_ms;     /* <= 0: 16 */
} DcEspConfig;

/* Arranca el lector con configuración. Devuelve DcEspResult; no bloquea. */
int  dc_esp_attach_config(const DcEspConfig* config);

/* Forma corta: paquete y PID, todo lo demás por defecto. */
int  dc_esp_attach(const char* package_or_null, int pid);
void dc_esp_detach(void);
int  dc_esp_attached(void);

/* Interruptor. Devuelve el estado nuevo (1 encendido, 0 apagado). */
int  dc_esp_set_enabled(int enabled);
int  dc_esp_enabled(void);

int  dc_esp_state(void);              /* DcEspState */
const char* dc_esp_status(void);      /* texto listo para mostrar */
const char* dc_esp_last_error(void);
int  dc_esp_target_count(void);

/* Proyecta el último frame. Devuelve cuántos rectángulos escribió. */
int  dc_esp_poll(float screen_width, float screen_height, DcEspBox* boxes, int capacity);

/* Copia el frame canónico para el motor de escena. Devuelve el nº de entidades. */
int  dc_esp_view(DcEspView* view);

#ifdef __cplusplus
}
#endif

#endif /* DC_ESP_API_H */
