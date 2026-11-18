

#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_sleep.h>

// =====================================
// 핀 설정
// =====================================

#define WAKEUP_PIN GPIO_NUM_5

#define SENSOR_PIN D2
#define BUTTON_PIN D3

#define RED_LED D4
#define GREEN_LED D5
#define BLUE_LED D6

#define BUZZER_PIN D7

// =====================================
// 자료형 선언
// =====================================

enum statevalue { red, green, blue };

enum mode {
  waitingstablestate,
  start
};


// =====================================
// 전역 변수
// =====================================

// 온도 센서
OneWire onewire(SENSOR_PIN);
DallasTemperature sensors(&onewire);

float t = 0;

// 비동기 온도 측정
unsigned long tempRequestTime = 0; // 측정시간
const unsigned long conversionTime = 750; // 750ms 마다 한 번 온도측정
bool conversionPending = false;  //변환 진행 중인가


// 버튼
unsigned long buttontime = 0;


// LED 상태
statevalue tempstate;


// 부저
int buzzer = 0;
unsigned long buzzertime = 0;
int buzzerrunning = 0;


// 작동 상태
mode state = waitingstablestate;


// 온도 샘플
const int totcnt = 5;
float samples[totcnt];

int cnt = 0;


// 온도 변화 및 안정화 조건
float changelimit = 3;
float stablelimit = 0.5;

int stablecnt = 0;
int changecnt = 0;


// 파란색 LED 및 수면 시간
unsigned long bluestarttime = 0;
int firstblue = 1;


// =====================================
// 함수 선언
// =====================================

void waitbutton();
void gosleep();

void setled(statevalue s);
void offled();

void ledcontrol();
void buzzercontrol();

void add(float t);
float maxminus();

void rainbowled();
void sleepindicator();

void processTemperature();


// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(9600);
  delay(500);

  Serial.println("System Start");

  // 온도 센서 초기화
  sensors.begin();

  // 비동기 온도 측정 설정
  sensors.setWaitForConversion(false); // 대기 시간 없이 넘어감


  // 버튼 설정
  pinMode(BUTTON_PIN, INPUT_PULLDOWN); 


  // LED 설정
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);


  // 부저 설정
  pinMode(BUZZER_PIN, OUTPUT);

  // 웨이크업 원인 확인
  esp_sleep_wakeup_cause_t cause =
    esp_sleep_get_wakeup_cause();


  if (cause == ESP_SLEEP_WAKEUP_GPIO) {

    Serial.println("Wakeup by button");

    // 버튼을 놓을 때까지 대기
    while (digitalRead(BUTTON_PIN) == HIGH) {
      delay(10);
    }

    buttontime = millis();
  }

  else {

    Serial.println("Waiting for button");

    waitbutton();
  }


  Serial.println("Button pressed");
  Serial.println("Temperature measurement start");


  // 첫 번째 온도 측정 요청
  sensors.requestTemperatures();

  tempRequestTime = millis();
  conversionPending = true;

}


// =====================================
// LED 제어
// =====================================

void setled(statevalue s) {

  analogWrite(RED_LED, s == red ? 255 : 0);
  analogWrite(GREEN_LED, s == green ? 255 : 0);
  analogWrite(BLUE_LED, s == blue ? 255 : 0);

}


void offled() {

  analogWrite(RED_LED, 0);
  analogWrite(GREEN_LED, 0);
  analogWrite(BLUE_LED, 0);

}


// =====================================
// 온도에 따른 LED 제어
// =====================================

void ledcontrol() {

  if (tempstate == red) {

    if (t <= 60) {
      tempstate = green;
    }

  }

  else if (tempstate == green) {

    if (t <= 45) {
      tempstate = blue;
    }

  }

  else if (tempstate == blue) {

    if (firstblue == 1) {

      bluestarttime = millis();
      firstblue = 0;

    }

    else {

      if (millis() - bluestarttime >= 5000) {

        Serial.println("Blue state -> Deep Sleep");

        gosleep();

      }

    }

  }

  setled(tempstate);

}


// =====================================
// 부저 제어
// =====================================

void buzzercontrol() {

  // Green 진입 시 최초 1회 작동
  if (tempstate == green &&
      buzzer == 0 &&
      buzzerrunning == 0) {

    digitalWrite(BUZZER_PIN, HIGH);

    buzzertime = millis();

    buzzerrunning = 1;
    buzzer = 1;

  }


  // 1초 후 부저 종료
  if (buzzerrunning == 1) {

    if (millis() - buzzertime >= 1000) {

      digitalWrite(BUZZER_PIN, LOW);

      buzzerrunning = 0;
      buzzertime = 0;

    }

  }

}


// =====================================
// 온도 샘플 저장
// =====================================

void add(float temp) {

  if (cnt < totcnt) {

    samples[cnt++] = temp;

  }

  else {

    for (int i = 0; i < totcnt - 1; i++) {

      samples[i] = samples[i + 1];

    }

    samples[totcnt - 1] = temp;

  }

}


// =====================================
// 최대 온도 - 최소 온도
// =====================================

float maxminus() {

  float maxtemp = samples[0];
  float mintemp = samples[0];

  for (int i = 0; i < totcnt; i++) {

    if (samples[i] > maxtemp) {
      maxtemp = samples[i];
    }

    if (samples[i] < mintemp) {
      mintemp = samples[i];
    }

  }

  return maxtemp - mintemp;

}


// =====================================
// 버튼 대기
// =====================================

void waitbutton() {

  // 버튼을 누를 때까지 대기
  while (digitalRead(BUTTON_PIN) == LOW) {
    delay(10);
  }


  // 버튼을 놓을 때까지 대기
  while (digitalRead(BUTTON_PIN) == HIGH) {
    delay(10);
  }


  buttontime = millis();

}


// =====================================
// Deep Sleep
// =====================================

void gosleep() {

  offled();

  digitalWrite(BUZZER_PIN, LOW);


  // GPIO5 버튼 웨이크업 설정
  esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(
    1ULL << WAKEUP_PIN,
    ESP_GPIO_WAKEUP_GPIO_HIGH
  );


  if (err != ESP_OK) {

    Serial.print("Wakeup setting error: ");
    Serial.println(err);

    return;

  }


  Serial.println("Entering Deep Sleep");

  esp_deep_sleep_start();

}


// =====================================
// 무지개 LED
// =====================================

void rainbowled() {

  int phase = (millis() / 20) % 600;

  int r, g, b;


  if (phase < 100) {

    r = 255;
    g = phase * 255 / 100;
    b = 0;

  }

  else if (phase < 200) {

    r = 255 - (phase - 100) * 255 / 100;
    g = 255;
    b = 0;

  }

  else if (phase < 300) {

    r = 0;
    g = 255;
    b = (phase - 200) * 255 / 100;

  }

  else if (phase < 400) {

    r = 0;
    g = 255 - (phase - 300) * 255 / 100;
    b = 255;

  }

  else if (phase < 500) {

    r = (phase - 400) * 255 / 100;
    g = 0;
    b = 255;

  }

  else {

    r = 255;
    g = 0;
    b = 255 - (phase - 500) * 255 / 100;

  }


  analogWrite(RED_LED, r);
  analogWrite(GREEN_LED, g);
  analogWrite(BLUE_LED, b);

}


// =====================================
// 수면 표시
// =====================================

void sleepindicator() {

  analogWrite(RED_LED, 0);
  analogWrite(GREEN_LED, 0);


  for (int i = 100; i >= 0; i -= 2) {

    analogWrite(BLUE_LED, i);

    delay(30);

  }


  offled();

}


// =====================================
// 온도 데이터 처리
// =====================================

void processTemperature() {

  // 온도 샘플 저장
  add(t);


  // 샘플 5개가 모이지 않으면 대기
  if (cnt < totcnt) {
    return;
  }


  // 최근 온도 변화량
  float change = samples[totcnt - 1] - samples[0];

  if (change <= -changelimit) {
    changecnt++;
  }

  else {
    changecnt = 0;
  }


  if (changecnt >= 3) {

    Serial.println("Sudden temperature change");

    gosleep();

  }

  // 온도 안정화 확인
  if (maxminus() < stablelimit) {
    stablecnt++;
  }

  else {
    stablecnt = 0;
  }


  // 초기 안정화 상태
  if (state == waitingstablestate) {

    if (stablecnt >= 3 ||
        millis() - buttontime > 10000) {

      offled();

      state = start;

      changecnt = 0;
      stablecnt = 0;


      if (t > 60) {
        tempstate = red;
      }

      else if (t > 45) {
        tempstate = green;
      }

      else {
        tempstate = blue;
      }


      Serial.println("Stabilization complete");

    }

  }

}


// =====================================
// LOOP
// =====================================

void loop() {


  // 초기 안정화 중 무지개 LED
  if (state == waitingstablestate) {

    rainbowled();

  }
  
  // 비동기 온도 변환 완료 확인
  if (conversionPending &&
      millis() - tempRequestTime >= conversionTime) {


    // 변환된 온도 읽기
    t = sensors.getTempCByIndex(0);

    conversionPending = false;


    // 센서 연결 오류 확인
    if (t == DEVICE_DISCONNECTED_C) {

      Serial.println("Device Disconnected");

    }

    else {

      Serial.print("Temperature: ");
      Serial.println(t);

      processTemperature();

    }


    // 다음 온도 측정 요청
    sensors.requestTemperatures();

    tempRequestTime = millis();

    conversionPending = true; 

  }


  // 실제 온도 표시 작동
  if (state == start) {

    ledcontrol();

    buzzercontrol();

  }

}

