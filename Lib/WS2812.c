#include "WS2812.h"
#include <board.h>
#include <hpm_clock_drv.h>
#include <hpm_common.h>
#include <hpm_dma_mgr.h>
#include <hpm_dmamux_drv.h>
#include <hpm_dmav2_drv.h>
#include <hpm_gpio_drv.h>
#include <hpm_gpiom_drv.h>
#include <hpm_gpiom_soc_drv.h>
#include <hpm_gptmr_drv.h>
#include <hpm_soc.h>
#include <string.h>

static uint32_t _gptmr_freq = 0;
static uint32_t _bit0_pluse_width = 0;
static uint32_t _bit1_pluse_width = 0;

static const uint32_t _WS2812_Freq = 800000;
static const uint32_t _WS2812_DATA_WIDTH = DMA_TRANSFER_WIDTH_WORD;

ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(8)
static dma_linked_descriptor_t descriptors[WS2812_LED_NUM - 1];
static dma_resource_t dma_resource_pool;

ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(4)
static uint32_t WS2812_LED_Buffer
#if WS2812_LED_CONNECT == WS2812_CONNECT_LINE
    [WS2812_LED_NUM][24];
#elif WS2812_LED_CONNECT == WS2812_CONNECT_MATRIX
    [WS2812_LED_COL][WS2812_LED_ROW][24];
#elif WS2812_LED_CONNECT == WS2812_CONNECT_3D
    [WS2812_LED_COL][WS2812_LED_ROW][WS2812_LED_LAYER][24];
#endif

static WS2812_LED_t WS2812_LED[WS2812_LED_NUM];

static volatile bool dma_is_done = false;

static void GPTMR_Init()
{
    gptmr_channel_config_t config;
    gptmr_channel_get_default_config(_WS2812_GPTMR_PTR, &config);
    gptmr_stop_counter(_WS2812_GPTMR_PTR, WS2812_GPTMR_CHANNLE);
    config.cmp_initial_polarity_high = true;
    config.dma_request_event = gptmr_dma_request_on_reload;
    config.reload = _gptmr_freq / _WS2812_Freq;
    config.cmp[0] = UINT32_MAX;
    config.enable_cmp_output = true;
    gptmr_channel_config(_WS2812_GPTMR_PTR, WS2812_GPTMR_CHANNLE, &config, false);
    gptmr_start_counter(_WS2812_GPTMR_PTR, WS2812_GPTMR_CHANNLE);

    dma_mgr_enable_channel(&dma_resource_pool);
}

/**
 * @brief 初始化LED连接方式，该函数为弱函数，库里仅提供默认的连接方式，用户可以自行实现
 */
ATTR_WEAK
void WS2812_LEDConnectInit(void)
{
#if WS2812_LED_CONNECT == WS2812_CONNECT_LINE
    // 默认情况下每一个灯都是首尾相连，链表中的第一个灯的数据是第一个灯的数据，最后一个灯的数据是第一个灯的数据
    for (size_t i = 0; i < WS2812_LED_NUM; i++)
    {
        WS2812_LED[i].buffer = &WS2812_LED_Buffer[i][0];
        if (i < WS2812_LED_NUM - 1)
        {
            WS2812_LED[i].next = &WS2812_LED[(i + 1) % WS2812_LED_NUM];
        }
    }
#elif WS2812_LED_CONNECT == WS2812_CONNECT_MATRIX
    // 矩阵连接方式，默认每一行的最后一个灯的数据是下一行的第一个灯的数据。
    // WS2812_LED 始终为一维数组，通过计算行和列的关系，将其映射到一维数组中
    for (int i = 0; i < WS2812_LED_COL; i++)
    {
        for (int j = 0; j < WS2812_LED_ROW; j++)
        {
            uint32_t index = i * WS2812_LED_ROW + j;
            WS2812_LED[index].buffer = WS2812_LED_Buffer[i][j];
            if (index < WS2812_LED_NUM - 1)
            {
                WS2812_LED[index].next = &WS2812_LED[index + 1];
            }
        }
    }
#elif WS2812_LED_CONNECT == WS2812_CONNECT_3D
    // 3D连接方式，默认每一层的最后一个灯的数据是下一层的第一个灯的数据。
    // WS2812_LED 始终为一维数组，通过计算行和列的关系，将其映射到一维数组中
    for (int i = 0; i < WS2812_LED_COL; i++)
    {
        for (int j = 0; j < WS2812_LED_ROW; j++)
        {
            for (int k = 0; k < WS2812_LED_LAYER; k++)
            {
                uint32_t index = i * WS2812_LED_ROW * WS2812_LED_LAYER + j * WS2812_LED_LAYER + k;
                WS2812_LED[index].buffer = WS2812_LED_Buffer[i][j][k];
                if (index < WS2812_LED_NUM - 1)
                {
                    WS2812_LED[index].next = &WS2812_LED[index + 1];
                }
            }
        }
    }
#endif
}

void WS2812_DMA_Callback(DMAV2_Type *ptr, uint32_t channel, void *user_data)
{
    (void)ptr;
    (void)channel;
    (void)user_data;

    dma_resource_t *resource = &dma_resource_pool;
    if (resource->channel == channel)
    {
        static uint32_t i = 0;
        i++;
        if (i == WS2812_LED_NUM)
        {
            gptmr_stop_counter(_WS2812_GPTMR_PTR, WS2812_GPTMR_CHANNLE);
            dma_mgr_disable_channel(resource);
            gptmr_clear_status(_WS2812_GPTMR_PTR, GPTMR_CH_RLD_STAT_MASK(WS2812_GPTMR_CHANNLE));

            HPM_IOC->PAD[_WS2812_DIN_PIN].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0);
            gpio_write_pin(HPM_GPIO0, GPIO_GET_PORT_INDEX(_WS2812_DIN_PIN), GPIO_GET_PIN_INDEX(_WS2812_DIN_PIN), 0);

            dma_is_done = true;
            i = 0;
        }
    }
}

void DMA_Init()
{
    dma_mgr_chn_conf_t ch_config;
    dma_resource_t *resource = NULL;

    resource = &dma_resource_pool;
    dma_mgr_get_default_chn_config(&ch_config);

    for (int i = 0; i < WS2812_LED_NUM - 1; i++)
    {
        ch_config.src_addr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&WS2812_LED[i + 1].buffer[0]);
        ch_config.src_mode = DMA_HANDSHAKE_MODE_NORMAL;
        ch_config.dst_addr = (uint32_t)&_WS2812_GPTMR_PTR->CHANNEL[WS2812_GPTMR_CHANNLE].CMP[0];
        ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
        ch_config.src_width = _WS2812_DATA_WIDTH;
        ch_config.dst_width = _WS2812_DATA_WIDTH;
        ch_config.dst_mode = DMA_HANDSHAKE_MODE_HANDSHAKE;
        ch_config.dst_addr_ctrl = DMA_ADDRESS_CONTROL_FIXED;
        ch_config.size_in_byte = 96;
        ch_config.en_dmamux = true;
        ch_config.dmamux_src = _WS2812_DMAMUX_SRC;
        ch_config.en_dmamux = true;
        ch_config.dmamux_src = _WS2812_DMAMUX_SRC;
        if (i == (WS2812_LED_NUM - 2))
        {
            ch_config.linked_ptr = 0;
        }
        else
        {
            ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&descriptors[i + 1]);
        }
        if (status_success !=
            dma_mgr_config_linked_descriptor(resource, &ch_config, (dma_mgr_linked_descriptor_t *)&descriptors[i]))
        {
            printf("generate dma desc fail\n");
            return;
        }
    }

    dma_mgr_get_default_chn_config(&ch_config);
    //    ch_config.src_addr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t) WS2812_LED[0].buffer);
    ch_config.src_addr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&WS2812_LED[0].buffer[0]);
    ch_config.dst_addr = (uint32_t)&_WS2812_GPTMR_PTR->CHANNEL[WS2812_GPTMR_CHANNLE].CMP[0];
    ch_config.src_mode = DMA_HANDSHAKE_MODE_NORMAL;
    ch_config.src_width = _WS2812_DATA_WIDTH;
    ch_config.src_addr_ctrl = DMA_ADDRESS_CONTROL_INCREMENT;
    ch_config.src_burst_size = DMA_NUM_TRANSFER_PER_BURST_1T;
    ch_config.dst_width = _WS2812_DATA_WIDTH;
    ch_config.dst_addr_ctrl = DMA_ADDRESS_CONTROL_FIXED;
    ch_config.dst_mode = DMA_HANDSHAKE_MODE_HANDSHAKE;
    ch_config.size_in_byte = 96;
    ch_config.priority = DMA_MGR_CHANNEL_PRIORITY_HIGH;
    ch_config.en_dmamux = true;
    ch_config.dmamux_src = _WS2812_DMAMUX_SRC;
#if WS2812_LED_NUM == 1
    ch_config.linked_ptr = 0;
#else
    ch_config.linked_ptr = core_local_mem_to_sys_address(HPM_CORE0, (uint32_t)&descriptors[0]);

#endif
    if (status_success != dma_mgr_setup_channel(resource, &ch_config))
    {
        printf("DMA setup channel failed\n");
        return;
    }
//    dma_mgr_setup_channel(resource, &ch_config);
    dma_mgr_install_chn_tc_callback(resource, WS2812_DMA_Callback, NULL);
    dma_mgr_enable_chn_irq(resource, DMA_MGR_INTERRUPT_MASK_TC);
    dma_mgr_enable_dma_irq_with_priority(resource, 1);
}

void WS2812_Init(void)
{
    HPM_IOC->PAD[_WS2812_DIN_PIN].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0); // 初始化GPIO
    gpiom_set_pin_controller(HPM_GPIOM, GPIO_GET_PORT_INDEX(_WS2812_DIN_PIN), GPIO_GET_PIN_INDEX(_WS2812_DIN_PIN),
                             gpiom_soc_gpio0);
    gpio_set_pin_output(HPM_GPIO0, GPIO_GET_PORT_INDEX(_WS2812_DIN_PIN), GPIO_GET_PIN_INDEX(_WS2812_DIN_PIN));
    gpio_write_pin(HPM_GPIO0, GPIO_GET_PORT_INDEX(_WS2812_DIN_PIN), GPIO_GET_PIN_INDEX(_WS2812_DIN_PIN), 0);

    _gptmr_freq = clock_get_frequency(_WS2812_GPTMR_NAME);  // 获取GPTMR的频率
    _bit0_pluse_width = _gptmr_freq / _WS2812_Freq / 3;     // 0的脉冲宽度
    _bit1_pluse_width = _gptmr_freq / _WS2812_Freq * 2 / 3; // 1的脉冲宽度

    WS2812_LEDConnectInit();
    for (int i = 0; i < WS2812_LED_NUM; i++)
    {
        WS2812_SetPixel(i, 0, 0, 0);
    }

    HPM_IOC->PAD[_WS2812_DIN_PIN].FUNC_CTL = _WS2812_DIN_FUNC; // 初始化GPIO
    dma_mgr_request_resource(&dma_resource_pool);
    DMA_Init();                                                // 初始化DMA
    GPTMR_Init();                                              // 初始化GPTMR
}

void WS2812_Update(void)
{
    while (!dma_is_done)
        ;
    dma_is_done = false;

    DMA_Init();                                                // 每次都重新配置一次
    HPM_IOC->PAD[_WS2812_DIN_PIN].FUNC_CTL = _WS2812_DIN_FUNC; // 初始化GPIO


    dma_mgr_enable_channel(&dma_resource_pool);
    gptmr_start_counter(_WS2812_GPTMR_PTR, WS2812_GPTMR_CHANNLE);
}

void WS2812_SetPixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index < WS2812_LED_NUM)
    {
        uint32_t *buf = &WS2812_LED_Buffer[index][0];
        // GRB
        for (int i = 0; i < 8; i++)
        {
            if (g & (1 << (7 - i)))
            {
                buf[i] = _bit0_pluse_width;
            }
            else
            {
                buf[i] = _bit1_pluse_width;
            }

            if (r & (1 << (7 - i)))
            {
                buf[i + 8] = _bit0_pluse_width;
            }
            else
            {
                buf[i + 8] = _bit1_pluse_width;
            }

            if (b & (1 << (7 - i)))
            {
                buf[i + 16] = _bit0_pluse_width;
            }
            else
            {
                buf[i + 16] = _bit1_pluse_width;
            }
        }
    }
}
