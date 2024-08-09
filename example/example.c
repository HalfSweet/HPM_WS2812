#include "WS2812.h"
#include "board.h"
#include "hpm_dmamux_drv.h"
#include "hpm_gptmr_drv.h"
#include "hpm_sysctl_drv.h"
#include <stdio.h>

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
    while (1)
    {
        /* when the dma transfer reload value complete, need wait the last pulse complete*/
        // 单个灯彩虹效果

        for (int j = 0; j < 256; j++)
        {
            uint8_t r, g, b;
            uint8_t pos = j & 255;
            if (pos < 85)
            {
                r = pos * 3;
                g = 255 - pos * 3;
                b = 0;
            }
            else if (pos < 170)
            {
                pos -= 85;
                r = 255 - pos * 3;
                g = 0;
                b = pos * 3;
            }
            else
            {
                pos -= 170;
                r = 0;
                g = pos * 3;
                b = 255 - pos * 3;
            }
            for (size_t i = 0; i < WS2812_LED_NUM; i++)
            {
                WS2812_SetPixel(i, r, g, b);
            }
            WS2812_Update();
            board_delay_ms(10);
        }
    }
    return 0;
}
