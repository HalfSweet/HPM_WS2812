#ifndef WS2812_TYPE_H
#define WS2812_TYPE_H

/**
 * @brief WS2812 灯珠连接方式
 *
 */
#define WS2812_CONNECT_LINE 0
#define WS2812_CONNECT_MATRIX 1
#define WS2812_CONNECT_3D 2

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} WS2812_RGB_t;

/**
 * @brief WS2812 灯珠链表，其中有一个指向灯珠buffer的地址和下一个灯珠的链表
 */
 typedef struct WS2812_LED {
     uint32_t *buffer;
     struct WS2812_LED *next;
 }WS2812_LED_t;

#endif //WS2812_TYPE_H
