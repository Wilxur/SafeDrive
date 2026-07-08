/******************************************************************************
 * File Name:   voice_player.h
 *
 * Description: Minimal I2S voice prompt playback module for SafeDrive DMS.
 *              Uses TDM0 + TLV320DAC3100 codec to play pre-recorded PCM
 *              audio on the on-board speaker.
 *******************************************************************************/
#ifndef VOICE_PLAYER_H
#define VOICE_PLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include "cy_pdl.h"

/* Voice prompt types — one per alarm scenario */
typedef enum {
    VOICE_READY = 0,     /* System ready */
    VOICE_SEATBELT,      /* Please fasten seatbelt */
    VOICE_PHONE,         /* Please don't use phone */
    VOICE_YAWN,          /* Please rest, fatigue detected */
    VOICE_HANDSOFF,      /* Please hold steering wheel */
    VOICE_COUNT
} voice_id_t;

/* One-time init: configure TDM I2S + TLV320DAC3100 codec.
 * @param i2c_context  Pointer to already-initialized I2C SCB context
 *                     (shared with display). Must be valid.
 * Call once at startup, after I2C hardware is initialized.
 * Returns 0 on success. */
int  voice_player_init(cy_stc_scb_i2c_context_t *i2c_context);

/* Play a voice prompt. Non-blocking — returns immediately.
 * The prompt plays in the background via I2S TX ISR.
 * If another prompt is already playing, it is NOT interrupted
 * (higher-priority alarms can cancel via voice_player_stop first). */
void voice_player_play(voice_id_t id);

/* Force-stop any ongoing playback. */
void voice_player_stop(void);

/* Returns true while audio is playing. */
bool voice_player_is_busy(void);

#endif /* VOICE_PLAYER_H */
