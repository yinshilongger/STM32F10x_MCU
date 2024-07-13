#include "stm32f10x.h"
#include "debug.h"

#ifdef DEBUG
#include "stdio.h"		//for printf

//���´���,֧��printf����,������Ҫѡ��use MicroLIB	  
#pragma import(__use_no_semihosting)             
//��׼����Ҫ��֧�ֺ���                 
struct __FILE 
{ 
	int handle; 
}; 

FILE __stdout;       
//����_sys_exit()�Ա���ʹ�ð�����ģʽ    
_sys_exit(int x) 
{ 
	x = x; 
} 
//�ض���fputc���� 
int fputc(int ch, FILE *f)
{ 	
	while((USART2->SR&0X40)==0);//ѭ������,ֱ���������   
	USART2->DR = (u8) ch;      
	return ch;
}
#else

//����������ӡ���ַ����飨\0��β�����˽ӿڻ�ռ�ýϳ�ʱ�䷢�����ݣ������ݲ���\0�������޷��˳�������ʹ�á�
static void debug(uint8_t *str)
{
	uint8_t i = 0;
	//while (str[i] != '\0' && USART_GetFlagStatus(USART2, USART_FLAG_TXE))		//��������д��һ��TXEû��׼���þ��˳�ѭ����ֹͣ����������
	while (str[i] != '\0')
	{
		while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET){}		//TXE��־ֻ�ܵ�����while��ѭ���������������ж������Ӱ��
		USART_SendData(USART2, str[i]);
		i++;
	}
	while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET){}
}

#endif

