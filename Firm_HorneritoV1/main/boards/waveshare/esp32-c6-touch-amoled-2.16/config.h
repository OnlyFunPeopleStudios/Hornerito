#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// 48000 (antes 24000): con salida a 24 kHz la música perdía 12-22 kHz y el
// recorte 44100->24000 plegaba esa banda dentro de lo audible ("de lata").
// 48000 en vez de 44100 porque opus (TTS/notificaciones) solo acepta
// 8/12/16/24/48 kHz. Entrada y salida van juntas: TX std y RX TDM comparten
// WS/BCLK (box_audio_codec.cc asserta input == output).
#define AUDIO_INPUT_SAMPLE_RATE  48000
#define AUDIO_OUTPUT_SAMPLE_RATE 48000

#define AUDIO_INPUT_REFERENCE    false
#define AUDIO_I2S_GPIO_MCLK  GPIO_NUM_19
#define AUDIO_I2S_GPIO_WS    GPIO_NUM_22
#define AUDIO_I2S_GPIO_BCLK  GPIO_NUM_20
#define AUDIO_I2S_GPIO_DIN   GPIO_NUM_21
#define AUDIO_I2S_GPIO_DOUT  GPIO_NUM_23

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_8
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_7
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO        GPIO_NUM_9
// Tecla derecha "+/KEY" del esquemático oficial (Waveshare): GPIO10, libre.
#define KEY_BUTTON_GPIO         GPIO_NUM_10


#define LCD_CS       GPIO_NUM_15
#define LCD_PCLK     GPIO_NUM_0
#define LCD_D0       GPIO_NUM_1
#define LCD_D1       GPIO_NUM_2
#define LCD_D2       GPIO_NUM_3
#define LCD_D3       GPIO_NUM_4
#define LCD_RST      GPIO_NUM_NC

#define TP_RST_GPIO GPIO_NUM_11
#define TP_INT_GPIO GPIO_NUM_5

#define LCD_H_RES 480
#define LCD_V_RES 480
 
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#endif // _BOARD_CONFIG_H_
