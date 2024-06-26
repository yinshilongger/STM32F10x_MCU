/**
  ******************************************************************************
  * @file    stm32f10x_pal_i2c.h
  * @author  铁头山羊stm32工作组
  * @version V1.0.0
  * @date    2023年2月24日
  * @brief   i2c总线驱动程序
  ******************************************************************************
  * @attention
  * Copyright (c) 2022 - 2023 东莞市明玉科技有限公司. 保留所有权力.
  ******************************************************************************
*/

#include "stm32f10x_pal_i2c.h"
#include "stm32f10x_pal.h"

#define ErrorJump(error) if((error)!=SUCCESS) goto TAG_ERROR;

static ErrorStatus ResetWhenBusy(PalI2C_HandleTypeDef *Handle);
static ErrorStatus SendStart(PalI2C_HandleTypeDef *Handle);
static ErrorStatus SendSlaveAddress7(PalI2C_HandleTypeDef *Handle, uint8_t SlaveAddress);
static ErrorStatus SendSlaveAddress10(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddress);
static ErrorStatus SendBytes(PalI2C_HandleTypeDef *Handle, const uint8_t *pData, uint16_t Size);
static ErrorStatus MasterTransmit(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t Use10BitAddr, const uint8_t *pData, uint16_t Size);
static ErrorStatus MasterReceive(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t Use10BitAddr, uint8_t *pBufferOut, uint16_t Size);
static ErrorStatus MemWrite(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t Use10BitAddr, uint16_t MemAddr, uint16_t MemAddrSize, const uint8_t *pData, uint16_t Size);
static ErrorStatus MemRead(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t Use10BitAddr, uint16_t MemAddr, uint16_t MemAddrSize, uint8_t *pBufferOut, uint16_t Size);


//
// @简介：将句柄的初始化参数设置为默认值
//        I2C_ClockSpeed               = 400k， 默认400k波特率（快速模式）
//        I2C_DutyCycle                = 2:1，  默认使用2:1的占空比
// 
// @注意：仍需要手动填写的项目
//        I2Cx                         - 所使用的I2C接口的名称
//
void PAL_I2C_InitHandle(PalI2C_HandleTypeDef *Handle)
{
	Handle->Init.I2C_ClockSpeed = 400000;
	Handle->Init.I2C_DutyCycle = I2C_DutyCycle_2;
	Handle->Init.Advanced.Remap = 0;
}

//
// @简介：初始化I2C总线
// @参数：Handle - I2C的句柄
// @返回：SUCCESS - 成功，ERROR - 失败
//
ErrorStatus PAL_I2C_Init(PalI2C_HandleTypeDef *Handle)
{
	GPIO_InitTypeDef GPIOInitStruct;
	
	Handle->ByteInterval = 0;
	
	// 1. 配置IO引脚
	if(Handle->Init.I2Cx == I2C1) // I2C1
	{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
		if(Handle->Init.Advanced.Remap == 0)
		{
			GPIOInitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
		}
		else if(Handle->Init.Advanced.Remap == 1)
		{
			RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
			GPIO_PinRemapConfig(GPIO_Remap_I2C1, ENABLE);
			GPIOInitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
		}
		GPIOInitStruct.GPIO_Mode = GPIO_Mode_AF_OD;
		GPIOInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
		
		GPIO_Init(GPIOB, &GPIOInitStruct);
	}
	else // I2C2
	{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
		
		GPIOInitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
		GPIOInitStruct.GPIO_Mode = GPIO_Mode_AF_OD;
		GPIOInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
		
		GPIO_Init(GPIOB, &GPIOInitStruct);
	}
	
	// 2. 开启I2Cx的时钟
	if(Handle->Init.I2Cx == I2C1) // I2C1
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
		RCC_APB2PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);
		RCC_APB2PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE);
	}
	else // I2C2
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
		RCC_APB2PeriphResetCmd(RCC_APB1Periph_I2C2, ENABLE);
		RCC_APB2PeriphResetCmd(RCC_APB1Periph_I2C2, DISABLE);
	}
	
	// 3. 初始化I2C
	I2C_InitTypeDef I2CInitStruct;
	I2CInitStruct.I2C_Mode = I2C_Mode_I2C;
	I2CInitStruct.I2C_ClockSpeed = Handle->Init.I2C_ClockSpeed;
	I2CInitStruct.I2C_DutyCycle = Handle->Init.I2C_DutyCycle;
	I2CInitStruct.I2C_Ack = I2C_Ack_Disable;
	I2C_Init(Handle->Init.I2Cx, &I2CInitStruct);
	
	I2C_Cmd(Handle->Init.I2Cx, ENABLE);
	
	return SUCCESS;
}

//
// @简介：发送数据给从机（7位从机地址）
// @参数：Handle - I2C句柄
// @参数：SlaveAddr - 从机地址
// @参数：pData - 要发送的数据（指针）
// @参数：Size - 数据长度
// @返回：SUCCESS - 发送成功，ERROR - 发送失败
//
ErrorStatus PAL_I2C_MasterTransmit(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, const uint8_t *pData, uint16_t Size)
{
	return MasterTransmit(Handle, SlaveAddr, 0, pData, Size);
}

//
// @简介：发送数据给从机（10位从机地址）
// @参数：Handle - I2C句柄
// @参数：SlaveAddr - 从机地址
// @参数：pData - 要发送的数据（指针）
// @参数：Size - 数据长度
// @返回：SUCCESS - 发送成功，ERROR - 发送失败
//
ErrorStatus PAL_I2C_MasterTransmit10(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, const uint8_t *pData, uint16_t Size)
{
	return MasterTransmit(Handle, SlaveAddr, 1, pData, Size);
}

//
// @简介：从从机读取数据（7位从机地址）
// @参数：Handle - I2C句柄
// @参数：SlaveAddr - 从机地址
// @参数：pBufferOut - 输出参数，用来接收数据
// @参数：Size - 需要接收的数据量
// @返回：SUCCESS - 接收成功 ERROR - 接收失败
//
ErrorStatus PAL_I2C_MasterReceive(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t *pBufferOut, uint16_t Size)
{
	return MasterReceive(Handle, SlaveAddr, 0, pBufferOut, Size);
}

//
// @简介：从从机读取数据（10位从机地址）
// @参数：Handle - I2C句柄
// @参数：SlaveAddr - 从机地址
// @参数：pBufferOut - 输出参数，用来接收数据
// @参数：Size - 需要接收的数据量
// @返回：SUCCESS - 接收成功 ERROR - 接收失败
//
ErrorStatus PAL_I2C_MasterReceive10(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t *pBufferOut, uint16_t Size)
{
	return MasterReceive(Handle, SlaveAddr, 1, pBufferOut, Size);
}

//
// @简介：设置字节的读写间隔
//        读写过程中每个字节之间的停顿时间（单位us）一些I2C从机设计存在缺陷，
//	      它们既不支持时钟拉伸（Clock Stretch）也不能承受高速的数据读写（连续
//	      写入时NAK），此时在连续读写时应当在每个字节之间增加延迟。该参数用于
//	      设置该延迟时间。
// @参数：Handle - I2C的句柄
// @参数：Interval - 字节间的收发间隔，单位us
//
void PAL_I2C_SetByteInterval(PalI2C_HandleTypeDef *Handle, uint32_t Interval)
{
	Handle->ByteInterval = Interval;
}

//
// @简介：写寄存器（7位从机地址）
// @参数：Handle - I2C句柄
// @参数：SlaveAddr - 从机地址
// @参数：MemAddr - 寄存器地址（支持8位地址和16位地址）
// @参数：MemAddrSize - 寄存器地址类型 I2C_MEMSIZE_8BIT - 8位 I2C_MEMSIZE_16BIT - 16位
// @参数：pData - 要发送的数据（指针）
// @参数：Size - 数据长度
// @返回：SUCCESS - 发送成功，ERROR - 发送失败
//
ErrorStatus PAL_I2C_MemWrite(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint16_t MemAddr, uint16_t MemAddrSize, const uint8_t *pData, uint16_t Size)
{
	return MemWrite(Handle, SlaveAddr, 0, MemAddr, MemAddrSize, pData, Size);
}

//
// @简介：写寄存器（10位从机地址）
// @参数：Handle - I2C句柄
// @参数：SlaveAddr - 从机地址
// @参数：MemAddr - 寄存器地址（支持8位地址和16位地址）
// @参数：MemAddrSize - 寄存器地址类型 I2C_MEMSIZE_8BIT - 8位 I2C_MEMSIZE_16BIT - 16位
// @参数：pData - 要发送的数据（指针）
// @参数：Size - 数据长度
// @返回：SUCCESS - 发送成功，ERROR - 发送失败
//
ErrorStatus PAL_I2C_MemWrite10(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint16_t MemAddr, uint16_t MemAddrSize, const uint8_t *pData, uint16_t Size)
{
	return MemWrite(Handle, SlaveAddr, 1, MemAddr, MemAddrSize, pData, Size);
}

//
// @简介：从从机的寄存器读取数据（7位从机地址）
// @参数：Handle - I2C句柄
// @参数：MemAddr - 寄存器地址（支持8位地址和16位地址）
// @参数：MemAddrSize - 寄存器地址类型 I2C_MEMSIZE_8BIT - 8位 I2C_MEMSIZE_16BIT - 16位
// @参数：pBufferOut - 输出参数，用来接收数据
// @参数：Size - 需要接收的数据量
// @返回：SUCCESS - 接收成功 ERROR - 接收失败
//
ErrorStatus PAL_I2C_MemRead(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint16_t MemAddr, uint16_t MemAddrSize, uint8_t *pBufferOut, uint16_t Size)
{
	return MemRead(Handle, SlaveAddr, 0, MemAddr, MemAddrSize, pBufferOut, Size);
}

//
// @简介：从从机的寄存器读取数据（10位从机地址）
// @参数：Handle - I2C句柄
// @参数：MemAddr - 寄存器地址（支持8位地址和16位地址）
// @参数：MemAddrSize - 寄存器地址类型 I2C_MEMSIZE_8BIT - 8位 I2C_MEMSIZE_16BIT - 16位
// @参数：pBufferOut - 输出参数，用来接收数据
// @参数：Size - 需要接收的数据量
// @返回：SUCCESS - 接收成功 ERROR - 接收失败
//
ErrorStatus PAL_I2C_MemRead10(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint16_t MemAddr, uint16_t MemAddrSize, uint8_t *pBufferOut, uint16_t Size)
{
	return MemRead(Handle, SlaveAddr, 1, MemAddr, MemAddrSize, pBufferOut, Size);
}

//
// @简介：开始数据发送（发送起始位和从机地址）
// @参数：Handle - I2C的句柄
// @参数：SlaveAddr - 从机地址
// @返回：SUCCESS - 成功，ERROR - 失败
//
ErrorStatus PAL_I2C_StartTx(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr)
{
	// #1.等待总线空闲
	if(ResetWhenBusy(Handle)!=SUCCESS)
	{
		return ERROR;
	}
	// #2.发送起始位
	SendStart(Handle);
	
	// #3.发送地址
	if(SendSlaveAddress7(Handle, SlaveAddr) != SUCCESS)
	{
		return ERROR;
	}
	
	// #4.清除ADDR
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
	
	return SUCCESS;
}

//
// @简介：开始数据发送（发送起始位和从机地址）（10位地址）
// @参数：Handle - I2C的句柄
// @参数：SlaveAddr - 从机地址
// @返回：SUCCESS - 成功，ERROR - 失败
//
ErrorStatus PAL_I2C_StartTx10(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr)
{
	// #1.等待总线空闲
	if(ResetWhenBusy(Handle)!=SUCCESS)
	{
		return ERROR;
	}
	// #2.发送起始位
	SendStart(Handle);
	
	// #3.发送地址
	if(SendSlaveAddress10(Handle, SlaveAddr) != SUCCESS)
	{
		return ERROR;
	}
	
	// #4.清除ADDR
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
	
	return SUCCESS;
}

//
// @简介：发送一个字节
// @参数：Handle - I2C的句柄
// @参数：Byte - 要发送的字节
// @返回：SUCCESS - 成功，ERROR - 失败
//
ErrorStatus PAL_I2C_SendByte(PalI2C_HandleTypeDef *Handle, uint8_t Byte)
{
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_TXE)==RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF)==SET)
		{
			return ERROR;
		}
	}
	
	I2C_SendData(Handle->Init.I2Cx, Byte);
	
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BTF)==RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF)==SET)
		{
			return ERROR;
		}
	}
	
	return SUCCESS;
}

//
// @简介：发送多个字节
// @参数：Handle - I2C的句柄
// @参数：pData - 要发送的字节
// @参数：Size - 要发送的字节的个数
// @返回：SUCCESS - 成功，ERROR - 失败
//
ErrorStatus PAL_I2C_SendBytes(PalI2C_HandleTypeDef *Handle, const uint8_t *pData, uint16_t Size)
{
	uint32_t i;
	
	for(i=0;i<Size;i++)
	{
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_TXE)==RESET)
		{
			if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF)==SET)
			{
				return ERROR;
			}
		}
		
		I2C_SendData(Handle->Init.I2Cx, pData[i]);
	}
	
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BTF)==RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF)==SET)
		{
			return ERROR;
		}
	}
	
	return SUCCESS;
}

//
// @简介：结束数据发送（并等待至总线空闲）
// @参数：Handle - I2C的句柄
//
void PAL_I2C_StopTx(PalI2C_HandleTypeDef *Handle)
{
	I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
	
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY) == SET);
}

//
// @简介：等待总线空闲
// @备注：等待超时后该方法会主动重置I2C总线
// 
static ErrorStatus ResetWhenBusy(PalI2C_HandleTypeDef *Handle)
{
	// 如果总线空闲直接返回
	if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY) == RESET)
	{
		return SUCCESS;
	}
	
	// 复位I2C
	if(Handle->Init.I2Cx == I2C1)
	{
		RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);
		RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE);
	}
	else
	{
		RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C2, ENABLE);
		RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C2, DISABLE);
	}
	
	// 重新初始化
	I2C_InitTypeDef I2C_InitStruct;
	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStruct.I2C_ClockSpeed = Handle->Init.I2C_ClockSpeed;
	I2C_InitStruct.I2C_DutyCycle = Handle->Init.I2C_DutyCycle;
	I2C_InitStruct.I2C_Ack = I2C_Ack_Disable;
	I2C_Init(Handle->Init.I2Cx, &I2C_InitStruct);
	
	// 复位之后总线仍忙，总线故障
	if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY) == SET)
	{
		return ERROR;
	}
	
	return SUCCESS;
}

//
// @简介：发送起始位
// @参数：Handle - I2C句柄
// @返回值：SUCCESS - 成功，ERROR - 失败（总线忙）
// 
static ErrorStatus SendStart(PalI2C_HandleTypeDef *Handle)
{
	// 发送Start
	I2C_GenerateSTART(Handle->Init.I2Cx, ENABLE);
	// 等待SB置位
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_SB) != SET);
	
	return SUCCESS;
}

//
// @简介：发送7位从机地址
// @参数：Handle - I2C句柄
// @参数：SlaveAddress - 7位地址，最低位为R#/W位
// @返回：SUCCESS - 成功 ERROR - 失败（没有从机响应）
// @注意：该方法并不主动清除AF和ADDR标志位
//
static ErrorStatus SendSlaveAddress7(PalI2C_HandleTypeDef *Handle, uint8_t SlaveAddress)
{
	I2C_ClearFlag(Handle->Init.I2Cx, I2C_FLAG_AF); // 清除AF
	
	// 向DR写入slave地址
	I2C_SendData(Handle->Init.I2Cx, SlaveAddress);
	
	// 等待地址发送完成
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_ADDR)==RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF)==SET)
		{
			return ERROR;
		}
	}
	
	return SUCCESS;
}

//
// @简介：发送10位从机地址
// @参数：Handle - I2C句柄
// @参数：SlaveAddress - 10位从机地址，最低位为R#/W位
// @返回：SUCCESS - 成功 ERROR - 失败（无从机响应）
// @注意：该方法并不主动清除AF和ADDR标志位
//
static ErrorStatus SendSlaveAddress10(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddress)
{
	I2C_ClearFlag(Handle->Init.I2Cx, I2C_FLAG_AF); // 清除AF
	
	// 发送header
	// 11110xx0，其中xx代表地址的高2位
	I2C_SendData(Handle->Init.I2Cx, (uint8_t)(((SlaveAddress & 0x0300) >> 7) | 0xf0));
	
	// 等待地址发送完成
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_ADD10)==RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF)==SET)
		{
			return ERROR;
		}
	}
	
	// 发送地址的低位
	I2C_SendData(Handle->Init.I2Cx, (uint8_t)(SlaveAddress & 0xff));
	
	// 等待地址发送完成
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_ADDR)==RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF)==SET)
		{
			return ERROR;
		}
	}
	
	return SUCCESS;
}

//
// @简介：发送数据
// @参数：Handle - I2C句柄
// @参数：pData - 要发送的数据
// @参数：Size  - 数据长度
// @返回：SUCCESS - 发送成功，ERROR - 发送失败（NAK）
// @注意：该方法不会主动清除AF标志位
//
static ErrorStatus SendBytes(PalI2C_HandleTypeDef *Handle, const uint8_t *pData, uint16_t Size)
{
	uint32_t i;
	
	// 发送数据
	for(i=0;i<Size;i++)
	{
		// 等待发送数据寄存器空
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_TXE)==RESET)
		{
			if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF)==SET)
			{
				return ERROR;
			}
		}
		// 发送数据
		I2C_SendData(Handle->Init.I2Cx, pData[i]);
		
		if((i!=Size-1) && Handle->ByteInterval != 0)
		{
			PAL_DelayUs(Handle->ByteInterval);
		}
	}
	
	return SUCCESS;
}

static ErrorStatus MasterTransmit(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t Use10BitAddr, const uint8_t *pData, uint16_t Size)
{
	// 等待总线空闲
	if(ResetWhenBusy(Handle)!=SUCCESS)
	{
		return ERROR;
	}
	
	ErrorJump(SendStart(Handle)) // 发送起始位
	
	// 发送地址
	// 7位地址
	if(!Use10BitAddr)
	{
		ErrorJump(SendSlaveAddress7(Handle, SlaveAddr&0xfe))
	}
	// 10位地址
	else
	{
		ErrorJump(SendSlaveAddress10(Handle, SlaveAddr&0xfffe))
	}
	
	// 清除ADDR标志位
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
	
	// 发送数据
	ErrorJump(SendBytes(Handle, pData, Size))
	
	// 等待BTF
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BTF) == RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF) == SET)
		{
			goto TAG_ERROR;
		}
	}
	
	// 发送停止条件
	I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
	
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY)==SET);
	
	return SUCCESS;
	
TAG_ERROR:
	
	// 发送停止条件
	I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
	
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY)==SET);
	
	return ERROR;
}

static ErrorStatus MasterReceive(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t Use10BitAddr, uint8_t *pBufferOut, uint16_t Size)
{
	// #1.等待总线空闲
	if(ResetWhenBusy(Handle)!=SUCCESS)
	{
		return ERROR;
	}
	
	ErrorJump(SendStart(Handle)) // 发送起始位
	
	// #2.发送地址
	if(!Use10BitAddr)// 7位地址
	{
		ErrorJump(SendSlaveAddress7(Handle, SlaveAddr|0x01))
	}
	else // 10位地址
	{
		ErrorJump(SendSlaveAddress10(Handle, SlaveAddr|0x0001))
	}
	
	// 开始接收数据
	I2C_AcknowledgeConfig(Handle->Init.I2Cx, ENABLE);
	I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Current);
	
	if(Size == 0)
	{
		__disable_irq();
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, DISABLE);
		I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Current);
		I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
		__enable_irq();
	}
	else if(Size == 1)
	{
		__disable_irq();
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, DISABLE);
		I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Current);
		I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
		__enable_irq();
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_RXNE) != SET);
		*pBufferOut = I2C_ReceiveData(Handle->Init.I2Cx);
	}
	else if(Size == 2)
	{
		__disable_irq();
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, DISABLE);
		I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Next);
		__enable_irq();
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BTF) == RESET);
		I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
	}
	else // N > 2
	{
		uint32_t i;
		
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, ENABLE);
		I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Current);
		
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
		
		for(i=0;i<Size-3;i++)
		{
			while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_RXNE) == RESET);
			*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
			
			if(Handle->ByteInterval != 0)
			{
				PAL_DelayUs(Handle->ByteInterval);
			}
		}
		
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BTF) == RESET);
		
		__disable_irq();
		
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, DISABLE);
		
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
		
		I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
		__enable_irq();
		
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
		
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_RXNE) == RESET);
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
	}
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY)==SET);
	return SUCCESS;
	
TAG_ERROR:
	
	I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY)==SET);
	return ERROR;
}

static ErrorStatus MemWrite(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t Use10BitAddr, uint16_t MemAddr, uint16_t MemAddrSize, const uint8_t *pData, uint16_t Size)
{
	// #1.等待总线空闲
	if(ResetWhenBusy(Handle)!=SUCCESS)
	{
		return ERROR;
	}
	
	ErrorJump(SendStart(Handle)) // 发送起始位
		
	// 发送地址
	// 7位地址
	if(!Use10BitAddr)
	{
		ErrorJump(SendSlaveAddress7(Handle, SlaveAddr&0xfe))
	}
	// 10位地址
	else
	{
		ErrorJump(SendSlaveAddress10(Handle, SlaveAddr&0xfffe))
	}
	
	// 清除ADDR标志位
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
	
	// 发送MemAddr
	if(MemAddrSize == I2C_MEMSIZE_8BIT) // 8位
	{
		ErrorJump(SendBytes(Handle, (uint8_t *)&MemAddr, 1))
	}
	else // 16位
	{
		uint16_t tmp;
		
		tmp = ((uint16_t)(MemAddr >> 8)) | ((uint16_t)(MemAddr << 8)); // 先发高字节
		
		ErrorJump(SendBytes(Handle, (uint8_t *)&tmp, 2))
	}
	
	// 发送数据
	ErrorJump(SendBytes(Handle, pData, Size))
	
	// 等待BTF
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BTF) == RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF) == SET)
		{
			goto TAG_ERROR;
		}
	}
	
	// 发送停止条件
	I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
	
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY)==SET);
	
	return SUCCESS;
	
TAG_ERROR:
	
	// 发送停止条件
	I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
	
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY)==SET);
	
	return ERROR;	
}

static ErrorStatus MemRead(PalI2C_HandleTypeDef *Handle, uint16_t SlaveAddr, uint8_t Use10BitAddr, uint16_t MemAddr, uint16_t MemAddrSize, uint8_t *pBufferOut, uint16_t Size)
{
	// #1.等待总线空闲
	if(ResetWhenBusy(Handle)!=SUCCESS)
	{
		return ERROR;
	}
	
	// #2.发送起始位
	ErrorJump(SendStart(Handle)) 
	
	// #3.发送地址
	if(!Use10BitAddr) // 7位地址
	{
		ErrorJump(SendSlaveAddress7(Handle, SlaveAddr&0xfe))
	}
	else // 10位地址
	{
		ErrorJump(SendSlaveAddress10(Handle, SlaveAddr&0xfffe))
	}
	
	// #4.清除ADDR
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
	I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
	
	// #5.发送MemAddr(内存地址)
	if(MemAddrSize == I2C_MEMSIZE_8BIT) // 8位地址
	{
		ErrorJump(SendBytes(Handle, (uint8_t *)&MemAddr,1))
	}
	else // 16位地址
	{
		uint16_t tmp;
		
		tmp = ((uint16_t)(MemAddr >> 8)) | ((uint16_t)(MemAddr << 8)); // 先发高字节
		
		ErrorJump(SendBytes(Handle, (uint8_t *)&tmp, 2))
	}
	
	// 等待TXE
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_TXE) == RESET)
	{
		if(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_AF) == SET)
		{
			goto TAG_ERROR;
		}
	}
	
	// 发送 Restart
	ErrorJump(SendStart(Handle))
	
	// 发送地址
	// 7位地址
	if(!Use10BitAddr)
	{
		ErrorJump(SendSlaveAddress7(Handle, SlaveAddr|0x01))
	}
	// 10位地址
	else
	{
		ErrorJump(SendSlaveAddress10(Handle, SlaveAddr|0x0001))
	}
	
	// 开始接收数据
	I2C_AcknowledgeConfig(Handle->Init.I2Cx, ENABLE);
	I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Current);
	
	if(Size == 0)
	{
		__disable_irq();
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, DISABLE);
		I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Current);
		I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
		__enable_irq();
	}
	else if(Size == 1)
	{
		__disable_irq();
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, DISABLE);
		I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Current);
		I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
		__enable_irq();
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_RXNE) != SET);
		*pBufferOut = I2C_ReceiveData(Handle->Init.I2Cx);
	}
	else if(Size == 2)
	{
		__disable_irq();
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, DISABLE);
		I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Next);
		__enable_irq();
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BTF) == RESET);
		I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
	}
	else // N > 2
	{
		uint32_t i;
		
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, ENABLE);
		I2C_NACKPositionConfig(Handle->Init.I2Cx, I2C_NACKPosition_Current);
		
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR1);
		I2C_ReadRegister(Handle->Init.I2Cx, I2C_Register_SR2);
		
		for(i=0;i<Size-3;i++)
		{
			while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_RXNE) == RESET);
			*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
			if(Handle->ByteInterval != 0)
			{
				PAL_DelayUs(Handle->ByteInterval);
			}
		}
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BTF) == RESET);
		
		I2C_AcknowledgeConfig(Handle->Init.I2Cx, DISABLE);
		
		__disable_irq();
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
		
		I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
		__enable_irq();
		
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
		
		while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_RXNE) == RESET);
		*pBufferOut++ = I2C_ReceiveData(Handle->Init.I2Cx);
	}
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY)==SET);
	return SUCCESS;
	
TAG_ERROR:
	
	I2C_GenerateSTOP(Handle->Init.I2Cx, ENABLE);
	while(I2C_GetFlagStatus(Handle->Init.I2Cx, I2C_FLAG_BUSY)==SET);
	return ERROR;
}
