#include "rf24g_key.h"

#if 1

static volatile u8 rf24g_rx_flag = 0;       // 是否收到了新的数据
volatile rf24g_recv_info_t rf24g_recv_info; // 存放接收到的数据包
volatile u8 chromatic_circle_val = 0;       // 存放色环按键对应的数值，范围：0x00~0xFF
volatile u8 rf24g_key_val = NO_KEY;         // 存放按键键值

const u8 rf24g_key_event_table[][RF34G_KEY_EVENT_MAX + 1] = {
    // 0,
    {RF24G_KEY_ON_OFF, RF24G_KEY_EVENT_ON_OFF_CLICK, RF24G_KEY_EVENT_ON_OFF_HOLD, RF24G_KEY_EVENT_ON_OFF_LOOSE},

    // 色环按键
    {RF24G_KEY_CHROMATIC_CIRCLE, RF24G_KEY_EVENT_CHROMATIC_CIRCLE_CLICK, RF24G_KEY_EVENT_CHROMATIC_CIRCLE_HOLD, RF24G_KEY_EVENT_CHROMATIC_CIRCLE_LOOSE},
    // 亮度加
    {RF24G_KEY_BRIGHTNESS_ADD, RF24G_KEY_EVENT_BRIGHTNESS_ADD_CLICK, RF24G_KEY_EVENT_BRIGHTNESS_ADD_HOLD, RF24G_KEY_EVENT_BRIGHTNESS_ADD_LOOSE},
    // 亮度减
    {RF24G_KEY_BRIGHTNESS_SUB, RF24G_KEY_EVENT_BRIGHTNESS_SUB_CLICK, RF24G_KEY_EVENT_BRIGHTNESS_SUB_HOLD, RF24G_KEY_EVENT_BRIGHTNESS_SUB_LOOSE},
    // 动态速度加
    {RF24G_KEY_DYNAMIC_SPEED_ADD, RF24G_KEY_EVENT_DYNAMIC_SPEED_ADD_CLICK, RF24G_KEY_EVENT_DYNAMIC_SPEED_ADD_HOLD, RF24G_KEY_EVENT_DYNAMIC_SPEED_ADD_LOOSE},
    // 动态速度减
    {RF24G_KEY_DYNAMIC_SPEED_SUB, RF24G_KEY_EVENT_DYNAMIC_SPEED_SUB_CLICK, RF24G_KEY_EVENT_DYNAMIC_SPEED_SUB_HOLD, RF24G_KEY_EVENT_DYNAMIC_SPEED_SUB_LOOSE},
    // 模式加
    {RF24G_KEY_MODE_ADD, RF24G_KEY_EVENT_MODE_ADD_CLICK, RF24G_KEY_EVENT_MODE_ADD_HOLD, RF24G_KEY_EVENT_MODE_ADD_LOOSE},
    // 模式减
    {RF24G_KEY_MODE_SUB, RF24G_KEY_EVENT_MODE_SUB_CLICK, RF24G_KEY_EVENT_MODE_SUB_HOLD, RF24G_KEY_EVENT_MODE_SUB_LOOSE},
    // 冷白亮度加，同时暖白亮度减
    {RF24G_KEY_COLD_WHITE_BRIGHTNESS_ADD, RF24G_KEY_EVENT_COLD_WHITE_BRIGHTNESS_ADD_CLICK, RF24G_KEY_EVENT_COLD_WHITE_BRIGHTNESS_ADD_HOLD, RF24G_KEY_EVENT_COLD_WHITE_BRIGHTNESS_ADD_LOOSE},
    // 冷白亮度减，同时暖白亮度加
    {RF24G_KEY_COLD_WHITE_BRIGHTNESS_SUB, RF24G_KEY_EVENT_COLD_WHITE_BRIGHTNESS_SUB_CLICK, RF24G_KEY_EVENT_COLD_WHITE_BRIGHTNESS_SUB_HOLD, RF24G_KEY_EVENT_COLD_WHITE_BRIGHTNESS_SUB_LOOSE},
    // 电机速度加
    {RF24G_KEY_MOTOR_SPEED_ADD, RF24G_KEY_EVENT_MOTOR_SPEED_ADD_CLICK, RF24G_KEY_EVENT_MOTOR_SPEED_ADD_HOLD, RF24G_KEY_EVENT_MOTOR_SPEED_ADD_LOOSE},
    // 电机速度减
    {RF24G_KEY_MOTOR_SPEED_SUB, RF24G_KEY_EVENT_MOTOR_SPEED_SUB_CLICK, RF24G_KEY_EVENT_MOTOR_SPEED_SUB_HOLD, RF24G_KEY_EVENT_MOTOR_SPEED_SUB_LOOSE},
    // 流星速度加
    {RF24G_KEY_METEOR_SPEED_ADD, RF24G_KEY_EVENT_METEOR_SPEED_ADD_CLICK, RF24G_KEY_EVENT_METEOR_SPEED_ADD_HOLD, RF24G_KEY_EVENT_METEOR_SPEED_ADD_LOOSE},
    // 流星速度减
    {RF24G_KEY_METEOR_SPEED_SUB, RF24G_KEY_EVENT_METEOR_SPEED_SUB_CLICK, RF24G_KEY_EVENT_METEOR_SPEED_SUB_HOLD, RF24G_KEY_EVENT_METEOR_SPEED_SUB_LOOSE},

};

volatile u8 rf24g_key_driver_event = 0; // 由key_driver_scan() 更新
volatile u8 rf24g_key_driver_value = 0; // 由key_driver_scan() 更新

static u8 rf24g_get_key_value(void); // 获取按键键值的函数声明
volatile struct key_driver_para rf24g_scan_para = {
    .scan_time = RF24G_KEY_SCAN_TIME_MS,                                                     // 按键扫描频率, 单位: ms
    .last_key = NO_KEY,                                                                      // 上一次get_value按键值, 初始化为NO_KEY;
    .filter_time = RF24G_KEY_SCAN_FILTER_TIME_MS,                                            // 按键消抖延时;
    .long_time = RF24G_KEY_LONG_TIME_MS / RF24G_KEY_SCAN_TIME_MS,                            // 按键判定长按数量
    .hold_time = (RF24G_KEY_LONG_TIME_MS + RF24G_KEY_HOLD_TIME_MS) / RF24G_KEY_SCAN_TIME_MS, // 按键判定HOLD数量
    .click_delay_time = RF24G_KEY_SCAN_CLICK_DELAY_TIME_MS,                                  // 按键被抬起后等待连击延时数量
    .key_type = KEY_DRIVER_TYPE_RF24GKEY,
    .get_value = rf24g_get_key_value,
};

// 底层按键扫描，由 __resolve_adv_report() 调用
void rf24g_scan(u8 *recv_buff)
{
    rf24g_recv_info_t *p = (rf24g_recv_info_t *)recv_buff;
    if (p->header1 == RF24G_HEADER_1 && p->header2 == RF24G_HEADER_2)
    {
        // printf_buf(recv_buff, sizeof(rf24g_recv_info_t)); // 打印接收到的数据包

        // rf24g_recv_info = *p; // 结构体变量赋值

        if (p->key_1 == RF24G_KEY_CHROMATIC_CIRCLE) // 如果是色环按键
        {
            rf24g_key_val = p->key_1;
            chromatic_circle_val = p->key_2;
        }
        else
        {
            rf24g_key_val = p->key_2;
        }

        rf24g_rx_flag = 1;
    }
}

static u8 rf24g_get_key_value(void)
{
    u8 key_value = 0;
    static u8 time_out_cnt = 0; // 加入超时，防止丢包（超时时间与按键扫描时间有关）
    static u8 last_key_value = 0;

    if (rf24g_rx_flag == 1) // 收到2.4G广播
    {
        rf24g_rx_flag = 0;

        // if (rf24g_recv_info.header1 == RF24G_HEADER_1 &&
        //     rf24g_recv_info.header2 == RF24G_HEADER_2)
        {
            // if (rf24g_recv_info.key_1 == RF24G_KEY_CHROMATIC_CIRCLE)
            // {
            //     // 如果是色环按键
            //     chromatic_circle_val = rf24g_recv_info.key_2; // 获取色环对应的数值
            //     key_value = rf24g_recv_info.key_1;            // 存放键值
            // }
            // else
            // {
            //     // 如果是其他按键
            //     key_value = rf24g_recv_info.key_2; // 存放键值
            // }
            key_value = rf24g_key_val;

            time_out_cnt = 30; // 2.4G接收可能会丢失100~200ms的数据包（响应会慢一些）
            last_key_value = key_value;

            // rf24g_recv_info.key_v = NO_KEY;
            return last_key_value;
        }
    }

    if (time_out_cnt != 0)
    {
        time_out_cnt--;
        return last_key_value;
    }

    return NO_KEY;
}

// 根据按键键值和key_driver_scan得到的事件值，转换为对应的按键事件
u8 rf24g_convert_key_event(u8 key_value, u8 key_driver_event)
{
    // 将key_driver_scan得到的key_event转换成自定义的key_event对应的索引
    // 索引对应 rf24g_key_event_table[][] 中的索引
    u8 key_event_index = 0; // 默认为0，0对应无效索引
    if (KEY_EVENT_CLICK == key_driver_event)
    {
        key_event_index = 1;
    }
    else if (KEY_EVENT_LONG == key_driver_event || KEY_EVENT_HOLD == key_driver_event)
    {
        // long和hold都当作hold处理
        key_event_index = 2;
    }
    else if (KEY_EVENT_UP == key_driver_event)
    {
        // 长按后松手
        key_event_index = 3;
    }

    if (0 == key_event_index || NO_KEY == key_value)
    {
        // 按键事件与上面的事件都不匹配
        // 得到的键值是无效键值
        return RF24G_KEY_EVENT_NONE;
    }

    for (u8 i = 0; i < sizeof(rf24g_key_event_table) / sizeof(rf24g_key_event_table[0]); i++)
    {
        if (key_value == rf24g_key_event_table[i][0])
        {
            return rf24g_key_event_table[i][key_event_index];
        }
    }

    // 如果运行到这里，都没有找到对应的按键，返回无效按键事件
    return RF24G_KEY_EVENT_NONE;
}

void rf24_key_handle(void)
{
    u8 rf24g_key_event = 0;

    if (NO_KEY == rf24g_key_driver_value)
        return;

    rf24g_key_event = rf24g_convert_key_event(rf24g_key_driver_value, rf24g_key_driver_event);
    rf24g_key_driver_value = NO_KEY;

    switch (rf24g_key_event)
    {
        // 收到短按、长按后松手，再执行对应的功能

    case RF24G_KEY_EVENT_ON_OFF_CLICK: // 短按 -- 开机
    {
        printf("key event on/off click\n");

        soft_turn_on_the_light();
    }
    break;
    case RF24G_KEY_EVENT_ON_OFF_HOLD: // 长按 -- 关机
    {
        printf("key event on/off hold\n");

        soft_turn_off_lights();
    }
    break;
        // case RF24G_KEY_EVENT_ON_OFF_LOOSE:
        // {
        //     printf("key event on/off loose\n");
        // }
        // break;

        // case RF

    } // switch (rf24g_key_event)
}
#endif
