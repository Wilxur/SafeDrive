#ifndef SHARED_DMS_H
#define SHARED_DMS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define DMS_MAX_BOXES   5

/* Alarm categories determined by the highest-priority dangerous class */
typedef enum {
    ALARM_NONE        = 0,   /* No dangerous class detected */
    ALARM_CLOSED_EYES = 1,   /* Driver eyes closed */
    ALARM_YAWN        = 2,   /* Driver yawning (fatigue) */
    ALARM_PHONE       = 3,   /* Driver using phone */
    ALARM_NO_SEATBELT = 4,   /* No seatbelt detected */
    ALARM_HANDS_OFF   = 5    /* Hands off steering wheel */
} dms_alarm_t;

/* Detection box structure — integer fields only, avoids floating-point format
 * differences between CM55 (MVE) and CM33 (no MVE) */
typedef struct {
    uint16_t class_id;      /* 0~7 */
    uint16_t confidence;    /* scaled x100: 78% stored as 7800 */
    int16_t  x, y, w, h;    /* top-left corner + width/height in pixel coords */
} dms_box_t;

/* Shared memory main structure — placed in m33_m55_shared region at 0x26480000 */
typedef struct {
    volatile bool     fresh;            /* CM55 sets true when data ready, CM33 clears after read */
    volatile uint32_t frame_id;         /* Monotonic frame counter for dedup */
    uint8_t           box_count;        /* Number of valid boxes (0..DMS_MAX_BOXES) */
    uint8_t           alarm_level;      /* 0=normal, 1-5=alarm (see dms_alarm_t) */
    dms_box_t         boxes[DMS_MAX_BOXES];
    char              top_class[16];    /* Highest-confidence class name for JSON */
    volatile bool     cm55_active;        /* true=CM55 in ACTIVE mode (YOLO running), false=IDLE */
    volatile uint32_t idle_frame_count;   /* Consecutive still-frame count in ACTIVE mode */
    volatile int32_t  credit_score;       /* Cloud credit score, CM33 writes after upload */
    volatile uint8_t  blacklisted;        /* Cloud blacklist flag, CM33 writes after upload */
    volatile bool     wifi_connected;     /* Wi-Fi STA connected (CM33 writes) */
    volatile bool     cloud_connected;    /* Cloud JWT login OK (CM33 writes) */
} __attribute__((packed)) dms_shared_mem_t;

/*
 * Shared memory fixed address in m33_m55_shared region.
 * S-AHB address 0x26480000, size 256KB, accessible by both CM55 and CM33_ns.
 * Verified against cymem_gnu_CM55_0.ld and cymem_gnu_CM33_0.ld.
 */
#define DMS_SHARED_MEM_ADDR  ((volatile dms_shared_mem_t *)0x26480000)

/* Memory barrier for multi-core cache coherence.
 * __DMB() ensures all explicit memory accesses complete before continuing.
 * Use before reading volatile shared fields and after writing them. */
#define DMS_BARRIER()  __DMB()

#endif
