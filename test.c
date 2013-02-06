#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "wiringPiSPI.h"
#include "wiringPi.h"
#include "bit_array.h"
#include "font.h"

#define COMMAND 0x4
#define RD 0x6
#define WR 0x5
#define SYS_DIS 0x00
#define COMMON_8NMOS 0x20
#define COMMON_8PMOS 0x28
#define MASTER_MODE 0x14
#define INT_RC 0x18
#define SYS_EN 0x01
#define LED_ON 0x03
#define LED_OFF 0x02
#define PWM_CONTROL 0xA0
#define BLINK_ON 0x09
#define BLINK_OFF 0x08

#define for_x for (x = 0; x < w; x++)
#define for_y for (y = 0; y < h; y++)
#define for_xy for_x for_y



int displays=4;
int height=8;
int width=32;
int font_width=8;
uint8_t *matrix;
uint8_t *matrix_buf;

void *reverse_endian(void *p, size_t size) {
  char *head = (char *)p;
  char *tail = head + size -1;

  for(; tail > head; --tail, ++head) {
    char temp = *head;
    *head = *tail;
    *tail = temp;
  }
  return p;
}



void print_byte(uint16_t x) {
  int n;

  for(n=0; n<8; n++){

    if (n%4==0)
      printf(" ");
    if ((x &0x80) != 0)
      printf("1");
    else
      printf("0");

    x = x <<1;
  }
}


void print_word(uint16_t x)
{
  print_byte(x>>8);
  print_byte(x);

}


void selectChip(int chip)
{
  switch (chip) {
  case 0:
 digitalWrite(0,0);
 digitalWrite(1,0);

    break;

  case 1:

 digitalWrite(0,0); 
digitalWrite(1,1);

    break;

  case 2:
 digitalWrite(0,1);
 digitalWrite(1,0);
    break;

  case 3:
 digitalWrite(0,1);
 digitalWrite(1,1);
    break;


  }


}


void random_screen(int chip, uint8_t fill){
  unsigned char matrix[34];
  int i;
  matrix[0] = 0b10100000;
  matrix[1] = 0b00111111;

  for (i=2;i<34;i++){
    matrix[i]=rand()%255;
  }
  wiringPiSPIDataRW(chip,matrix,34);

}


void clear_matrix()
{
  memset(matrix,0,width*height*displays/8);
}

void print_display(int num) {
  int x, y;
  uint8_t n;

  for (x=0; x < width; x++) {
    n = *(matrix+x+(num*width));
    for (y=0; y < height; y++) {

      if ((n &0x80) != 0)
	printf("=");
      else
	printf(" ");

      n = n <<1;

    }
    printf("\n");
  }

}

void write_screen(int chip, uint8_t *screen){

  uint8_t i;
  int size = width * height / 8;
  uint8_t *output = malloc(size+2);
  uint16_t data;
  uint8_t write;

  *output = 0b10100000;
  *(output+1) = 0;


  
  bitarray_copy(screen, 0, width*height, (output+1), 2);
  selectChip(chip);
  wiringPiSPIDataRW(0,output,size+1);
  
  data = WR;
  data <<= 7;
  data |= 63; //last address on screen
  data <<= 4;
  write = (0x0f & *(screen+31));
  data |= write;
  data <<= 2;
  //print_word(data);
  //printf(" = %d  \n",i*2+1); 
  reverse_endian(&data, sizeof(data));
  selectChip(chip);
   wiringPiSPIDataRW(0, &data, 2);


  /*
  for (i = 0; i< 32; i++)
    {
    
  data = WR;
  data <<= 7;
  data |= i*2;
  data <<= 4;
  write = (0xf0 & *(screen+i));
  write >>=4;

  data |= write;
  data <<= 2;
  //print_word(data);
  //printf(" = %d  \n",i*2);   
reverse_endian(&data, sizeof(data));

 selectChip(chip);
  wiringPiSPIDataRW(0, &data, 2);

  data = WR;
  data <<= 7;
  data |= i*2+1;
  data <<= 4;
  write = (0x0f & *(screen+i));
  data |= write;
  data <<= 2;
  //print_word(data);
  //printf(" = %d  \n",i*2+1); 
  reverse_endian(&data, sizeof(data));
  selectChip(chip);
   wiringPiSPIDataRW(0, &data, 2);

    }
  */  
}


void write_matrix() {
  int i;
  int size = width * height / 8;
  for(i=0;i<displays; i++) {
    write_screen(i, matrix + (size*i));
  }
}


void scroll_matrix() {
  int i,n;

  for (i=0; i < 128; i++) {
    for (n=0; n<128; n++) {
      if (n >= (128-i-2))
	*(matrix+n) = 0;
      else
	*(matrix+n) = *(matrix + n + 1);
    }
    //    print_display(0);
    write_matrix();
    usleep(50000);
  }
}


void scroll_matrix_once(int offset) {
  int i,n;
  int len = width * displays;


  for (n=0; n < (len-1); n++) {
 	*(matrix+n) = *(matrix + n + 1);
  }
  *(matrix+n) = *(matrix_buf + offset);

  write_matrix();
  usleep(12500);
}

void draw_pixel(uint8_t x, uint8_t y, uint8_t color) {
  if (y >= height) return;
  if (x >= width*displays) return;


  uint8_t mask;
  if (color) {
    *(matrix + x) = *(matrix + x) | (1 << y); // find the bit in the byte that needs to be turned on;
  }else {
   *(matrix + x) = *(matrix + x) & ~(1 << y); // find the bit in the byte that needs to be turned on;
  }



}

void draw_char(char c, int offset, uint8_t *buf) {
  int row, col,x,y;
  
     for (row=0; row<8; row++) {
	uint8_t z = font_data[c][row];
	for(col=0; col<8; col++) {
	  x = offset * font_width + col;
	  y = 8 - row;

	  if ((z &0x80) != 0) {
	    *(buf + x) = *(buf + x) | (1 << y); // find the bit in the byte that needs to be turned on;
	  }else {
	    *(buf + x) = *(buf + x) & ~(1 << y); // find the bit in the byte that needs to be turned on;
	  }

	  //draw_pixel(i*8+col,8-row,1);
	
	  z = z <<1;
	}
     }
}

void write_message(char *message) {
  int max_len = width*displays/font_width;
  int i, pix;
  int row;
  int col;
  int msg_len = strlen(message);
  int tot_len = msg_len + max_len;
  int delta = msg_len - max_len;

  for (i=0; i < msg_len; i++ ) {
 
      char c = message[i];
      draw_char(c, 0, matrix_buf);
      for (pix = 0; pix < font_width; pix++){
	scroll_matrix_once(pix);
      }
      // }
  }

}


void evolve(void *u, int w, int h)
{
  int x,y,y1,x1;
  unsigned (*univ)[w] = u;
  unsigned new[h][w];

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      int n = 0;
      //printf("x: %d y: %d \n", x, y);
      for( y1 = y - 1; y1 <= y + 1; y1++)
	for ( x1 = x - 1; x1 <= x + 1; x1++){
	  //printf(" x1: %d y1: %d [%d][%d] \n", x1, y1,(y1 + h) % h,(x1 + w) % w);
	  if (univ[(y1 + h) % h][(x1 + w) % w])
	    n++;
	}
      if (univ[y][x]) n--;
      new[y][x] = (n == 3 || (n == 2 && univ[y][x]));
    }
  }
  
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) { 
      univ[y][x] = new[y][x]; 
    }
  }
}

void show_game(void *u, int w, int h)
{
  int x,y;

  int (*univ)[w] = u;
  for (x = 0; x < w; x++) {
    for (y = 0; y < h; y++) {
      
      draw_pixel(x, y, univ[y][x]);
    }
  }
  write_matrix();
}

void game(int w, int h) {
  int x,y,i;
  unsigned univ[h][w];
  while(1) {
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) { 
      univ[y][x] = rand() < RAND_MAX / 10 ? 1 : 0; 
    }
  }
    show_game(univ, w, h);
  sleep(1);
  for (i=0; i<60; i++) {

    evolve(univ, w, h);
    show_game(univ, w, h);
    usleep(300000);
  }
  }

}


void sendcommand(int chip, uint8_t cmd) {
  uint16_t data=0;
  uint8_t temp;
  int err;
  uint8_t *access;

  data = COMMAND;

  data <<= 8;
  data |= cmd;
  data <<= 5;
 

  reverse_endian(&data, sizeof(data));
 

  selectChip(chip);
   wiringPiSPIDataRW(0, &data, 2);
 
}


void blink(int chip, int blinky) {
  if (blinky)
    sendcommand(chip, BLINK_ON);
  else
    sendcommand(chip, BLINK_OFF);
}

void set_brightness(int chip, uint8_t pwm) {
  if (pwm > 15)
{
 pwm = 15;
 }
  
  sendcommand(chip, PWM_CONTROL | pwm);
}




int main(void) {
  int ce = 0;
  int cs2 = 1;
  int i;

  unsigned char test;
  unsigned char BITMASK = (unsigned char) 0xff;
  unsigned char writeCommand[2];
  srand(time(NULL));

  matrix = (uint8_t *)malloc(displays * width * height / 8);
  matrix_buf = (uint8_t *)malloc(font_width);

if (wiringPiSPISetup(0, 2560000) <0)
  printf ( "SPI Setup Failed: %s\n", strerror(errno));





 if (wiringPiSetup() == -1)
   exit(1);
 pinMode(0, OUTPUT);
 pinMode(1, OUTPUT);



 for (i=0; i < displays; i++) {
 sendcommand(i, SYS_EN);
 sendcommand(i, LED_ON);
 sendcommand(i, MASTER_MODE);
 sendcommand(i, INT_RC);
 sendcommand(i, COMMON_8NMOS);
 blink(i, 0);
 set_brightness(i,15);
 }
 

 
 
 
   game(width * displays, height);
 // write_message("Merry Christmas and a Happy New Year! I really hope this works and looks cool. Let's go eat some ice cream cones.");
 // write_message("Good Morning Carolyn & Elena! Are you ready for a fun day? Perhaps a trip to the library?");




}
