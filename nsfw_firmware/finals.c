// file: finals.c
// Firmware for Dual Ultrasonic Privacy Guardian

#ifndef F_CPU
#define F_CPU 1000000UL // System Clock: 1MHz internal oscillator
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h>

#define BAUD 9600

// ========== Pin Definitions ==========
#define TRIG1 PB0
#define ECHO1 PD2 // INT0
#define TRIG2 PC0
#define ECHO2 PD3 // INT1

#define LED_R PB1 // Red LED (Digital ON/OFF)
#define LED_G PD6 // Green LED (Timer0 OC0A PWM)
#define LED_B PD5 // Blue LED (Timer0 OC0B PWM)

#define BUZZ  PB2 // Buzzer (Digital ON/OFF)

#define BTN   PC4 // Button (PCINT12, internal pull-up)

#define PING_INTERVAL_MS 70

// ========== Global Variables ==========
// These are defined here, before any function that uses them.

// --- Thresholds (can be updated via UART) ---
volatile uint16_t danger_threshold_cm = 50;
volatile uint16_t warning_threshold_cm = 100;

// --- Sensor 1 ---
volatile uint16_t echo1_start = 0, echo1_end = 0;
volatile bool echo1_done = false;
// --- Sensor 2 ---
volatile uint16_t echo2_start = 0, echo2_end = 0;
volatile bool echo2_done = false;
// --- Button ---
volatile bool btn_pressed_flag = false;
// --- System State Flags ---
volatile bool led_alerts_enabled = true;
volatile bool buzzer_alerts_enabled = true;

// --- UART command buffer (for threshold updates) ---
volatile char cmd_buffer[16];
volatile uint8_t cmd_index = 0;
volatile bool cmd_ready = false;


// ================== UART Communication ==================
static void uart_init(void){
    UCSR0A = (1<<U2X0);
    uint16_t ubrr_val = (F_CPU / (8UL * BAUD)) - 1;
    UBRR0H = (uint8_t)(ubrr_val >> 8);
    UBRR0L = (uint8_t)(ubrr_val & 0xFF);
    UCSR0B = (1<<TXEN0) | (1<<RXEN0) | (1<<RXCIE0); // Enable RX Complete Interrupt
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
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

// ================== Interrupt Service Routines ==================
ISR(INT0_vect){
  if (PIND & (1<<ECHO1)) { echo1_start = TCNT1; }
  else { echo1_end = TCNT1; echo1_done = true; }
}

ISR(INT1_vect){
  if (PIND & (1<<ECHO2)) { echo2_start = TCNT1; }
  else { echo2_end = TCNT1; echo2_done = true; }
}

ISR(PCINT1_vect){
  static uint8_t last = 1;
  uint8_t now = (PINC & (1<<BTN)) ? 1 : 0;
  if (last == 1 && now == 0){ btn_pressed_flag = true; }
  last = now;
}

// UART RX Complete Interrupt Service Routine
ISR(USART_RX_vect){
    char c = UDR0;

    if (c != '\n' && c != '\r') {
        if (cmd_index < sizeof(cmd_buffer) - 1) {
            cmd_buffer[cmd_index++] = c;
        }
    } else {
        // End of command received
        cmd_buffer[cmd_index] = '\0'; // Null-terminate the string
        cmd_index = 0; // Reset index for next command
        cmd_ready = true; // Set flag for main loop to process
    }
}

// ================== Helper Functions ==================
static inline void trig_pulse(volatile uint8_t *port, uint8_t pin){
  *port &= ~(1<<pin); _delay_us(2);
  *port |=  (1<<pin); _delay_us(10);
  *port &= ~(1<<pin);
}

static void t1_init_1us(void){
  TCCR1A = 0;
  TCCR1B = (1<<CS10);
}

static inline uint16_t us_to_cm(uint16_t us){
  return (uint16_t)(us / 58);
}

static inline void led_set(uint8_t r, uint8_t g, uint8_t b){
  OCR0A = g;
  OCR0B = b;
  if (r > 0) { PORTB |= (1<<LED_R); }
  else { PORTB &= ~(1<<LED_R); }
}

static inline void buzzer_set(uint8_t on){
  if (on) { PORTB &= ~(1<<BUZZ); }
  else { PORTB |= (1<<BUZZ); }
}

static void parse_and_update_config(char* buffer) {
    if (buffer[0] == 'W' && buffer[1] == ':') {
        warning_threshold_cm = atoi(buffer + 2);
    }
    if (buffer[0] == 'D' && buffer[1] == ':') {
        danger_threshold_cm = atoi(buffer + 2);
    }
    // New commands to toggle LED and Buzzer alerts independently
    if (buffer[0] == 'L' && buffer[1] == '\0') {
        led_alerts_enabled = !led_alerts_enabled;
    }
    if (buffer[0] == 'B' && buffer[1] == '\0') {
        buzzer_alerts_enabled = !buzzer_alerts_enabled;
    }
}

// ================== Initialization ==================
static void io_init(void){
  DDRB |= (1<<TRIG1) | (1<<LED_R) | (1<<BUZZ);
  DDRC |= (1<<TRIG2);
  DDRD |= (1<<LED_G) | (1<<LED_B);

  DDRD &= ~((1<<ECHO1)|(1<<ECHO2));
  DDRC &= ~(1<<BTN);
  PORTC |= (1<<BTN);

  EICRA = (1<<ISC00) | (1<<ISC10);
  EIMSK = (1<<INT0) | (1<<INT1);
  PCICR |= (1<<PCIE1);
  PCMSK1 |= (1<<PCINT12);

  TCCR0A = (1<<COM0A1)|(1<<COM0B1)|(1<<WGM01)|(1<<WGM00);
  TCCR0B = (1<<CS01)|(1<<CS00);

  t1_init_1us();
  uart_init();
  sei();
}

// ================== Main Program Loop ==================
int main(void){
  io_init();

  led_set(0,0,255);
  _delay_ms(300);
  led_set(0,0,0);

  uart_print("Dual ultrasonic ready...\r\n");

  uint16_t d1_last = 999, d2_last = 999, dmin = 999;

  #define HISTORY_SIZE 10
  uint8_t measurement_history[HISTORY_SIZE] = {0};
  uint8_t history_index = 0;
  uint8_t danger_count = 0;
  uint8_t warning_count = 0;

  while(1){
    if (cmd_ready) { // Check if a full command has been received by the ISR
        cmd_ready = false;
        parse_and_update_config((char*)cmd_buffer);

        // Debug: Acknowledge the config update by sending back the new values
        uart_print("CFG W=");
        uart_print_u16(warning_threshold_cm);
        uart_print(" D=");
        uart_print_u16(danger_threshold_cm);
        uart_print("\r\n");
    }

    if (btn_pressed_flag){
      btn_pressed_flag = false;
      // Physical button acts as a master toggle for both
      bool master_toggle_state = !led_alerts_enabled || !buzzer_alerts_enabled;
      led_alerts_enabled = master_toggle_state;
      buzzer_alerts_enabled = master_toggle_state;

      // Report the new state back to the backend
      uart_print("STATE:L");
      uart_print_u16(led_alerts_enabled);
      uart_print(",B");
      uart_print_u16(buzzer_alerts_enabled);
      uart_print("\r\n");

      if(led_alerts_enabled){ led_set(0,255,0); }
      else { led_set(0,0,0); }
      _delay_ms(80);
    }

    echo1_done = false;
    trig_pulse(&PORTB, TRIG1);
    uint16_t guard = 0;
    while(!echo1_done && guard++ < 40000) _delay_us(1);
    if(echo1_done){
      uint16_t us = (uint16_t)(echo1_end - echo1_start);
      d1_last = (us > 100 && us < 30000) ? us_to_cm(us) : 999;
    } else {
      d1_last = 999;
    }

    _delay_ms(PING_INTERVAL_MS/2);

    echo2_done = false;
    trig_pulse(&PORTC, TRIG2);
    guard = 0;
    while(!echo2_done && guard++ < 40000) _delay_us(1);
    if(echo2_done){
      uint16_t us = (uint16_t)(echo2_end - echo2_start);
      d2_last = (us > 100 && us < 30000) ? us_to_cm(us) : 999;
    } else {
      d2_last = 999;
    }

    if(d1_last == 999 && d2_last == 999) dmin = 999;
    else if(d1_last == 999) dmin = d2_last;
    else if(d2_last == 999) dmin = d1_last;
    else dmin = (d1_last < d2_last) ? d1_last : d2_last;

    uint8_t oldest_measurement = measurement_history[history_index];
    if (oldest_measurement == 2) danger_count--;
    else if (oldest_measurement == 1) warning_count--;

    uint8_t current_measurement_state;
    if (dmin <= danger_threshold_cm) {
      current_measurement_state = 2;
      danger_count++;
    } else if (dmin <= warning_threshold_cm) {
      current_measurement_state = 1;
      warning_count++;
    } else {
      current_measurement_state = 0;
    }

    measurement_history[history_index] = current_measurement_state;
    history_index = (history_index + 1) % HISTORY_SIZE;

    static uint8_t alert_level = 0;
    uint8_t R=0,G=0,B=0;
    bool should_buzz = false;

    if(danger_count > (HISTORY_SIZE / 2)){
      R = 255; G = 0; B = 0;
      if (alert_level < 2) { should_buzz = true; }
      alert_level = 2;
    } else if (warning_count > (HISTORY_SIZE / 2)) {
      R = 255; G = 180; B = 0;
      if (alert_level < 1) { should_buzz = true; }
      alert_level = 1;
    } else {
      R = 0; G = 255; B = 0;
      alert_level = 0;
    }

    if (led_alerts_enabled) {
      led_set(R,G,B);
    } else {
      led_set(0,0,0);
    }

    if (buzzer_alerts_enabled && should_buzz) {
      if (alert_level == 2) { // Danger: two beeps
        buzzer_set(1); _delay_ms(50); buzzer_set(0);
        _delay_ms(80);
        buzzer_set(1); _delay_ms(50); buzzer_set(0);
      } else { // Warning: one beep
        buzzer_set(1); _delay_ms(50); buzzer_set(0);
      }
    } else {
      buzzer_set(0);
    }

    uart_print("DIST:");
    uart_print_u16(dmin);
    uart_print("\r\n");

    _delay_ms(PING_INTERVAL_MS/2);
  }
}
