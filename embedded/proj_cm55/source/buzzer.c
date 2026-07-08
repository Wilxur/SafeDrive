#include "buzzer.h"
#include "cy_pdl.h"
#include "retarget_io_init.h"

#define BUZZER_PORT  GPIO_PRT14
#define BUZZER_PIN   6U

void buzzer_init(void)
{
    Cy_GPIO_SetDrivemode(BUZZER_PORT, BUZZER_PIN, CY_GPIO_DM_STRONG);
    Cy_GPIO_SetHSIOM(BUZZER_PORT, BUZZER_PIN, HSIOM_SEL_GPIO);
    Cy_GPIO_Write(BUZZER_PORT, BUZZER_PIN, 0U);
    printf("[BUZZER] Init OK (P14.6 GPIO)\r\n");
}

/* Toggle GPIO at freq Hz for duration_ms */
static void beep(uint32_t freq, uint32_t duration_ms)
{
    uint32_t half_period_us = 1000000U / (freq * 2U);  /* half cycle in microseconds */
    uint32_t toggles = (uint32_t)((uint64_t)freq * 2ULL * duration_ms / 1000ULL);

    for (uint32_t i = 0; i < toggles; i++) {
        Cy_GPIO_Inv(BUZZER_PORT, BUZZER_PIN);
        Cy_SysLib_DelayUs(half_period_us);
    }
    Cy_GPIO_Write(BUZZER_PORT, BUZZER_PIN, 0U);
}

void buzzer_alert(buzz_type_t type)
{
    switch (type) {
    case BUZZ_PHONE:
        /* 800Hz, 100ms × 2, gap 100ms */
        beep(800, 100);
        Cy_SysLib_Delay(100);
        beep(800, 100);
        break;

    case BUZZ_YAWN:
        /* 1kHz, 200ms × 2, gap 300ms */
        beep(1000, 200);
        Cy_SysLib_Delay(300);
        beep(1000, 200);
        break;

    case BUZZ_SEATBELT:
        /* 1.5kHz, 500ms sustained */
        beep(1500, 500);
        break;

    case BUZZ_HANDSOFF:
        /* 2kHz, 50ms × 5, gap 50ms */
        for (int i = 0; i < 5; i++) {
            beep(2000, 50);
            Cy_SysLib_Delay(50);
        }
        break;

    default:
        break;
    }
}
