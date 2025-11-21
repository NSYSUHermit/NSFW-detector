#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h>

#define BAUD 9600

#define TRIG PB0
#define ECHO PD2

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

static inline void uart_putc(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0 = c; }
static void uart_print(const char*s){ while(*s) uart_putc(*s++); }
static void uart_print_u16(uint16_t v){ char b[8]; itoa(v, b, 10); uart_print(b); }

// Timer1 1us
static void t1_init_1us(void){
  TCCR1A = 0;
#if (F_CPU == 1000000UL)
  TCCR1B = (1<<CS10);
#elif (F_CPU == 8000000UL)
  TCCR1B = (1<<CS11);
#else
  TCCR1B = (1<<CS11);
#endif
}

// ---- HC-SR04 ECHO 
volatile uint16_t echo_start = 0, echo_end = 0;
volatile bool echo_busy = false, echo_done = false;

// INT0：ECHO 
ISR(INT0_vect){
  if (PIND & (1<<ECHO)) { 
    echo_start = TCNT1;
    echo_busy  = true;
  } else {          
    echo_end = TCNT1;
    echo_busy = false;
    echo_done = true;
  }
}

static void echo_int_init(void){
  // 任意邏輯變化觸發中斷（上升/下降都會進）
  EICRA = (1<<ISC00);
  EIFR  = (1<<INTF0);   // 清舊的 pending
  EIMSK = (1<<INT0);
}

static inline void trig_pulse(void){
  // 10 us 高脈衝
  PORTB &= ~(1<<TRIG);
  _delay_us(2);
  PORTB |=  (1<<TRIG);
  _delay_us(10);
  PORTB &= ~(1<<TRIG);
}

// 58 us ≈ 1 cm
static uint16_t measure_cm_blocking(uint16_t timeout_us){
  echo_done = false;
  echo_busy = false;

  trig_pulse();

  uint16_t guard = 0;
  while(!echo_done && guard < timeout_us){
    _delay_us(1);
    guard++;
  }

  if(!echo_done) return 0; 
  uint16_t width_us = (uint16_t)(echo_end - echo_start)
  if(width_us < 100) {
    return 0;
  }
  return (uint16_t)(width_us / 58);
}

int main(void){
  // I/O
  DDRB |= (1<<TRIG);
  DDRD &= ~(1<<ECHO);

  uart_init();
  t1_init_1us();
  echo_int_init();
  sei();

  uart_print("Ultrasonic Test Start @ ");
  uart_print_u16(BAUD);
  uart_print(" bps, F_CPU=");
  uart_print_u16((uint16_t)(F_CPU/1000000UL));
  uart_print("MHz\r\n");

  while(1){
    uint16_t cm = measure_cm_blocking(40000);
    if(cm > 0){
      uart_print("Distance: ");
      uart_print_u16(cm);
      uart_print(" cm\r\n");
    }else{
      uart_print("No echo\r\n");
    }
    _delay_ms(500);
  }
}
