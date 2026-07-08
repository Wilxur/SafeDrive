#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

typedef enum {
    BUZZ_NONE,
    BUZZ_PHONE,       /* 800Hz, short double-beep */
    BUZZ_YAWN,        /* 1kHz, long beep */
    BUZZ_SEATBELT,    /* 1.5kHz, sustained */
    BUZZ_HANDSOFF,    /* 2kHz, urgent pulse */
} buzz_type_t;

void buzzer_init(void);
void buzzer_alert(buzz_type_t type);

#endif
