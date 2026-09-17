#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include "frame.h"

/* 接收缓冲区大小，按实际需要调整（STM32 上需计入 RAM 开销） */
#define RXBUF_SIZE 256

/* 接收缓冲区
 *
 * 用途：将「收集字节」与「解析帧」解耦，对应嵌入式的经典分层 ——
 *
 *     串口中断：每收到一个字节 → rxbuf_feed()          轻量，只做存入
 *     主循环  ：有空就调       → rxbuf_take_frames()   解析放这里
 *
 * 实现说明：此处用「直线缓冲」，消费后将剩余字节整体前移。
 * 产品代码更常用「环形缓冲」（head/tail 指针，写到末尾绕回开头），
 * 优点是不搬数据；代价是数据可能绕圈、内存上不连续，
 * 而 extract_frames 需要连续内存，解析前须先"拉直"。
 * 本项目以逻辑清晰为先，故采用直线缓冲。
 */
typedef struct {
    uint8_t  buf[RXBUF_SIZE];   /* 存放收到的字节 */
    uint16_t len;               /* 当前存了多少字节 */
    uint16_t overflow;          /* 因缓冲区满而被丢弃的字节数（调试用） */
} RxBuf;

/* 清空缓冲区 */
void rxbuf_init(RxBuf *rb);

/* 喂入一个字节（在串口中断里调用）
 * 缓冲区满时丢弃该字节并累加 overflow，不越界写。 */
void rxbuf_feed(RxBuf *rb, uint8_t b);

/* 从缓冲区取出所有完整帧，并把已消费的字节移出缓冲区
 * 返回：取到的帧数 */
int rxbuf_take_frames(RxBuf *rb, Frame *out, int max);

#endif /* RINGBUF_H */
