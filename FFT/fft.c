#include "fft.h"
#include "HMI.h"

extern uint16_t ADC_Buffer[1024];
extern TIM_HandleTypeDef htim3;

/* 变量 */
#define FFT_LEN 1024
#define ADC_LEN 1024
#define TIM3_CLK_HZ  20000000UL  /* 240MHz / (PSC+1=12) = 20MHz */

uint8_t ifftFlag = 0;
int BaseIdx = 0;         // 基波下标
int wave_type;           // 波形类别 1是正弦 2是三角 3是方波
float fs = 100000.0f;    // 采样率
float FFT_Freq = 0;      // FFT计算得到频率
float FFT_Ampl = 0;      // FFT计算得到的幅值
float DC = 0;            // 直流偏置
float FFT_mag_max = {0}; // 幅度谱最大值
uint32_t FFT_mag_max_index = 0;



float FFT_Output[FFT_LEN];
float FFT_Input[FFT_LEN * 2];
float FFT_mag[FFT_LEN]; // 幅度谱
float IFFT_Output[FFT_LEN];

uint8_t EnableWindow = 1;           // 是否加窗
float Window_OutputBuffer[ADC_LEN]; // 窗函数输出缓冲

static float select_fs(float freq)
{
    static float cur = 100000.0f;
    if      (cur ==   20000.0f && freq >   1200.0f) cur =  200000.0f;
    else if (cur ==  200000.0f && freq <    800.0f) cur =   20000.0f;
    else if (cur ==  200000.0f && freq >  12000.0f) cur = 2000000.0f;
    else if (cur == 2000000.0f && freq <   8000.0f) cur =  200000.0f;
    return cur;
}

static void apply_fs(float new_fs)
{
    uint32_t arr = (uint32_t)((float)TIM3_CLK_HZ / new_fs + 0.5f) - 1;
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    fs = new_fs;
}

void showdata(float *buffer, uint16_t n)
{
    for (int i = 0; i < n; i++)
    {
        printf("%.3f\n", buffer[i]);
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

    // 清零缓冲区
    memset(FFT_Input, 0, sizeof(FFT_Input));
    memset(FFT_mag, 0, sizeof(FFT_mag));
    memset(FFT_Output, 0, sizeof(FFT_Output));

    // 计算ADC数据的平均值（DC偏置）
    uint32_t adc_sum = 0;
    for (int i = 0; i < 1024; i++)
    {
        adc_sum += ADC_Buffer[i];
    }
    DC = adc_sum / 1024.0f;
    HMI_send_float("x_dc", DC * 3.3f / 65536.0f); // 转为电压值（V）
                                                  // 是否加窗
    window();

    // 消除DC偏置后再转浮点和加窗
    for (int i = 0; i < 1024; i++)
    {
        FFT_Input[i * 2] = ((float)ADC_Buffer[i] - DC) * Window_OutputBuffer[i];
        FFT_Input[i * 2 + 1] = 0;
    }

    arm_cfft_f32(&arm_cfft_sR_f32_len1024, FFT_Input, 0, 1);

    // showdata(FFT_Input,FFT_LEN);

    // 计算幅度谱
    arm_cmplx_mag_f32(FFT_Input, FFT_mag, FFT_LEN);

    // Hanning窗功率补偿+归一化
    float window_power_correction = 2.0f;
    for (uint16_t i = 0; i < FFT_LEN; i++)
    {
        if (i == 0)
        {
            FFT_mag[i] = FFT_mag[i] / FFT_LEN * window_power_correction;
        }
        else
        {
            FFT_mag[i] = FFT_mag[i] * 2.0f / FFT_LEN * window_power_correction;
        }
    }

    Process_FFT_mag(FFT_mag, &FFT_mag_max, &FFT_mag_max_index);

    ADC_FFT_Get_Wave_Mes(FFT_mag_max_index, fs, &FFT_Ampl, &FFT_Freq, 2);

    Find_BaseIndex();
    wave_type_detect();
    if (FFT_Freq > 50.0f)
    {
        float new_fs = select_fs(FFT_Freq);
        if (new_fs != fs)
            apply_fs(new_fs);
    }
}

/*fft caculate */
// 从频谱中提取信号，找到主频，计算信号频率和幅度。
void Process_FFT_mag(float *FFT_mag, float *FFT_mag_max, uint32_t *FFT_mag_max_index)
{

    // 找幅度谱前一半数据，找到最大值和索引
    arm_max_f32(FFT_mag, FFT_LEN / 2, FFT_mag_max, FFT_mag_max_index);

    // 求频率：最大值结果*采样率/FFT长度
    FFT_Freq = (float)(*FFT_mag_max_index) * fs / (float)FFT_LEN;

    // 求幅值：最大值结果索引*2/FFT长度 前面已经进行过归一处理了，所以这里不需要再除以FFT_LEN了/*2
    FFT_Ampl = *FFT_mag_max;
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
    for (int i = 0; i < FFT_LEN; i++)
    {
        IFFT_Output[i] = FFT_Input[2 * i]; // 取实部
    }
}

/*Hanning窗*/
void window(void)
{
    if (EnableWindow)
    {
        for (int i = 0; i < ADC_LEN; i++)
        {
            float tempCos = cosf(2.0f * PI * i / (ADC_LEN - 1));
            Window_OutputBuffer[i] = 0.5f * (1.0f - tempCos); // Hanning
        }
        window_power_correction = 1.5f;
    }
    else
    {
        for (int i = 0; i < ADC_LEN; i++)
        {
            Window_OutputBuffer[i] = 1.0f; // 不加窗
        }
        window_power_correction = 1.0f;
    }
}

/* 找到基波的下标*/
void Find_BaseIndex(void)
{
    BaseIdx = 0;
    float max_val = 0;
    for (int i = 2; i < FFT_LEN / 2; i++)
    { // 遍历 0 ~ Fs/2 部分
        if (FFT_mag[i] > max_val)
        {
            max_val = FFT_mag[i];
            BaseIdx = i; 
        }
    }
}

static int ClassifyWaveform(void)
{
    float sum_abs = 0.0f, sum_sq = 0.0f;
    uint32_t peak_count = 0;
    uint16_t max_v = 0, min_v = 65535;

    /* 一次遍历求极值 */
    for (uint32_t i = 0; i < ADC_LEN; i++)
    {
        if (ADC_Buffer[i] > max_v)
            max_v = ADC_Buffer[i];
        if (ADC_Buffer[i] < min_v)
            min_v = ADC_Buffer[i];
    }

    float vpp = (float)(max_v - min_v);
    if (vpp < 655.0f)
        return 0; /* 信号过弱（< ~0.033V），返回 UNKNOWN */

    float offset = (float)min_v + vpp * 0.5f;
    float threshold = vpp * 0.10f; /* 峰值区间：vpp 上下 10% */

    /* 二次遍历统计 Kf 与 Rpeak */
    for (uint32_t i = 0; i < ADC_LEN; i++)
    {
        float val = (float)ADC_Buffer[i] - offset;
        sum_abs += fabsf(val);
        sum_sq += val * val;
        if ((float)ADC_Buffer[i] >= (float)max_v - threshold ||
            (float)ADC_Buffer[i] <= (float)min_v + threshold)
        {
            peak_count++;
        }
    }

    float v_rms = sqrtf(sum_sq / (float)ADC_LEN);
    float v_avg = sum_abs / (float)ADC_LEN;
    if (v_avg < 1e-6f)
        return 0;

    float k_f = v_rms / v_avg;                         /* 波形因子 */
    float r_peak = (float)peak_count / (float)ADC_LEN; /* 峰值占比 */

    if (r_peak > 0.80f && k_f < 1.05f)
        return 3; /* 方波：绝大多数点在两端，Kf≈1 */
    if (r_peak < 0.25f && k_f > 1.13f)
        return 2; /* 三角波：峰值停留极短，Kf大 */
    return 1;     /* 正弦波（默认） */
}

void wave_type_detect(void)
{
    int stat_type = 0;

    if (BaseIdx < 171)
    {
        /* 低频段（基波 < 16.7kHz）：3次谐波在奈奎斯特内，使用 FFT 谐波比值法 */
        float ratio = FFT_mag[3 * BaseIdx] / FFT_mag[BaseIdx];
        if (ratio < 0.05f)
            fft_type = 1; /* 正弦波 */
        else if (ratio < 0.20f)
            fft_type = 2; /* 三角波 */
        else
            fft_type = 3; /* 方波   */
        wave_type = fft_type;
    }
    else if (BaseIdx >= 205)
    {
        /* 高频段（基波 > 20kHz）：谐波超出奈奎斯特，完全依赖统计法 */
        stat_type = ClassifyWaveform();
        wave_type = (stat_type != 0) ? stat_type : 1;
    }
    else
    {
        /* 过渡区（16.7kHz ~ 20kHz）：两法各出结论，不一致时信任统计法 */
        stat_type = ClassifyWaveform();
        /* 娉ㄦ剰锛氭鍖洪棿 3*BaseIdx 宸茶秴 Nyquist锛孎FT 姣斿�间粎渚涘弬鑰� */
        float ratio = FFT_mag[3 * BaseIdx] / FFT_mag[BaseIdx];
        if (ratio < 0.05f)
            fft_type = 1;
        else if (ratio < 0.20f)
            fft_type = 2;
        else
            fft_type = 3;

        if (stat_type == 0 || stat_type == fft_type)
        {
            wave_type = fft_type; /* 一致或统计法失效，信任 FFT */
        }
        else
        {
            wave_type = stat_type; /* 不一致，信任统计法 */
        }
    }

    switch (wave_type)
    {
    case 1:
        HMI_send_string("t0", "sine");
        break;
    case 2:
        HMI_send_string("t0", "triangle");
        break;
    case 3:
        HMI_send_string("t0", "square");
        break;
    default:
        HMI_send_string("t0", "unknown");
        break;
    }
}

/*杈撳叆鍙傛暟涓篎FT璁＄畻鍚庣殑缁撴灉锛岃緭鍑虹煫姝ｅ悗鐨勯鐜囧拰骞呭害

FFT_mag_max_index				FFT结果中峰值的位置
fs				采样频率
FFT_Ampl	    矫正后的幅值
Freq[0]			矫正后的频率
correctNum		矫正的点数，一般取2即可，确保峰值左右的correctNum内没有其他信号
FFT_mag		FFT结果的幅值数组
*/

void ADC_FFT_Get_Wave_Mes(uint32_t FFT_mag_max_index, float fs, float *FFT_Ampl, float *Freq, int correctNum)
{
    int i;
    float DatePower1 = 0, DatePower2 = 0, f;
    for (i = -correctNum; i <= correctNum; i++)
    {
        DatePower1 += (FFT_mag_max_index + i) * FFT_mag[FFT_mag_max_index + i] * FFT_mag[FFT_mag_max_index + i];
        DatePower2 += FFT_mag[FFT_mag_max_index + i] * FFT_mag[FFT_mag_max_index + i];
    }
    f = DatePower1 / DatePower2;
    Freq[0] = f * fs / FFT_LEN;
    *FFT_Ampl = FFT_mag[FFT_mag_max_index] * 3.3f / 65536.0f;
    HMI_send_float("x0", *FFT_Ampl);
    HMI_send_float("x1", Freq[0]);
}
