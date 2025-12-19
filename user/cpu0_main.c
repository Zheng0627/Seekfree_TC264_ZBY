/*********************************************************************************************************************
 * TC264 Opensourec Library 即（TC264 开源库）是一个基于官方 SDK 接口的第三方开源库
 * Copyright (c) 2022 SEEKFREE 逐飞科技
 *
 * 本文件是 TC264 开源库的一部分
 *
 * TC264 开源库 是免费软件
 * 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
 * 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
 *
 * 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
 * 甚至没有隐含的适销性或适合特定用途的保证
 * 更多细节请参见 GPL
 *
 * 您应该在收到本开源库的同时收到一份 GPL 的副本
 * 如果没有，请参阅<https://www.gnu.org/licenses/>
 *
 * 额外注明：
 * 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
 * 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
 * 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
 * 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
 *
 * 文件名称          cpu0_main
 * 公司名称          成都逐飞科技有限公司
 * 版本信息          查看 libraries/doc 文件夹内 version 文件 版本说明
 * 开发环境          ADS v1.10.2
 * 适用平台          TC264D
 * 店铺链接          https://seekfree.taobao.com/
 *
 * 修改记录
 * 日期              作者                备注
 * 2022-09-15       pudding            first version
 * 2025-12-05       Zheng              添加IPS114液晶屏显示代码
 * 2025-12-06       Zheng              添加电机编码器测速代码
 * 2025-12-10       Zheng              优化循迹算法（没优化好-预览版发布）
 ********************************************************************************************************************/
#include "zf_common_headfile.h"
#pragma section all "cpu0_dsram"
// 全局变量存放处
int car_go = 0;                        // 0:停止 1:发车
int16 center_line_avg = 0;             // 直线循迹模式下车道中心线平均位置
#define CENTER_LINE_LEN (60)           // 中心线长度（像素点个数）
#define CENTER_LINE_OUTLIER_THRESH (5) // 像素距离超过该阈值视为离群
// 全局变量存放处
#pragma section all "cpu0_dsram"

// 有刷电机相关配置
// 根据实际的电机连接情况更改L/R轮的DIR和PWM引脚定义
#define PWM_DEFAULT_DUTY (100)                // n乘这个数换算成常见的百分比占空比
#define PWM_BASE_DUTY (10 * PWM_DEFAULT_DUTY) // 基础占空比 太小了转不动 太大了会起飞
#define DIR_L (P02_4)                         // 可以调换顺序 或者把线反着接
#define PWM_L (ATOM0_CH5_P02_5)
#define DIR_R (P02_6)
#define PWM_R (ATOM0_CH7_P02_7)
int16 TARGET_SPEED = 7;  // 目标速度 后续PID/LQR控制会用到
int16 WHEEL_SPEED_L = 0; // 左轮速度
int16 WHEEL_SPEED_R = 0; // 右轮速度

// 编码器配置
#define ENCODER_1 (TIM2_ENCODER)
#define ENCODER_1_A (TIM2_ENCODER_CH1_P33_7)
#define ENCODER_1_B (TIM2_ENCODER_CH2_P33_6)
#define ENCODER_3 (TIM4_ENCODER)
#define ENCODER_3_A (TIM4_ENCODER_CH1_P02_8)
#define ENCODER_3_B (TIM4_ENCODER_CH2_P00_9)

// 板载LED灯定义
#define LED1 (P20_9)
#define LED2 (P20_8)
#define LED3 (P21_5)
#define LED4 (P21_4)
int led_statu = 0; // 用于LED流水点亮

// 板载KEY定义
#define KEY1 (P20_6)
#define KEY2 (P20_7)
#define KEY3 (P11_2)
#define KEY4 (P11_3)

// mt9v03x摄像头相关定义
uint8 binary_threshold = 100;             // 二值化阈值
uint8 binary_image[MT9V03X_W][MT9V03X_H]; // 用于存放二值化图像的数组
const uint8 *image_temp;                  // 用于临时存放图像行指针
void image_to_binary(const uint8 *image, uint8 binary_threshold)
{
    for (uint32 i = 0; i < MT9V03X_H; i++)
    {
        image_temp = image + i * MT9V03X_W;
        for (uint32 j = 0; j < MT9V03X_W; j++)
        {
            if (*(image_temp + j) > binary_threshold)
            {
                binary_image[j][i] = 1;
            }
            else
            {
                binary_image[j][i] = 0;
            }
        }
    }
}
int row_change_point_count(uint8 row)
{
    int count = 0;
    for (uint32 j = 54; j < 134; j++)
    {
        if (binary_image[j][row] != binary_image[j - 1][row])
        {
            // Skip isolated single-pixel flips (noise) where neighbors stay the same
            if (j + 1 < MT9V03X_W &&
                binary_image[j + 1][row] == binary_image[j - 1][row] &&
                binary_image[j][row] != binary_image[j + 1][row])
            {
                continue;
            }
            count++;
        }
    }
    return count;
}
int line_change_point_count(uint8 line)
{
    int count = 0;
    for (uint32 i = 40; i < 100; i++)
    {
        if (binary_image[line][i] != binary_image[line][i - 1])
        {
            // Skip isolated single-pixel flips (noise) where neighbors stay the same
            if (i + 1 < MT9V03X_H &&
                binary_image[line][i + 1] == binary_image[line][i - 1] &&
                binary_image[line][i] != binary_image[line][i + 1])
            {
                continue;
            }
            count++;
        }
    }
    return count;
}

int core0_main(void)
{
    clock_init(); // 获取时钟频率<务必保留>
    debug_init(); // 初始化默认调试串口 UART3

    // 初始化IPS114液晶屏
    ips114_init();
    ips114_set_color(RGB565_WHITE, RGB565_BLACK); // 设置默认前景色和背景色
    ips114_clear();

    // 初始化MT9V03X摄像头
    mt9v03x_init();
    /* 使用全局的 binary_image 数组 */

    // 初始化有刷电机驱动相关引脚和PWM
    gpio_init(DIR_R, GPO, GPIO_HIGH, GPO_PUSH_PULL); // GPIO 初始化为输出 默认上拉输出高
    pwm_init(PWM_R, 17000, 0);                       // PWM 通道初始化频率 17KHz 占空比初始为 0
    gpio_init(DIR_L, GPO, GPIO_HIGH, GPO_PUSH_PULL); // GPIO 初始化为输出 默认上拉输出高
    pwm_init(PWM_L, 17000, 0);                       // PWM 通道初始化频率 17KHz 占空比初始为 0
    gpio_set_level(DIR_R, GPIO_HIGH);                // 右轮正转
    gpio_set_level(DIR_L, GPIO_HIGH);                // 左轮正转
    // 初始化编码器
    encoder_dir_init(ENCODER_1, ENCODER_1_A, ENCODER_1_B); // 编码器1初始化
    encoder_dir_init(ENCODER_3, ENCODER_3_A, ENCODER_3_B); // 编码器3初始化
    pit_ms_init(CCU60_CH0, 30);                            // PIT定时器中断 用于计算轮速

    // 初始化板载LED灯 低电平点亮
    gpio_init(LED1, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED1 输出 默认低电平 推挽输出模式
    gpio_init(LED2, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED2 输出 默认低电平 推挽输出模式
    gpio_init(LED3, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED3 输出 默认低电平 推挽输出模式
    gpio_init(LED4, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED4 输出 默认低电平 推挽输出模式
    // pit_ms_init(CCU60_CH1, 110);                   // PIT定时器中断 用于流水点灯 但是没啥卵用

    // 初始化板载KEY
    gpio_init(KEY1, GPI, GPIO_HIGH, GPI_PULL_UP); // 初始化 KEY1 输入 默认高电平 上拉输入
    gpio_init(KEY2, GPI, GPIO_HIGH, GPI_PULL_UP); // 初始化 KEY2 输入 默认高电平 上拉输入
    gpio_init(KEY3, GPI, GPIO_HIGH, GPI_PULL_UP); // 初始化 KEY3 输入 默认高电平 上拉输入
    gpio_init(KEY4, GPI, GPIO_HIGH, GPI_PULL_UP); // 初始化 KEY4 输入 默认高电平 上拉输入

    // 按键功能控制定时器
    pit_ms_init(CCU61_CH0, 50);
    // UART3
    pit_ms_init(CCU61_CH1, 1000);

    cpu_wait_event_ready(); // 等待所有核心初始化完毕<务必保留>

    while (TRUE)
    {
        if (row_change_point_count(40) >= 2 && line_change_point_count(54) == 0 && line_change_point_count(134) == 0 && car_go == 1) // 直线循迹模式
        {
            uint8 center_line[CENTER_LINE_LEN];
            for (uint8 i = 40; i < 100; i++)
            {
                uint8 line_first_pos = 0;
                uint8 line_last_pos = 0;
                for (uint8 j = 54; j < 134; j++)
                {
                    if (binary_image[j][i])
                    {
                        line_first_pos = j;
                        break;
                    }
                }
                for (int j = 134; j >= 54; j--)
                {
                    if (binary_image[j][i])
                    {
                        line_last_pos = j;
                        break;
                    }
                }
                center_line[i - 40] = (line_first_pos + line_last_pos) / 2;
            }

            // 一次均值+剔除离群，再求均值
            int32 sum_raw = 0;
            for (uint8 idx = 0; idx < CENTER_LINE_LEN; idx++)
            {
                sum_raw += center_line[idx];
            }
            int16 mean_raw = (int16)(sum_raw / CENTER_LINE_LEN);

            int32 filtered_sum = 0;
            uint8 filtered_count = 0;
            for (uint8 idx = 0; idx < CENTER_LINE_LEN; idx++)
            {
                int16 diff = center_line[idx] - mean_raw;
                if (diff < 0)
                {
                    diff = -diff;
                }
                if (diff <= CENTER_LINE_OUTLIER_THRESH)
                {
                    filtered_sum += center_line[idx];
                    filtered_count++;
                }
            }

            if (filtered_count > 0)
            {
                center_line_avg = (int16)(filtered_sum / filtered_count);
            }
            else
            {
                center_line_avg = mean_raw; // 极端情况：全部被判离群，回退到初始均值
            }

            if (center_line_avg >= 96)
            {
                pwm_set_duty(PWM_L, PWM_BASE_DUTY);
                pwm_set_duty(PWM_R, 0);
            }
            else if (center_line_avg <= 92)
            {
                pwm_set_duty(PWM_L, 0);
                pwm_set_duty(PWM_R, PWM_BASE_DUTY);
            }
            else
            {
                pwm_set_duty(PWM_L, PWM_BASE_DUTY);
                pwm_set_duty(PWM_R, PWM_BASE_DUTY);
            }
        }
        if (row_change_point_count(40) == 0 && row_change_point_count(100) == 2 && line_change_point_count(54) == 2 && line_change_point_count(134) == 0 && car_go == 1) // 左转弯
        {
            pwm_set_duty(PWM_L, 0);
            pwm_set_duty(PWM_R, PWM_BASE_DUTY);
            while (1)
            {
                if (row_change_point_count(40) == 2)
                {
                    break;
                }
                else
                {
                    pwm_set_duty(PWM_L, 0);
                    pwm_set_duty(PWM_R, PWM_BASE_DUTY);
                }
            }
        }
        if (row_change_point_count(40) == 0 && row_change_point_count(100) == 2 && line_change_point_count(54) == 0 && line_change_point_count(134) == 2 && car_go == 1) // 右转弯
        {
            pwm_set_duty(PWM_L, PWM_BASE_DUTY);
            pwm_set_duty(PWM_R, 0);
            while (1)
            {
                if (row_change_point_count(40) == 2)
                {
                    break;
                }
                else
                {
                    pwm_set_duty(PWM_L, PWM_BASE_DUTY);
                    pwm_set_duty(PWM_R, 0);
                }
            }
        }
    }

    // return 0;
}

/*这个中断函数用来计算轮速
每30ms进入一次中断
*/
IFX_INTERRUPT(cc60_pit_ch0_isr, 0, CCU6_0_CH0_ISR_PRIORITY)
{
    interrupt_global_enable(0); // 开启中断嵌套
    pit_clear_flag(CCU60_CH0);
    WHEEL_SPEED_L = -1 * encoder_get_count(ENCODER_1) / 30;
    WHEEL_SPEED_R = encoder_get_count(ENCODER_3) / 30;
    encoder_clear_count(ENCODER_1);
    encoder_clear_count(ENCODER_3);
    image_to_binary((const uint8 *)mt9v03x_image, binary_threshold);
}

/*这个中断函数用来点灯
不知道有什么用 但是很爽:>
*/
// IFX_INTERRUPT(cc60_pit_ch1_isr, 0, CCU6_0_CH1_ISR_PRIORITY)
// {
//     interrupt_global_enable(0); // 开启中断嵌套
//     pit_clear_flag(CCU60_CH1);
//     led_statu++;
//     if (led_statu == 1)
//     {
//         gpio_toggle_level(LED1);
//     }
//     else if (led_statu == 2)
//     {
//         gpio_toggle_level(LED2);
//     }
//     else if (led_statu == 3)
//     {
//         gpio_toggle_level(LED3);
//     }
//     else if (led_statu == 4)
//     {
//         gpio_toggle_level(LED4);
//     }
//     else if (led_statu == 5)
//     {
//         led_statu = 0;
//     }
// }

IFX_INTERRUPT(cc61_pit_ch0_isr, 0, CCU6_1_CH0_ISR_PRIORITY)
{
    interrupt_global_enable(0); // 开启中断嵌套
    pit_clear_flag(CCU61_CH0);
    if (gpio_get_level(KEY3) == GPIO_LOW)
    {
        binary_threshold++;
    }
    if (gpio_get_level(KEY4) == GPIO_LOW)
    {
        binary_threshold--;
    }
    if (gpio_get_level(KEY1) == GPIO_LOW)
    {
        car_go = 1; // 发车
    }

    if (mt9v03x_finish_flag && car_go == 0)
    {
        ips114_show_gray_image(0, 0, (const uint8 *)mt9v03x_image, MT9V03X_W, MT9V03X_H, MT9V03X_W, MT9V03X_H, binary_threshold); // 显示灰度图像
        ips114_draw_line(54, 40, 134, 40, RGB565_RED);
        ips114_draw_line(134, 40, 134, 100, RGB565_RED);
        ips114_draw_line(54, 100, 134, 100, RGB565_RED);
        ips114_draw_line(54, 40, 54, 100, RGB565_RED);
        ips114_draw_line(center_line_avg, 40, center_line_avg, 100, RGB565_GREEN);
        mt9v03x_finish_flag = 0;
    }
}

IFX_INTERRUPT(cc61_pit_ch1_isr, 0, CCU6_1_CH1_ISR_PRIORITY)
{
    interrupt_global_enable(0); // 开启中断嵌套
    pit_clear_flag(CCU61_CH1);
    // printf("binary_image_data:\n");
    // for (uint16 current_binary_line = 0; current_binary_line < MT9V03X_H; current_binary_line++)
    // {
    //     printf("line %d: ", current_binary_line);
    //     for (uint32 j = 0; j < MT9V03X_W; j++)
    //     {
    //         printf("%d ", binary_image[j][current_binary_line]);
    //     }
    //     printf("\n");
    // }

    // 计算并打印第59行（从0开始计数）的第一个和最后一个1的位置
    // {
    //     int16 first_pos = -1;
    //     int16 last_pos = -1;
    //     find_first_last_one_positions(59, &first_pos, &last_pos);
    //     printf("row 59 first_1=%d, last_1=%d\n", first_pos, last_pos);
    // }
}

#pragma section all restore
// **************************** 代码区域 ****************************
