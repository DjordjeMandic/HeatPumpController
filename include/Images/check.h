#pragma once
#include <Arduino.h>

// 'check', 32x32px
const unsigned char checkSymbolMono [128] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x03, 0xf0, 
	0x00, 0x00, 0x07, 0xf8, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x3f, 0xf0, 
	0x00, 0x00, 0x7f, 0xe0, 0x02, 0x00, 0xff, 0xc0, 0x07, 0x01, 0xff, 0x80, 0x0f, 0x83, 0xff, 0x00, 
	0x1f, 0xc7, 0xfe, 0x00, 0x3f, 0xef, 0xfc, 0x00, 0x1f, 0xff, 0xf8, 0x00, 0x0f, 0xff, 0xf0, 0x00, 
	0x07, 0xff, 0xe0, 0x00, 0x03, 0xff, 0xc0, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0xff, 0x00, 0x00, 
	0x00, 0x7e, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
