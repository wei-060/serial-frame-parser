#include "frame.h"
#include "crc16.h"

/* ------------------------------------------------------------
 * 内部辅助
 * ------------------------------------------------------------ */

/* 帧内 CRC 字段的计算范围：frame[2] 起，共 3 + N 字节
 *   地址 1 + 功能码 1 + 长度 1 + 数据 N
 * 不含帧头，也不含 CRC 本身。
 */
static uint16_t frame_crc_calc(const uint8_t *frame)
{
    return crc16_modbus(frame + 2, (uint16_t)(3 + frame[4]));
}

/* 帧内 CRC 字段的存放位置：frame[5 + N] 起，2 字节，低字节在前
 * 所以拼装方式是 frame[p] | (frame[p+1] << 8)。
 * 若改成 (frame[p] << 8) | frame[p+1]，拼出来的字节序相反，无法与计算值相等。
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
        return 0;                       /* 帧头不符，不是一帧 */
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

/* 从 stream[start] 起查找帧头 AA 55
 * 返回 AA 所在下标；一直找到末尾仍未出现则返回 len。
 *
 * 判据是「连续两字节」，而非单个 AA。
 * 因此 AA AA 55 这种序列会跳过第一个 AA：
 *   下标 0 处：stream[0]==AA 但 stream[1]!=55 → 不成立
 *   下标 1 处：stream[1]==AA 且 stream[2]==55 → 返回 1
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
            /* 已无候选帧头，返回。consumed 的取值分两种：
             *   末尾恰为孤立 0xAA → 它可能是下一帧帧头的前半个字节，
             *                       配对的 0x55 尚未到达，故保留该字节
             *   其余情况         → 全部字节处理完毕
             * 这是流式处理与一次性处理整段字节流的区别所在。 */
            if (len > 0 && stream[len - 1] == 0xAA) {
                *consumed = (uint16_t)(len - 1);
            } else {
                *consumed = len;
            }
            return count;
        }

        /* ② 长度字段位于 stream[pos+4]，先确认可读（否则越界读到缓冲区之外） */
        if (pos + 5 > len) {
            *consumed = pos;
            return count;
        }

        need = (uint16_t)(7 + stream[pos + 4]);

        /* 帧体未收全，即断包：停在帧头处，等后续字节补齐 */
        if (pos + need > len) {
            *consumed = pos;
            return count;
        }

        /* 结果数组已满 */
        if (count >= max) {
            *consumed = pos;
            return count;
        }

        /* ③ 帧头是候选，CRC 结果才是判决依据 */
        if (parse_frame(stream + pos, &f) && f.crc_ok) {
            out[count] = f;
            count++;
            p = (uint16_t)(pos + need);     /* 真帧：整帧跳过 */
        } else {
            p = (uint16_t)(pos + 1);        /* 假帧头：前进 1 格重新查找 */
            /* 此处不能用 pos + need：假帧头后面的"长度字段"是数据段或噪声里的
             * 任意字节，按它跳会越过后面的真帧。 */
        }
    }
}
