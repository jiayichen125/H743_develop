#include "fft.h"
#include "HMI.h"

extern uint16_t ADC_Buffer[1024];


/* 变量 */
#define FFT_LEN 1024
#define ADC_LEN 1024

uint8_t ifftFlag = 0; 
<<<<<<< HEAD
int BaseIdx = 0; // �����±�
int wave_type; // ������� 1=���� 2=���� 3=���� 4=��ݲ�
float fs=100000.0f; // ������
float FFT_Freq=0;  //FFT����õ�Ƶ��
float FFT_Ampl=0;  //FFT����õ��ķ�ֵ 
float DC=0;//ֱ��ƫ��
float FFT_mag_max={0};  //���������ֵ
=======
int BaseIdx = 0; // 基波下标
int wave_type;//波形类别 1是正弦 2是三角 3是方波
float fs=100000.0f;//采样率
float FFT_Freq=0;  //FFT计算得到频率
float FFT_Ampl=0;  //FFT计算得到的幅值 
float DC=0;//直流偏置
float FFT_mag_max={0};  //幅度谱最大值
>>>>>>> a7e16709d97bdaf316d8aec032364cee4daa59d3
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
		printf("%.3f\n",buffer[i]);
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
	
<<<<<<< HEAD
  //�Ƿ�Ӵ� 
   window();

  // ����DCƫ�ú���ת����ͼӴ�
  for(int i = 0; i <FFT_LEN; i++)
=======
  // 计算ADC数据的平均值（DC偏置）
  uint32_t adc_sum = 0;
  for(int i = 0; i < 1024; i++)
    {
        adc_sum += ADC_Buffer[i];
    }
  DC= adc_sum / 1024.0f; 

  //是否加窗 
   window();

  // 消除DC偏置后再转浮点和加窗
  for(int i = 0; i < 1024; i++)
>>>>>>> a7e16709d97bdaf316d8aec032364cee4daa59d3
    {
        FFT_Input[i * 2] = ((float)ADC_Buffer[i]) * Window_OutputBuffer[i];
        FFT_Input[i * 2 + 1] = 0;                    
    }
 
  arm_cfft_f32(&arm_cfft_sR_f32_len1024, FFT_Input, 0, 1);
		
	//showdata(FFT_Input,FFT_LEN);
		
	//计算幅度谱
	arm_cmplx_mag_f32(FFT_Input,FFT_mag,FFT_LEN);
	
<<<<<<< HEAD
    //����ֱ��ƫ��
    DC = FFT_mag[0] / FFT_LEN;
    HMI_send_float("x2", DC / 65536.0f * 3.3f);

	// Hanning�����ʲ���+��һ��
	float window_power_correction =1.5f;
=======
	// Hanning窗功率补偿+归一化
	float window_power_correction =2.0f;
>>>>>>> a7e16709d97bdaf316d8aec032364cee4daa59d3
	for (uint16_t i=0;i<FFT_LEN;i++){
			 if(i==0){
				  FFT_mag[i]=FFT_mag[i]/FFT_LEN * window_power_correction;				 
				}else{
					FFT_mag[i]=FFT_mag[i]*2.0f/FFT_LEN * window_power_correction;
				}
	}
	
	Process_FFT_mag(FFT_mag,&FFT_mag_max,&FFT_mag_max_index);
	ADC_FFT_Get_Wave_Mes(FFT_mag_max_index,fs,&FFT_Ampl,&FFT_Freq,2);
    Find_BaseIndex();
    wave_type_detect();
}

/*fft caculate */
//从频谱中提取信号，找到主频，计算信号频率和幅度。
void Process_FFT_mag(float *FFT_mag,float *FFT_mag_max,uint32_t *FFT_mag_max_index)
{

<<<<<<< HEAD
	//�ҷ�����ǰһ�����ݣ��ҵ����ֵ������
	arm_max_f32(&FFT_mag[1],FFT_LEN/2-1,FFT_mag_max,FFT_mag_max_index);
	
    *FFT_mag_max_index+=1; //��Ϊarm_max_f32�Ǵ�FFT_mag[1]��ʼ�ҵģ���������Ҫ��1

	//��Ƶ�ʣ����ֵ���*������/FFT����
=======
	//找幅度谱前一半数据，找到最大值和索引
	arm_max_f32(FFT_mag,FFT_LEN/2,FFT_mag_max,FFT_mag_max_index);
	
	//求频率：最大值结果*采样率/FFT长度
>>>>>>> a7e16709d97bdaf316d8aec032364cee4daa59d3
	FFT_Freq=(float)(*FFT_mag_max_index)*fs/(float)FFT_LEN;
	
	//求幅值：最大值结果索引*2/FFT长度 前面已经进行过归一处理了，所以这里不需要再除以FFT_LEN了/*2
	FFT_Ampl=*FFT_mag_max;

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
    BaseIdx = 1;
    float max_val = 0;
<<<<<<< HEAD
    for (int i = 1; i < FFT_LEN / 2; i++) { // ���� 0 ~ Fs/2 ����
=======
    for (int i = 2; i < FFT_LEN / 2; i++) { // 遍历 0 ~ Fs/2 部分
>>>>>>> a7e16709d97bdaf316d8aec032364cee4daa59d3
        if (FFT_mag[i] > max_val) {
            max_val = FFT_mag[i];
            BaseIdx = i; // 记录基波的索引
        }
    }
}

/* 时域统计分类
 * 返回值：1=正弦波  2=三角波  3=方波  0=未知（信号过弱）
 */
static int ClassifyWaveform(void)
{
    float    sum_abs = 0.0f, sum_sq = 0.0f;
    uint32_t peak_count = 0;
    uint16_t max_v = 0, min_v = 65535;

    /* 一次遍历求极值 */
    for(uint32_t i = 0; i < ADC_LEN; i++) {
        if(ADC_Buffer[i] > max_v) max_v = ADC_Buffer[i];
        if(ADC_Buffer[i] < min_v) min_v = ADC_Buffer[i];
    }

    float vpp = (float)(max_v - min_v);
    if(vpp < 655.0f) return 0; /* 信号过弱（< ~0.033V），返回 UNKNOWN */

    float offset    = (float)min_v + vpp * 0.5f;
    float threshold = vpp * 0.10f; /* 峰值区间：vpp 上下 10% */

    /* 二次遍历统计 Kf 与 Rpeak */
    for(uint32_t i = 0; i < ADC_LEN; i++) {
        float val = (float)ADC_Buffer[i] - offset;
        sum_abs += fabsf(val);
        sum_sq  += val * val;
        if((float)ADC_Buffer[i] >= (float)max_v - threshold ||
           (float)ADC_Buffer[i] <= (float)min_v + threshold) {
            peak_count++;
        }
    }

    float v_rms  = sqrtf(sum_sq / (float)ADC_LEN);
    float v_avg  = sum_abs / (float)ADC_LEN;
    if(v_avg < 1e-6f) return 0;

    float k_f    = v_rms / v_avg;                       /* 波形因子 */
    float r_peak = (float)peak_count / (float)ADC_LEN;  /* 峰值占比 */

    if(r_peak > 0.80f && k_f < 1.05f) return 3; /* 方波：绝大多数点在两端，Kf≈1 */
    if(r_peak < 0.25f && k_f > 1.13f) return 2; /* 三角波：峰值停留极短，Kf大 */
    return 1;                                    /* 正弦波（默认） */
}

/*波形判断（FFT谐波法 + 时域统计法联合判决）*/
void wave_type_detect(void)
{
    int stat_type = 0, fft_type = 0;

    if(BaseIdx < 171) {
        /* 低频段（基波 < 16.7kHz）：3次谐波在奈奎斯特内，使用 FFT 谐波比值法 */
        float ratio = FFT_mag[3 * BaseIdx] / FFT_mag[BaseIdx];
        if     (ratio < 0.05f) fft_type = 1; /* 正弦波 */
        else if(ratio < 0.20f) fft_type = 2; /* 三角波 */
        else                   fft_type = 3; /* 方波   */
        wave_type = fft_type;

    } else if(BaseIdx >= 205) {
        /* 高频段（基波 > 20kHz）：谐波超出奈奎斯特，完全依赖统计法 */
        stat_type = ClassifyWaveform();
        wave_type = (stat_type != 0) ? stat_type : 1;

<<<<<<< HEAD
    // ��������͸���г���ķ�ֵ��
    float ratio3 = FFT_mag[3*BaseIdx] / FFT_mag[BaseIdx];  // 3��г��
    float ratio5 = (5*BaseIdx < FFT_LEN/2) ? FFT_mag[5*BaseIdx] / FFT_mag[BaseIdx] : 0;  // 5��г��
    float ratio7 = (7*BaseIdx < FFT_LEN/2) ? FFT_mag[7*BaseIdx] / FFT_mag[BaseIdx] : 0;  // 7��г��
    
    // �����ж��߼�
    if (ratio3 < 0.05f) {
        // ���Ҳ���ֻ��3��г����С
        wave_type = 1;
        HMI_send_string("t0", "���Ҳ�");
    } 
    else if (ratio3 < 0.20f && ratio5 < 0.05f && ratio7 < 0.05f) {
        // ���ǲ���3��г���еȣ�5,7��г����С
        wave_type = 2;
        HMI_send_string("t0", "���ǲ�");
    } 
    else if (ratio3 > 0.15f && ratio5 > 0.05f && ratio7 > 0.02f) {
        // ������3,5,7��г����ǿ
        wave_type = 3;
        HMI_send_string("t0", "����");
    }
    else {
        // Ĭ����Ϊ����
        wave_type = 3;
        HMI_send_string("t0", "����");
=======
    } else {
        /* 过渡区（16.7kHz ~ 20kHz）：两法各出结论，不一致时信任统计法 */
        stat_type = ClassifyWaveform();
        /* 注意：此区间 3*BaseIdx 已超 Nyquist，FFT 比值仅供参考 */
        float ratio = FFT_mag[3 * BaseIdx] / FFT_mag[BaseIdx];
        if     (ratio < 0.05f) fft_type = 1;
        else if(ratio < 0.20f) fft_type = 2;
        else                   fft_type = 3;

        if(stat_type == 0 || stat_type == fft_type) {
            wave_type = fft_type;  /* 一致或统计法失效，信任 FFT */
        } else {
            wave_type = stat_type; /* 不一致，信任统计法 */
        }
    }

    switch(wave_type) {
        case 1:  HMI_send_string("t0", "sine"); break;
        case 2:  HMI_send_string("t0", "triangle"); break;
        case 3:  HMI_send_string("t0", "square");   break;
        default: HMI_send_string("t0", "unknown");   break;
>>>>>>> a7e16709d97bdaf316d8aec032364cee4daa59d3
    }
}

/*输入参数为FFT计算后的结果，输出矫正后的频率和幅度

FFT_mag_max_index				FFT结果中峰值的位置
fs				采样频率
FFT_Ampl	    矫正后的幅值
Freq[0]			矫正后的频率
correctNum		矫正的点数，一般取2即可，确保峰值左右的correctNum内没有其他信号
FFT_mag		FFT结果的幅值数组	
*/

void ADC_FFT_Get_Wave_Mes(uint32_t FFT_mag_max_index,float fs,float *FFT_Ampl,float *Freq,int correctNum)
{
    int i;                                 
    float DatePower1=0,DatePower2=0,f;
    for(i=-correctNum;i<=correctNum;i++)     
      {
          DatePower1+=(FFT_mag_max_index+i)*FFT_mag[FFT_mag_max_index+i]*FFT_mag[FFT_mag_max_index+i];
          DatePower2+=FFT_mag[FFT_mag_max_index+i]*FFT_mag[FFT_mag_max_index+i];
      }
    f=DatePower1/DatePower2;
    Freq[0] = f*fs/FFT_LEN;
    *FFT_Ampl = sqrtf(DatePower2) * 3.3f / 65536.0f;  // k=1, 去掉2倍, 直接出电压
    HMI_send_float("x0", *FFT_Ampl);
	HMI_send_float("x1",Freq[0]);
}
