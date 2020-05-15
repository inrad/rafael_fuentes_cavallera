
//programa para el control de una unidad de cultivo inteligente 



#include <SoftwareSerial.h>        //  libreria para comunicacion serie virtual
#include <Wire.h>                  //  libreria para interfaz I2C
#include "uRTCLib.h"               // libreria para el manejo del modulo RTC
#include <NewPing.h>               // libreria para manejo del sensor de distancia 
#include <EEPROM.h>                // libreria para guardar valores no volatiles de tiempos de produccion
#include  <Servo.h>                // libreria control del servo para ajustar presencia
#include <SimpleDHT.h>             // libreria para el manejo del sensor der temperatura




#define LLAVE_INICIO 13           //puldador de start
#define STEP 6                    // control PAP
#define DIR 5                     //.........
#define HUMEDAD_SUSTRATO A0       // valor bruto humedad del sustrato
#define NIVEL_AGUA 3              // nivel de agua 0=falta, 1=ok
#define NIVEL_ABONO 12            // nivel de abono 0=falta, 1=ok
#define BOMBA_AGUA 15             // actuador para regar
#define BOMBA_ABONO 16            // actuador para regar con fertilizante
#define AIREADOR 17               // bomba para oxigenar y revolver el riego
#define LED_FULL 0                // led principal, sol del cultivo
#define LED_EMERGENCIA 1          // testigo de emergencias o falta de algo
#define PINDHT11 9                // sensor de temperatura y humedad
#define FC_POSTE 2                // interrupcion externa final de carrera del poste


SimpleDHT11 dht11(PINDHT11);
Servo myservo;    
NewPing sonar(7,8,45);            // (TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE)          sensor de distancia
uRTCLib rtc;
SoftwareSerial BLT(10, 11);       // (RX,TX)



int valor_sensor_distancia;                            // sensor ultrasonico hc-sr04
byte velocidad_pap = 20;                               // velocidad del motor paso a paso
byte posicion_servo=0;                                 //control del servo
byte flag_obstaculo_encontrado=1;                      // variables auxiliares para control del sensor ultrasonic
byte contador_obstaculos;                              // .............
byte contador_intentos;                                // aulixiliar de limite de repeticiones de ajuste
String fecha_inicio;                                   // fecha de inicio del cultivo
int address_fecha_inicio=55;                           // "metodo" para controlar fecha de inicio del cultivo..
int address_fecha_ya_establecida=1;                    // ..no volatil                                
int repetir_ajuste_luz;                                // variables para cantidad de repeticiones de ajuste de luz
int valor_humedad;
int nivel_humedad_sustrato;                             // valores de 1 a 4 humedad del sustrato

         
void setup() {
  
 BLT.flush();                                          //hc-06
 delay(500);
 BLT.begin(9600);
 Serial.begin(9600);       
 Wire.begin();
 
 //rtc.set_rtc_address(0x68);   //una sola vez
  
 //  rtc.set(0, 42, 16, 6, 2, 5, 15);                // usar una sola vez 
 //  RTCLib::set(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)

 //EEPROM.write(2,0);   activar solamente una solo vez.. 
 //EEPROM.write(500,0);    ..idem





pinMode (LLAVE_INICIO,INPUT_PULLUP);
pinMode (STEP, OUTPUT);
pinMode (DIR, OUTPUT); 
pinMode (HUMEDAD_SUSTRATO, INPUT);
pinMode (NIVEL_AGUA, INPUT);
pinMode (NIVEL_ABONO, INPUT);
pinMode (BOMBA_AGUA, OUTPUT);
pinMode (BOMBA_ABONO, OUTPUT);
pinMode (AIREADOR, OUTPUT);
pinMode (LED_FULL, OUTPUT);
pinMode (LED_EMERGENCIA, OUTPUT);
pinMode (PINDHT11, INPUT);

myservo.attach(4);
attachInterrupt(digitalPinToInterrupt(FC_POSTE), parar_todo, RISING);   

}

void loop() {
 
rtc.refresh();
  delay(50);

 
   while (LLAVE_INICIO==0)
   {
   Serial.print ("presionar boton de inicio");       
   delay (2000);
   }
             
   if (EEPROM.read(address_fecha_ya_establecida)==!1)
   {
      fecha_actual();
      fecha_inicio = fecha_actual();
      EEPROM.put (address_fecha_inicio,fecha_inicio);
      delay(200);
      EEPROM.write(address_fecha_ya_establecida,1);
   }

  comunicacion_bluetooth();

  rtc.refresh();
  digitalWrite (LED_FULL, HIGH);  
  if (rtc.dayOfWeek()== 2){
    while (repetir_ajuste_luz<3){
       for (posicion_servo=0; posicion_servo<150; posicion_servo= posicion_servo+30){
          do {
            myservo.write(posicion_servo); 
            delay (300);
            lectura_sensor_obstaculo();
                if (lectura_sensor_obstaculo()==1){
                   digitalWrite (DIR, HIGH);
                   digitalWrite (STEP,HIGH);
                   delay(velocidad_pap);
                   digitalWrite (STEP,LOW);
                                                  }
             contador_obstaculos=0;
             contador_intentos=0;
              }     
        while (flag_obstaculo_encontrado==1);
           repetir_ajuste_luz++;
      
                                                                                    }
                                  }
                            }

  if(rtc.dayOfWeek()==3)
  repetir_ajuste_luz=0;




int valor_humedad;
int aux_sustrato1;

    
  valor_humedad =analogRead (HUMEDAD_SUSTRATO);
  delay(200);
  aux_sustrato1= constrain(valor_humedad, 250,980);
  delay(200);
  nivel_humedad_sustrato= map(aux_sustrato1,250,980,1,4);
  delay (1500);

byte contador_riego_agua;
byte regar;
int address_contador_riego=500;


  if (nivel_humedad_sustrato >=3)
  {
     contador_riego_agua=EEPROM.read (address_contador_riego);
      if (contador_riego_agua <3)
       regar=1;
       else
       regar=0;


      switch (regar)
      {
          case 1:
  
              if (digitalRead( NIVEL_AGUA)==1)
                    {
                    digitalWrite(BOMBA_AGUA, HIGH);
                    delay (10000);
                    digitalWrite(BOMBA_AGUA, LOW);
                    delay(200);
                    contador_riego_agua++;
                    EEPROM.update (address_contador_riego,contador_riego_agua);

                    }
              else    
              emergencia();
              break;
              
          case 2:
          
              if (digitalRead( NIVEL_ABONO)==1)
                {
               digitalWrite(BOMBA_ABONO, HIGH);
               delay (10000);
               digitalWrite(BOMBA_AGUA, LOW);
               delay(200);
               contador_riego_agua=0;
               EEPROM.update (address_contador_riego,contador_riego_agua);

                }
           
              else
              emergencia();
              break;
      } 
   }


}





// ---------------------------------------------------------------------
   int comunicacion_bluetooth(){
   String ff;
if (BLT.available()){
   char flag;
   flag= BLT.read();
   if (flag=='1'){
  ff=EEPROM.get(address_fecha_inicio,fecha_inicio);
  BLT.print(F("fecha de inicio cultivo"));
  BLT.print(ff);
  BLT.print('\r');
  BLT.print(F("nivel_humedad_sustrato"));
  BLT.print(nivel_humedad_sustrato);
  BLT.print('\r');
  BLT.print(F("humedad y temperatura"));

  
   }
}
   }

String fecha_actual () { 

rtc.refresh();
delay(200);
String mes =String (rtc.month());
String dia = String (rtc.day());
String ano = String (rtc.year());
delay(200); 
String diabarra =String(dia +"/");
String mesbarra = String (mes +"/");
String fechabarramedia = diabarra + mesbarra ;
String fechabarra= fechabarramedia + ano;
delay (200);
return fechabarra;
}
  








int lectura_sensor_obstaculo (){
  
int valor_sonar;
while (contador_intentos<10){
contador_intentos++;
valor_sonar =sonar.convert_cm(sonar.ping_median(20));
delay(100);
if (valor_sonar != 0)
contador_obstaculos++;
                            }

if (contador_obstaculos >=7) 
flag_obstaculo_encontrado=1;
else 
flag_obstaculo_encontrado=0;
return flag_obstaculo_encontrado;
}







void parar_todo(){
 digitalWrite (DIR, LOW);
  digitalWrite (STEP,HIGH);
     delay(20);
     digitalWrite (STEP,LOW);  
  }




   
void emergencia() {

digitalWrite (LED_EMERGENCIA,HIGH);
if (BLT.available())
{
BLT.print(F("estanque de agua o abono bajo el nivel minimo"));
delay(1000);
BLT.println (fecha_actual ());          //variable con la fecha actual
BLT.println();
}
 }




void temp_humed(){


byte temperature = 0;
  byte humidity = 0;
BLT.print((int)temperature); 
BLT.print(" *C, "); 
BLT.print('\r');
BLT.print((int)humidity);
BLT.println(" H");

}







 
