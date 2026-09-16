#include "crc16.h"

/* CRC 的本质是二进制多项式除法，而在这个域里「异或 = 无进位减法」。
 *
 * 逐字节计算：
 *   1) 把当前字节异或进余数低位（相当于把它"放进去"参与除法）
 *   2) 逐位右移 8 次：
 *        最低位为 1 → 说明"够减"，异或一次多项式
 *        最低位为 0 → 只移位
 *
 * 为什么是 8 次：一个字节 8 个 bit，长除法就是一位一位做的。
 * 为什么是 0xA001 而不是 0x8005：右移版算法使用的多项式是"反转形式"，
 *                                移位方向、多项式、初值三者必须配套。
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
