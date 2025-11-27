// file: finals.c  (fixed version)

// ================== CLOCK ==================
#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h>

#define BAUD 9600

// ========== Pin 定義 ==========
#define TRIG1 PB0
#define ECHO1 PD2 // INT0
#define TRIG2 PC0
#define ECHO2 PD3 // INT1

#define LED_R PB1 // 紅：數位 on/off
#define LED_G PD6 // 綠：OC0A PWM
#define LED_B PD5 // 藍：OC0B PWM

#define BUZZ  PB2 // 蜂鳴器：數位 on/off

#define BTN   PC4 // PCINT12 按鈕（pull-up）

// ========== 參數 ==========
#define PING_INTERVAL_MS        70
#define DANGER_THRESHOLD_CM    50  // 危險閾值 (50cm)
#define WARNING_THRESHOLD_CM   100 // 警告閾值 (100cm)

// ---- Ultrasound 1 ----
volatile uint16_t echo1_start = 0, echo1_end = 0;
volatile bool echo1_done = false;

// ---- Ultrasound 2 ----
volatile uint16_t echo2_start = 0, echo2_end = 0;
volatile bool echo2_done = false;

// ---- Button ----
volatile bool btn_pressed_flag = false;

// ---- UART ----
static void uart_init(void){
#if (F_CPU == 1000000UL) && (BAUD == 9600)
    UCSR0A = (1<<U2X0); 
    UBRR0H = 0;
    UBRR0L = 12;          // 1e6/(8*9600)-1 ≈ 12
#elif (F_CPU == 8000000UL) && (BAUD == 115200)
    UCSR0A = (1<<U2X0); 
    UBRR0H = 0;
    UBRR0L = 8;           // 8e6/(8*115200)-1 ≈ 8
#else
    UCSR0A = 0;
    uint16_t ubrr = (uint16_t)(F_CPU/16UL/BAUD - 1UL);
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xFF);
#endif
    UCSR0B = (1<<TXEN0) | (1<<RXEN0);               // TX/RX
    UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);             // 8N1
}
    
    

static inline void uart_putc(char c){
  while(!(UCSR0A & (1<<UDRE0)));
  UDR0 = c;
}

static void uart_print(const char *s){
  while(*s) uart_putc(*s++);
}

static void uart_print_u16(uint16_t v){
  char b[8];
  itoa(v, b, 10);
  uart_print(b);
}

static inline char uart_getc(void) {
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

static void parse_and_update_config(char* buffer) {
    if (buffer[0] == 'W' && buffer[1] == ':') warning_threshold_cm = atoi(buffer + 2);
    if (buffer[0] == 'D' && buffer[1] == ':') danger_threshold_cm = atoi(buffer + 2);
}

// ================== Interrupts ==================

// Ultrasound echo INT0
ISR(INT0_vect){
  if (PIND & (1<<ECHO1)) {     // rising
    echo1_start = TCNT1;
  } else {                     // falling
    echo1_end = TCNT1;
    echo1_done = true;
  }
}

// Ultrasound echo INT1
ISR(INT1_vect){
  if (PIND & (1<<ECHO2)) {     // rising
    echo2_start = TCNT1;
  } else {                     // falling
    echo2_end = TCNT1;
    echo2_done = true;
  }
}

// Button PC4 pin-change
ISR(PCINT1_vect){
  static uint8_t last = 1;
  uint8_t now = (PINC & (1<<BTN)) ? 1 : 0; // pull-up: 1 idle, 0 pressed
  // detect falling edge (按下瞬間)
  if (last == 1 && now == 0){
    btn_pressed_flag = true;
  }
  last = now;
}

// ================== Helpers ==================

static inline void trig_pulse(volatile uint8_t *port, uint8_t pin){
  *port &= ~(1<<pin); _delay_us(2);
  *port |=  (1<<pin); _delay_us(10);
  *port &= ~(1<<pin);
}

// Timer1: 1 tick = 1us (在 1MHz 下)
static void t1_init_1us(void){
  TCCR1A = 0;
  TCCR1B = (1<<CS10); // no prescale, 1 tick = 1 us
}

// 58 us ≈ 1 cm
static inline uint16_t us_to_cm(uint16_t us){
  return (uint16_t)(us / 58);
}

// map 函數：線性映射
static uint16_t map_u16(uint16_t x,
                        uint16_t in_min, uint16_t in_max,
                        uint16_t out_min, uint16_t out_max)
{
  if (x <= in_min) return out_min;
  if (x >= in_max) return out_max;
  uint32_t num = (uint32_t)(x - in_min) * (out_max - out_min);
  return (uint16_t)(out_min + num / (in_max - in_min));
}

// LED：共陰，G/B 用 Timer0 PWM，R 用數位 on/off
static inline void led_set(uint8_t r, uint8_t g, uint8_t b){
  OCR0A = g;  // PD6
  OCR0B = b;  // PD5
  if (r > 0){
    PORTB |=  (1<<LED_R);
  }else{
    PORTB &= ~(1<<LED_R);
  }
}

// Buzzer：暫時用數位 on/off（接近啟動、遠離關掉）
static inline void buzzer_set(uint8_t on){
  if (on){
    PORTB &= ~(1<<BUZZ);
  }else{
    PORTB |=  (1<<BUZZ);
  }
}

// ================== 初始化 ==================
static void io_init(void){
  // ---- Ultrasound ----
  DDRB |= (1<<TRIG1);    // TRIG1 output
  DDRC |= (1<<TRIG2);    // TRIG2 output
  DDRD &= ~((1<<ECHO1)|(1<<ECHO2)); // ECHO inputs

  // ---- LED & Buzzer ----
  DDRB |= (1<<LED_R) | (1<<BUZZ);
  DDRD |= (1<<LED_G) | (1<<LED_B);

  // ---- Button ----
  DDRC &= ~(1<<BTN);
  PORTC |= (1<<BTN);     // enable pull-up

  // ---- External Interrupts for Echo ----
  EICRA = (1<<ISC00) | (1<<ISC10); // any change on INT0/INT1
  EIFR  = (1<<INTF0) | (1<<INTF1); // clear pending
  EIMSK = (1<<INT0) | (1<<INT1);   // enable INT0/INT1

  // ---- Pin Change Interrupt for Button (PCINT12 on PC4) ----
  PCICR  |= (1<<PCIE1);
  PCMSK1 |= (1<<PCINT12);

  // ---- Timer0: Fast PWM 用於 LED G/B ----
  TCCR0A = (1<<COM0A1)|(1<<COM0B1)|(1<<WGM01)|(1<<WGM00);
  TCCR0B = (1<<CS01)|(1<<CS00); // clk/64

  // ---- Timer1: 1us counter for echo ----
  t1_init_1us();

  // ---- UART ----
  uart_init();

  // Global interrupt
  sei();
}

// ================== 主程式 ==================
int main(void){
  io_init();

  // 開機閃一下藍燈
  led_set(0,0,255);
  _delay_ms(300);
  led_set(0,0,0);

  uart_print("Dual ultrasonic ready @ F_CPU=1MHz, 9600bps\r\n");

  bool system_enabled = true;
  uint16_t d1_last = 999, d2_last = 999, dmin = 999;

  // ---- 滑動窗口濾波器初始化 ----
  #define HISTORY_SIZE 10
  uint8_t measurement_history[HISTORY_SIZE] = {0}; // 0:安全, 1:警告, 2:危險
  uint8_t history_index = 0;
  uint8_t danger_count = 0;
  uint8_t warning_count = 0;

  while(1){
    // --- Non-blocking check for incoming commands from backend ---
    if (UCSR0A & (1 << RXC0)) {
        char command_buffer[16];
        uint8_t i = 0;
        char c;
        // Read until newline or buffer is full
        while (i < sizeof(command_buffer) - 1) {
            c = uart_getc();
            if (c == '\n' || c == '\r') break;
            command_buffer[i++] = c;
        }
        command_buffer[i] = '\0'; // Null-terminate the string
        parse_and_update_config(command_buffer);
    }

    // ---- Button 處理：Enable / Mute ----
    if (btn_pressed_flag){
      btn_pressed_flag = false;
      system_enabled = !system_enabled;

      // 簡單 feedback：Enable → 綠，Mute → 全關
      if(system_enabled){
        led_set(0,255,0);
      }else{
        led_set(0,0,0);
        buzzer_set(0);
      }
      _delay_ms(80); // debounce
    }

    // ---- 超音波 #1 測距 ----
    echo1_done = false;
    trig_pulse(&PORTB, TRIG1);
    uint16_t guard = 0;
    while(!echo1_done && guard++ < 40000){
      _delay_us(1);
    }
    if(echo1_done){
      uint16_t us = (uint16_t)(echo1_end - echo1_start);
      if(us > 100 && us < 30000){
        d1_last = us_to_cm(us);
      }else{
        d1_last = 999;
      }
    }else{
      d1_last = 999;
    }

    _delay_ms(PING_INTERVAL_MS/2);

    // ---- 超音波 #2 測距 ----
    echo2_done = false;
    trig_pulse(&PORTC, TRIG2);
    guard = 0;
    while(!echo2_done && guard++ < 40000){
      _delay_us(1);
    }
    if(echo2_done){
      uint16_t us = (uint16_t)(echo2_end - echo2_start);
      if(us > 100 && us < 30000){
        d2_last = us_to_cm(us);
      }else{
        d2_last = 999;
      }
    }else{
      d2_last = 999;
    }

    // ---- 選最小距離 ----
    if(d1_last == 999 && d2_last == 999){
      dmin = 999;
    }else if(d1_last == 999){
      dmin = d2_last;
    }else if(d2_last == 999){
      dmin = d1_last;
    }else{
      dmin = (d1_last < d2_last) ? d1_last : d2_last;
    }

    // ---- 滑動窗口濾波 ----
    // 1. 移除最舊的記錄
    uint8_t oldest_measurement = measurement_history[history_index];
    if (oldest_measurement == 2) danger_count--;
    else if (oldest_measurement == 1) warning_count--;

    // 2. 根據當前測量值，決定新記錄的狀態
    uint8_t current_measurement_state;
    if (dmin <= danger_threshold_cm) {
      current_measurement_state = 2; // 危險
      danger_count++;
    } else if (dmin <= warning_threshold_cm) {
      current_measurement_state = 1; // 警告
      warning_count++;
    } else {
      current_measurement_state = 0; // 安全
    }

    // 3. 將新記錄存入歷史，並移動指針
    measurement_history[history_index] = current_measurement_state;
    history_index = (history_index + 1) % HISTORY_SIZE;

    // ---- LED / Buzzer 行為 ----
    if(system_enabled){
      static uint8_t alert_level = 0; // 0:安全, 1:警告, 2:危險
      uint8_t R=0,G=0,B=0;

      // 4. 根據 "投票" 結果決定最終狀態
      if(danger_count > 5){
        // 第三階段 (危險)
        R = 255;
        G = 0;
        B = 0;
        if (alert_level < 2) { // 從非危險區進入危險區時觸發
          buzzer_set(1); _delay_ms(50); buzzer_set(0); // 嗶
          _delay_ms(80);                             // 嗶嗶之間的間隔
          buzzer_set(1); _delay_ms(50); buzzer_set(0); // 嗶
        }
        alert_level = 2;
      } else if (warning_count > 5) {
        // 第二階段 (警告)
        R = 255; // 黃燈 = 紅 + 綠
        G = 180;
        B = 0;
        if (alert_level < 1) { // 從安全區進入警告區時觸發
          buzzer_set(1); _delay_ms(50); buzzer_set(0); // 嗶
        }
        alert_level = 1;
      } else {
        // 第一階段 (安全)
        R = 0;
        G = 255;
        B = 0;
        alert_level = 0;
      }

      led_set(R,G,B);
    }else{
      // Mute 狀態：關閉 LED + buzzer
      led_set(0,0,0);
      buzzer_set(0);
    }

    // ---- 串列輸出，給 terminal / Python / Django ----
    uart_print("U:");
    uart_print_u16(d1_last);
    uart_print(" cm  D:");
    uart_print_u16(d2_last);
    uart_print(" cm  Min:");
    uart_print_u16(dmin);
    uart_print(system_enabled ? "  [EN]\r\n" : "  [MUTE]\r\n");

    _delay_ms(PING_INTERVAL_MS/2);
  }
}
