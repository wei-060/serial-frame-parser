#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/* CRC16-Modbus 校验
 *
 * 参数：
 *   data : 待校验数据的首地址
 *   len  : 字节数
 * 返回：16 位 CRC 值
 *
 * 算法参数：多项式 0xA001（0x8005 的反转形式），初值 0xFFFF。
 * 说明：0xA001 是「右移版」算法专用的反转多项式形式；
 *       若写成 0x8005（未反转），结果会全错。
 */
uint16_t crc16_modbus(const uint8_t *data, uint16_t len);

#endif /* CRC16_H */
