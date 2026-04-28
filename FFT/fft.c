#include "fft.h"
#include "HMI.h"

extern uint16_t ADC_Buffer[1024];


/* 变量 */
#define FFT_LEN 1024
#define ADC_LEN 1024

uint8_t ifftFlag = 0; 
int BaseIdx = 0; // 基波下标
int wave_type;//波形类别 1是正弦 2是三角 3是方波
float fs=20000.f;//采样率
float FFT_Freq=0;  //FFT计算得到频率
float FFT_Ampl=0;  //FFT计算得到的幅值 
float VPP,Ud=0;//峰峰值，直流偏置
float FFT_mag_max={0};  //幅度谱最大值
uint32_t FFT_mag_max_index=0;

/* 输入和输出缓冲 */

float FFT_Output[FFT_LEN]; 
float FFT_Input[FFT_LEN*2]; 
float FFT_mag[FFT_LEN];//幅度谱
float IFFT_Output[FFT_LEN];


uint8_t EnableWindow=1; // 是否加窗
float Window_OutputBuffer[ADC_LEN]; // 窗函数输出缓冲


void showdata(float*buffer,uint16_t n)
{
	for(int i=0;i<n;i++)
    {
		printf("%d:\t%.3f\n",i,buffer[i]);
	}
}

void FFT_Process(void)
{
	/*
	arm_rfft_fast_instance_f32 S;
	arm_rfft_fast_init_f32(&S, FFT_LEN);
	
	ifftFlag = 0; 
	for(int i=0; i<FFT_LEN; i++)
	{
		FFT_Input[i] = ADC_Buffer[i]*Window_OutputBuffer[i];
	}
	arm_rfft_fast_f32(&S, FFT_Input, FFT_Output, ifftFlag);
	*/
	
	//清零缓冲区
	memset (FFT_Input,0,sizeof(FFT_Input));
	memset (FFT_mag,0,sizeof(FFT_mag));
	memset (FFT_Output,0,sizeof(FFT_Output));
	
	for(int i=0;i<1024;i++)
    {
      FFT_Input[i * 2] = ADC_Buffer[i] * Window_OutputBuffer[i];
      FFT_Input[i * 2 + 1] = 0;                    
    }
		
  arm_cfft_f32(&arm_cfft_sR_f32_len1024, FFT_Input, 0, 1);
		
	//showdata(FFT_Input,FFT_LEN);
		
	//计算幅度谱
	arm_cmplx_mag_f32(FFT_Input,FFT_mag,FFT_LEN);
	
	// Hanning窗功率补偿+归一化
	float window_power_correction =1.5f;
	for (uint16_t i=0;i<FFT_LEN;i++){
			 if(i==0){
				  FFT_mag[i]=FFT_mag[i]/FFT_LEN * window_power_correction;				 
				}else{
					FFT_mag[i]=FFT_mag[i]*2.0f/FFT_LEN * window_power_correction;
				}
	}
	
  for	(uint16_t i=0;i<FFT_LEN;i++){
       FFT_Output[i]=sqrt(FFT_mag[i]);
	}		
	
  //改
	Process_FFT_mag(FFT_mag,FFT_mag_max,FFT_mag_max_index);

	//ADC_FFT_Get_Wave_Mes(FFT_mag_max_index,fs,&VPP,&FFT_Freq,2);
   
}

/*fft caculate */
//从频谱中提取信号，找到主频，计算信号频率和幅度。
void Process_FFT_mag(float *FFT_mag,float FFT_mag_max,uint32_t FFT_mag_max_index)
{

	//找幅度谱前一半数据，找到最大值和索引
	arm_max_f32(FFT_mag,FFT_LEN/2,&FFT_mag_max,&FFT_mag_max_index);
	
	//求频率：最大值结果*采样率/FFT长度
	FFT_Freq=(float)FFT_mag_max_index*fs/(float)FFT_LEN;
	
	//求幅值：最大值结果索引*2/FFT长度
	FFT_Ampl=FFT_mag_max*2.0f/(float)FFT_LEN;//幅度
	
	//求直流偏置
	Ud=FFT_mag[0]/(float)FFT_LEN;

}

void IFFT_Process(void)
{
	/*
	arm_rfft_fast_instance_f32 S;
	arm_rfft_fast_init_f32(&S, FFT_LEN);

    ifftFlag = 1;
	arm_rfft_fast_f32(&S, FFT_Output, IFFT_Output, ifftFlag);
	*/
    arm_cfft_f32(&arm_cfft_sR_f32_len1024, FFT_Input, 1, 1);

    // 提取实部作为 IFFT 输出
    for (int i = 0; i < FFT_LEN; i++) {
        IFFT_Output[i] = FFT_Input[2*i];  // 取实部
    }
}


/*Hanning窗*/
void window(void)
{
    for (int i = 0; i < ADC_LEN; i++)
    {
        if (EnableWindow)
        {
            float tempCos = cosf(2.0f * PI * i / (ADC_LEN - 1)); 
            Window_OutputBuffer[i] = 0.5f * (1.0f - tempCos);    // Hanning
        }
        else
        {
            Window_OutputBuffer[i] = 1.0f;                       // 不加窗
        }
    }
}


/* 找到基波的下标*/
void Find_BaseIndex(void)
{
    BaseIdx = 0;
    float max_val = 0;
    for (int i = 2; i < FFT_LEN / 2; i++) { // 遍历 0 ~ Fs/2 部分
        if (FFT_Output[i] > max_val) {
            max_val = FFT_Output[i];
            BaseIdx = i; // 记录基波的索引
        }
    }
}

/*波形判断*/
void Wave_Type(void){
	
}

/*输入参数为FFT计算后的结果，输出矫正后的频率和幅度

Row				FFT结果中峰值的位置
fs				采样频率
VPP[0]			矫正后的幅值
Freq[0]			矫正后的频率
correctNum		矫正的点数，一般取2即可，确保峰值左右的correctNum内没有其他信号
FFT_Output		FFT结果的幅值数组	
*/

void ADC_FFT_Get_Wave_Mes(uint32_t Row,float fs,float *VPP,float *Freq,int correctNum)
{
    int i;
    float k=2.667;                                     
    float DatePower1=0,DatePower2=0,f;
    for(i=-correctNum;i<=correctNum;i++)     
      {
          DatePower1+=(Row+i)*FFT_Output[Row+i]*FFT_Output[Row+i];
          DatePower2+=FFT_Output[Row+i]*FFT_Output[Row+i];
      }
      f=DatePower1/DatePower2;
      Freq[0] = f*fs/FFT_LEN;
      VPP[0] = 2.0f*sqrtf(k*DatePower2);
			HMI_send_float("x0.val",VPP[0]);
			HMI_send_float("x1.val",Freq[0]);
}
