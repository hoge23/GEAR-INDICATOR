#include "arduino_secrets.h"

#include "LedControl.h"
#include <avr/pgmspace.h>

// ==================================================
// 1. GPIOãã³è¨­å®
// ==================================================
LedControl lc(2, 4, 3, 1); // DIN=2, CLK=4, CS=3

const int PIN_ILLUM = 6;
const int SENSOR_PINS[] = {A0, A1, A2, A3};
const int NUM_SENSORS = 4;

// ==================================================
// 2. ã°ã­ã¼ãã«å¤æ°
// ==================================================
int rawValues[NUM_SENSORS];
int diffValues[NUM_SENSORS];

// ==================================================
// 3. æ ¡æ­£å¤ã»ãã©ã¡ã¼ã¿
// ==================================================
const int  NEUTRAL_BASELINE[NUM_SENSORS] = {409, 420, 395, 427};
const long DIST_THRESHOLD = 30000;
const int  LOOP_DELAY_MS  = 100;

// ==================================================
// 4. ã®ã¢å®ç¾©ã»å¤å®ãã¼ãã«
// ==================================================
enum Gear { GEAR_1, GEAR_2, GEAR_3, GEAR_4, GEAR_5, GEAR_R, GEAR_N, GEAR_UNKNOWN };

struct GearPoint {
  Gear gear;
  int  diff[NUM_SENSORS];
};

const GearPoint GEAR_TABLE[] PROGMEM = {
  {GEAR_1, {-143,  73,  -2,   58}},
  {GEAR_2, {  90, -173,  84,  -28}},
  {GEAR_3, { -75,  69,  -51,   57}},
  {GEAR_4, {  79, -74,   83,  -93}},
  {GEAR_5, {  30,  73, -201,   77}},
  {GEAR_R, {  78,  24,  107, -230}},
  {GEAR_N, {   0,   0,    0,    0}},
  {GEAR_N, { -21, -106,  48,   27}}, // 1â2é
  {GEAR_N, {  43,  54, -171,    3}}  // 5âRé
};
const int NUM_GEAR_POINTS = sizeof(GEAR_TABLE) / sizeof(GEAR_TABLE[0]);

// ==================================================
// 5. ãã©ã³ãï¼8x8ã»å¤æ´ãªãï¼
// ==================================================
const byte FONT_N[] PROGMEM = {
  B11000011,
  B11100011,
  B11110011,
  B11111011,
  B11011111,
  B11001111,
  B11000111,
  B11000011
};

const byte FONT_1[] PROGMEM = {
  B00011000,
  B00111000,
  B01111000,
  B00011000,
  B00011000,
  B00011000,
  B00011000,
  B00011000
};

const byte FONT_2[] PROGMEM = {
  B00111100,
  B01100110,
  B01100110,
  B00000110,
  B00111100,
  B01100000,
  B01111110,
  B01111110
};

const byte FONT_3[] PROGMEM = {
  B00111100,
  B01111110,
  B01000110,
  B00011100,
  B00011100,
  B01000110,
  B01111110,
  B00111100
};

const byte FONT_4[] PROGMEM = {
  B00011100,
  B00111100,
  B01101100,
  B11001100,
  B11111111,
  B11111111,
  B00001100,
  B00001100
};

const byte FONT_5[] PROGMEM = {
  B01111110,
  B01111110,
  B01100000,
  B01111100,
  B00001110,
  B01100110,
  B01111110,
  B00111100
};

const byte FONT_R[] PROGMEM = {
  B11111100,
  B11001110,
  B11000110,
  B11001110,
  B11111000,
  B11011100,
  B11001110,
  B11000111
};

// ==================================================
// 6. è¡¨ç¤ºã»å¤å®é¢æ°
// ==================================================

// å³90Â°åè»¢ãã¦æç»ï¼ã¡ãã¤ãå¯¾ç­æ¸ã¿ï¼
void drawBitmap(const byte* bitmap) {
  if (!bitmap) {
    lc.clearDisplay(0);
    return;
  }

  byte rotated[8] = {0};

  // åè»¢å¤æï¼ (x,y) â (7-y, x)
  for (int y = 0; y < 8; y++) {
    byte src = pgm_read_byte(&bitmap[y]);
    for (int x = 0; x < 8; x++) {
      if (src & (1 << (7 - x))) {
        rotated[x] |= (1 << y);
      }
    }
  }

  // ã¾ã¨ãã¦æç»ï¼ã¡ãã¤ããªãï¼
  for (int row = 0; row < 8; row++) {
    lc.setRow(0, row, rotated[row]);
  }
}

void displayGear(Gear gear) {
  switch (gear) {
    case GEAR_1: drawBitmap(FONT_1); break;
    case GEAR_2: drawBitmap(FONT_2); break;
    case GEAR_3: drawBitmap(FONT_3); break;
    case GEAR_4: drawBitmap(FONT_4); break;
    case GEAR_5: drawBitmap(FONT_5); break;
    case GEAR_R: drawBitmap(FONT_R); break;
    case GEAR_N: drawBitmap(FONT_N); break;
    default:     drawBitmap(NULL);   break;
  }
}

Gear detectGear() {
  long minDist = -1;
  Gear result  = GEAR_UNKNOWN;

  for (int i = 0; i < NUM_GEAR_POINTS; i++) {
    long sumSq = 0;
    for (int s = 0; s < NUM_SENSORS; s++) {
      int target = (int)pgm_read_word(&(GEAR_TABLE[i].diff[s]));
      long d = (long)diffValues[s] - target;
      sumSq += d * d;
    }
    if (minDist < 0 || sumSq < minDist) {
      minDist = sumSq;
      result  = (Gear)pgm_read_byte(&(GEAR_TABLE[i].gear));
    }
  }
  return (minDist > DIST_THRESHOLD) ? GEAR_UNKNOWN : result;
}

// ==================================================
// 7. Arduino æ¨æºå¦ç
// ==================================================
void setup() {
  Serial.begin(9600);
  pinMode(PIN_ILLUM, INPUT_PULLUP);

  lc.shutdown(0, false);
  lc.setIntensity(0, 1);
  lc.clearDisplay(0);

  Serial.println(F("System Online"));
}

void loop() {
  // ã»ã³ãµã¼èª­ã¿åã
  for (int i = 0; i < NUM_SENSORS; i++) {
    rawValues[i]  = analogRead(SENSOR_PINS[i]);
    diffValues[i] = rawValues[i] - NEUTRAL_BASELINE[i];
  }

  // ã®ã¢å¤å®
  static Gear lastGear = GEAR_UNKNOWN;
  Gear currentGear = detectGear();

  // è¡¨ç¤ºã¯å¤åæã®ã¿ï¼å®å®åï¼
  if (currentGear != lastGear) {
    displayGear(currentGear);
    lastGear = currentGear;
  }

  // è¼åº¦å¶å¾¡
  lc.setIntensity(0, digitalRead(PIN_ILLUM) ? 10 : 1);

  // ãããã°åºå
  Serial.print(F("RAW: "));
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(rawValues[i]);
    Serial.print(i < NUM_SENSORS - 1 ? ',' : ' ');
  }

  Serial.print(F("| DIFF: "));
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(diffValues[i]);
    Serial.print(i < NUM_SENSORS - 1 ? ',' : ' ');
  }

  static const char* const names[] = {"1","2","3","4","5","R","N","?"};
  Serial.print(F("| GEAR: "));
  Serial.println(names[currentGear]);

  delay(LOOP_DELAY_MS);
}
