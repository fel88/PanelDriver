#include "EPD_My_sdfat.h"
#include "GUI_Paint.h"
#include "EPD_SDCard.h"
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

#include <EEPROM.h>


#include <Wire.h>


bool screensRevert = false;


#include <Arduino.h>
#include <Wire.h>


sFONT* font = &Font12;

int fontHeight = 12;
//int cols=54;//35 for font24; 54 for font16;85 for font12
//int rows=22;//17 for font24; 27 for font16; 29 for font12(gap=3)
int cols = 80;  //35 for font24; 54 for font16;85 for font12
int rows = 25;  //17 for font24; 27 for font16; 29 for font12(gap=3)
int page = 1;
int pages = 778;
//int gap=2;
int gap = 3;
//int fontWidth=11;//11 for font16, 17 for font24, 7 for font12
int fontWidth = 7;  //11 for font16, 17 for font24, 7 for font12



UWORD Image_Width_Byte;
UWORD Bmp_Width_Byte;
int width;
int bookWidth;
int height;
int bookHeight;



extern SdFat sd;

#define MFILE_WRITE (O_RDWR | O_CREAT | O_AT_END)
#define MFILE_READ (O_RDONLY)


extern UBYTE _readBuff[801];



void fastDisplayBuffer() {
  EPD_5IN83_V2_Power(true);
  EPD_5IN83_V2_TurnOnDisplay();
}
void displayBuffer() {
  EPD_5IN83_V2_Display();
  //EPD_5IN83_Sleep();
}


void Paint_DrawString_Flow(UWORD Xstart, const unsigned char* pString,
                           sFONT* Font, UWORD Color_Background, UWORD Color_Foreground) {
  UWORD Xpoint = Xstart;


  if (Xstart > Paint_Image.Image_Width /*|| Ystart > Paint_Image.Image_Height*/) {
    DEBUG("Paint_DrawString_Flow Input exceeds the normal display range\r\n");
    return;
  }
  UBYTE Data_Black, Data;

  int indexb = 0;
  UBYTE accum = 0;
  for (int j = 0; j < Font->Height; j++)
    for (int i = 0; i < 81 * 8; i++) {
      int data = 0;
      if (i < Xstart || i >= (Xstart + strlen(pString) * Font->Width)) {
        data = WHITE;
      } else {

        data = WHITE;
        int symbolIdx = (i - Xstart) / Font->Width;
        int column = (i - Xstart) % Font->Width;
        char ch = pString[symbolIdx];
        int offset = (ch - ' ');
        if (ch > '~') {
          offset = ('~' - ' ') + (ch - 0xC0) + 1;
        }

        int Char_Offset = offset * Font->Height * (Font->Width / 8 + ((Font->Width % 8) != 0 ? 1 : 0));
        const unsigned char* ptr = &Font->table[Char_Offset];
        bool draw = false;
        UWORD Page, Column;
        for (Page = 0; Page < Font->Height; Page++) {
          for (Column = 0; Column < Font->Width; Column++) {
            if (Page == j && Column == column)
              draw = true;

            //To determine whether the font background color and screen background color is consistent
            if (FONT_BACKGROUND == Color_Background) {  //this process is to speed up the scan
              if (draw)
                if ((pgm_read_byte(ptr) & (0x80 >> (Column % 8))) != 0)

                {
                  //Paint_DrawPoint_Flow(Xpoint + Column, Ypoint + Page, Color_Foreground, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                  data = Color_Foreground;
                }
            } else {
              if (draw)
                if ((pgm_read_byte(ptr) & (0x80 >> (Column % 8))) != 0) {
                  //Paint_DrawPoint_Flow(Xpoint + Column, Ypoint + Page, Color_Foreground, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                  data = Color_Foreground;
                } else {
                  //Paint_DrawPoint_Flow(Xpoint + Column, Ypoint + Page, Color_Background, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                  data = Color_Background;
                }
            }
            //One pixel is 8 bits
            if (Column % 8 == 7) {
              ptr++;
            }

            if (draw)
              break;
          }
          // Write a line
          if (Font->Width % 8 != 0)
            ptr++;
          if (draw)
            break;
        }
      }

      //setBit(ref accum, indexb, data == BLACK ? 1 : 0);
      if (data == BLACK)
        accum |= (1 << (7 - indexb));
      indexb++;
      if (indexb == 8) {

        indexb = 0;
        EPD_5IN83_V2_SendData(accum);
        accum = 0;
      }
    }
}


void FillWhiteLine(int lines = 1) {


  for (int i = 0; i < Image_Width_Byte * lines; i++) {
    EPD_5IN83_V2_SendData(0x00);
  }
}
void FillBlackLine(int lines = 1) {


  for (int i = 0; i < Image_Width_Byte * lines; i++) {
    EPD_5IN83_V2_SendData(0xff);
  }
}



void setup() {


  width = EPD_5IN83_V2_WIDTH;
  height = EPD_5IN83_V2_HEIGHT;
  randomSeed(analogRead(A0));
  Image_Width_Byte = (width % 8 == 0) ? (width / 8) : (width / 8 + 1);

  // clock_prescale_set(clock_div_2);

  //ADCSRA = 0;
  ADCSRA &= ~(1 << ADEN);


  rows = (448 - 1) / (gap + fontHeight) - 1;
  cols = 600 / fontWidth;

  rows -= 5;
  cols -= 5;



  initShield();
  drawTestPage();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();
}

void initShield() {
  //digitalWrite(wemosPin,HIGH);
  //Serial3.println("sleep");


  //digitalWrite(wemosRstPin,HIGH);
  delay(500);
  DEV_Module_Init();

  EPD_5IN83_V2_Init();
  //EPD_5IN83_Clear();
  //Paint_NewImage(IMAGE_BW, EPD_5IN83_WIDTH, EPD_5IN83_HEIGHT, IMAGE_ROTATE_0, IMAGE_COLOR_INVERTED);
  Paint_NewImage(IMAGE_BW, EPD_5IN83_V2_WIDTH, EPD_5IN83_V2_HEIGHT, screensRevert ? IMAGE_ROTATE_180 : IMAGE_ROTATE_0, IMAGE_COLOR_INVERTED);
  /*SDCard_Init();

  if (i2ceeprom.begin(EEPROM_ADDR)) {  // you can stick the new i2c addr in here, e.g. begin(0x51);
    //Serial.println("Found I2C EEPROM");
  } else {
    //Serial.println("I2C EEPROM not identified ... check your connections?\r\n");
    while (1) delay(10);
  }*/
}

void preClear() {

  //2.Drawing on the image
  EPD_5IN83_V2_SendCommand(0x10);
  UBYTE ReadBuff[1] = { 0 };

  int Width = (EPD_5IN83_V2_WIDTH % 8 == 0) ? (EPD_5IN83_V2_WIDTH / 8) : (EPD_5IN83_V2_WIDTH / 8 + 1);
  int Height = EPD_5IN83_V2_HEIGHT;

  //EPD_5IN83_V2_SendCommand(0x10);
  for (int i = 0; i < Height; i++) {
    for (int j = 0; j < Width; j++) {
      EPD_5IN83_V2_SendData(0x00);
    }
  }
  EPD_5IN83_V2_SendCommand(0x13);
}

void flowDrawEnd() {

  //fastDisplayBuffer();
  EPD_5IN83_V2_TurnOnDisplay();
}





void drawTestPage() {


  EPD_5IN83_V2_Power(true);

  Paint_Clear(BLACK);
  preClear();
  int cntr = 0;


  Paint_DrawString_Flow(0, /*cntr * fontHeight,*/ "..", &Font12, WHITE, BLACK);

  cntr++;



  String filename = "hello world";

  Paint_DrawString_Flow(0, /*cntr * fontHeight,*/ filename.c_str(), &Font12, WHITE, BLACK);


  cntr++;





  //EPD_5IN83_V2_Display();
  FillWhiteLine(EPD_5IN83_V2_HEIGHT - cntr * 12);
  flowDrawEnd();
  EPD_5IN83_V2_Power(false);
}


void loop() {
}
