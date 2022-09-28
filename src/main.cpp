#include <Arduino.h>
#include <EEPROM.h>
    /*--LOGICA--
   Bulbo presion aceite-> Solo un cable(utilizo el bulbo original(N/A)). Bulbo Temperatura-> por el momento solo un cable(es N/A(solo envia corriente cuando esta sobrepasado de temperatura)). Carga Bat-> toma corriente de la bateria misma.12.5 o < PocaCarga. 14,5 o >.DemasiadaCarga.
------ I D E A S  ------------*/

byte aceiteActual;
byte aceiteDespues;
byte tempActual;
byte tempDespues;

// Posición de MEMORIA Eeprom valores antes y pos encendido
const byte actualAceiteEeprom = 1; //ESPACIO de valor antes del fallo de aceite y que en su proxima encendida se debe actualizar.
const byte fallaAceiteEeprom = 2; //ESPACIO de  valor que debe cambiar cuando se produce la falla.
const byte actualTempEeprom = 3;
const byte fallaTempEeprom = 4;

//INVENTAR: si cada 10" está SIN inputAceite, acumular en variable para que,si en reiteradas ocaciones de todos modos lo termine apagando.
byte inputPres = A0; //representa la presion de aceite. \Esto SE REEMPLAZA(cuando este en el auto)
byte inputTemp = A1; //repre la temperatura.                         \Esto SE REEMPLAZA(cuando este en el auto)
int inputBat = A2; //repre la carga bateria.                          \Esto SE REEMPLAZA(cuando este en el auto)
const byte ledAceite = 13;
const byte ledTemp = 12;
const byte ledBat = 11;
const byte sound = 8;
byte ledOk  = 4;
byte pres ; // Variable GLOBAL de presion aceite.
int temp; // var GLOBAL de temperatura.
int bat ; // var Global carga de bateria.
const double batLow = 120.3; // var de bateria(bajo(parpadea))
const double batHigh = 150.7 ;// var de bateria(Alto(constante)!)
const short int espera = 2000;
byte cortaCorriente = 9;
bool estadoArranque = false;
bool valor = true;
byte prolongarApagado = LOW;
byte ledFalla = ledOk;
byte senalArranque = 5; /*mmm*/


void enContacto();
void dioArranque();
void problemaPresionAceite(bool);
void problemaTemperatura(bool);
void problemaCargabateria(bool);
void refrescarDatos();
void btnMasTiempo();
void interrumpirApagado(byte);
void interm_ledYsound(byte);
void apagarMotor();

/* CTRL + F -> busco palabras especificas  || CTROL + D -> delete */


void setup() {

  Serial.begin(115000);

  pinMode(ledAceite, OUTPUT);
  pinMode(ledTemp, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(ledOk, OUTPUT);
  pinMode(sound, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(inputPres, INPUT_PULLUP);
  pinMode(inputTemp, INPUT_PULLUP);
  pinMode(inputBat, INPUT);
  pinMode(7, INPUT);
  digitalWrite(cortaCorriente, LOW); // CortaCorriente del motor N/A(No esta activado)
  pinMode(senalArranque,INPUT);
  pinMode(2, LOW);
  pinMode(3, INPUT_PULLUP);
  // Configuramos los pines de interrupciones para que detecten un cambio de bajo a alto
  attachInterrupt(digitalPinToInterrupt(2), dioArranque, FALLING);
  attachInterrupt(digitalPinToInterrupt(3), btnMasTiempo, FALLING);
  
  //lectura memorias eeproms
  aceiteActual = EEPROM.read(actualAceiteEeprom);  // valor que tiene guardado antes de la falla y que persistirá hasta el nuevo encendido.
  aceiteDespues = EEPROM.read(fallaAceiteEeprom); //valor que se incrementa cuando se genere un apagado por falla de aceite.
  tempActual = EEPROM.read(actualTempEeprom);
  tempDespues = EEPROM.read(fallaTempEeprom);

  if(aceiteActual != aceiteDespues){
    //enciendo led rojo parpadeante y acualizo el valor en  la eeprom.
    EEPROM.update(actualAceiteEeprom, aceiteDespues);
    digitalWrite(ledAceite,1);     
  }

  if(tempActual != tempDespues){
    //enciendo led rojo parpadeante y acualizo el valor en  la eeprom.
    EEPROM.update(actualTempEeprom, tempDespues);
    digitalWrite(ledTemp,1);     
  }
  
  enContacto();
}

void refrescarDatos(){
    temp = digitalRead(inputTemp);
    pres = digitalRead(inputPres);
    bat = analogRead(inputBat);
    return;
}


//  BATERIA      12.3LOW    ---   14.7High
void problemaCargabateria(bool verifBat){
  while (verifBat == true){
    ledFalla = ledBat;
    digitalWrite(ledOk, 0);
    digitalWrite(ledBat, 1);
    delay(500);
    digitalWrite(ledBat, 0);
    delay(500);
    Serial.println((String)"bat: " + bat );
    refrescarDatos();
	  verifBat = bat < batLow || bat > batHigh;
    bool verifTemp = temp ==  LOW && pres ==  HIGH ;
    if(verifTemp){problemaTemperatura(verifTemp); }
    bool verifPresAceite = pres == LOW;
    if(verifPresAceite){problemaPresionAceite(verifPresAceite);}

    if(prolongarApagado == HIGH){ interrumpirApagado(ledFalla);ledFalla = ledOk;return; } 
  }
}

// TEMPERATURA mala = LOW || 0 !
void problemaTemperatura(bool verifTemp) {
  if (verifTemp) {
    digitalWrite(ledOk, 0);
    delay(espera);
    refrescarDatos();
    for (byte i = 0; i <= 10 && verifTemp == true ; i++){
      ledFalla = ledTemp;
      interm_ledYsound(ledFalla);
      refrescarDatos();
      verifTemp = temp ==  LOW && pres ==  HIGH ;
      Serial.println((String)"verifTemp:"+ verifTemp);
      if(prolongarApagado == HIGH){  interrumpirApagado(ledFalla);return; }
      }
      if(temp == LOW) {
        Serial.println("Temperatura: Apagar motor");

        tempActual = EEPROM.read(actualTempEeprom);
        EEPROM.write(fallaTempEeprom, (tempActual+1));
        tempDespues = EEPROM.read(fallaTempEeprom);
        Serial.println((String)"tempActual "+tempActual +" vs "+ tempDespues);

        apagarMotor();
        while(prolongarApagado == LOW){
          digitalWrite(ledFalla, 1);
          delay(500);
          digitalWrite(ledFalla, 0);
          delay(500);
        }
      }
    }else{
        return;
      }
}



// PRESION ACEITE baja = LOW || 0
void problemaPresionAceite(bool verifPresAceite) {
  if (verifPresAceite) {
    digitalWrite(ledOk, 0);
    delay(espera);
    refrescarDatos();
    for (byte i = 0; i <= 10 && verifPresAceite == true ; i++) {
      ledFalla = ledAceite;
      interm_ledYsound(ledFalla);
      refrescarDatos();
      verifPresAceite = pres == LOW;
      Serial.println((String)"prolongarApagado"+prolongarApagado+" /ACEITE: " + pres + "// Temp: " + temp + "//Bat: " + bat + "// estadoArranque: " + estadoArranque);
      if(prolongarApagado == HIGH){ interrumpirApagado(ledFalla); return;}
    }
    if ( pres == LOW ) {
      Serial.println("Aceite apagar motor");
    
    /*IMPLEMENTACION*/
      aceiteActual = EEPROM.read(actualAceiteEeprom);
      EEPROM.write(fallaAceiteEeprom, (aceiteActual+1));
      aceiteDespues = EEPROM.read(fallaAceiteEeprom);/* estas dos lineas se deberian eliminar en produccion*/
      Serial.println((String)"aceiteActual "+aceiteActual +" vs "+ aceiteDespues);
      apagarMotor();
      while(prolongarApagado == LOW){
        digitalWrite(ledFalla, 1);
        delay(500);
        digitalWrite(ledFalla, 0);
        delay(500);
      }
    }
    }else{
        return;//no se
      }
}



//      APAGAR MOTOR ((N/C)Cuando recibe corriente Corta la continuidad(Rele inversor)).
void apagarMotor() {
  digitalWrite(cortaCorriente, 1); 
  return;
}

void btnMasTiempo() {
  digitalWrite(cortaCorriente, LOW); //=== 0.  No apago el motor
  prolongarApagado = HIGH;
  //return;
}
//Interrumpir el apagado y el sonoro, pero que el led correspodiente a falla(si la hubiese) siga parpadeando
void interrumpirApagado(byte ledFalla){
  digitalWrite(cortaCorriente, LOW);
  digitalWrite(sound, 1); //↓↓Estas 7 lineas Deben modificar el sonido.
  delay(100);
  digitalWrite(sound, 0);
  delay(100);
  digitalWrite(sound, 1);
  delay(100);
  digitalWrite(sound, 0); //↑↑ Estas 7 lineas Deben modificar el sonido. 
  for (byte i = 0; i < 20; i++) {
    prolongarApagado = LOW;
    digitalWrite(ledOk, valor); //asi comienza encendido
    digitalWrite(ledFalla, valor); //asi comienza encendido
    valor = !valor;
    delay(400);
    if(i == 20){ //Avisa que se termino el InterrumpirApagado.
      digitalWrite(sound, 1);
      delay(300);
      digitalWrite(sound, 0);
      }
    if(prolongarApagado == HIGH){
        interrumpirApagado(ledFalla);  }
  }
}
void interm_ledYsound(byte ledFalla){
  digitalWrite(ledFalla, 1);
  delay(500);
  digitalWrite(ledFalla, 0);
  digitalWrite(sound, 1);
  delay(500);
  digitalWrite(sound, 0);
}

void dioArranque() {
  estadoArranque = true; //inicia en neg y pasa a positivo
  digitalWrite(ledAceite,0); 
  digitalWrite(ledTemp,0); 
}

void enContacto() {
  while(!estadoArranque) {
    digitalWrite(ledOk,valor);
    valor = !valor;
    delay(200);
    Serial.println((String)"estadoArranque: " + estadoArranque);
  } 
}



//PLATFORMIO PIDE QUE TODO SE DECLARE ANTES DE SER LLAMADO.
void loop() {
  ledFalla = ledOk;
  digitalWrite(ledOk, 1); 

  //POSTERGA FUNCIONAMIENTO SI NO ARRANCA A LA PRIMERA
  bool valorSenalArranque = digitalRead(senalArranque);
  Serial.println((String)"valorSenalArranque "+valorSenalArranque);
  if(senalArranque == 1){
    estadoArranque = false;
    //if(!estadoArranque){}
    enContacto();
    
  } //corregir fallo de que si apreto 8 veces el interrup apagado, pasaran 80' hasta apagarse

  //FUNCIONES
if(prolongarApagado == HIGH){
  interrumpirApagado(ledFalla);
}
  
//verificaion Booleana
refrescarDatos();
bool verifBat = bat < batLow || bat > batHigh;
  if(verifBat){problemaCargabateria(verifBat);}
bool verifTemp = temp ==  LOW && pres ==  HIGH ;
  if(verifTemp){problemaTemperatura(verifTemp); }
bool verifPresAceite = pres == LOW;
  if(verifPresAceite){problemaPresionAceite(verifPresAceite);}
Serial.println((String)"verifBat: "+verifBat+ "/ verifTemp:"+ verifTemp+"/ verifPresAceite:"+verifPresAceite);
  
}