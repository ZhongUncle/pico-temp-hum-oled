//
// ZhongUncle 2023-08-28
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "font.h"

//根据你SSD1306的规格设置SSD1306_HEIGHT为32或64
#define SSD1306_HEIGHT              64
#define SSD1306_WIDTH               128

#define SSD1306_I2C_ADDR            _u(0x3C)

//SSD1306的I2C时钟速率（CLK），一般是400000（40MHz），但是会有频闪现象，为了减缓频闪和提升响应速度，建议使用1000000（100MHz）
#define SSD1306_I2C_CLK             1000000

//SSD1306的命令（更多可见SSD1306数据表 https://www.digikey.com/htmldatasheets/production/2047793/0/0/1/ssd1306.html）
#define SSD1306_SET_MEM_MODE        _u(0x20)
#define SSD1306_SET_COL_ADDR        _u(0x21)
#define SSD1306_SET_PAGE_ADDR       _u(0x22)
#define SSD1306_SET_HORIZ_SCROLL    _u(0x26)
#define SSD1306_SET_SCROLL          _u(0x2E)

#define SSD1306_SET_DISP_START_LINE _u(0x40)

#define SSD1306_SET_CONTRAST        _u(0x81)
#define SSD1306_SET_CHARGE_PUMP     _u(0x8D)

#define SSD1306_SET_SEG_REMAP       _u(0xA0)
#define SSD1306_SET_ENTIRE_ON       _u(0xA4)
#define SSD1306_SET_ALL_ON          _u(0xA5)
#define SSD1306_SET_NORM_DISP       _u(0xA6)
#define SSD1306_SET_INV_DISP        _u(0xA7)
#define SSD1306_SET_MUX_RATIO       _u(0xA8)
#define SSD1306_SET_DISP            _u(0xAE)
#define SSD1306_SET_COM_OUT_DIR     _u(0xC0)
#define SSD1306_SET_COM_OUT_DIR_FLIP _u(0xC0)

#define SSD1306_SET_DISP_OFFSET     _u(0xD3)
#define SSD1306_SET_DISP_CLK_DIV    _u(0xD5)
#define SSD1306_SET_PRECHARGE       _u(0xD9)
#define SSD1306_SET_COM_PIN_CFG     _u(0xDA)
#define SSD1306_SET_VCOM_DESEL      _u(0xDB)

//页高
#define SSD1306_PAGE_HEIGHT         _u(8)
//页的数量
#define SSD1306_NUM_PAGES           (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
//页面内容的缓冲区域，这部分内容在渲染之后会显示到屏幕上
#define SSD1306_BUF_LEN             (SSD1306_NUM_PAGES * SSD1306_WIDTH)

#define SSD1306_WRITE_MODE         _u(0xFE)
#define SSD1306_READ_MODE          _u(0xFF)

//渲染区域的结构体
struct render_area {
    uint8_t start_col;
    uint8_t end_col;
    uint8_t start_page;
    uint8_t end_page;

    int buflen;
};

//计算渲染区域的长度
void calc_render_area_buflen(struct render_area *area) {
    
    area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

void SSD1306_send_cmd(uint8_t cmd) {
    // I2C write process expects a control byte followed by data
    // this "data" can be a command or data to follow up a command
    // Co = 1, D/C = 0 => the driver expects a command（控制为1，数据或命令为0）
    //I2C写入进程需要一个控制字节和数据。数据可以也是一个命令或者跟着命令的数据。比如说
    uint8_t buf[2] = {0x80, cmd};
    //第一个参数表示用的哪个i2c控制器，i2c0或i2c1。这里默认为i2c0
    //第二个参数是要读取的设备的7位地址。这个前面定义宏的时候设置了，为0x3C
    //第三个参数是指向接收数据的缓冲的指针。你可能会疑问上面不是数组吗？C里面数组名其实就是指针）
    //第四个参数是接收数据的长度（单位为字节）。这里就为2
    //第五个参数如果是真，那么主机在交换数据之后保持对bus的控制。这里为false
    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, buf, 2, false);
}

void SSD1306_send_cmd_list(uint8_t *buf, int num) {
    for (int i=0;i<num;i++)
        SSD1306_send_cmd(buf[i]);
}

void SSD1306_send_buf(uint8_t buf[], int buflen) {
    //水平地址模式下，列地址指针自动增加并且包括下一个页，所以可以一个交换就可以发送包含整个帧的缓冲
    //复制帧缓冲到一个新的缓冲中是为了在开始添加一个控制字节
    uint8_t *temp_buf = malloc(buflen + 1);

    temp_buf[0] = 0x40;
    memcpy(temp_buf+1, buf, buflen);

    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, temp_buf, buflen + 1, false);

    free(temp_buf);
}

void SSD1306_init() {
    /* 这里是重置进程为默认情况的完整流程，但是不同厂商的可能不一样 */
    uint8_t cmds[] = {
        SSD1306_SET_DISP,               //关闭显示器
        /* 内存映射 */
        SSD1306_SET_MEM_MODE,           //设置内存地址模式。0为横向地址模式，1为纵向地址模式，2为页地址模式
        0x00,                           //设置为横向寻址模式
        /* 分辨率和布局 */
        SSD1306_SET_DISP_START_LINE,    //设置显示的开始行为0（后面没有设置参数就为0）
        SSD1306_SET_SEG_REMAP | 0x01,   //重新映射分区，列地址127被映射到SEG0
        SSD1306_SET_MUX_RATIO,          //设置多路传输速率
        SSD1306_HEIGHT - 1,             //显示为高度减去1（因为这里是从0开始的）
        SSD1306_SET_COM_OUT_DIR | 0x08, //输出扫描方向。这里是从底部往上扫描，也就是COM[N-1]到COM0
        SSD1306_SET_DISP_OFFSET,        //设置显示的偏移量
        0x00,                           //设置为无偏移
        SSD1306_SET_COM_PIN_CFG,        //设置COM（common）针脚硬件配置。板子会指定一个特殊值

/* 128x32分辨率会使用0x02，128x64分辨率可以使用0x12，如果不能正常工作，那么使用0x22或0x32 */
#if ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 32))
        0x02,
#elif ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 64))
        0x12,
#else
        0x02,
#endif
        
        /* 计时和驱动规划 */
        SSD1306_SET_DISP_CLK_DIV,       //设置显示的时钟除法比（divide ratio，这个中文术语是啥？）
        0x80,                           //标准频率中1的除法比
        SSD1306_SET_PRECHARGE,          //设置每次交换的周期
        0xF1,                           //板子上产生的Vcc
        SSD1306_SET_VCOM_DESEL,         //设置VCOMH取消级别
        0x30,                           //0.83xVcc
        /* 显示 */
        SSD1306_SET_CONTRAST,           //设置对比度
        0xFF,                            //设置为满的0xFF
        SSD1306_SET_ENTIRE_ON,          //设置整个显示器来跟随RAM内容（也就是显示 RAM的内容）
        SSD1306_SET_NORM_DISP,          //设置常规显示（不是颠倒的）
        SSD1306_SET_CHARGE_PUMP,        //设置充电泵 set charge pump
        0x14,                           //板子上产生的Vcc
        SSD1306_SET_SCROLL | 0x00,      //设置这个会停用水平滚动。这很重要，因为如果启用了滚动，那么当内存写入将出错
        SSD1306_SET_DISP | 0x01, //打开显示器
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
}

void render(uint8_t *buf, struct render_area *area) {
    //用*area更新显示器的某个区域
    uint8_t cmds[] = {
        SSD1306_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1306_SET_PAGE_ADDR,
        area->start_page,
        area->end_page
    };
    
    SSD1306_send_cmd_list(cmds, count_of(cmds));
    SSD1306_send_buf(buf, area->buflen);
}

/* 从字符集从获取字符ch的下标 */
static inline int GetFontIndex(uint8_t ch) {
    if (ch >= ' ' && ch <= 127) {
        return  ch - ' ';
    }
    else return  0; //如果没找到返回空格的下标
}

/* 输出一个字符ch到buf，起始位置为（x, y） */
static void WriteChar(uint8_t *buf, int16_t x, int16_t y, uint8_t ch) {
    //如果超出屏幕了
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

    //之前说过每一行其实是一页，一页的高度是8个像素，这里的y也是行的上界。
    y = y/8;
    
    //获取字符ch的下标
    int idx = GetFontIndex(ch);
    //这里是计算了实际初始位置，也就是第几行（y * 128）的第几个（x）
    int fb_idx = y * 128 + x;

    //输出字符ch的每一位
    for (int i=0;i<8;i++) {
        buf[fb_idx++] = font[idx * 8 + i];
    }
}

/* 输出多个字符就是输出字符串了 */
static void WriteString(uint8_t *buf, int16_t x, int16_t y, char *str) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

    while (*str) {
        WriteChar(buf, x, y, *str++);
        x+=8;
    }
}

const uint DHT_PIN = 16;
const uint MAX_TIMINGS = 85;

typedef struct {
    float humidity;
    float temperature;
} dht_reading;

void read_from_dht(dht_reading *result) {
    //信号值有40bit，也就是5个字节
    int data[5] = {0, 0, 0, 0, 0};
    //纪录上一种信号的类型
    uint last = 1;
    //纪录当前信号是第几位数据
    uint j = 0;
    
    /* Pico发送信号阶段 */
    //信号方向是DHT_PIN到GPIO_OUT，给DHT11发送一些信号。这里的dir是direction
    gpio_set_dir(DHT_PIN, GPIO_OUT);
    //给DHT_PIN输出低电平，并且维持18ms，这样让DHT11可以获取到这个信号
    gpio_put(DHT_PIN, 0);
    sleep_ms(18);
    //给DHT_PIN输出高电平，维持40us，用来等待 DHT11 的回应
    gpio_put(DHT_PIN,1);
    sleep_us(40);

    /* DHT发送信号阶段 */
    //反转信号方向，从DHT_PIN到GPIO_IN，这样来获取DHT11的数据
    gpio_set_dir(DHT_PIN, GPIO_IN);
    
    //这里开始获取和处理DHT11的输出的信号
    for (uint i = 0; i < MAX_TIMINGS; i++) {
        //count用来计时的，单位为255
        uint count = 0;
        //如果获取的电平与上一个信号的电平相同，那么一直循环，这个用来消耗上文提到的等待时间
        //一开始DHT11有80us的低电平来作为回复，所以第一次循环的时候last为1（高电平）就直接跳过循环了，然后last也变成了0（低电平）。这样第二次循环的时候就会进入下面的循环耗时间。DHT11拉高电平之后就重复一遍这个操作
        //后续DHT11信号前面的50us也是这样处理的
        while (gpio_get(DHT_PIN) == last) {
            //开始迭代计数进行计时
            count++;
            sleep_us(1);
            //如果当前时常已经超过100us，那么这次整个外层迭代跳过，但是由于break只能跳过一层迭代，所以外层还要break一次
            //选择任意大于信号最大周期的值即可，不一定是100
            if (count == 100)
                break;
        }
        //将当前电平赋值给DHT_PIN
        last = gpio_get(DHT_PIN);
        //这个判断可不是多余的，这是用来彻底跳出外层迭代。不然会出现先显示湿度和温度的整数部分，再显示湿度和温度的浮点部分。一次获取无法获取完整的数据
        if (count == 100)
            break;

        //这里开始获取数据了
        //i>=4是因为前三个循环里分别是回应保持低电平、拉高电平和第一个数据位开始的50us
        //i % 2 == 0是因为奇数次都是数据位开始的50us，没有存放的数据
        if ((i >= 4) && (i % 2 == 0)) {
            //j/8刚好可以表示这是第几个字节
            //右移一位，这样8bit的数据只用设置最右的一位即可
            data[j / 8] <<= 1;
            //如果当前信号超过35，那么将最右一位设置为1
            //这个35是因为26～28表示0，大于这个范围的便表示1了，但是由于通信时间可能需要一段时间，所以留了一些冗余
            if (count > 35)
                data[j / 8] |= 1;
            j++;
        }
    }

    //获取到40位数据并且校验和(data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))也是对的，那么将结果保存到result指针指向的结构体中
    if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) {
        result->humidity = (float) ((data[0] << 8) + data[1]) / 10;
        if (result->humidity > 100) {
            result->humidity = data[0];
        }
        result->temperature = (float) (((data[2] & 0x7F) << 8) + data[3]) / 10;
        if (result->temperature > 125) {
            result->temperature = data[2];
        }
        if (data[2] & 0x80) {
            result->temperature = -result->temperature;
        }
    } else {
        //如果数据不对那么打印debug信息
        printf("Bad data\n");
    }
}

int main() {
    stdio_init_all();
    /* 初始化 GPIO */
    gpio_init(DHT_PIN);
    
    /* 初始化默认的i2c控制器以及针脚 */
    i2c_init(i2c_default, SSD1306_I2C_CLK);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    
    /* 初始化SSD1306 */
    SSD1306_init();

    /* 初始化 frame_area 渲染区域（大小为SSD1306_WIDTH乘以SSD1306_NUM_PAGES） */
    struct render_area frame_area = {
        start_col: 0,
        end_col : SSD1306_WIDTH - 1,
        start_page : 0,
        end_page : SSD1306_NUM_PAGES - 1
    };
    
    /* 计算frame_area图像缓冲长度 */
    calc_render_area_buflen(&frame_area);
    
    /* 声明一个数组来存放缓冲，长度为上面的计算的 */
    uint8_t buf[SSD1306_BUF_LEN];
    /* 预先清空缓冲和屏幕 */
    //给这个数组每个元素分配为0（因为可能这个内存之前没有被清空，这样会显示花屏）
    memset(buf, 0, SSD1306_BUF_LEN);
    //然后渲染这个缓冲
    render(buf, &frame_area);
    
    //存放温度和湿度的字符串，方便后面渲染到屏幕上
    char temp[16];
    char hum[16];

    //第一次之前等待1000ms，不然可能第一次获取数据会失败
    sleep_ms(1000);
    
    /* 不断循环获取数据并且渲染到屏幕上 */
    while (true) {
        //声明一个dht_reading变量：reading，然后将从DHT11读取的数据存放到reading中
        dht_reading reading;
        read_from_dht(&reading);
        
        //给这个数组每个元素分配为0，不然上次显示的内容可能还在缓冲中，这样下次渲染显示会出问题
        memset(buf, 0, SSD1306_BUF_LEN);
        
        //生成字符串用来打印，分别存放到temp和hum字符数组中，然后再将其写入到缓冲数组buf中
        sprintf(temp, "temp = %.02f C", reading.temperature);
        //这个printf是给串口USB输出，后面演示可以看到
        printf("temp = %.02f C\n", reading.temperature);
        WriteString(buf, 0, 0, temp);
        sprintf(hum, "hum  = %.02f %%", reading.humidity);
        printf("hum  = %.02f %%\n", reading.humidity);
        //由于行高为8，所以第二行写入的y值要加8，当然你如果想让行距好看一些，可以设置为10，比如说这里
        WriteString(buf, 0, 8, hum);
        
        //渲染缓冲
        render(buf, &frame_area);
        
        //等待6000ms，因为DHT11的最小响应时间为6秒
        sleep_ms(6000);
    }
}
