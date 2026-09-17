#include "frame.h"
#include "crc16.h"

/* ------------------------------------------------------------
 * 内部辅助
 * ------------------------------------------------------------ */

/* 计算一帧「应该有」的 CRC 值
 * 范围：从 frame[2]（地址）开始，共 3 + N 个字节
 *       （地址 1 + 功能码 1 + 长度 1 + 数据 N）
 *       不含帧头，也不含 CRC 自己。
 */
static uint16_t frame_crc_calc(const uint8_t *frame)
{
    return crc16_modbus(frame + 2, (uint16_t)(3 + frame[4]));
}

/* 取出帧里「实际存的」CRC 值
 * 存放顺序是低字节在前，所以要写成 frame[p] | (frame[p+1] << 8)。
 * 写反成 (frame[p] << 8) | frame[p+1] 会全错。
 */
static uint16_t frame_crc_stored(const uint8_t *frame)
{
    uint16_t p = (uint16_t)(5 + frame[4]);
    return (uint16_t)(frame[p] | (frame[p + 1] << 8));
}


/* ------------------------------------------------------------
 * 单帧解析
 * ------------------------------------------------------------ */

int parse_frame(const uint8_t *frame, Frame *out)
{
    uint16_t i, got, stored;

    if (frame[0] != 0xAA || frame[1] != 0x55) {
        return 0;                       /* 帧头不对，不是一帧 */
    }

    out->addr = frame[2];
    out->func = frame[3];
    out->len  = frame[4];

    for (i = 0; i < out->len; i++) {
        out->data[i] = frame[5 + i];
    }

    got    = frame_crc_calc(frame);
    stored = frame_crc_stored(frame);

    out->crc_ok = (got == stored) ? 1 : 0;
    return 1;                           /* 结构完整，CRC 结果看 crc_ok */
}


/* ------------------------------------------------------------
 * 分帧
 * ------------------------------------------------------------ */

/* 从 stream[start] 开始找帧头 AA 55
 * 返回 AA 所在的下标；找到末尾都没找到则返回 len。
 *
 * 关键：不能"看到 AA 就返回"，必须确认下一位是 55。
 * 这样自然就处理了 AA AA 55 的情况：
 *   位置 0 的 AA，下一位是 AA（不是 55）→ 不匹配，继续
 *   位置 1 的 AA，下一位是 55            → 匹配，返回 1
 */
static uint16_t find_head(const uint8_t *stream, uint16_t len, uint16_t start)
{
    uint16_t i;

    for (i = start; i + 1 < len; i++) {
        if (stream[i] == 0xAA && stream[i + 1] == 0x55) {
            return i;
        }
    }
    return len;
}

int extract_frames(const uint8_t *stream, uint16_t len,
                   Frame *out, int max, uint16_t *consumed)
{
    uint16_t p = 0;
    int      count = 0;
    uint16_t pos, need;
    Frame    f;

    for (;;) {
        /* ① 找下一个候选帧头 */
        pos = find_head(stream, len, p);

        if (pos == len) {
            *consumed = len;    /* 全部扫描完毕，都可以丢弃 */
            return count;
        }

        /* ② 先确认长度字段读得到，再读它（否则越界读别人的内存） */
        if (pos + 5 > len) {
            *consumed = pos;
            return count;
        }

        need = (uint16_t)(7 + stream[pos + 4]);

        /* 帧体还没收全 → 断包，停在这里等 */
        if (pos + need > len) {
            *consumed = pos;
            return count;
        }

        /* 结果数组装不下了 */
        if (count >= max) {
            *consumed = pos;
            return count;
        }

        /* ③ 帧头只是候选，CRC 才是判决 */
        if (parse_frame(stream + pos, &f) && f.crc_ok) {
            out[count] = f;
            count++;
            p = (uint16_t)(pos + need);     /* 真帧：跳过整帧 */
        } else {
            p = (uint16_t)(pos + 1);        /* 假帧头：只前进 1 格重新找 */
            /* 注意：这里绝不能写成 pos + need ——
             * 假帧头后面的"长度字段"是垃圾值，按它跳会吞掉后面的真帧。 */
        }
    }
}
