/******************************************************************************
 * File Name:   voice_player.c
 *
 * Description: Minimal I2S voice prompt playback for SafeDrive DMS.
 *              TDM0 + TLV320DAC3100 codec → on-board speaker.
 *              Based on app_i2s.c from PSOC Edge Mains Powered Local Voice.
 *
 * PCM format: 16 kHz, 16-bit signed, mono. ISR duplicates to stereo.
 *******************************************************************************/

#include "voice_player.h"
#include "cy_pdl.h"
#include "cybsp.h"
#include "mtb_hal.h"
#include "mtb_tlv320dac3100.h"
#include "FreeRTOS.h"
#include "task.h"

/* ========== Voice prompt PCM data ========== */
#include "voice_ready.h"
#include "voice_seatbelt.h"
#include "voice_phone.h"
#include "voice_yawn.h"
#include "voice_handsoff.h"

/* ========== Hardware constants ========== */
#define HW_FIFO_SIZE                (64U)       /* TDM TX FIFO depth in 16-bit words */
#define I2S_WORD_LEN                (16U)
#define I2S_PLAYBACK_RATE_HZ        (16000U)
#define I2S_MCLK_HZ                 (2048000UL)  /* 16kHz * 128 = 2.048MHz */
#define I2S_CLK_DIV                 (24U)        /* 49.152MHz / 2.048MHz = 24 */
#define SYSCLK_DIV_WAIT_MS          (50U)

#define I2C_ADDR_TLV                (0x18)
#define I2C_FREQ_HZ                 (100000UL)
#define I2S_INTR_PRIORITY           (2U)

/* I2S channel count for mono→stereo duplication */
#define I2S_CHANNELS                (2U)

/* ========== Voice prompt table ========== */
typedef struct {
    const int16_t *data;
    uint32_t       sample_count;   /* Number of int16_t samples (mono) */
} voice_entry_t;

static const voice_entry_t voice_table[VOICE_COUNT] = {
    [VOICE_READY]    = { audio_ready,    sizeof(audio_ready)    / sizeof(int16_t) },
    [VOICE_SEATBELT] = { audio_seatbelt, sizeof(audio_seatbelt) / sizeof(int16_t) },
    [VOICE_PHONE]    = { audio_phone,    sizeof(audio_phone)    / sizeof(int16_t) },
    [VOICE_YAWN]     = { audio_yawn,     sizeof(audio_yawn)     / sizeof(int16_t) },
    [VOICE_HANDSOFF] = { audio_handsoff, sizeof(audio_handsoff) / sizeof(int16_t) },
};

/* ========== Playback state ========== */
static volatile const int16_t *g_pcm_buf = NULL;
static volatile uint32_t        g_pcm_total = 0;
static volatile uint32_t        g_pcm_idx = 0;
static volatile bool            g_playing = false;

/* ========== Hardware state ========== */
static bool                     g_i2s_initialized = false;
static mtb_hal_i2c_t            g_i2c_hal_obj;

/* ========== I2C helper — uses display's already-initialized I2C context ========== */
static void i2c_setup(cy_stc_scb_i2c_context_t *i2c_context)
{
    cy_rslt_t result;
    mtb_hal_i2c_cfg_t i2c_cfg = {
        .is_target   = false,
        .address     = I2C_ADDR_TLV,
        .frequency_hz = I2C_FREQ_HZ,
        .address_mask = MTB_HAL_I2C_DEFAULT_ADDR_MASK,
        .enable_address_callback = false,
    };

    /* Reuse the display's I2C context — hardware is already initialized */
    result = mtb_hal_i2c_setup(&g_i2c_hal_obj,
                                &CYBSP_I2C_CONTROLLER_hal_config,
                                i2c_context, NULL);
    if (CY_RSLT_SUCCESS != result) {
        printf("[VOICE] I2C HAL setup failed: 0x%08lx\n", (unsigned long)result);
        return;
    }

    result = mtb_hal_i2c_configure(&g_i2c_hal_obj, &i2c_cfg);
    if (CY_RSLT_SUCCESS != result) {
        printf("[VOICE] I2C configure failed: 0x%08lx\n", (unsigned long)result);
        return;
    }
}

/* ========== TDM (I2S) TX ISR ========== */
static void i2s_tx_isr(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t intr_status = Cy_AudioTDM_GetTxInterruptStatusMasked(TDM_STRUCT0_TX);

    if (CY_TDM_INTR_TX_FIFO_TRIGGER & intr_status) {
        for (uint32_t i = 0; i < HW_FIFO_SIZE; i++) {
            int16_t s = 0;
            if (g_playing && g_pcm_buf != NULL && g_pcm_idx < g_pcm_total) {
                s = g_pcm_buf[g_pcm_idx++];
            }
            /* Write left channel, duplicate to right for stereo */
            Cy_AudioTDM_WriteTxData(TDM_STRUCT0_TX, (uint32_t)(int32_t)s);
            Cy_AudioTDM_WriteTxData(TDM_STRUCT0_TX, (uint32_t)(int32_t)s);
            i++;  /* compensate: stereo uses 2 FIFO slots per loop */
        }

        /* Auto-stop when playback completes */
        if (g_playing && g_pcm_idx >= g_pcm_total) {
            g_playing = false;
            g_pcm_buf = NULL;
        }
    } else if (CY_TDM_INTR_TX_FIFO_UNDERFLOW & intr_status) {
        /* Underflow — benign during transitions */
    }

    Cy_AudioTDM_ClearTxInterrupt(TDM_STRUCT0_TX, CY_TDM_INTR_TX_MASK);

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* ========== TDM init ========== */
static void tdm_init(void)
{
    const cy_stc_sysint_t isr_cfg = {
        .intrSrc     = (IRQn_Type)tdm_0_interrupts_tx_0_IRQn,
        .intrPriority = I2S_INTR_PRIORITY,
    };

    Cy_SysInt_Init((cy_stc_sysint_t *)&isr_cfg, i2s_tx_isr);

    cy_en_tdm_status_t status = Cy_AudioTDM_Init(TDM_STRUCT0,
                                                   &CYBSP_TDM_CONTROLLER_0_config);
    if (CY_TDM_SUCCESS != status) {
        printf("[VOICE] TDM init failed: 0x%x\n", (unsigned)status);
        return;
    }
}

/* ========== TLV320DAC3100 codec init via I2C ========== */
static void codec_init(cy_stc_scb_i2c_context_t *i2c_context)
{
    /* Set TDM clock divider for 16 kHz */
    Cy_SysClk_PeriPclkDisableDivider(
        (en_clk_dst_t)CYBSP_TDM_CONTROLLER_0_CLK_DIV_GRP_NUM,
        CY_SYSCLK_DIV_16_5_BIT, 0U);
    Cy_SysClk_PeriPclkSetFracDivider(
        (en_clk_dst_t)CYBSP_TDM_CONTROLLER_0_CLK_DIV_GRP_NUM,
        CY_SYSCLK_DIV_16_5_BIT, 0U,
        I2S_CLK_DIV - 1, 0U);
    Cy_SysClk_PeriPclkEnableDivider(
        (en_clk_dst_t)CYBSP_TDM_CONTROLLER_0_CLK_DIV_GRP_NUM,
        CY_SYSCLK_DIV_16_5_BIT, 0U);

    vTaskDelay(pdMS_TO_TICKS(SYSCLK_DIV_WAIT_MS));

    /* Init I2C and codec */
    i2c_setup(i2c_context);
    mtb_tlv320dac3100_init(&g_i2c_hal_obj);
    mtb_tlv320dac3100_configure_clocking(I2S_MCLK_HZ,
                                          (mtb_tlv320dac3100_dac_sample_rate_t)I2S_PLAYBACK_RATE_HZ,
                                          I2S_WORD_LEN,
                                          TLV320DAC3100_SPK_AUDIO_OUTPUT);
    mtb_tlv320dac3100_activate();
    mtb_tlv320dac3100_adjust_speaker_output_volume(127); /* max volume */
}

/* ========== Enable / disable TDM TX ========== */
static void tdm_tx_enable(void)
{
    Cy_AudioTDM_EnableTx(TDM_STRUCT0_TX);
    Cy_AudioTDM_ClearTxInterrupt(TDM_STRUCT0_TX, CY_TDM_INTR_TX_MASK);
    Cy_AudioTDM_SetTxInterruptMask(TDM_STRUCT0_TX, CY_TDM_INTR_TX_MASK);

    /* Prime FIFO with zeros to start transmission.
     * HW_FIFO_SIZE is 64 slots; stereo = 2 slots per sample pair,
     * so we fill 32 pairs = 64 total. */
    for (uint32_t i = 0; i < HW_FIFO_SIZE / 2; i++) {
        Cy_AudioTDM_WriteTxData(TDM_STRUCT0_TX, 0UL);
        Cy_AudioTDM_WriteTxData(TDM_STRUCT0_TX, 0UL);
    }
}

static void tdm_tx_activate(void)
{
    NVIC_EnableIRQ((IRQn_Type)tdm_0_interrupts_tx_0_IRQn);
    Cy_AudioTDM_ActivateTx(TDM_STRUCT0_TX);
}

static void tdm_tx_deactivate(void)
{
    NVIC_DisableIRQ((IRQn_Type)tdm_0_interrupts_tx_0_IRQn);
    Cy_AudioTDM_DeActivateTx(TDM_STRUCT0_TX);
    Cy_AudioI2S_DisableTx(TDM_STRUCT0_TX);
    Cy_AudioI2S_EnableTx(TDM_STRUCT0_TX);
    Cy_AudioTDM_DeActivateTx(TDM_STRUCT0_TX);
    Cy_AudioI2S_DisableTx(TDM_STRUCT0_TX);
}

static void tdm_tx_disable(void)
{
    Cy_AudioTDM_DisableTx(TDM_STRUCT0_TX);
    Cy_AudioTDM_ClearTxInterrupt(TDM_STRUCT0_TX, CY_TDM_INTR_TX_MASK);
    Cy_AudioTDM_ClearTxTriggerInterruptMask(TDM_STRUCT0_TX);
}

/* ========== Public API ========== */

int voice_player_init(cy_stc_scb_i2c_context_t *i2c_context)
{
    if (g_i2s_initialized) return 0;

    tdm_init();
    codec_init(i2c_context);

    /* Start TDM TX running — ISR fills zeros when idle */
    tdm_tx_enable();
    tdm_tx_activate();

    g_i2s_initialized = true;

    printf("[VOICE] Init OK (I2S 16kHz, TLV320DAC3100, speaker)\n");
    return 0;
}

void voice_player_play(voice_id_t id)
{
    if (!g_i2s_initialized || id >= VOICE_COUNT) return;
    if (g_playing) return;  /* Don't interrupt current playback */

    const voice_entry_t *v = &voice_table[id];

    g_pcm_buf   = v->data;
    g_pcm_total = v->sample_count;
    g_pcm_idx   = 0;
    g_playing   = true;

    /* Returns immediately — ISR picks up the buffer and plays it.
     * When done, ISR sets g_playing = false and fills zeros. */
}

void voice_player_stop(void)
{
    g_playing = false;
    g_pcm_buf = NULL;
    g_pcm_total = 0;
    g_pcm_idx = 0;
}

bool voice_player_is_busy(void)
{
    return g_playing;
}
