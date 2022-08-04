#include "stm32f10x.h"                  // Device header

//这个数组是存储在RAM运行内存中的，用于存储DMA从ADC-DR中搬运出来的ADC转换值
uint16_t ADC_Value[4];

void ADC_Init_t()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIOA_InitStructure;
	//配置IO口为模拟输入模式,该模式是ADC的专属模式
	//该模式下IO口是无效的，防止IO口的输入输出对模拟电压造成干扰
	GPIOA_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIOA_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
	GPIOA_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIOA_InitStructure);
	//以上代码配置完，IO口就被配置为模拟输入的引脚了
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);	
	//配置ADC时钟为RCC的6分频即12MHz
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);
	//选择规则组的输入通道
		//选择通道0，排序第1，采用时间为55.5个ADCCLK周期
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
		//若写入其他通道进序列
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_55Cycles5);	
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_55Cycles5);	
	ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 4, ADC_SampleTime_55Cycles5);	
	
	ADC_InitTypeDef ADC_InitStructure;
	//本次程序的代码用的是单次转换，扫描模式
	//连续转换模式配置为单次转换
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	//扫描模式配置
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	//通道数目配置，指定在【扫描模式】下总共需要扫描前多少个通道（非扫描模式下，写任何值都只有序列1有效而已）
	ADC_InitStructure.ADC_NbrOfChannel = 4;
	//数据位对齐，选择右对齐，即读取的12位数据在16位寄存器中占后12位,这种情况下转换出来的就是实际读取值
		//若选择左对齐，则相当于在右对齐的基础上左移4位，占寄存器的高12位
		//此时读取的数据是实际数值的2的4次方倍，即8倍
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	//外部触发源选择，即选择触发转换的源头，本程序我们用软件触发，不需外部触发，填ADC_ExternalTrigConv_None
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	//配置ADC为独立工作模式，即ADC1和ADC2各转换各自的，该参数的其他值则是双ADC模式配置相关的
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_Init(ADC1, &ADC_InitStructure);
	//至此ADC配置就完成了
	
	//若需中断和看门口则可以在下面继续配置
	
	/***************配置DMA*****************/
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	DMA_InitTypeDef DMA_InitStructure;
	//要运输的外设地址（注意，外设地址也可填寄存器地址，实际上无论是外设还是寄存器都是寄存器）
		//此处DR是ADC转换完成后存储规则组数据的地方
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(ADC1->DR));
	//外设向外传输数据的数据宽度，我们取DR寄存器的低16位数据，是半个字的大小
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	//外设数据搬运后是否执行地址自增，此处无需自增，始终在DR的这一个地方取数据即可
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	//要运输的寄存器地址（注意，寄存器地址也可填外设地址，实际上无论是外设还是寄存器都是寄存器）
	//DMA_PeripheralBaseAddr、DMA_InitStructure.DMA_MemoryBaseAddr
	//他们只是传输的站点名字而已
		//将ADC转换数据搬运到RAM运行内存的数组中进行存储
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ADC_Value;
	//寄存器的数据宽度，同DR寄存器
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	//寄存器数据搬运后是否执行地址自增，需要自增，每获得一个数据挪一个存储的位置
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	//数据传输方向，指定外设站点地址是源端还是目的地，本程序选择为源端，即数据从外设站点传输到寄存器站点
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	//数据传输计数器，其是16位，取值范围为0~65535，这里每次需要搬运4个数据
	DMA_InitStructure.DMA_BufferSize = 4;
	//选择是否为寄存器到寄存器的数据转运，即选择软件触发或硬件触发，寄存器到寄存器就是软件触发（软件触发条件下一直会有软件触发，无须用代码重复触发）
	//这里选择硬件触发
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	//打开ADC对DMA搬运申请通道，当ADC的一个通道转换完成后就会申请DMA搬运
	ADC_DMACmd(ADC1, ENABLE);
	//传输计数器是否自动重装，由于本程序是软件触发，不可选择这个重装循环模式，
	//选择正常模式，当计数器减为0时DMA不再搬运数据（尽管Cmd开关仍然ENABLE着），即该模式下只搬运一次数据
	//要进行第二次数据搬运，需给计数器重新赋值
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	//DMA请求优先级,因为DMA对外联系的总通道只有1个，所以当有多个DMA通道有转运需求时
	//需要有优先级来分配总通道的使用权限
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	//选择DMA1，通道1，由于本程序是软件触发，选择任何的通道都开源）
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);	
	//初始化函数调用后，DMA马上，但若没有搬运触发信号，也无法开始搬运，需等待一个搬运触发信号
	DMA_Cmd(DMA1_Channel1, ENABLE);
	
	//开启ADC电源
	ADC_Cmd(ADC1, ENABLE);
	
	//开启电源后，根据手册的建议，我们还需对ADC进行校准
	//复位校准函数，开始复位校准
	ADC_ResetCalibration(ADC1);
	//复位校准完成标志函数（完成校准返回0，未完成校准时为1）
		//如果没有校准完成则在循环中一直等待直到校准完成
	while(ADC_GetResetCalibrationStatus(ADC1));
	//开始校准函数，启动校准
	ADC_StartCalibration(ADC1);
	//等待校准完成函数
		//while循环原理同上个while
	while(ADC_GetCalibrationStatus(ADC1));
}

//此函数一经调用并执行完成，则ADC转换值则就会出现在ADC_Value[4]的数组里了
void ADC_GetValue()
{
	//给端菜的服务员DMA开后续端4次菜的工资
	//重新赋值（开工资）需要关闭DMA
	DMA_Cmd(DMA1_Channel1, DISABLE);
	//选择要重新写计数值的通道并写入其重新写入的值（端4次菜）
	DMA_SetCurrDataCounter(DMA1_Channel1, 4);
	//赋值完成后，打开DMA再次进行数据搬运工作
	DMA_Cmd(DMA1_Channel1, ENABLE);
	
	//软件触发转换器，由于ADC还是单次转换模式，故仍需要软件触发
	//（跟厨师ADC说，给DMA这个服务员开好工资了，他可以工资了，你可以开始做菜了。即触发一下厨师开始工作）
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	
	//此情形下无需再等待ADC转换完成，当DMA搬运完成时，已可证明ADC转换完成，详细解释见后文
//	//读取ADC规则/通道组转换完成标志位EOC，转换完成标志位置1，未完成置0
//	//未完成转换则在while循环中等待转换完成
//	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
	
	/* 后续程序执行流程
	 * 1.（因为ADC_DMACmd(ADC1, ENABLE)已经使能工作）所以ADC每个通道转换完成后都会申请DMA搬运ADC-DR中的数据
	 * 2. DMA对ADC-DR进行数据搬运
	 * 3. DMA搬运完成后会置Flag为SET
	 * 4. 以下程序会跳出循环
	 */
	 
	//等待搬运运完成
		//循环判断条件中的函数用来获取通道1的搬运完成标志位，搬运完成返回SET
		//程序能够跳出循环也说明DMA的4次搬运已完成（端菜工资用完），此时ADC的转换工作在此前也已经完成，故上面无需有while来等待ADC转换完成
	while(DMA_GetFlagStatus(DMA1_FLAG_TC1) != SET);
	//清楚标志位
	DMA_ClearFlag(DMA1_FLAG_TC1);
}
