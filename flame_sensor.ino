//  Pin  Function
//   0   RS485 receive
//   1   RS485 transmit
//   2   Pushbutton - Turn on Pilot Gas
//   3   RS485 transmit enable
//   5   LED, Green #2
//   9   Timer Reset, HIGH = enable relay & gas
//  11   Timer Trigger, Rising Edge = turn on gas
//  12   Timer Trigger, Rising Edge = turn on relay
//  14   LED, Red
//  15   Transistor gain measurement
//  16   Flame Measurement Signal
//  17   LED, Green #3 (highest signal)
//  20   Input voltage (12V) measurement (divided by 11)
//  21   LED, Green #1 (lowest signal)
// A12/DAC  AC signal output


IntervalTimer dactimer;
uint16_t dacdata[32];
volatile bool newcycle=false;
uint32_t consecutive_flame_ok_count=0;
elapsedMillis ok_led_millis;
bool ok_led_state;

#define THRESHOLD_TOOMUCH   900
#define THRESHOLD_3_LED    6000
#define THRESHOLD_2_LED   20000
#define THRESHOLD_1_LED   50000 

void setup() {
  pinMode(2, INPUT_PULLUP);
  pinMode(3, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(17, OUTPUT);
  pinMode(21, OUTPUT);
  digitalWrite(11, LOW);
  digitalWrite(12, LOW);
  delay(1);
  digitalWrite(9, HIGH); // enable timer chip
  analogWriteResolution(12);
  analogWriteFrequency(10, 1000);
  analogWrite(10, 2148);
  analogReadAveraging(1); // less ADC charge injection
  for (int i=0; i < 32; i++) {
    dacdata[i] = sinf(float(i) * (2.0 * 3.14159 / 32.0)) * 2046 + 2048;
  }
  dactimer.begin(dacupdate, 31.25);
}

void dacupdate() {
  static int index=0;
  analogWrite(A12, dacdata[index]);
  if (index == 0) newcycle = true;
  index = (index + 1) & 31;
}

void loop() {
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);
  delayMicroseconds(100);
  newcycle = false;
  while (!newcycle) {
    // wait for beginning of AC waveform, gives more
    // reproducible result than starting at random AC phase
  }
  pinMode(16, INPUT_DISABLE);
  elapsedMicros usec = 0;
  while (usec < THRESHOLD_1_LED) {
    int a = analogRead(16);
    if (a > 650) break;
    delayMicroseconds(25); // limit ADC speed, less charge injection
  }
  // flame is actual measurement, smaller numbers = more fire
  unsigned int flame = usec;
  Serial.print("flame = ");
  Serial.println(flame);
  bool flameok = false;
  if (flame >= THRESHOLD_TOOMUCH && flame < THRESHOLD_3_LED) {
    // Strong signal, all 3 LEDs on
    digitalWrite(21, HIGH);
    digitalWrite(5, HIGH);
    digitalWrite(17, HIGH);
    flameok = true;
  } else if (flame >= THRESHOLD_3_LED && flame < THRESHOLD_2_LED) {
    // Moderate signal, 2 LEDs on
    digitalWrite(21, HIGH);
    digitalWrite(5, HIGH);
    digitalWrite(17, LOW);
    flameok = true;
  } else if (flame >= THRESHOLD_2_LED && flame < THRESHOLD_1_LED) {
    // Weak signal, 1 LED on
    digitalWrite(21, HIGH);
    digitalWrite(5, LOW);
    digitalWrite(17, LOW);
    flameok = true;
  } else {
    // No fire seen, all LEDs off
    digitalWrite(21, LOW);
    digitalWrite(5, LOW);
    digitalWrite(17, LOW);
    flameok = false;
  }
  bool gas_triggered = false;
  if (flameok) {
    // Fire detected, any signal strength
    if (consecutive_flame_ok_count < 0x7FFFFFFF) {
      consecutive_flame_ok_count++;
    }
    if (consecutive_flame_ok_count >= 4) {
      // Trigger timers to turn on gas & relay only
      // when 4 consecutive good flame measurements
      digitalWrite(11, HIGH);
      digitalWrite(12, HIGH);
      delayMicroseconds(50);
      digitalWrite(11, LOW);
      digitalWrite(12, LOW);
      gas_triggered = true;
    }
  } else {
    // No fire detected, do nothing.  Timers
    // will automatically turn off gas & relay
    consecutive_flame_ok_count = 0;
  }
  
  if (!gas_triggered && digitalRead(2) == LOW) {
    // Trigger gas only when red button pressed
    digitalWrite(14, HIGH);
    digitalWrite(11, HIGH);
    delayMicroseconds(50);
    digitalWrite(11, LOW);
  } else {
    digitalWrite(14, LOW);
  }
  
  if (ok_led_millis > 1500) {
    ok_led_millis -= 1500;
    if (ok_led_state) {
      ok_led_state = false;
      digitalWrite(13, LOW);
    } else {
      ok_led_state = true;
      digitalWrite(13, HIGH);
    }
  }
  
}
