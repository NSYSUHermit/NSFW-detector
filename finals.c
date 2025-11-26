#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h>

#define BAUD 9600

#define TRIG1 PB0
#define ECHO1 PD2 // INT0
#define TRIG2 PC0
#define ECHO2 PD3 // INT1

#define LED_R PB1 // red
#define LED_G PD6 // green
#define LED_B PD5 // blue

#define BUZZ PB2 // buzzer

#define BTN PC4 // pull-up

#define PING_INTERVAL_MS 70
#define DANGER_THRESHOLD_CM 50 
// danger threshold
#define WARNING_THRESHOLD_CM 100 // waning threshold

// Ultrasound Up
volatile uint16_t echo1_start = 0, echo1_end = 0;
volatile bool echo1_done = false;

// Ultrasound Down
volatile uint16_t echo2_start = 0, echo2_end = 0;
volatile bool echo2_done = false;

volatile bool btn_pressed_flag = false;

// UART
static void uart_init(void){
#if (F_CPU == 1000000UL) && (BAUD == 9600)
    UCSR0A = (1<<U2X0);
    UBRR0H = 0;
    UBRR0L = 12;
#elif (F_CPU == 8000000UL) && (BAUD == 115200)
    UCSR0A = (1<<U2X0); 
    UBRR0H = 0;
    UBRR0L = 8;
#else
    UCSR0A = 0;
    uint16_t ubrr = (uint16_t)(F_CPU/16UL/BAUD - 1UL);
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xFF);
#endif
    UCSR0B = (1<<TXEN0) | (1<<RXEN0); // TX/RX
    UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
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

// Ultrasound echo INT0: up
ISR(INT0_vect){
  if (PIND & (1<<ECHO1)) {
    echo1_start = TCNT1;
  } else { 
    echo1_end = TCNT1;
    echo1_done = true;
  }
}

// Ultrasound echo INT1: dn
ISR(INT1_vect){
  if (PIND & (1<<ECHO2)) {
    echo2_start = TCNT1;
  } else {
    echo2_end = TCNT1;
    echo2_done = true;
  }
}

// Button PC4 pin-change
ISR(PCINT1_vect){
  static uint8_t last = 1;
  uint8_t now = (PINC & (1<<BTN)) ? 1 : 0; // pull-up: 1 idle, 0 pressed
  // detect falling edge
  if (last == 1 && now == 0){
    btn_pressed_flag = true;
  }
  last = now;
}

static inline void trig_pulse(volatile uint8_t *port, uint8_t pin){
  *port &= ~(1<<pin); _delay_us(2);
  *port |=  (1<<pin); _delay_us(10);
  *port &= ~(1<<pin);
}

// Timer1: 1 tick = 1us
static void t1_init_1us(void){
  TCCR1A = 0;
  TCCR1B = (1<<CS10); // no prescale, 1 tick = 1 us
}

// us transfer
static inline uint16_t us_to_cm(uint16_t us){
  return (uint16_t)(us / 58);
}

// map
static uint16_t map_u16(uint16_t x,uint16_t in_min, uint16_t in_max,uint16_t out_min, uint16_t out_max)
{
  if (x <= in_min) return out_min;
  if (x >= in_max) return out_max;
  uint32_t num = (uint32_t)(x - in_min) * (out_max - out_min);
  return (uint16_t)(out_min + num / (in_max - in_min));
}

// LED：Timer0 PWM，R for on/off
static inline void led_set(uint8_t r, uint8_t g, uint8_t b){
  OCR0A = g;  // PD6
  OCR0B = b;  // PD5
  if (r > 0){
    PORTB |=  (1<<LED_R);
  }else{
    PORTB &= ~(1<<LED_R);
  }
}

// Buzzer  on and off
static inline void buzzer_set(uint8_t on){
  if (on){
    PORTB &= ~(1<<BUZZ);
  }else{
    PORTB |=  (1<<BUZZ);
  }
}

static void io_init(void){
  // Ultrasound
  DDRB |= (1<<TRIG1);
  DDRC |= (1<<TRIG2);
  DDRD &= ~((1<<ECHO1)|(1<<ECHO2)); 

  DDRB |= (1<<LED_R) | (1<<BUZZ);
  DDRD |= (1<<LED_G) | (1<<LED_B);

  DDRC &= ~(1<<BTN);
  PORTC |= (1<<BTN);   

  // Echo
  EICRA = (1<<ISC00) | (1<<ISC10);
  EIFR  = (1<<INTF0) | (1<<INTF1); // clear pending
  EIMSK = (1<<INT0) | (1<<INT1); // enable INT0/INT1

  // Pin Change Interrupt for Button
  PCICR  |= (1<<PCIE1);
  PCMSK1 |= (1<<PCINT12);

  // Timer0 for Fast PWM LED
  TCCR0A = (1<<COM0A1)|(1<<COM0B1)|(1<<WGM01)|(1<<WGM00);
  TCCR0B = (1<<CS01)|(1<<CS00);

  t1_init_1us();
  uart_init();
  sei();
}

int main(void){
  io_init();

  // show start 
  led_set(0,0,255);
  _delay_ms(300);
  led_set(0,0,0);

  uart_print("Dual ultrasonic in lin a6:\r\n");

  bool system_enabled = true;
  uint16_t d1_last = 999, d2_last = 999, dmin = 999;

  // use sliding window filter
  #define HISTORY_SIZE 10
  uint8_t measurement_history[HISTORY_SIZE] = {0}; // 0:save, 1:warning, 2:danger
  uint8_t history_index = 0;
  uint8_t danger_count = 0;
  uint8_t warning_count = 0;

  while(1){
    // Button for Enable and Mute
    if (btn_pressed_flag){
      btn_pressed_flag = false;
      system_enabled = !system_enabled;

      if(system_enabled){
        led_set(0,255,0);
      }else{
        led_set(0,0,0);
        buzzer_set(0);
      }
      _delay_ms(80); // debounce
    }

    // get distances from two sensor
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

    // select min distance as result
    if(d1_last == 999 && d2_last == 999){
      dmin = 999;
    }else if(d1_last == 999){
      dmin = d2_last;
    }else if(d2_last == 999){
      dmin = d1_last;
    }else{
      if (d1_last < d2_last) {
        dmin = d1_last;
      } else {
        dmin = d2_last;
      }
    }

    // slideing window
    uint8_t oldest_measurement = measurement_history[history_index];
    if (oldest_measurement == 2) danger_count--;
    else if (oldest_measurement == 1) warning_count--;

    uint8_t current_measurement_state;
    if (dmin <= DANGER_THRESHOLD_CM) {
      current_measurement_state = 2; // danger
      danger_count++;
    } else if (dmin <= WARNING_THRESHOLD_CM) {
      current_measurement_state = 1; // warning
      warning_count++;
    } else {
      current_measurement_state = 0; // safe
    }

    measurement_history[history_index] = current_measurement_state;
    history_index = (history_index + 1) % HISTORY_SIZE;

    // LED and Buzzer
    if(system_enabled){
      static uint8_t alert_level = 0;
      uint8_t R=0,G=0,B=0;

      if(danger_count > 5){\
        // danger: red and beep twice
        R = 255;
        G = 0;
        B = 0;
        if (alert_level < 2) { 
          buzzer_set(1); _delay_ms(50); buzzer_set(0);
          _delay_ms(80);                            
          buzzer_set(1); _delay_ms(50); buzzer_set(0);
        }
        alert_level = 2;
      } else if (warning_count > 5) {
        // warning: yellow and beep once
        R = 255; 
        G = 180;
        B = 0;
        if (alert_level < 1) {
          buzzer_set(1); _delay_ms(50); buzzer_set(0); 
        }
        alert_level = 1;
      } else {
        // safe state
        R = 0;
        G = 255;
        B = 0;
        alert_level = 0;
      }

      led_set(R,G,B);
    }else{
      // Mute
      led_set(0,0,0);
      buzzer_set(0);
    }

    // output in terminal
    uart_print_u16(d1_last);
    uart_print(" cm  D:");
    uart_print_u16(d2_last);
    uart_print(" cm  Min:");
    uart_print_u16(dmin);
    
    if (system_enabled) {
      uart_print("  [EN]\r\n");
    } else {
      uart_print("  [MUTE]\r\n");
    }

    _delay_ms(PING_INTERVAL_MS/2);
  }
}
