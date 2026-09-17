#include "crc16.h"

/* CRC 在数学上对应二进制多项式除法，该运算域内「异或」即为无进位减法。
 *
 * 逐字节的处理过程：
 *   1) 当前字节异或进余数低位，参与本轮除法
 *   2) 右移 8 次，每次看移出的最低位：
 *        为 1 → 余数不小于多项式，异或一次多项式
 *        为 0 → 仅移位
 *
 * 移位次数为 8，对应一个字节的 8 个 bit —— 长除法逐位进行，一字节做 8 轮。
 * 多项式取 0xA001 而非 0x8005：右移版算法使用反转形式的多项式，
 * 移位方向、多项式、初值三者需配套，换用 0x8005 则结果不符。
 */
uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i, j;

    for (i = 0; i < len; i++) {
        crc = crc ^ data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
