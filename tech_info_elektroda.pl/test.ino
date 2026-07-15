// original: https://www.elektroda.pl/rtvforum/topic3762233.html

#include <SPI.h>
#define CS 10
//SPI pins
//CLK - D13
//DATA MOSI - D11
//CS - 10


void Send(byte b){
  SPI.transfer(b);
  delayMicroseconds(10);
}


void VFD_init(){
  digitalWrite(CS, LOW);
  Send(B01110000);
  digitalWrite(CS, HIGH);

  delay(500);

  digitalWrite(CS, LOW);
  Send(B01110000);
  digitalWrite(CS, HIGH);  
}

void VFD_clear(){
  digitalWrite(CS, LOW);
  Send(B00010000);
  for(byte i=0;i<=15;i++)
    Send(B00100000);
  digitalWrite(CS, HIGH);  

  delay(10);

  digitalWrite(CS, LOW);
  Send(B00110000);
  for(byte i=0;i<=15;i++)
    Send(B00100000);
  digitalWrite(CS, HIGH);  

  
}

void Text2VFD(String s){
  for(byte i=0;i<=15 && s.charAt(i)!=0;i++){
    digitalWrite(CS, LOW);
    Send(0x10|(15-i));    
    Send(s.charAt(i));
    digitalWrite(CS, HIGH);  
  }
  
}

void VFD_bright(byte b){
  digitalWrite(CS, LOW);
  Send(0x50|b);
  digitalWrite(CS, HIGH);  
}

void CurON(byte b){
  digitalWrite(CS, LOW);
  Send(0x30|b);
  Send(1);
  digitalWrite(CS, HIGH);
}

void CurOFF(byte b){
  digitalWrite(CS, LOW);
  Send(0x30|b);
  Send(0);
  digitalWrite(CS, HIGH);  
}





void setup() {
  delay(100);
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);
  SPI.begin();  
  SPI.beginTransaction(SPISettings(500000, LSBFIRST, SPI_MODE0));
  VFD_init();
  VFD_clear();
  delay(500);
  VFD_bright(7);
}

bool rotate(String &n1,String &n2){
bool change=false;  
  for(byte n=0;n<=15 && n1[n]!=0 && n2[n]!=0 && n1[n]>=32 && n1[n]<=126 && n2[n]>=32 && n2[n]<=126;n++){
    if(n1[n]!=n2[n]){
      n1[n]=n1[n]+1;
      if(n1[n]>126)
        n1[n]=32;
        
      change=true;
    }
    
  }
  
  return change;
}

void loop() {

  VFD_clear();
  Text2VFD("    ArturAVS    ");
  
  delay(2000); 

  String n1="    ArturAVS    ";
  String n2="[ elektroda.pl ]";
  String n3="- - - 2020 - - -";

  Text2VFD(n1);
  delay(500);
  do{
    Text2VFD(n1);
    delay(60);
  }while (rotate(n1,n2));
  
  Text2VFD(n1);
  delay(1000);
  do{
    Text2VFD(n1);
    delay(60);
  }while (rotate(n1,n3));

  Text2VFD(n1);
  delay(1000);
  
  Text2VFD("    ArturAVS    ");
  for(byte n=0;n<=20;n++){
    for(char k=15;k>=0;k--){
      delay(20);
      CurON(k);
    }
    for(char k=15;k>=0;k--){
      delay(20);
      CurOFF(k);
    }

 }

  VFD_clear();

  Text2VFD("[ elektroda.pl ]");

  delay(3000);

String baner="               [ elektroda.pl ]               ";  

  for(byte n=0;n<=20;n++){
    for(char k=31;k>0;k--){
      Text2VFD(baner.substring(k,k+16));
      delay(100);
    }  
    VFD_clear();
    delay(100);
  }

 

  
  

}
