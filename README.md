# HPM WS2812 驱动库

- 本仓库为HPM的WS2812驱动库，目前仅在HPM5301上验证成功，理论上可用于所有HPM系列产品。

- 本驱动库支持DMA+定时器和SPI的方式驱动WS2812
  - DMA+定时器方式驱动WS2812需要占用一路DMA通道和一个通用定时器。
  - SPI方式驱动WS2812需要占用一路SPI通道，SPI作为主机，但只需要MOSI引脚。


## 使用方法

- 在主工程的`CMakeLists.txt`中使用`add_subdirectory`添加本库即可将本库添加到工程中。然后复制`Lib`目录下的`WS2812_Conf_Template.h`到工程的头文件目录下，并重命名为`WS2812_Conf.h`。在`WS2812_Conf.h`中配置WS2812的引脚、灯珠数量等参数即可。

- 如果需要使用SPI方式驱动，需要在`WS2812_Conf.h`中使能`WS2812_USE_SPI`宏为1，需要定义WS2812_SPI，WS2812_SPI_CLCOK，WS2812_DIN萨三个宏，分别为SPI通道、SPI时钟源、SPI数据MOSI输出引脚 