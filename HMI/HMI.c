#include "HMI.h"
#include "usart.h"

void HMI_send_string(char* name, char* showdata)
{
    //printf("t0.txt=\"%d\"\xff\xff\xff", num);
    printf("%s=\"%s\"\xff\xff\xff", name, showdata);
}
 
void HMI_send_number(char* name, int num)
{
    // printf("t0.txt=\"%d\"\xff\xff\xff", num);
    printf("%s=%d\xff\xff\xff", name, num);
}
//
void HMI_send_float(char* name, float num)
{
     //printf("x0.val=\"%d\"\xff\xff\xff",(int)(num*1000));
    printf("%s=%d\xff\xff\xff", name, (int)(num * 1000));//本质上依旧是整数，几位小数要调整100的值
}
 
//波形显示 
void HMI_Wave(char* name, int ch, int val)
{
    printf("add %s,%d,%d\xff\xff\xff", name, ch, val);
}
 

//透传
void HMI_Wave_Fast(char* name, int ch, int count, int* show_data)
{
    int i;
    printf("addt %s,%d,%d\xff\xff\xff", name, ch, count);
    HAL_Delay(100);
    for (i = 0; i < count; i++)
        printf("%c", show_data[i]);
    printf("\xff\xff\xff");
}

//清空
void HMI_Wave_Clear(char* name, int ch)
{
    printf("cle %s,%d\xff\xff\xff", name, ch);
}


