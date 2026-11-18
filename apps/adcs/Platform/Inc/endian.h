/* libcsp expects BSD endian helpers; newlib for bare-metal ARM lacks them. */
#ifndef ADCS_ENDIAN_H
#define ADCS_ENDIAN_H
#include <stdint.h>
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error This adapter is for the little-endian STM32 target only
#endif
#define htobe16(x) __builtin_bswap16((uint16_t)(x))
#define htobe32(x) __builtin_bswap32((uint32_t)(x))
#define htobe64(x) __builtin_bswap64((uint64_t)(x))
#define be16toh(x) htobe16(x)
#define be32toh(x) htobe32(x)
#define be64toh(x) htobe64(x)
#define htole16(x) ((uint16_t)(x))
#define htole32(x) ((uint32_t)(x))
#define htole64(x) ((uint64_t)(x))
#define le16toh(x) htole16(x)
#define le32toh(x) htole32(x)
#define le64toh(x) htole64(x)
#endif
