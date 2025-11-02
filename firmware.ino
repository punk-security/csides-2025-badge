#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

/// FLASH at 16Mhz otherwise it no worky
/// Set to starutp time 64 ms to prevent reset button corruption

#include "WORLD_IR_CODES.h"

// ==== CONFIGURATION ====

// Pin definitions
#define IRLED_PIN   PIN_PB1  // TCA0 WO1
#define TRIGGER_PIN PIN_PA7
#define NEOPIXEL_DATA PIN_PA4
#define NEOPIXEL_PWR PIN_PA3


# define WAKE_TIME_MS 2700000 // 60 mins

// External IR code data
extern const IrCode* const EUpowerCodes[] PROGMEM;
extern uint8_t num_EUcodes;
extern const IrCode* const NApowerCodes[] PROGMEM;
extern uint8_t num_NAcodes;

// ==== GLOBALS ====
uint8_t bitsleft_r = 0;
uint8_t bits_r = 0;
const uint8_t* code_ptr;

// ==== FUNCTION PROTOTYPES ====
void setupPWM();
void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t usePWM);
void delay_ten_us(uint16_t us10);
uint8_t read_bits(uint8_t count);
void sendIRCode(const uint8_t* code_data);
void sendAllCodes();

// NEOPIXELS
#include <tinyNeoPixel_Static.h>
#define NUMLEDS 4
byte pixels[NUMLEDS * 3];
tinyNeoPixel strip = tinyNeoPixel(NUMLEDS, NEOPIXEL_DATA, NEO_GRB, pixels);

// ==== SETUP ====

#include <avr/sleep.h>

void RTC_init()
{
  /* Initialize RTC: */
  while (RTC.STATUS > 0)
  {
    ;                                   /* Wait for all register to be synchronized */
  }
  RTC.CLKSEL = RTC_CLKSEL_INT1K_gc;    /* 1kHz Internal Ultra-Low-Power Oscillator (OSCULP32K) */
}

void mini_sleep( uint8_t period = RTC_PERIOD_CYC16_gc)
{
  // default timer is about 16ms
  RTC.PITINTCTRL = RTC_PI_bm;  // Enable RTC interrupt
  RTC.PITCTRLA = period | RTC_PITEN_bm; // Set timer to 2s
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  RTC.PITINTCTRL = ~(RTC_PI_bm); // Disable RTC interrupt
}

ISR(RTC_PIT_vect)
{
  RTC.PITINTFLAGS = RTC_PI_bm;  // Clear RTC interrupt flag otherwise keep coming back here
}


void sleep()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  PORTA.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_LEVEL_gc; // enable pullup and interrupt
  digitalWrite(NEOPIXEL_PWR, LOW); // turn off LED power rail
  sleep_enable();
  sleep_cpu();
  // sleep resumes here
  PORTA.PIN7CTRL = PORT_PULLUPEN_bm; // renable pullup but no interrupt
  digitalWrite(NEOPIXEL_PWR, HIGH);  // turn on LED power rail
  setupTCAO();
  setupPWM();
}

ISR(PORTA_PORT_vect) {
  PORTA.INTFLAGS = PORT_INT7_bm; // Clear Pin 7 interrupt flag otherwise keep coming back here
}

void setupTCAO()
{
  takeOverTCA0();   
  PORTMUX.CTRLC &= 1 << 1;
}

void setup() {
  ADC0.CTRLA &= ~ADC_ENABLE_bm; 
  pinMode(IRLED_PIN, OUTPUT);
  pinMode(NEOPIXEL_PWR, OUTPUT);
  pinMode(NEOPIXEL_DATA, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  digitalWrite(NEOPIXEL_PWR, HIGH);
  strip.begin();
//  // Set IR LED pin as output
  PORTA.DIRSET = (1 << IRLED_PIN);
  setupTCAO();
  setupPWM();
  RTC_init();
}

void setupPWM() {


  // Configure TCA0 for 38kHz freq PWM on WO0
  TCA0.SINGLE.CTRLB   = (TCA_SINGLE_CMP1EN_bm | TCA_SINGLE_WGMODE_SINGLESLOPE_gc);
  TCA0.SINGLE.PER = 420;     // 16MHz / 8 / 210 ≈ 9.5kHz × 4 = 38kHz
  TCA0.SINGLE.CMP1 = 70;     // ~33% duty cycle
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV8_gc; // Enable only in xmitCodeElement
  //TCA0.SINGLE.CTRLA   = TCA_SINGLE_ENABLE_bm;
}

void setAllPixels(int r, int g, int b, bool show = false)
{
  for (int i = 0; i < NUMLEDS; i++) 
  {
    strip.setPixelColor(i,r,g,b);
  }
  if(show)
    strip.show();
}

#define RED 30,0,0
#define GREEN 0,8,0
#define BLUE 0,0,30
#define CSIDES_BLUE 0,40,30
#define ORANGE 20,8,0
#define PINK 6,0,4
#define PURPLE 6,0,15
#define OLIVE 2,2,0
#define YELLOW 18,12,0
#define YELLOWGREEN 20,30,20
#define OFF 0,0,0

int rainbow_rotate(int x)
{
  strip.setPixelColor((x+4) % 5,RED);
  strip.setPixelColor((x+3) % 5,GREEN);
  strip.setPixelColor((x+2) % 5,ORANGE);
  strip.setPixelColor((x+1) % 5,PURPLE);
  strip.setPixelColor(x % 5,BLUE);
  strip.show();
  return 200;
}

int red_eye (int x)
{
  setAllPixels(OFF);
  strip.setPixelColor(0,(x % 40) * 5,0,0);
  strip.show();
  return 20;
}

int power_up (int x)
{
  int n = x % 60;
  int s = n % 10;
  setAllPixels(OFF);
  switch (n){
    case 0 ... 9:
      strip.setPixelColor(0, 0, s * 20, 0);
      break;
    case 10 ... 19:
      strip.setPixelColor(0, 0, 255, 0);
      strip.setPixelColor(1, 0, s * 20, 0);
      break;
    case 20 ... 29:
      strip.setPixelColor(0, 0, 255, 0);
      strip.setPixelColor(1, 0, 255, 0);
      strip.setPixelColor(2, 0, s * 20, 0);
      break;
    case 30 ... 39:
      strip.setPixelColor(0, 0, 255, 0);
      strip.setPixelColor(1, 0, 255, 0);
      strip.setPixelColor(2, 0, 255, 0);
      strip.setPixelColor(3, 0, s * 20, 0);
      break;
    default:
      setAllPixels(0,255,0);
  }
  strip.show();
  return 50;
}

int alternate(int x, int r, int g, int b, int r2, int g2, int b2, int ret = 100)
{
  if(x % 2 == 0)
  {
    strip.setPixelColor(0,r,g,b);
    strip.setPixelColor(1,r2,g2,b2);
    strip.setPixelColor(2,r,g,b);
    strip.setPixelColor(3,r2,g2,b2);
  }
  else
  {
    strip.setPixelColor(0,r2,g2,b2);
    strip.setPixelColor(1,r,g,b);
    strip.setPixelColor(2,r2,g2,b2);
    strip.setPixelColor(3,r,g,b);
  }
  strip.show();
  return ret;
}

int p_4fox(int x)
{
  setAllPixels(10,10,10);
  int n = x % 4;
    strip.setPixelColor(n, ORANGE);
  strip.show();
  return 200;
}

int p_4fox_2(int x)
{
  setAllPixels(ORANGE);
  int n = x % 4;
    strip.setPixelColor(n, 10,10,10);
  strip.show();
  return 200;
}

int cyberis(int x)
{
  strip.setPixelColor( x % 4,       0,0,10);
  strip.setPixelColor( (x + 1) % 4, 0,5,10);
  strip.setPixelColor( (x + 2) % 4, 0,10,0);
  strip.setPixelColor( (x + 3) % 4, 9,4,0);
  strip.show();
  return 30;
}

int prism(int x)
{
  int n = x % 10 + 10;
  setAllPixels(0,5 * n, 3 * n, true);
  return 20;
}

int punk_2(int x)
{
  int n = x % 5;
  switch (n)
  {
    case 0:
      setAllPixels(RED, true); break;
    case 1:
      setAllPixels(GREEN, true); break;
    case 2:
      setAllPixels(ORANGE, true); break;
    case 3:
      setAllPixels(PURPLE, true); break;
    case 4:
      setAllPixels(BLUE, true); break;
  }
  return 500;
}

int police(int x)
{
  if( x % 2 == 0)
  {
    strip.setPixelColor(0,255,0,0);
    strip.setPixelColor(3,255,0,0);
    strip.setPixelColor(2,0,0,255);
    strip.setPixelColor(1,0,0,255);
  }
  else
  {
    strip.setPixelColor(0,0,0,255);
    strip.setPixelColor(3,0,0,255);
    strip.setPixelColor(2,255,0,0);
    strip.setPixelColor(1,255,0,0);
  }
  strip.show();
  return 200;
}

int knightrider(uint8_t i, int r, int g, int b, int r2, int g2, int b2)
{
  setAllPixels(r2,g2,b2);
  int n = i % 10;
  if (n < 4)
  {
    strip.setPixelColor(n,r,g,b);
  }
  else if (n > 5)
  {
    strip.setPixelColor(10-n,r,g,b);
  }
  strip.show();
  return 50;
}

int glitch(uint8_t i){
  int b = i % 10;
  int l = i / 50;
  setAllPixels(OFF);
  strip.setPixelColor(l, 0 , 30 -b, 20 - b);
  strip.show();
  return 10;
}

int csides(int x)
{
  int g = x % 40;
  if (g > 20) g = 40 - g;
  strip.setPixelColor(0,CSIDES_BLUE);
  strip.setPixelColor(1,CSIDES_BLUE);
  strip.setPixelColor(2,0,g,30);
  strip.setPixelColor(3,BLUE);
  strip.show();
  if (g == 0)
  {
    return 500;
  }
  return 100;
}

uint16_t time_pin_low(uint16_t max_ms)
{
  // blocking for up to max_ms
for (int i = 0; i < 20; i++) {
  if (digitalRead(TRIGGER_PIN) == HIGH) {
    return 0;
  }
  delay(5);
}
  uint16_t t = 100;
  while(digitalRead(TRIGGER_PIN) == LOW)
  {
    delay(5);
    t = t + 5;
    if ( t > max_ms )
      return(max_ms);
  }
  return(t);
}
// ==== MAIN LOOP ====

void loop() {
  int mode = 0;
  int interval;
  uint16_t button_low_time = 0;
  uint32_t total_interval = 0;
  int i = 0;
  while(true)
  {
    if ( mode == 0 )
    {
      interval = csides(i);
    }
    else if ( mode == 1)
    {
      interval = rainbow_rotate(i);
    }
    else if ( mode == 2)
    {
      interval = police(i);
    }
    else if ( mode == 3)
    {
      interval = red_eye(i);
    }
    else if ( mode == 4)
    {
      interval = power_up(i);
    }
    else if ( mode == 5)
    {
      interval = alternate(i,PINK,PURPLE,40);
    }
    else if ( mode == 6)
    {
      interval = p_4fox(i);
    }
    else if ( mode == 7)
    {
      interval = p_4fox_2(i);
    }
    else if ( mode == 8)
    {
      interval = knightrider(i,RED, OFF);
    }
    else if ( mode == 9)
    {
      interval = prism(i);
    }
    else if ( mode == 10)
    {
      interval = punk_2(i);
    }
    else if ( mode == 11)
    {
      interval = glitch(i);
    }
    else if ( mode == 12)
    {
      interval = knightrider(i,GREEN, OFF);
    }
    else if ( mode == 13)
    {
      interval = cyberis(i);
    }
    else
    {
      mode = 0;
      continue;
    }
    i++;
    // This section breaks down the sleep interval to catch button presses
    total_interval = total_interval + interval;
    while(interval > 0)
    {
      mini_sleep();
      interval = interval - 10;
      button_low_time = time_pin_low(3000);
      if (button_low_time > 200)
      {
       /*
       * MAIN MENU
       * NO PRESS = CONTINUE
       * SHORT PRESS = CHANGE FLASHY MODE
       * MEDIUM PRESS = SEND CODES
       * LONG PRESS = SLEEP
       */
        if (button_low_time == 3000)
        {
          setAllPixels(RED,true);
          while(digitalRead(TRIGGER_PIN) == LOW)
          {
            // WAIT FOR RELEASE BEFORE SLEEPING otherwise we wake back up!
            delay(5);
          }
          delay(50); //DEBOUNCE
          sleep();
        }
        else if (button_low_time > 500)
        {
          sendAllCodes();
          digitalWrite(NEOPIXEL_PWR, HIGH);
        }
        else
        {
          mode++;
        }
        // reset timer
        i = 0;
        total_interval = 0;
      }

    }
    // At the end of each interval, see if we need to sleep
    if (total_interval > WAKE_TIME_MS)
    {
      total_interval = 0;
      i = 0;
      sleep();
    }
  }
}

// ==== CORE FUNCTIONS ====

void flash(int r, int g, int b)
{
    strip.setPixelColor(0, r,g,b);
    strip.setPixelColor(1, 0,0,0);
    strip.setPixelColor(2, 0,0,0);
    strip.setPixelColor(3, 0,0,0);
    digitalWrite(NEOPIXEL_PWR,HIGH);
    delay(1);
    strip.show();
    mini_sleep(RTC_PERIOD_CYC32_gc);
    digitalWrite(NEOPIXEL_PWR,LOW);
    mini_sleep(RTC_PERIOD_CYC512_gc);
}

void sendAllCodes() {
  flash(0,1,0);
  for (uint8_t i = 0; i < num_EUcodes; i++) {
    const IrCode* code = (const IrCode*)pgm_read_word(&EUpowerCodes[i]);
    sendIRCode((const uint8_t*)code);
    flash(1,0,0);
    if (time_pin_low(2000) > 50)
    {
      return;
    }
  }
  for (uint8_t i = 0; i < num_NAcodes; i++) {
    const IrCode* code = (const IrCode*)pgm_read_word(&NApowerCodes[i]);
    sendIRCode((const uint8_t*)code);
    flash(0,0,1);
    if (time_pin_low(2000) > 50)
    {
      return;
    }
  }
}

void sendIRCode(const uint8_t* code_data) {
  const uint8_t freq = pgm_read_byte(code_data++);
  const uint8_t numpairs = pgm_read_byte(code_data++);
  const uint8_t bitcompression = pgm_read_byte(code_data++);

  const uint16_t* time_ptr = (const uint16_t*)pgm_read_word(code_data);
  code_data += 2;

  code_ptr = (const uint8_t*)pgm_read_word(code_data);

  bitsleft_r = 0;

  for (uint8_t k = 0; k < numpairs; k++) {
    uint8_t index = read_bits(bitcompression);
    uint16_t ti = index * 2;
    uint16_t ontime = pgm_read_word(&time_ptr[ti]);
    uint16_t offtime = pgm_read_word(&time_ptr[ti + 1]);

    xmitCodeElement(ontime, offtime, (freq != 0));
  }

  bitsleft_r = 0;
}

void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t usePWM) {
  if (usePWM) {
    TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm; // enable PWM
  } else {
    VPORTA.OUT |= (1 << IRLED_PIN);
  }

  delay_ten_us(ontime);

  if (usePWM) {
    TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm; // disable PWM
    VPORTA.OUT &= ~(1 << IRLED_PIN);
  } else {
    VPORTA.OUT &= ~(1 << IRLED_PIN);
  }

  delay_ten_us(offtime);
}

uint8_t read_bits(uint8_t count) {
  uint8_t tmp = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (bitsleft_r == 0) {
      bits_r = pgm_read_byte(code_ptr++);
      bitsleft_r = 8;
    }
    bitsleft_r--;
    tmp |= ((bits_r >> bitsleft_r) & 1) << (count - 1 - i);
  }
  return tmp;
}

void delay_ten_us(uint16_t us10) {
  while (us10--) _delay_us(10);
}
