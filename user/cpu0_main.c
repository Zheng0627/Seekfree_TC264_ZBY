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
 * 2025-12-06       Zheng              添加电机编码器测速代码(但是没操出来PID)
 ********************************************************************************************************************/
#include "zf_common_headfile.h"
#pragma section all "cpu0_dsram"
// 将本语句与#pragma section all restore语句之间的全局变量都放在CPU0的RAM中
#define PWM_DEFAULT_DUTY (100) // n*defult_duty
#define DIR_L (P02_4)
#define PWM_L (ATOM0_CH5_P02_5)
#define DIR_R (P02_6)
#define PWM_R (ATOM0_CH7_P02_7)

#define ENCODER_1 (TIM2_ENCODER)
#define ENCODER_1_A (TIM2_ENCODER_CH1_P33_7)
#define ENCODER_1_B (TIM2_ENCODER_CH2_P33_6)
#define ENCODER_3 (TIM4_ENCODER)
#define ENCODER_3_A (TIM4_ENCODER_CH1_P02_8)
#define ENCODER_3_B (TIM4_ENCODER_CH2_P00_9)
int16 TARGET_SPEED = 17;
int16 WHEEL_SPEED_L = 0;
int16 WHEEL_SPEED_R = 0;

#define LED1 (P20_9)
#define LED2 (P20_8)
#define LED3 (P21_5)
#define LED4 (P21_4)
int led_statu = 0;

// **************************** 代码区域 ****************************
int core0_main(void)
{
    clock_init(); // 获取时钟频率<务必保留>
    debug_init(); // 初始化默认调试串口
    // 此处编写用户代码 例如外设初始化代码等
    // 初始化IPS114液晶屏
    ips114_init();                                // 初始化IPS114液晶屏
    ips114_set_color(RGB565_WHITE, RGB565_BLACK); // 设置默认前景色和背景色
    ips114_clear();
    ips114_full(RGB565_WHITE);
    ips114_show_rgb565_image(0, 27, (const uint16 *)gImage_seekfree_logo, 240, 80, 240, 80, 0);
    system_delay_ms(800);
    ips114_clear();

    gpio_init(DIR_R, GPO, GPIO_HIGH, GPO_PUSH_PULL); // GPIO 初始化为输出 默认上拉输出高
    pwm_init(PWM_R, 17000, 0);                       // PWM 通道初始化频率 17KHz 占空比初始为 0
    gpio_init(DIR_L, GPO, GPIO_HIGH, GPO_PUSH_PULL); // GPIO 初始化为输出 默认上拉输出高
    pwm_init(PWM_L, 17000, 0);                       // PWM 通道初始化频率 17KHz 占空比初始为 0
    gpio_set_level(DIR_R, GPIO_HIGH);                // 右轮正转
    pwm_set_duty(PWM_R, 6 * PWM_DEFAULT_DUTY);       // 右轮占空比
    gpio_set_level(DIR_L, GPIO_HIGH);                // 左轮正转
    pwm_set_duty(PWM_L, 6 * PWM_DEFAULT_DUTY);       // 左轮占空比

    encoder_dir_init(ENCODER_1, ENCODER_1_A, ENCODER_1_B); // 编码器1初始化
    encoder_dir_init(ENCODER_3, ENCODER_3_A, ENCODER_3_B); // 编码器3初始化
    pit_ms_init(CCU60_CH0, 60);                            // PIT 定时器初始化 60ms中断一次 用于计算轮速

    gpio_init(LED1, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED1 输出 默认高电平 推挽输出模式
    gpio_init(LED2, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED2 输出 默认高电平 推挽输出模式
    gpio_init(LED3, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED3 输出 默认高电平 推挽输出模式
    gpio_init(LED4, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED4 输出 默认高电平 推挽输出模式
    pit_ms_init(CCU60_CH1, 110);                   // PIT 定时器初始化 20ms中断一次 用于 LED 灯闪烁

    // 此处编写用户代码 例如外设初始化代码等
    cpu_wait_event_ready(); // 等待所有核心初始化完毕
    system_delay_ms(1000);
    while (TRUE)
    {
        // 此处编写需要循环执行的代码
        system_delay_ms(10);
        // ips114_clear();
        ips114_show_string(0, 0, "L_speed:");
        ips114_show_string(0, 40, "R_speed:");
        ips114_show_string(0, 80, "Diff:");
        ips114_show_int(80, 0, WHEEL_SPEED_L, 5);  // 显示编码器1计数值
        ips114_show_int(80, 40, WHEEL_SPEED_R, 5); // 显示编码器3计数值
        ips114_show_int(80, 80, WHEEL_SPEED_L - WHEEL_SPEED_R, 5);
        // 爆艹PID

        // 此处编写需要循环执行的代码
    }
}

IFX_INTERRUPT(cc60_pit_ch0_isr, 0, CCU6_0_CH0_ISR_PRIORITY)
{
    interrupt_global_enable(0); // 开启中断嵌套
    pit_clear_flag(CCU60_CH0);
    WHEEL_SPEED_L = -1 * encoder_get_count(ENCODER_1) / 60;
    WHEEL_SPEED_R = encoder_get_count(ENCODER_3) / 60;
    encoder_clear_count(ENCODER_1);
    encoder_clear_count(ENCODER_3);
    int err_L = TARGET_SPEED - WHEEL_SPEED_L;
    int err_R = TARGET_SPEED - WHEEL_SPEED_R;
}

IFX_INTERRUPT(cc60_pit_ch1_isr, 0, CCU6_0_CH1_ISR_PRIORITY)
{
    interrupt_global_enable(0); // 开启中断嵌套
    pit_clear_flag(CCU60_CH1);
    led_statu++;
    if (led_statu == 1)
    {
        gpio_toggle_level(LED2);
    }
    else if (led_statu == 2)
    {
        gpio_toggle_level(LED1);
    }
    else if (led_statu == 3)
    {
        gpio_toggle_level(LED3);
    }
    else if (led_statu == 4)
    {
        gpio_toggle_level(LED4);
    }
    else if (led_statu == 5)
    {
        led_statu = 0;
    }
}

#pragma section all restore
// **************************** 代码区域 ****************************
