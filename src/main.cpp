#include <Arduino.h>
#include "PinChangeInterrupt.h"

#define RED_PIN             9
#define GREEN_PIN           10
#define BLUE_PIN            11

/* CH9 : LED ON/OFF [1800 < PWM : ON] [1800 > PWM : OFF] */
/* CH2 : LED Brightness [PWM : MIN ~ MAX -> Brightness : 0 ~ 100] */
/* CH1 : LED Color [PWM : MIN ~ MID ~ MAX -> COLOR : R < G > B] */
#define pinRC9_ONOFF        A0
#define pinRC2_Brightness   A1
#define pinRC1_Color        A2 

// 실제 PWM 범위
#define PWM_MIN             1068
#define PWM_MID             1492
#define PWM_MAX             1932

// 실제 PWM 범위에 따른 LED 색상
#define COLOR_RED_MAX       1354  // 1/3 PWM 지점
#define COLOR_GREEN_MIN     1355
#define COLOR_GREEN_MAX     1641  // 2/3 PWM 지점
#define COLOR_BLUE_MIN      1642

// LED GREEN 색상 가장 밝은 지점
#define COLOR_GREEN_MID     ((COLOR_GREEN_MIN + COLOR_GREEN_MAX) / 2)

/* 9Channel 초기값 */
volatile int            nRC9PulseWidth = PWM_MIN;
volatile unsigned long  uRC9StartHigh  = 0;
volatile boolean        bNewRC9Pulse   = false;

/* 2Channel 초기값 */
volatile int            nRC2PulseWidth = PWM_MID;
volatile unsigned long  uRC2StartHigh  = 0;
volatile boolean        bNewRC2Pulse   = false;

/* 1Channel 초기값 */
volatile int            nRC1PulseWidth = PWM_MID;
volatile unsigned long  uRC1StartHigh  = 0;
volatile boolean        bNewRC1Pulse   = false;


void pwmRC9_ONOFF();
void pwmRC2_Brightness();
void pwmRC1_Color();

/* 초기 PIN I/O, PCINT, Serial Monitor 설정*/
void setup() {
  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);
  pinMode(pinRC9_ONOFF, INPUT_PULLUP);
  pinMode(pinRC2_Brightness, INPUT_PULLUP);
  pinMode(pinRC1_Color, INPUT_PULLUP);

  attachPCINT(digitalPinToPCINT(pinRC9_ONOFF), pwmRC9_ONOFF, CHANGE);
  attachPCINT(digitalPinToPCINT(pinRC2_Brightness), pwmRC2_Brightness, CHANGE);
  attachPCINT(digitalPinToPCINT(pinRC1_Color), pwmRC1_Color, CHANGE);

  Serial.begin(9600);
  Serial.println("RC Channel Decoding Test");
}

//---------------------------------------------------
/* PWM RC Channel Decoding */
//---------------------------------------------------
void pwmRC9_ONOFF() {
  if (digitalRead(pinRC9_ONOFF) == HIGH) {
    uRC9StartHigh = micros();
  } else if (uRC9StartHigh && !bNewRC9Pulse) {
    nRC9PulseWidth = (int)(micros() - uRC9StartHigh);
    uRC9StartHigh  = 0;
    bNewRC9Pulse   = true;
  }
}

void pwmRC2_Brightness() {
  if (digitalRead(pinRC2_Brightness) == HIGH) {
    uRC2StartHigh = micros();
  } else if (uRC2StartHigh && !bNewRC2Pulse) {
    nRC2PulseWidth = (int)(micros() - uRC2StartHigh);
    uRC2StartHigh  = 0;
    bNewRC2Pulse   = true;
  }
}

void pwmRC1_Color() {
  if (digitalRead(pinRC1_Color) == HIGH) {
    uRC1StartHigh = micros();
  } else if (uRC1StartHigh && !bNewRC1Pulse) {
    nRC1PulseWidth = (int)(micros() - uRC1StartHigh);
    uRC1StartHigh  = 0;
    bNewRC1Pulse   = true;
  }
}
//---------------------------------------------------
/* LED Control */
//---------------------------------------------------

// LED Color 밝기 조절 변수
volatile int ONOFF = 0;
volatile int RED_Brightness = 0;
volatile int GREEN_Brightness = 0;
volatile int BLUE_Brightness = 0;
volatile float LED_Brightness = 0;

/* PWM 값에 따른 ON/OFF 제어 */
void LED_ONOFF_Control() {
  if (bNewRC9Pulse) {
    bNewRC9Pulse = false;
    ONOFF = (nRC9PulseWidth > 1800) ? 1 : 0;
  }
}

/* PWM 값에 따른 밝기 제어 */
void LED_Brightness_Control() {
  if (bNewRC2Pulse) {
    bNewRC2Pulse = false;
    LED_Brightness = map(nRC2PulseWidth, PWM_MIN, PWM_MAX, 0, 100);
  }
}

/* PWM 값에 따른 색 변화 제어 */
void LED_Color_Control() {
  if (ONOFF == 0) {
    // Turn off all LEDs if ONOFF is 0
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
    return;
  }

  if (bNewRC1Pulse) {
    bNewRC1Pulse = false;

    if (nRC1PulseWidth <= COLOR_RED_MAX) { /* PWM <= COLOR_RED_MAX : RED  */
      RED_Brightness = map(nRC1PulseWidth, PWM_MIN, COLOR_RED_MAX, 255, 0);
      GREEN_Brightness = BLUE_Brightness = 0;
    } 
    else if (nRC1PulseWidth <= COLOR_GREEN_MAX) { /* COLOR_GREEN_MIN <= PWM <= COLOR_GREEN_MAX : GREEN */
      if (nRC1PulseWidth <= COLOR_GREEN_MID) {
        GREEN_Brightness = map(nRC1PulseWidth, COLOR_GREEN_MIN, COLOR_GREEN_MID, 0, 255);
      } else {
        GREEN_Brightness = map(nRC1PulseWidth, COLOR_GREEN_MID, COLOR_GREEN_MAX, 255, 0);
      }
      RED_Brightness = BLUE_Brightness = 0;
    }
    else if (nRC1PulseWidth <= PWM_MAX) { /* PWM >= COLOR_BLUE_MIN : BLUE */
      BLUE_Brightness = map(nRC1PulseWidth, COLOR_BLUE_MIN, PWM_MAX, 0, 255);
      GREEN_Brightness = RED_Brightness = 0;
    }

    // LED_Brightness_Control() 에서 설정한 밝기를 적용
    RED_Brightness   = (int)(RED_Brightness * LED_Brightness / 100);
    GREEN_Brightness = (int)(GREEN_Brightness * LED_Brightness / 100);
    BLUE_Brightness  = (int)(BLUE_Brightness * LED_Brightness / 100);

    // 제어 신호에 대한 결과를 적용
    analogWrite(RED_PIN, constrain(RED_Brightness, 0, 255));
    analogWrite(GREEN_PIN, constrain(GREEN_Brightness, 0, 255));
    analogWrite(BLUE_PIN, constrain(BLUE_Brightness, 0, 255));
  }
}
//---------------------------------------------------

unsigned long prevMillis = 0;            // 마지막으로 출력한 시각 저장
const unsigned long interval = 1000UL;   // 1초 (1000ms)

void loop() {
  /* PWM 신호 확인을 위한 Serial Monitor 출력 */
  unsigned long now = millis(); 
  if (now - prevMillis >= interval) {
    prevMillis = now;  // 타이머 리셋
    Serial.println(String(nRC9PulseWidth) + " | " + String(nRC2PulseWidth) + " | " + String(nRC1PulseWidth));
  }
  /* LED 제어 */
  LED_ONOFF_Control();
  LED_Brightness_Control();
  LED_Color_Control();
}
