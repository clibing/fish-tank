### 一、鱼缸

半自动的小型DIY鱼缸

### 二、功能实现

#### 功能介绍

* [ ] 吸粪便等杂物
* [ ] 夜间照明
* [ ] 供氧
* [ ] 太阳能供电
* [ ] 绿植
* [ ] 晒台
* [ ] 其他

#### 软件部分

* [ ] 自动配网
* [ ] 固件升级
* [ ] mqtt-ssl通道
* [ ] 指令下发
* [ ] 温度、湿度上报
* [ ] 今日天气

#### 电路周围

* [ ] OLED 显示如下
  + 温度、湿度
  + 水温
  + 今日天气
  + 当前运行状态(Running,Error)
* [ ] 温湿度模块
  采集当前空气下的温度、湿度
* [ ] 温敏电阻采集水温  
* [ ] 摄像头采集照片(Esp8266不支持,内存太小，cpu频率低等原因)

### 三、整体实现

* 3.1 所需物品

* 3.2 效果图
 

### 四、源码实现

* 4.1 物联控制端

```flow
power=>start: 加电
shutdown=>end: 关机
restart=>operation: 重启
nvs-init=>operation: nvs初始化
nvs-status=>condition: nvs初始化是否成功

open-led=>operation: 打开照明
danger-sys-led=>operation: 红灯点亮配网异常
danger-sc-led=>operation: 红灯点亮配网异常
danger-wifi-led=>operation: 红灯点亮配网异常
danger-mqtt-led=>operation: 红灯点亮配网异常

oled-init=>operation: 显示屏初始化
oled-draw=>operation: OLED显示

gpio-init=>operation: 按键gpio初始 (控制端GPIO在接入MQTT后在初始化)
btn-press=>condition: 配网按键是否被按下
btn-press_duration=>condition: 是否长按10秒
startconfig=>operation: 配网(StringConfig)
sc-status=>condition: 配网状态
sc-timeout=>condition: 配网超时

wifi-init=>subroutine: 网络初始
wifi-status=>condition: 网络接入状态
wifi-timeout=>condition: 联网超时

mqtt-init=>subroutine: 接入MQTT
mqtt-status=>condition: 接入状态
mqtt-sub=>subroutine: 订阅、设置掉线遗言

high-level=>inputoutput: 输出高电平
low-level=>inputoutput: 输出低电平
message-water=>condition: 接收到水泵消息
message-o2=>condition: 接收到供氧消息
message-timer=>condition: 当前时间
water-pump=>operation: 打开水泵
o2-pump=>operation: 打开供养
other-ext=>operation: 扩展业务

power->nvs-init->nvs-status
nvs-status(no)->danger-sys-led->shutdown
nvs-status(yes)->gpio-init->btn-press
btn-press(no, left)->wifi-init
btn-press(yes)->btn-press_duration
btn-press_duration(no)->open-led
btn-press_duration(yes)->startconfig->sc-status
sc-status(yes)->restart->power
sc-status(no)->danger-sc-led->sc-timeout
sc-timeout(no)->startconfig
sc-timeout(yes, left)->restart

wifi-init->wifi-status
wifi-status(no)->wifi-timeout->danger-wifi-led
wifi-status(yes)->mqtt-init


mqtt-init->mqtt-status
mqtt-status(no)->danger-mqtt-led
mqtt-status(yes)->mqtt-sub
mqtt-sub->message-water
message-water(yes)->water-pump
message-water(no)->message-o2
message-o2(yes)->o2-pump
message-o2(no)->message-timer
message-timer(yes)->oled-draw
message-timer(no)->other-ext

```



* 4.2 后台平台

* 4.3 相关资料

### 五、参考

* [sdk](https://github.com/espressif/ESP8266_RTOS_SDK)
* [document](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/?badge=latest)