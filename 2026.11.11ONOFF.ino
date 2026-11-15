
#include <OneWire.h>
#include <DallasTemperature.h>
# include <esp_sleep.h> 

# define WAKEUP_PIN GPIO_NUM_3 

// 가변저항 설정 . 온도보정 - 2 .  들고다니면서 온도 흔들리는경우 . deep sleep mode, 작동 모드 전력 측정, 배터리 잔량  , 가변저항으로 온도 커스텀 기능
OneWire onewire(2); // pull up 저항 연결
DallasTemperature sensors(&onewire);
float t; 


unsigned long buttontime = 0;

void setup() {
  
 Serial.begin(9600);
 sensors.begin();

 pinMode (3,INPUT_PULLDOWN); // 기기 작동용 푸쉬버튼 설정

 pinMode(4,OUTPUT);  //red led
 pinMode(5,OUTPUT); // green led
 pinMode(6,OUTPUT); // blue led 

 pinMode(7,OUTPUT); // buzzer 제어

 pinMode (21, INPUT_PULLUP); // 

 esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause(); 

 if (cause == ESP_SLEEP_WAKEUP_GPIO){
  while (digitalRead(3)== HIGH ){
    delay(10); 
  } 
  buttontime =millis(); // 버튼을 놓을 때까지 대기

 }
 else{
  waitbutton();
 }

}

enum statevalue { red, green, blue };
statevalue tempstate;

int buzzer = 0;
unsigned long buzzertime = 0;
int buzzerrunning = 0;

enum mode{ 
  waitingstablestate, // 안정 상태 대기중
  start // 안정 상태 도달 완료-> 안정상태 온도조건에서의 led 점등을 시작
};
mode state = waitingstablestate ; 

const int totcnt = 5; // 누적할 온도의 갯수
float samples[totcnt]; // sample 배열
int cnt =0; // 몇개 들어왔는가

float changelimit = 3;
float stablelimit = 0.5;
int stablecnt=0; // 안정화 횟수
int changecnt = 0;

unsigned long bluestarttime = 0;
int firstblue = 1;

void setled(statevalue s){
  digitalWrite(4, s == red);
  digitalWrite(5, s == green);
  digitalWrite(6, s== blue );
}

void offled(){
  digitalWrite(4,LOW);
  digitalWrite(5,LOW);
  digitalWrite(6,LOW);
}

void ledcontrol(){ // 온도에 따른 led 제어 + 깜빡임 보정 (내려가는 경우만 고려) ,
  if (tempstate == red){
    if (t<=60){
      tempstate = green;
    }
  }
  else if (tempstate ==green){
    
    if (t<=45){
      tempstate =blue;
    }
    else {
      tempstate =green;
    }
  }
  else if (tempstate == blue){
    if (firstblue == 1){
      bluestarttime = millis();
      firstblue = 0;
    }
    else{
      if (millis()-bluestarttime>=5000){
        gosleep();
        
      }
    }
  }
  
  setled(tempstate);

}

void buzzercontrol(){
  if (digitalRead(21)== HIGH){
    digitalWrite (7,LOW);
    buzzerrunning = 0;
    buzzertime = 0 ;
    return;
  }
  if (tempstate == green&& buzzer ==0 && buzzerrunning == 0 ){
    digitalWrite(7,HIGH);
    buzzertime = millis();
    buzzerrunning = 1;
    buzzer = 1;
  }
  if (buzzerrunning == 1){
    if (millis()-buzzertime >=1000){
      digitalWrite (7,LOW);
      buzzerrunning = 0 ;
      buzzertime = 0;
    }
  }

}

void add(float t){ //sample 배열에 온도값 저장
  if (cnt<totcnt){
    samples[cnt++]= t;
  }
  else{
    for (int i=0;i<totcnt-1;i++){
      samples[i]=samples[i+1];  
    }
    samples[totcnt-1]=t;
  }
}

float maxminus(){ // 배열의 max-min 값 반환
  float maxtemp = samples[0];
  float mintemp = samples[0];

  for (int i=0;i<totcnt;i++){
    if (samples[i]>maxtemp){
      maxtemp=samples[i];
    }
    if (samples[i]<mintemp){
      mintemp=samples[i];
    }
  }
  float result= maxtemp-mintemp;
  return result;
}

void waitbutton(){ 
  while (digitalRead(3)==LOW){
    delay(10);
  }
  
  while (digitalRead(3)==HIGH){
    delay(10);
  }
  buttontime = millis();
}



void gosleep(){
  offled();
  digitalWrite(7,LOW);

  esp_deep_sleep_enable_gpio_wakeup(
    1ULL << WAKEUP_PIN, // gpio 3 를 wakeupbutton 으로 설정
    ESP_GPIO_WAKEUP_GPIO_HIGH // gpio 3 high 일떄 wakeup
  );
  
  esp_deep_sleep_start();

}

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

  analogWrite(4, r);
  analogWrite(5, g);
  analogWrite(6, b);
}

void sleepindicator() {
  analogWrite(4, 0);
  analogWrite(5, 0);

  for (int i = 100; i >= 0; i -= 2) {
    analogWrite(6, i);
    delay(30);
  }

  offled();
}

void loop (){
  if (state == waitingstablestate){
    rainbowled();
  }

  sensors.requestTemperatures();
  t = sensors.getTempCByIndex(0);

  if (t == -127){
    return ;
  }

  add(t);

  if (cnt<totcnt){
    return ;
  } // 배열 다 안찼으면 바로 Return 

  float change = samples[totcnt-1]- samples[0];

  if (change <=-changelimit){
    changecnt++;
  }
  else{
    changecnt = 0;
  }   

  if (maxminus()<stablelimit){
    stablecnt++;
  }
  else{
    stablecnt = 0;
  }

  if (changecnt>=3){

    gosleep();
  }


  if (state == waitingstablestate ){

    if (stablecnt>=3 || millis() - buttontime >10000){
      offled();
      state = start;
      if (t>60){
        tempstate =red;
      }
      else if (t>45){
        tempstate = green;
      }
      else{
        tempstate = blue;
      }
    }
  }
  else if (state == start){
    
   
    ledcontrol();
    buzzercontrol();
  }
}
