#include "WS2812.h"
#include <stdio.h>
#include "board.h"
#include "hpm_sysctl_drv.h"
#include "hpm_gptmr_drv.h"
#include "hpm_dmamux_drv.h"

#ifdef HPMSOC_HAS_HPMSDK_DMAV2

#include "hpm_dmav2_drv.h"

#else
#include "hpm_dma_drv.h"
#endif

#include "hpm_debug_console.h"


int main(void)
{
    board_init();
    WS2812_Init();
    board_delay_ms(10);
    WS2812_SetPixel(0, 0, 0, 0);
    WS2812_SetPixel(1, 0, 0, 0);
    WS2812_Update();
    while (1) {
        /* when the dma transfer reload value complete, need wait the last pulse complete*/
        WS2812_SetPixel(0, 255, 0, 0);
        WS2812_Update();
        board_delay_ms(1000);

        WS2812_SetPixel(0, 0, 255, 0);
        WS2812_Update();
        board_delay_ms(1000);

        WS2812_SetPixel(0, 0, 0, 255);
        WS2812_Update();
        board_delay_ms(1000);
    }
    return 0;
}
