#ifndef RELIB_SPI_FLASH_HEADER_H
#define RELIB_SPI_FLASH_HEADER_H

enum flash_size_map {
	FLASH_SIZE_4M_MAP_256_256 = 0,  /**<  Flash size : 4Mbits. Map : 256KBytes + 256KBytes */
	FLASH_SIZE_2M,                  /**<  Flash size : 2Mbits. Map : 256KBytes */
	FLASH_SIZE_8M_MAP_512_512,      /**<  Flash size : 8Mbits. Map : 512KBytes + 512KBytes */
	FLASH_SIZE_16M_MAP_512_512,     /**<  Flash size : 16Mbits. Map : 512KBytes + 512KBytes */
	FLASH_SIZE_32M_MAP_512_512,     /**<  Flash size : 32Mbits. Map : 512KBytes + 512KBytes */
	FLASH_SIZE_16M_MAP_1024_1024,   /**<  Flash size : 16Mbits. Map : 1024KBytes + 1024KBytes */
	FLASH_SIZE_32M_MAP_1024_1024,    /**<  Flash size : 32Mbits. Map : 1024KBytes + 1024KBytes */
	FLASH_SIZE_32M_MAP_2048_2048,    /**<  attention: don't support now ,just compatible for nodemcu;
	                                       Flash size : 32Mbits. Map : 2048KBytes + 2048KBytes */
	FLASH_SIZE_64M_MAP_1024_1024,     /**<  Flash size : 64Mbits. Map : 1024KBytes + 1024KBytes */
	FLASH_SIZE_128M_MAP_1024_1024     /**<  Flash size : 128Mbits. Map : 1024KBytes + 1024KBytes */
};
typedef enum flash_size_map flash_size_map_t;

typedef struct __attribute__((packed)) {
	uint8_t id;               // = 0xE9
	uint8_t number_segs;      // Number of segments
	uint8_t spi_interface;    // SPI Flash Interface (0 = QIO, 1 = QOUT, 2 = DIO, 0x3 = DOUT)
	uint8_t spi_freq: 4;      // Low four bits: 0 = 40MHz, 1= 26MHz, 2 = 20MHz, 0x3 = 80MHz
	flash_size_map_t flash_map: 4;     // High four bits: 0 = 512K, 1 = 256K, 2 = 1M, 3 = 2M, 4 = 4M, ...
} spi_flash_header_st;
static_assert(sizeof(spi_flash_header_st) == 0x4, "spi_flash_header_st size mismatch");

#endif /* RELIB_SPI_FLASH_HEADER_H */
