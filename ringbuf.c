#include "ringbuf.h"

void rxbuf_init(RxBuf *rb)
{
    rb->len      = 0;
    rb->overflow = 0;
}

void rxbuf_feed(RxBuf *rb, uint8_t b)
{
    if (rb->len >= RXBUF_SIZE) {
        rb->overflow++;         /* 已满：该字节丢弃，同时累加计数以便定位问题 */
        return;
    }
    rb->buf[rb->len] = b;
    rb->len++;
}

int rxbuf_take_frames(RxBuf *rb, Frame *out, int max)
{
    uint16_t consumed = 0;
    uint16_t n, i;

    /* extract_frames 经 consumed 返回本次扫描到的位置 */
    n = extract_frames(rb->buf, rb->len, out, max, &consumed);

    /* 消费：将已处理的 consumed 个字节移出，剩余字节整体前移。
     * 不做这一步则缓冲区只增不减，最终填满；
     * 且每次调用都会重复扫描此前已处理过的字节。 */
    if (consumed > 0) {
        for (i = 0; i < (uint16_t)(rb->len - consumed); i++) {
            rb->buf[i] = rb->buf[consumed + i];
        }
        rb->len = (uint16_t)(rb->len - consumed);
    }
    return n;
}
