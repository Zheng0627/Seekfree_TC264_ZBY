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
 ********************************************************************************************************************/
#include "zf_common_headfile.h"
#pragma section all "cpu0_dsram"
// 将本语句与#pragma section all restore语句之间的全局变量都放在CPU0的RAM中

// 有刷电机相关配置
// 根据实际的电机连接情况更改L/R轮的DIR和PWM引脚定义
#define PWM_DEFAULT_DUTY (100) // n乘这个数换算成常见的百分比占空比
#define PWM_BASE_DUTY (6 * PWM_DEFAULT_DUTY)
#define DIR_L (P02_4)
#define PWM_L (ATOM0_CH5_P02_5)
#define DIR_R (P02_6)
#define PWM_R (ATOM0_CH7_P02_7)
// PID 控制配置
#define PID_SAMPLE_MS (60) // 与 CCU60_CH0 定时器周期保持一致
#define PID_SAMPLE_S (PID_SAMPLE_MS / 1000.0f)
#define PID_INT_LIMIT (400.0f) // 积分限幅，避免风up
typedef struct
{
    float kp;
    float ki;
    float kd;
    float integral;
    float prev_err;
    float integral_limit;
} pid_ctrl_t;

static inline float clamp_float(float val, float min, float max)
{
    if (val < min)
    {
        return min;
    }
    if (val > max)
    {
        return max;
    }
    return val;
}

static inline uint32_t pid_run(pid_ctrl_t *pid, float target, float measure, float base_duty)
{
    float err = target - measure;
    pid->integral = clamp_float(pid->integral + err * PID_SAMPLE_S, -pid->integral_limit, pid->integral_limit);
    float derivative = (err - pid->prev_err) / PID_SAMPLE_S;
    pid->prev_err = err;

    float output = base_duty + (pid->kp * err) + (pid->ki * pid->integral) + (pid->kd * derivative);
    output = clamp_float(output, 0.0f, (float)PWM_DUTY_MAX);
    return (uint32_t)output;
}

static pid_ctrl_t pid_left = {.kp = 20.1f, .ki = 5.0f, .kd = 2.0f, .integral = 0.0f, .prev_err = 0.0f, .integral_limit = PID_INT_LIMIT};
static pid_ctrl_t pid_right = {.kp = 22.0f, .ki = 10.0f, .kd = 1.0f, .integral = 0.0f, .prev_err = 0.0f, .integral_limit = PID_INT_LIMIT};
// 将本语句与#pragma section all restore语句之间的全局变量都放在CPU0的RAM中
#pragma section all "cpu0_dsram"
// 编码器配置
#define ENCODER_1 (TIM2_ENCODER)
#define ENCODER_1_A (TIM2_ENCODER_CH1_P33_7)
#define ENCODER_1_B (TIM2_ENCODER_CH2_P33_6)
#define ENCODER_3 (TIM4_ENCODER)
#define ENCODER_3_A (TIM4_ENCODER_CH1_P02_8)
#define ENCODER_3_B (TIM4_ENCODER_CH2_P00_9)
int16 TARGET_SPEED = 21; // 目标速度 后续PID/LQR控制会用到
int16 WHEEL_SPEED_L = 0; // 左轮速度
int16 WHEEL_SPEED_R = 0; // 右轮速度
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

int core0_main(void)
{
    clock_init(); // 获取时钟频率<务必保留>
    debug_init(); // 初始化默认调试串口

    // 初始化IPS114液晶屏
    ips114_init();
    ips114_set_color(RGB565_WHITE, RGB565_BLACK); // 设置默认前景色和背景色
    ips114_clear();
    ips114_full(RGB565_WHITE);
    ips114_show_rgb565_image(0, 27, (const uint16_t *)gImage_seekfree_logo, 240, 80, 240, 80, 0);
    system_delay_ms(800); // 显示逐飞logo 0.8秒
    ips114_clear();

    // 初始化MT9V03X摄像头
    mt9v03x_init();

    // 初始化有刷电机驱动相关引脚和PWM
    gpio_init(DIR_R, GPO, GPIO_HIGH, GPO_PUSH_PULL); // GPIO 初始化为输出 默认上拉输出高
    pwm_init(PWM_R, 17000, 0);                       // PWM 通道初始化频率 17KHz 占空比初始为 0
    gpio_init(DIR_L, GPO, GPIO_HIGH, GPO_PUSH_PULL); // GPIO 初始化为输出 默认上拉输出高
    pwm_init(PWM_L, 17000, 0);                       // PWM 通道初始化频率 17KHz 占空比初始为 0
    gpio_set_level(DIR_R, GPIO_HIGH);                // 右轮正转
    pwm_set_duty(PWM_R, PWM_BASE_DUTY);              // 右轮初始占空比
    gpio_set_level(DIR_L, GPIO_HIGH);                // 左轮正转
    pwm_set_duty(PWM_L, PWM_BASE_DUTY);              // 左轮初始占空比
    // 初始化编码器
    encoder_dir_init(ENCODER_1, ENCODER_1_A, ENCODER_1_B); // 编码器1初始化
    encoder_dir_init(ENCODER_3, ENCODER_3_A, ENCODER_3_B); // 编码器3初始化
    pit_ms_init(CCU60_CH0, 60);                            // PIT定时器中断 用于计算轮速

    // 初始化板载LED灯 低电平点亮
    gpio_init(LED1, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED1 输出 默认低电平 推挽输出模式
    gpio_init(LED2, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED2 输出 默认低电平 推挽输出模式
    gpio_init(LED3, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED3 输出 默认低电平 推挽输出模式
    gpio_init(LED4, GPO, GPIO_LOW, GPO_PUSH_PULL); // 初始化 LED4 输出 默认低电平 推挽输出模式
    pit_ms_init(CCU60_CH1, 110);                   // PIT定时器中断 用于流水点灯 但是没啥卵用

    // 初始化板载KEY
    gpio_init(KEY1, GPI, GPIO_HIGH, GPI_PULL_UP); // 初始化 KEY1 输入 默认高电平 上拉输入
    gpio_init(KEY2, GPI, GPIO_HIGH, GPI_PULL_UP); // 初始化 KEY2 输入 默认高电平 上拉输入
    gpio_init(KEY3, GPI, GPIO_HIGH, GPI_PULL_UP); // 初始化 KEY3 输入 默认高电平 上拉输入
    gpio_init(KEY4, GPI, GPIO_HIGH, GPI_PULL_UP); // 初始化 KEY4 输入 默认高电平 上拉输入

    // 拿来测试的PIT定时器
    pit_ms_init(CCU61_CH0, 20);

    cpu_wait_event_ready(); // 等待所有核心初始化完毕<务必保留>
    while (TRUE)
    {
        // ips114_show_int(0, 0, TARGET_SPEED, 3);   // 显示目标速度
        // ips114_show_int(0, 16, WHEEL_SPEED_L, 5); // 显示左轮速度
        // ips114_show_int(0, 32, WHEEL_SPEED_R, 5); // 显示右轮速度
    }
}

/*这个中断函数用来计算轮速
每60ms进入一次中断
*/
IFX_INTERRUPT(cc60_pit_ch0_isr, 0, CCU6_0_CH0_ISR_PRIORITY)
{
    interrupt_global_enable(0); // 开启中断嵌套
    pit_clear_flag(CCU60_CH0);
    WHEEL_SPEED_L = -1 * encoder_get_count(ENCODER_1) / 60;
    WHEEL_SPEED_R = encoder_get_count(ENCODER_3) / 60;
    encoder_clear_count(ENCODER_1);
    encoder_clear_count(ENCODER_3);
    uint32_t duty_left = pid_run(&pid_left, (float)TARGET_SPEED, (float)WHEEL_SPEED_L, (float)PWM_BASE_DUTY);
    uint32_t duty_right = pid_run(&pid_right, (float)TARGET_SPEED, (float)WHEEL_SPEED_R, (float)PWM_BASE_DUTY);
    pwm_set_duty(PWM_L, duty_left);
    pwm_set_duty(PWM_R, duty_right);
}

/*这个中断函数用来点灯
不知道有什么用 但是很爽:>
*/
IFX_INTERRUPT(cc60_pit_ch1_isr, 0, CCU6_0_CH1_ISR_PRIORITY)
{
    interrupt_global_enable(0); // 开启中断嵌套
    pit_clear_flag(CCU60_CH1);
    led_statu++;
    if (led_statu == 1)
    {
        gpio_toggle_level(LED1);
    }
    else if (led_statu == 2)
    {
        gpio_toggle_level(LED2);
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

IFX_INTERRUPT(cc61_pit_ch0_isr, 0, CCU6_1_CH0_ISR_PRIORITY)
{
    interrupt_global_enable(0); // 开启中断嵌套
    pit_clear_flag(CCU61_CH0);
    if (mt9v03x_finish_flag)
    {
        ips114_displayimage03x((const uint8 *)mt9v03x_image, MT9V03X_W, MT9V03X_H);                                          // 显示原始图像
        ips114_show_gray_image(MT9V03X_W, 0, (const uint8 *)mt9v03x_image, MT9V03X_W, MT9V03X_H, MT9V03X_W, MT9V03X_H, 100); // 显示灰度图像
        mt9v03x_finish_flag = 0;
    }
}

#pragma section all restore
// **************************** 代码区域 ****************************
