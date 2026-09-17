#include "ringbuf.h"

void rxbuf_init(RxBuf *rb)
{
    rb->len      = 0;
    rb->overflow = 0;
}

void rxbuf_feed(RxBuf *rb, uint8_t b)
{
    if (rb->len >= RXBUF_SIZE) {
        rb->overflow++;         /* 满了就丢，但记一笔，方便定位问题 */
        return;
    }
    rb->buf[rb->len] = b;
    rb->len++;
}

int rxbuf_take_frames(RxBuf *rb, Frame *out, int max)
{
    uint16_t consumed = 0;
    uint16_t n, i;

    /* extract_frames 通过 consumed 告知「处理到哪个位置了」 */
    n = extract_frames(rb->buf, rb->len, out, max, &consumed);

    /* 消费：把已处理的 consumed 个字节移出缓冲区，剩余整体前移。
     * 不做这一步，缓冲区会一直涨，最后灌满；
     * 而且每次都会重复扫描已经处理过的老字节。 */
    if (consumed > 0) {
        for (i = 0; i < (uint16_t)(rb->len - consumed); i++) {
            rb->buf[i] = rb->buf[consumed + i];
        }
        rb->len = (uint16_t)(rb->len - consumed);
    }
    return n;
}
