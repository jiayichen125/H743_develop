#include "fft.h"
#include "HMI.h"

extern uint16_t ADC_Buffer[1024];
extern TIM_HandleTypeDef htim3;  /* ç”¨äºåŠ¨æ€åˆ‡æ¢é‡‡æ ·ç‡ */

/* ±äÁ¿ */
#define FFT_LEN 1024
#define ADC_LEN 1024
#define TIM3_CLK_HZ  20000000UL  /* 240MHz / (PSC+1=12) = 20MHz */

uint8_t ifftFlag = 0;
int BaseIdx = 0;         // »ù²¨ÏÂ±ê
int wave_type;           // ²¨ĞÎÀà±ğ 1ÊÇÕıÏÒ 2ÊÇÈı½Ç 3ÊÇ·½²¨
float fs = 100000.0f;    // ²ÉÑùÂÊ
float FFT_Freq = 0;      // FFT¼ÆËãµÃµ½ÆµÂÊ
float FFT_Ampl = 0;      // FFT¼ÆËãµÃµ½µÄ·ùÖµ
float DC = 0;            // Ö±Á÷Æ«ÖÃ
float FFT_mag_max = {0}; // ·ù¶ÈÆ××î´óÖµ
uint32_t FFT_mag_max_index = 0;

/* ÊäÈëºÍÊä³ö»º³å */

float FFT_Output[FFT_LEN];
float FFT_Input[FFT_LEN * 2];
float FFT_mag[FFT_LEN]; // ·ù¶ÈÆ×
float IFFT_Output[FFT_LEN];

uint8_t EnableWindow = 1;           // ÊÇ·ñ¼Ó´°
float Window_OutputBuffer[ADC_LEN]; // ´°º¯ÊıÊä³ö»º³å

/* ---------------------------------------------------------------
 * è‡ªé€‚åº”é‡‡æ ·ç‡
 * æ¡£ä½ï¼š20kHz / 200kHz / 2MHz
 * å¸¦è¿Ÿæ»ï¼ˆÂ±20%ï¼‰é˜²æ­¢è¾¹ç•Œé¢‘ç‡åå¤è·³æ¡£
 * --------------------------------------------------------------- */
static float select_fs(float freq)
{
    static float cur = 100000.0f;  /* ä¸ MX_TIM3_Init ARR=199 å¯¹åº” */
    if      (cur ==   20000.0f && freq >   1200.0f) cur =  200000.0f;
    else if (cur ==  200000.0f && freq <    800.0f) cur =   20000.0f;
    else if (cur ==  200000.0f && freq >  12000.0f) cur = 2000000.0f;
    else if (cur == 2000000.0f && freq <   8000.0f) cur =  200000.0f;
    return cur;
}

/* ç›´æ¥ä¿®æ”¹ TIM3->ARRï¼Œä¸åœå®šæ—¶å™¨ï¼Œä¸‹ä¸€å¸§ DMA å³ç”Ÿæ•ˆ
 * æ³¨æ„ï¼š2MHz æ¡£ï¼ˆARR=9ï¼‰è¦æ±‚ ADC æ€»è½¬æ¢æ—¶é—´ < 500nsï¼Œè¯·ç¡®è®¤ ADC å†…æ ¸æ—¶é’Ÿ */
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

    // ÇåÁã»º³åÇø
    memset(FFT_Input, 0, sizeof(FFT_Input));
    memset(FFT_mag, 0, sizeof(FFT_mag));
    memset(FFT_Output, 0, sizeof(FFT_Output));

    // ¼ÆËãADCÊı¾İµÄÆ½¾ùÖµ£¨DCÆ«ÖÃ£©
    uint32_t adc_sum = 0;
    for (int i = 0; i < 1024; i++)
    {
        adc_sum += ADC_Buffer[i];
    }
    DC = adc_sum / 1024.0f;
    HMI_send_float("x_dc", DC * 3.3f / 65536.0f); // ×ªÎªµçÑ¹Öµ£¨V£©
                                                  // ÊÇ·ñ¼Ó´°
    window();

    // Ïû³ıDCÆ«ÖÃºóÔÙ×ª¸¡µãºÍ¼Ó´°
    for (int i = 0; i < 1024; i++)
    {
        FFT_Input[i * 2] = ((float)ADC_Buffer[i] - DC) * Window_OutputBuffer[i];
        FFT_Input[i * 2 + 1] = 0;
    }

    arm_cfft_f32(&arm_cfft_sR_f32_len1024, FFT_Input, 0, 1);

    // showdata(FFT_Input,FFT_LEN);

    // ¼ÆËã·ù¶ÈÆ×
    arm_cmplx_mag_f32(FFT_Input, FFT_mag, FFT_LEN);

    // Hanning´°¹¦ÂÊ²¹³¥+¹éÒ»»¯
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

    /* æ ¹æ®æœ¬å¸§é¢‘ç‡ä¸ºä¸‹ä¸€å¸§é€‰æ‹©æœ€ä½³é‡‡æ ·ç‡ï¼ˆ> 50Hz æ‰ä¿¡ä»»æµ‹é‡ç»“æœï¼‰ */
    if (FFT_Freq > 50.0f)
    {
        float new_fs = select_fs(FFT_Freq);
        if (new_fs != fs)
            apply_fs(new_fs);
    }
}

/*fft caculate */
// ´ÓÆµÆ×ÖĞÌáÈ¡ĞÅºÅ£¬ÕÒµ½Ö÷Æµ£¬¼ÆËãĞÅºÅÆµÂÊºÍ·ù¶È¡£
void Process_FFT_mag(float *FFT_mag, float *FFT_mag_max, uint32_t *FFT_mag_max_index)
{

    // ÕÒ·ù¶ÈÆ×Ç°Ò»°ëÊı¾İ£¬ÕÒµ½×î´óÖµºÍË÷Òı
    arm_max_f32(FFT_mag, FFT_LEN / 2, FFT_mag_max, FFT_mag_max_index);

    // ÇóÆµÂÊ£º×î´óÖµ½á¹û*²ÉÑùÂÊ/FFT³¤¶È
    FFT_Freq = (float)(*FFT_mag_max_index) * fs / (float)FFT_LEN;

    // Çó·ùÖµ£º×î´óÖµ½á¹ûË÷Òı*2/FFT³¤¶È Ç°ÃæÒÑ¾­½øĞĞ¹ı¹éÒ»´¦ÀíÁË£¬ËùÒÔÕâÀï²»ĞèÒªÔÙ³ıÒÔFFT_LENÁË/*2
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

    // ÌáÈ¡Êµ²¿×÷Îª IFFT Êä³ö
    for (int i = 0; i < FFT_LEN; i++)
    {
        IFFT_Output[i] = FFT_Input[2 * i]; // È¡Êµ²¿
    }
}

/*Hanning´°*/
void window(void)
{
    for (int i = 0; i < ADC_LEN; i++)
    {
        if (EnableWindow)
        {
            float tempCos = cosf(2.0f * PI * i / (ADC_LEN - 1));
            Window_OutputBuffer[i] = 0.5f * (1.0f - tempCos); // Hanning
        }
        else
        {
            Window_OutputBuffer[i] = 1.0f; // ²»¼Ó´°
        }
    }
}

/* ÕÒµ½»ù²¨µÄÏÂ±ê*/
void Find_BaseIndex(void)
{
    BaseIdx = 0;
    float max_val = 0;
    for (int i = 2; i < FFT_LEN / 2; i++)
    { // ±éÀú 0 ~ Fs/2 ²¿·Ö
        if (FFT_mag[i] > max_val)
        {
            max_val = FFT_mag[i];
            BaseIdx = i; // ¼ÇÂ¼»ù²¨µÄË÷Òı
        }
    }
}

/* Ê±ÓòÍ³¼Æ·ÖÀà
 * ·µ»ØÖµ£º1=ÕıÏÒ²¨  2=Èı½Ç²¨  3=·½²¨  0=Î´Öª£¨ĞÅºÅ¹ıÈõ£©
 */
static int ClassifyWaveform(void)
{
    float sum_abs = 0.0f, sum_sq = 0.0f;
    uint32_t peak_count = 0;
    uint16_t max_v = 0, min_v = 65535;

    /* Ò»´Î±éÀúÇó¼«Öµ */
    for (uint32_t i = 0; i < ADC_LEN; i++)
    {
        if (ADC_Buffer[i] > max_v)
            max_v = ADC_Buffer[i];
        if (ADC_Buffer[i] < min_v)
            min_v = ADC_Buffer[i];
    }

    float vpp = (float)(max_v - min_v);
    if (vpp < 655.0f)
        return 0; /* ĞÅºÅ¹ıÈõ£¨< ~0.033V£©£¬·µ»Ø UNKNOWN */

    float offset = (float)min_v + vpp * 0.5f;
    float threshold = vpp * 0.10f; /* ·åÖµÇø¼ä£ºvpp ÉÏÏÂ 10% */

    /* ¶ş´Î±éÀúÍ³¼Æ Kf Óë Rpeak */
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

    float k_f = v_rms / v_avg;                         /* ²¨ĞÎÒò×Ó */
    float r_peak = (float)peak_count / (float)ADC_LEN; /* ·åÖµÕ¼±È */

    if (r_peak > 0.80f && k_f < 1.05f)
        return 3; /* ·½²¨£º¾ø´ó¶àÊıµãÔÚÁ½¶Ë£¬Kf¡Ö1 */
    if (r_peak < 0.25f && k_f > 1.13f)
        return 2; /* Èı½Ç²¨£º·åÖµÍ£Áô¼«¶Ì£¬Kf´ó */
    return 1;     /* ÕıÏÒ²¨£¨Ä¬ÈÏ£© */
}

/*²¨ĞÎÅĞ¶Ï£¨FFTĞ³²¨·¨ + Ê±ÓòÍ³¼Æ·¨ÁªºÏÅĞ¾ö£©*/
void wave_type_detect(void)
{
    int stat_type = 0, fft_type = 0;

    if (BaseIdx < 171)
    {
        /* µÍÆµ¶Î£¨»ù²¨ < 16.7kHz£©£º3´ÎĞ³²¨ÔÚÄÎ¿üË¹ÌØÄÚ£¬Ê¹ÓÃ FFT Ğ³²¨±ÈÖµ·¨ */
        float ratio = FFT_mag[3 * BaseIdx] / FFT_mag[BaseIdx];
        if (ratio < 0.05f)
            fft_type = 1; /* ÕıÏÒ²¨ */
        else if (ratio < 0.20f)
            fft_type = 2; /* Èı½Ç²¨ */
        else
            fft_type = 3; /* ·½²¨   */
        wave_type = fft_type;
    }
    else if (BaseIdx >= 205)
    {
        /* ¸ßÆµ¶Î£¨»ù²¨ > 20kHz£©£ºĞ³²¨³¬³öÄÎ¿üË¹ÌØ£¬ÍêÈ«ÒÀÀµÍ³¼Æ·¨ */
        stat_type = ClassifyWaveform();
        wave_type = (stat_type != 0) ? stat_type : 1;
    }
    else
    {
        /* ¹ı¶ÉÇø£¨16.7kHz ~ 20kHz£©£ºÁ½·¨¸÷³ö½áÂÛ£¬²»Ò»ÖÂÊ±ĞÅÈÎÍ³¼Æ·¨ */
        stat_type = ClassifyWaveform();
        /* ×¢Òâ£º´ËÇø¼ä 3*BaseIdx ÒÑ³¬ Nyquist£¬FFT ±ÈÖµ½ö¹©²Î¿¼ */
        float ratio = FFT_mag[3 * BaseIdx] / FFT_mag[BaseIdx];
        if (ratio < 0.05f)
            fft_type = 1;
        else if (ratio < 0.20f)
            fft_type = 2;
        else
            fft_type = 3;

        if (stat_type == 0 || stat_type == fft_type)
        {
            wave_type = fft_type; /* Ò»ÖÂ»òÍ³¼Æ·¨Ê§Ğ§£¬ĞÅÈÎ FFT */
        }
        else
        {
            wave_type = stat_type; /* ²»Ò»ÖÂ£¬ĞÅÈÎÍ³¼Æ·¨ */
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

/*ÊäÈë²ÎÊıÎªFFT¼ÆËãºóµÄ½á¹û£¬Êä³ö½ÃÕıºóµÄÆµÂÊºÍ·ù¶È

FFT_mag_max_index				FFT½á¹ûÖĞ·åÖµµÄÎ»ÖÃ
fs				²ÉÑùÆµÂÊ
FFT_Ampl	    ½ÃÕıºóµÄ·ùÖµ
Freq[0]			½ÃÕıºóµÄÆµÂÊ
correctNum		½ÃÕıµÄµãÊı£¬Ò»°ãÈ¡2¼´¿É£¬È·±£·åÖµ×óÓÒµÄcorrectNumÄÚÃ»ÓĞÆäËûĞÅºÅ
FFT_mag		FFT½á¹ûµÄ·ùÖµÊı×é
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
