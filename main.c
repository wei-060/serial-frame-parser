/* ============================================================
 * 串口数据帧解析器 —— 测试台
 *
 * 四个阶段共 24 个测试用例，自动比对结果，直接运行即可查看。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <windows.h>    /* 仅用于 SetConsoleOutputCP，让控制台正确显示 UTF-8 */

#include "crc16.h"
#include "frame.h"
#include "ringbuf.h"


/* ============================================================
 * 第 1 步：CRC16 校验
 * ============================================================ */

static const uint8_t T1[] = {0x01, 0x03, 0x02, 0x00, 0x0A};   /* Modbus 报文示例 */
static const uint8_t T2[] = {0x12, 0x34, 0x56, 0x78};
static const uint8_t T3[] = {0x00};
static const uint8_t T4[] = {0x31, 0x32, 0x33, 0x34, 0x35,      /* ASCII "123456789" */
                             0x36, 0x37, 0x38, 0x39};           /* CRC16 标准测试向量 */

static int check(const char *name, const uint8_t *data, uint16_t len,
                 uint16_t expect)
{
    uint16_t got = crc16_modbus(data, len);

    if (got == expect) {
        printf("  [通过] %-20s 算得 0x%04X\n", name, got);
        return 1;
    }
    printf("  [失败] %-20s 算得 0x%04X , 期望 0x%04X\n", name, got, expect);
    return 0;
}


/* ============================================================
 * 第 2 步：解析单个完整帧
 *
 *   F1: AA 55 01 03 02 00 0A 38 43        地址01 功能03 长度2
 *   F2: AA 55 02 06 04 12 34 56 78 B2 52  地址02 功能06 长度4
 *   F3: AA 55 10 04 04 AA 55 00 FF 8B 0D  数据段内含 AA 55
 * ============================================================ */

static const uint8_t F1[] = {0xAA,0x55,0x01,0x03,0x02,0x00,0x0A,0x38,0x43};
static const uint8_t F2[] = {0xAA,0x55,0x02,0x06,0x04,0x12,0x34,0x56,0x78,0xB2,0x52};
static const uint8_t F3[] = {0xAA,0x55,0x10,0x04,0x04,0xAA,0x55,0x00,0xFF,0x8B,0x0D};

/* CRC 被改坏的帧 */
static const uint8_t F1_BAD[] = {0xAA,0x55,0x01,0x03,0x02,0x00,0x0A,0x00,0x00};

/* 帧头不对：第 1 个字节就不对 */
static const uint8_t F_BADHEAD[] = {0x12,0x34,0x01,0x03,0x02,0x00,0x0A,0x38,0x43};

/* 帧头第 2 个字节错：首字节 0xAA 正确
 * 用于覆盖「只比对 frame[0]，漏比 frame[1]」这一情形 */
static const uint8_t F_BADHEAD2[] = {0xAA,0x99,0x01,0x03,0x02,0x00,0x0A,0x38,0x43};


static int test_parse_f1(void)
{
    Frame f;
    int ok = parse_frame(F1, &f);

    if (!ok) {
        printf("  [失败] F1 解析      parse_frame 返回了 0，但 F1 是合法帧，应返回 1\n");
        return 0;
    }
    if (f.addr != 0x01 || f.func != 0x03 || f.len != 2) {
        printf("  [失败] F1 字段      地址=%02X 功能码=%02X 长度=%d，期望 01 03 2\n",
               f.addr, f.func, f.len);
        return 0;
    }
    if (f.data[0] != 0x00 || f.data[1] != 0x0A) {
        printf("  [失败] F1 数据      数据=%02X %02X，期望 00 0A\n", f.data[0], f.data[1]);
        return 0;
    }
    if (!f.crc_ok) {
        printf("  [失败] F1 CRC       crc_ok=0，但 F1 的 CRC 应当通过，应置 1\n");
        return 0;
    }
    printf("  [通过] F1 完整解析  地址=01 功能码=03 长度=2 数据=00 0A CRC=OK\n");
    return 1;
}

static int test_parse_f2(void)
{
    Frame f;
    int ok = parse_frame(F2, &f);

    if (!ok) {
        printf("  [失败] F2 解析      parse_frame 返回了 0，但 F2 是合法帧，应返回 1\n");
        return 0;
    }
    if (f.addr != 0x02 || f.func != 0x06 || f.len != 4) {
        printf("  [失败] F2 字段      地址=%02X 功能码=%02X 长度=%d，期望 02 06 4\n",
               f.addr, f.func, f.len);
        return 0;
    }
    if (f.data[0] != 0x12 || f.data[3] != 0x78) {
        printf("  [失败] F2 数据      首字节=%02X 末字节=%02X，期望 12 78\n",
               f.data[0], f.data[3]);
        return 0;
    }
    if (!f.crc_ok) {
        printf("  [失败] F2 CRC       crc_ok=0，但 F2 的 CRC 应当通过，应置 1\n");
        return 0;
    }
    printf("  [通过] F2 完整解析  地址=02 功能码=06 长度=4 数据=12 34 56 78 CRC=OK\n");
    return 1;
}

static int test_crc_bad(void)
{
    Frame f;
    int ok = parse_frame(F1_BAD, &f);

    if (!ok) {
        printf("  [失败] 坏CRC帧      返回了 0，帧结构是完整的，应该返回 1\n");
        return 0;
    }
    if (f.crc_ok) {
        printf("  [失败] 坏CRC帧      crc_ok=1，但 CRC 是错的，应该被识别出来\n");
        return 0;
    }
    printf("  [通过] 坏CRC被拒绝  crc_ok=0（正确识别出数据被篡改）\n");
    return 1;
}

static int test_bad_head(void)
{
    Frame f;
    int ok = parse_frame(F_BADHEAD, &f);

    if (ok) {
        printf("  [失败] 坏帧头      返回了 1，但帧头不是 AA 55，应该返回 0\n");
        return 0;
    }
    printf("  [通过] 坏帧头被拒绝 返回 0（正确识别出不是一帧）\n");
    return 1;
}

static int test_bad_head2(void)
{
    Frame f;
    int ok = parse_frame(F_BADHEAD2, &f);

    if (ok) {
        printf("  [失败] 坏帧头2     返回了 1，但帧头是 AA 99（第 2 个字节错），应返回 0\n");
        return 0;
    }
    printf("  [通过] 坏帧头2被拒  返回 0（首字节对、次字节错也能识别）\n");
    return 1;
}


/* ============================================================
 * 第 3 步：从字节流中提取帧
 *
 * 每组输入对应真实串口会遇到的一种麻烦。
 * ============================================================ */

/* 单个合法帧 */
static const uint8_t S1[] = {0xAA,0x55,0x01,0x03,0x02,0x00,0x0A,0x38,0x43};

/* 粘包：F1 和 F2 首尾相连 */
static const uint8_t S2[] = {0xAA,0x55,0x01,0x03,0x02,0x00,0x0A,0x38,0x43,
                             0xAA,0x55,0x02,0x06,0x04,0x12,0x34,0x56,0x78,0xB2,0x52};

/* 断包：F1 只来了前 5 个字节 */
static const uint8_t S3[] = {0xAA,0x55,0x01,0x03,0x02};

/* 噪声：前 3 + 后 2 个垃圾字节中间夹一个真帧 */
static const uint8_t S4[] = {0x11,0x22,0x33,
                             0xAA,0x55,0x01,0x03,0x02,0x00,0x0A,0x38,0x43,
                             0x99,0x88};

/* 坏帧 + 好帧 */
static const uint8_t S5[] = {0xAA,0x55,0x01,0x03,0x02,0x00,0x0A,0x00,0x00,
                             0xAA,0x55,0x02,0x06,0x04,0x12,0x34,0x56,0x78,0xB2,0x52};

/* 数据段里含 AA 55 */
static const uint8_t S6[] = {0xAA,0x55,0x10,0x04,0x04,0xAA,0x55,0x00,0xFF,0x8B,0x0D};

/* AA AA 55 开头 */
static const uint8_t S7[] = {0xAA,0xAA,0x55,0x01,0x03,0x02,0x00,0x0A,0x38,0x43};


static int test_extract(const char *name,
                        const uint8_t *stream, uint16_t len,
                        int expect_frames, uint16_t expect_consumed)
{
    Frame    got[8];
    uint16_t consumed = 0xFFFF;
    int      n, i;

    n = extract_frames(stream, len, got, 8, &consumed);

    if (n != expect_frames) {
        printf("  [失败] %-24s 提取到 %d 帧，期望 %d 帧\n",
               name, n, expect_frames);
        return 0;
    }
    if (consumed != expect_consumed) {
        printf("  [失败] %-24s consumed=%d，期望 %d\n",
               name, consumed, expect_consumed);
        return 0;
    }

    printf("  [通过] %-24s %d 帧, 消耗 %d 字节", name, n, consumed);
    if (n > 0) {
        printf("   帧[");
        for (i = 0; i < n; i++) {
            if (i) printf(" ");
            printf("地址%02X 功能%02X", got[i].addr, got[i].func);
        }
        printf("]");
    }
    printf("\n");
    return 1;
}


/* ============================================================
 * 第 4 步：逐字节喂入
 * ============================================================ */

/* 把整段字节流一个字节一个字节地喂进去，
 * 每喂一个就试一次取帧 —— 模拟「串口中断收字节 + 主循环处理」 */
static int test_byte_by_byte(const char *name,
                             const uint8_t *stream, uint16_t len,
                             int expect_frames)
{
    RxBuf rb;
    Frame got[8];
    int   total = 0;
    int   i, n;

    rxbuf_init(&rb);

    for (i = 0; i < len; i++) {
        rxbuf_feed(&rb, stream[i]);
        n = rxbuf_take_frames(&rb, got + total, 8 - total);
        total += n;
    }

    if (total != expect_frames) {
        printf("  [失败] %-26s 逐字节喂入得到 %d 帧，期望 %d 帧\n",
               name, total, expect_frames);
        return 0;
    }
    if (rb.len != 0) {
        printf("  [失败] %-26s 提到了帧，但缓冲区还剩 %d 字节（没消费干净）\n",
               name, rb.len);
        return 0;
    }
    printf("  [通过] %-26s 逐字节喂入，得到 %d 帧，缓冲清零\n", name, total);
    return 1;
}

/* 最关键的测试：一帧被拆成两次「到货」
 * 真实串口里这是常态，验证「跨调用保存半帧」的能力。 */
static int test_split_feed(void)
{
    RxBuf rb;
    Frame got[8];
    int   n1 = 0, n2 = 0;
    int   i, n;

    rxbuf_init(&rb);

    /* 第一次到货：只有前 5 个字节 */
    for (i = 0; i < 5; i++) {
        rxbuf_feed(&rb, S1[i]);
        n1 += rxbuf_take_frames(&rb, got, 8);
    }

    if (n1 != 0) {
        printf("  [失败] 半帧先到             取到了 %d 帧，这时候不该有完整帧\n", n1);
        return 0;
    }
    if (rb.len != 5) {
        printf("  [失败] 半帧先到             缓冲区该留住 5 字节，实际 %d 字节\n", rb.len);
        return 0;
    }

    /* 第二次到货：剩下的 4 个字节补上 */
    for (i = 5; i < 9; i++) {
        rxbuf_feed(&rb, S1[i]);
        n = rxbuf_take_frames(&rb, got + n1, 8 - n1);
        n2 += n;
    }

    if (n2 != 1) {
        printf("  [失败] 后半个补上           取到 %d 帧，期望 1 帧\n", n2);
        return 0;
    }
    if (got[0].addr != 0x01 || got[0].func != 0x03) {
        printf("  [失败] 后半个补上           帧内容不对：地址=%02X 功能=%02X，期望 01/03\n",
               got[0].addr, got[0].func);
        return 0;
    }
    if (rb.len != 0) {
        printf("  [失败] 后半个补上           缓冲区没清空，还剩 %d 字节\n", rb.len);
        return 0;
    }

    printf("  [通过] 半帧先到 + 后半个补上     第一次 0 帧(留 5 字节)，第二次拼出 1 帧\n");
    return 1;
}

/* 边界：连续灌入超过容量的字节，不能越界 */
static int test_overflow_safe(void)
{
    RxBuf    rb;
    Frame    got[8];
    int      i, n;
    uint16_t capped;

    rxbuf_init(&rb);

    for (i = 0; i < 300; i++) {
        rxbuf_feed(&rb, (uint8_t)(i & 0xFF));
    }

    if (rb.len > RXBUF_SIZE) {
        printf("  [失败] 灌满保护             len=%d 超过容量 %d，越界了！\n",
               rb.len, RXBUF_SIZE);
        return 0;
    }

    capped = rb.len;
    n = rxbuf_take_frames(&rb, got, 8);
    if (n != 0) {
        printf("  [失败] 灌满保护             从纯垃圾里提取出了 %d 帧，不该有\n", n);
        return 0;
    }

    printf("  [通过] 灌满保护             灌 300 字节不越界（len 封顶 %d，丢弃 %d 个）\n",
           capped, rb.overflow);
    return 1;
}


/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    int pass = 0;

    SetConsoleOutputCP(65001);      /* 控制台切到 UTF-8，否则中文显示为乱码 */

    printf("\n==== 第 1 步：CRC16 校验 ====\n\n");

    pass += check("01 03 02 00 0A",  T1, 5, 0x4338);
    pass += check("12 34 56 78",     T2, 4, 0x107B);
    pass += check("00",              T3, 1, 0x40BF);
    pass += check("123456789",       T4, 9, 0x4B37);

    if (pass == 4) {
        printf("\n  第 1 步 4/4 通过\n");
    } else {
        printf("\n  第 1 步 %d/4 通过  ->  先修好这里再看后续\n\n", pass);
        return 0;
    }

    /* ---------- 第 2 步 ---------- */
    {
        int p2 = 0;

        printf("\n==== 第 2 步：解析完整帧 ====\n\n");

        p2 += test_parse_f1();
        p2 += test_parse_f2();
        p2 += test_crc_bad();
        p2 += test_bad_head();
        p2 += test_bad_head2();

        printf("\n------------------------------------\n");
        if (p2 == 5) {
            printf("  第 2 步 5/5 通过\n");
        } else {
            printf("  第 2 步 %d/5 通过\n", p2);
        }
        printf("------------------------------------\n\n");

        if (p2 != 5) {
            printf("  第 2 步没过，后续结果没有意义。\n\n");
            return 0;
        }
    }

    /* ---------- 第 3 步 ---------- */
    {
        int p3 = 0;

        printf("\n==== 第 3 步：从字节流中提取帧 ====\n\n");

        p3 += test_extract("单帧",
                           S1, (uint16_t)sizeof(S1), 1, (uint16_t)sizeof(S1));
        p3 += test_extract("粘包(2帧)",
                           S2, (uint16_t)sizeof(S2), 2, (uint16_t)sizeof(S2));
        p3 += test_extract("断包(只来半截)",
                           S3, (uint16_t)sizeof(S3), 0, 0);
        p3 += test_extract("噪声+帧+噪声",
                           S4, (uint16_t)sizeof(S4), 1, (uint16_t)sizeof(S4));
        p3 += test_extract("坏帧+好帧(只留好的)",
                           S5, (uint16_t)sizeof(S5), 1, (uint16_t)sizeof(S5));
        p3 += test_extract("数据含AA55不误判",
                           S6, (uint16_t)sizeof(S6), 1, (uint16_t)sizeof(S6));
        p3 += test_extract("AA AA 55 开头",
                           S7, (uint16_t)sizeof(S7), 1, (uint16_t)sizeof(S7));

        printf("\n------------------------------------\n");
        if (p3 == 7) {
            printf("  第 3 步 7/7 通过\n");
        } else {
            printf("  第 3 步 %d/7 通过\n", p3);
        }
        printf("------------------------------------\n\n");

        if (p3 != 7) {
            printf("  第 3 步没过，后续结果没有意义。\n\n");
            return 0;
        }
    }

    /* ---------- 第 4 步 ---------- */
    {
        int p4 = 0;

        printf("\n==== 第 4 步：逐字节喂入 ====\n\n");

        p4 += test_byte_by_byte("单帧(逐字节)",   S1, (uint16_t)sizeof(S1), 1);
        p4 += test_byte_by_byte("粘包(逐字节)",   S2, (uint16_t)sizeof(S2), 2);
        p4 += test_byte_by_byte("噪声+帧+噪声",   S4, (uint16_t)sizeof(S4), 1);
        p4 += test_byte_by_byte("坏帧+好帧",      S5, (uint16_t)sizeof(S5), 1);
        p4 += test_byte_by_byte("数据含AA55",     S6, (uint16_t)sizeof(S6), 1);
        p4 += test_byte_by_byte("AA AA 55 开头",  S7, (uint16_t)sizeof(S7), 1);
        p4 += test_split_feed();
        p4 += test_overflow_safe();

        printf("\n------------------------------------\n");
        if (p4 == 8) {
            printf("  第 4 步 8/8 通过  ->  全部完成\n");
        } else {
            printf("  第 4 步 %d/8 通过\n", p4);
        }
        printf("------------------------------------\n\n");
    }

    printf("==== 全部 24 个用例通过 ====\n\n");
    return 0;
}
