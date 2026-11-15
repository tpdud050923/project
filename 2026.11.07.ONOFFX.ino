
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_sleep_h>

OneWire onewire(2);
DallasTemperature sensors(&onewire);
float t;  

// blue - room T - blue 로 갈때 부저 / led 작동하려면 컵 교체 확인필요
void setup() {
 
 Serial.begin(9600);
 sensors.begin();
 
 pinMode(9,OUTPUT);
 pinMode(10,OUTPUT);
 pinMode(8,OUTPUT); // led 제어

 pinMode(11,OUTPUT); // buzzer 제어

}

// 한번 내려간 후에는 다시 올라가지 않는다, 현재 상태 설정
enum statevalue { red, green,blue };
statevalue tempstate;

int buzzer = 0;
unsigned long buzzertime = 0; 
int buzzerrunning = 0;

enum mode{ 
  waitingchange, //변화 대기중
  waitingsteadystate, // 안정 상태 대기중
  start // 안정 상태 도달 완료-> 안정상태 온도조건에서의 led 점등을 시작
};
mode state = waitingchange;

int totcnt = 5; // 누적할 온도의 갯수
float samples[totcnt]; // sample 배열
int cnt =0; // 몇개 들어왔는가

float changelimit = 3;
float steadylimit = 0.5;
int stablecnt=0; // 안정화 횟수

unsigned long bluestarttime = 0;
unsigned long bluetime = 0 ;
int firstblue = 0;

void setled(statevalue s){
  digitalWrite(9, s == red);
  digitalWrite(10, s == green);
  digitalWrite(8, s== blue );
}

void offled(){
  digitalWrite(9,LOW);
  digitalWrite(10,LOW);
  digitalWrite(8,LOW);
}

void ledcontrol(float t){ // 온도에 따른 led 제어 + 깜빡임 보정 (내려가는 경우만 고려) , 온도가 올라가는 경우는 컵을 교체한 경우로 리셋됨 --> 히스테리시스 삭제가능
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
    
      state =waitingchange;
      cnt = 0;
      stablecnt = 0;

    
  }
  serial.printin(tempstate);
  serial.println(t);
  setled(tempstate);

}

void buzzercontrol(){
  if (tempstate == green&& buzzer ==0 && buzzerrunning == 0 ){
    digitalWrite(11,HIGH);
    buzzertime = millis();
    buzzerrunning = 1;
    buzzer = 1;
  }
  if (buzzerrunning == 1){
    if (millis()-buzzertime >=1000){
      digitalWrite (11,LOW);
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

  for (int i=1;i<5;i++){
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

void loop(){

  sensors.requestTemperatures();
  t = sensors.getTempCByIndex(0);

  add(t);

  if (cnt<5){ // 배열 다 차기 전인경우 (2초 이하일떄) --> delay 클수도 있다
    return;
  }

  if (state == waitingchange){ // 전원 on 에서 변화 시작까지
    
    if (maxminus()>=changelimit){
      state = waitingsteadystate;
      stablecnt = 0; 
    }
    
  }
 
  else if (state == waitingsteadystate){ // 변동에서 안정화상태까지
    if (maxminus()<=steadylimit){
      stablecnt++;
      
    }
    else{
      stablecnt = 0; // ** 안정화상태에서 벗어나는 경우 cnt 누적 초기화 --> 안 해도 될수도있음
    }

    if (stablecnt >=3){ // 안정화 시  최초 led 설정
      state =start; 
      buzzer = 0;
      if (t>60){
        tempstate = red; 
      }
      else if (t>=45){ 
        tempstate = green; 
      }
      else { 
        tempstate = blue;
      }
      setled(tempstate);
      buzzercontrol();
    }
  }
  else if (state ==start){ // 안정화 상태에서의 led 온도에 따른 제어
    
    if (maxminus()>= changelimit){ // 안정화 상태 벗어날 경우 
      state = waitingsteadystate;
      stablecnt=0; 
      cnt = 0;
      offled();

    }
    else{ 
      ledcontrol(t);
      buzzercontrol();
      
    }
  }
}
