#include "FPGA.hpp"
#include "delay.hpp"
#include "stm.hpp"
#include "main.h"

#define LOG_LEVEL	LOG_LEVEL_DEBUG
#define LOG_MODULE	"FPGA"
#include "Log.h"

extern SPI_HandleTypeDef hspi3;

static inline void Low(GPIO_TypeDef *gpio, uint16_t pin) {
	gpio->BSRR = pin << 16;
}
static inline void High(GPIO_TypeDef *gpio, uint16_t pin) {
	gpio->BSRR = pin;
}

bool FPGA::Init() {
	// Reset FPGA
	High(FPGA_RESET_GPIO_Port, FPGA_RESET_Pin);
	SetMode(Mode::FPGA);
	Delay::us(1);
	Low(FPGA_RESET_GPIO_Port, FPGA_RESET_Pin);
	Delay::ms(10);

//    /**SPI3 GPIO Configuration
//    PB3 (JTDO-TRACESWO)     ------> SPI3_SCK
//    PB4 (NJTRST)     ------> SPI3_MISO
//    PB5     ------> SPI3_MOSI
//    */
//	GPIO_InitTypeDef GPIO_InitStruct;
//    GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_5;
//    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_3|GPIO_PIN_5);
//    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//
////
////    for(uint8_t i=0;i<1;i++) {
////    	GPIOB->BSRR = GPIO_PIN_3;
////    	Delay::ms(1);
////    	GPIOB->BSRR = GPIO_PIN_3 << 16;
////    	Delay::ms(1);
////    }
//
//    GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
//    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_3|GPIO_PIN_5);
//    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//
//    uint16_t dummy = 0x0000;
////    HAL_SPI_Transmit(&hspi3, (uint8_t*) &dummy, 1, 100);
//
	// Check if FPGA response is as expected
	uint16_t cmd[2] = {0x4000, 0x0000};
	uint16_t recv[2];
//	HAL_SPI_TransmitReceive(&hspi3, (uint8_t*) cmd, (uint8_t*) recv, 2, 100);


	Low(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
	HAL_SPI_TransmitReceive(&hspi3, (uint8_t*) cmd, (uint8_t*) recv, 2, 100);
	High(FPGA_CS_GPIO_Port, FPGA_CS_Pin);

	if(recv[1] != 0xF0A5) {
		LOG_ERR("Initialization failed, got 0x%04x instead of 0xF0A5", recv[1]);
	}

	LOG_DEBUG("Initialized, status register: 0x%04x", recv[0]);
	return true;
}

void FPGA::WriteRegister(Reg reg, uint16_t value) {
	uint16_t cmd[2] = {(uint16_t) (0x8000 | (uint16_t) reg), value};
	Low(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
	HAL_SPI_Transmit(&hspi3, (uint8_t*) cmd, 2, 100);
	High(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
}

void FPGA::WriteMAX2871Default(uint32_t *DefaultRegs) {
	WriteRegister(Reg::MAX2871Def0LSB, DefaultRegs[0] & 0xFFFF);
	WriteRegister(Reg::MAX2871Def0MSB, DefaultRegs[0] >> 16);
	WriteRegister(Reg::MAX2871Def1LSB, DefaultRegs[1] & 0xFFFF);
	WriteRegister(Reg::MAX2871Def1MSB, DefaultRegs[1] >> 16);
	WriteRegister(Reg::MAX2871Def3LSB, DefaultRegs[3] & 0xFFFF);
	WriteRegister(Reg::MAX2871Def3MSB, DefaultRegs[3] >> 16);
	WriteRegister(Reg::MAX2871Def4LSB, DefaultRegs[4] & 0xFFFF);
	WriteRegister(Reg::MAX2871Def4MSB, DefaultRegs[4] >> 16);
}

void FPGA::WriteSweepConfig(uint16_t pointnum, uint32_t *SourceRegs, uint32_t *LORegs,
		uint8_t attenuation, uint64_t frequency) {
	uint16_t send[8];
	// select which point this sweep config is for
	send[0] = pointnum & 0x1FFF;
	// assemble sweep config from required fields of PLL registers
	send[1] = (LORegs[4] & 0x00700000) >> 14 | (LORegs[3] & 0xFC000000) >> 26;
	// Select source LP filter
	if(frequency >= 3500000000) {
		send[1] |= 0x0600;
	} else if(frequency >= 1800000000) {
		send[1] |= 0x0400;
	} else if(frequency >= 900000000) {
		send[1] |= 0x0200;
	}
	send[2] = (LORegs[1] & 0x00007FF8) << 1 | (LORegs[0] & 0x00007800) >> 11;
	send[3] = (LORegs[0] & 0x000007F8) << 5 | (LORegs[0] & 0x7F800000) >> 23;
	send[4] = (LORegs[0] & 0x007F8000) >> 7 | (attenuation & 0x7F) << 1 | (SourceRegs[4] & 0x00400000) >> 22;
	send[5] = (SourceRegs[4] & 0x00300000) >> 6 | (SourceRegs[3] & 0xFC000000) >> 18 | (SourceRegs[1] & 0x00007F80) >> 7;
	send[6] = (SourceRegs[1] & 0x00000078) << 9 | (SourceRegs[0] & 0x00007FF8) >> 3;
	send[7] = (SourceRegs[0] & 0x7FFF8000) >> 15;
	Low(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
	HAL_SPI_Transmit(&hspi3, (uint8_t*) send, 8, 100);
	High(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
}

static inline int64_t sign_extend_64(int64_t x, uint16_t bits) {
	int64_t m = 1ULL << (bits - 1);
	return (x ^ m) - m;
}

static FPGA::ReadCallback callback;
static uint16_t raw[18];

bool FPGA::InitiateSampleRead(ReadCallback cb) {
	callback = cb;
	uint16_t cmd = 0xC000;
	uint16_t status;

	// Bug in SPI slave implementation: Slave needs clk strobe to reload status register
	HAL_SPI_TransmitReceive(&hspi3, (uint8_t*) &cmd, (uint8_t*) &status, 1,
				100);

	Low(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
	HAL_SPI_TransmitReceive(&hspi3, (uint8_t*) &cmd, (uint8_t*) &status, 1,
			100);
	if (!(status & 0x0004)) {
		// no new data available yet
		High(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
		LOG_WARN("ISR without new data, status: 0x%04x", status);
//		return false;
	}
	// Start data read
	HAL_SPI_Receive_DMA(&hspi3, (uint8_t*) raw, 18);
	return true;
}

extern "C" {
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
	FPGA::SamplingResult result;
	High(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
	// Assemble data from words
	result.P1I = sign_extend_64(
			(uint64_t) raw[17] << 32 | (uint32_t) raw[16] << 16 | raw[15], 48);
	result.P1Q = sign_extend_64(
			(uint64_t) raw[14] << 32 | (uint32_t) raw[13] << 16 | raw[12], 48);
	result.P2I = sign_extend_64(
			(uint64_t) raw[11] << 32 | (uint32_t) raw[10] << 16 | raw[9], 48);
	result.P2Q = sign_extend_64(
			(uint64_t) raw[8] << 32 | (uint32_t) raw[7] << 16 | raw[6], 48);
	result.RefI = sign_extend_64(
			(uint64_t) raw[5] << 32 | (uint32_t) raw[4] << 16 | raw[3], 48);
	result.RefQ = sign_extend_64(
			(uint64_t) raw[2] << 32 | (uint32_t) raw[1] << 16 | raw[0], 48);
	if (callback) {
		callback(result);
	}
}
}

void FPGA::StartSweep() {
	Low(FPGA_AUX3_GPIO_Port, FPGA_AUX3_Pin);
	Delay::us(1);
	High(FPGA_AUX3_GPIO_Port, FPGA_AUX3_Pin);
}

void FPGA::AbortSweep() {
	Low(FPGA_AUX3_GPIO_Port, FPGA_AUX3_Pin);
}

void FPGA::SetMode(Mode mode) {
	switch(mode) {
	case Mode::FPGA:
		// Both AUX1/2 low
		Low(FPGA_AUX1_GPIO_Port, FPGA_AUX1_Pin);
		Low(FPGA_AUX2_GPIO_Port, FPGA_AUX2_Pin);
		Delay::us(1);
		High(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
		break;
	case Mode::SourcePLL:
		Low(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
		Low(FPGA_AUX2_GPIO_Port, FPGA_AUX2_Pin);
		Delay::us(1);
		High(FPGA_AUX1_GPIO_Port, FPGA_AUX1_Pin);
		break;
	case Mode::LOPLL:
		Low(FPGA_CS_GPIO_Port, FPGA_CS_Pin);
		Low(FPGA_AUX1_GPIO_Port, FPGA_AUX1_Pin);
		Delay::us(1);
		High(FPGA_AUX2_GPIO_Port, FPGA_AUX2_Pin);
		break;
	}
	Delay::us(1);
}
