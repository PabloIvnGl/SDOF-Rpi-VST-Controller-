///////////-------------- CODIGO SDOF DELAY CONTROL MIDI USB 2X 8cc 4note ----------------////////////// PABLO GUTIERREZ LEGENT 2025
///////////---------Este codigo compila la version V1.1 del controlador midi SDOFDELAY --///////////////
///////////---------SDOFDELAYCONTROL es una superficie de control midi con dos paginas --///////////////
///////////---------MIDI de eventos midi,ocho ControlChange(CC) y  cuatro MidiNote(NOTE)-///////////////  
#include "MIDIUSB.h"               // Librería MIDI por USB para sparkfun promicro
#include <ResponsiveAnalogRead.h>  // Librería ResponsiveAnalogRead  https://github.com/dxinteractive/ResponsiveAnalogRead para lectura de pot

const int N_POTS = 8;     // Número de potenciómetros
const int N_BOT  = 4;     // Número de botones
 

const int midipageBotPIN = 15;   // Pin del botón de cambio de página MIDI
int ledmidipagePIN = 2;          // Pin del LED indicador de página MIDI
int onLedPIN       = 3;          // Pin del LED de encendido y parpadeo de nota MIDI



int botPin[N_BOT]   = { 7, 16,  5, 14};       // Pines digitales de los botones
int botnote[N_BOT]  = {36, 37, 38, 39};     // Notas MIDI asignadas a cada botón


int potPin[N_POTS] = {  A6, A3, A9, A0, A7, A8, A1, A2};  // Pines analógicos para los potenciómetros
int potCC[N_POTS] =  {   1,  2,  3,  4,  5,  6,  7,  8};  // Números de Control Change (CC) MIDI asignados a cada potenciómetro

int potReading[N_POTS] = { 0 };  // Inicializacion lecturas crudas de los potenciómetros
int potState[N_POTS] = { 0 };    // Inicializacion valores suavizados de los potenciómetros
int potPState[N_POTS] = { 0 };   // Inicializacion valores anteriores de los potenciómetros

int midiState[N_POTS] = { 0 };    // Inicializacion valores MIDI mapeados actuales
int midiPState[N_POTS] = { 0 };   // Inicializacion valores MIDI mapeados anteriores

int botState[N_BOT]          = {0};
bool botpreviousState[N_BOT] = {false, false, false, false};
bool swPressed[N_BOT]        = {false, false, false, false};
bool swPreviousState[N_BOT]  = {false, false, false, false};
bool swledState[N_BOT]       = {false, false, false, false};

const byte potThreshold = 20;           // Umbral mínimo de cambio para detectar movimiento del potenciómetro
const int POT_TIMEOUT = 300;            // Tiempo de espera (ms) para detectar que el potenciómetro dejó de moverse
unsigned long pPotTime[N_POTS] = { 0 };  // Tiempo del último cambio detectado en cada potenciómetro
unsigned long potTimer[N_POTS] = { 0 };  // Tiempo transcurrido desde el último cambio en cada potenciómetro

byte POT_CH = 0;  // Canal MIDI para los potenciómetros
byte BOT_CH = 0;  // Canal MIDI para los botones
float snapMultiplier = 0.01;                      // Multiplicador de suavizado para las lecturas analógicas
ResponsiveAnalogRead responsivePot[N_POTS] = {};  // Arreglo de objetos ResponsiveAnalogRead (se inicializa en Setup)


// ---- BOTÓN DE CAMBIO DE PÁGINA MIDI ----
bool midibuttonPressed = false;
bool midibuttonPreviousState = false;
bool midiledState = false;  // Estado del LED de página MIDI (encendido/apagado)


void setup() {
  

  Serial.begin(9600);  // Inicializa la comunicación serie para depuración
  
  pinMode(midipageBotPIN, INPUT_PULLUP); // Botón de página MIDI como entrada con pull-up interno
  pinMode(ledmidipagePIN, OUTPUT);       // LED de página MIDI como salida
  pinMode(onLedPIN      , OUTPUT);       // LED de encendido como salida

  // blink de encendido secuencia de parpadeo de bienvenida al encender 
  digitalWrite(onLedPIN, HIGH);
      delay(500);
      digitalWrite(onLedPIN, LOW);
      delay(90);
  digitalWrite(onLedPIN, HIGH);
      delay(90);
  digitalWrite(onLedPIN, LOW);

      
  for(int i = 0; i < N_BOT; i++) {        // Configura los pines de botones con pull-up interno
    pinMode(botPin[i], INPUT_PULLUP);
  }

  for (int i = 0; i < N_POTS; i++) {
    responsivePot[i] = ResponsiveAnalogRead(0, true, snapMultiplier);  // Inicializa los potenciómetros con suavizado
    responsivePot[i].setAnalogResolution(1023);                        // Resolución analógica de 10 bits (0-1023)
  }
}

void loop() {
  // Bucle principal: se ejecuta repetidamente


  // ---- LECTURA DE BOTONES MIDI (NOTAS) ----
  for(int i = 0; i < N_BOT; i++) {
    botState[i] = digitalRead(botPin[i]);
  }

  // ---- BOTÓN DE CAMBIO DE PÁGINA MIDI ----
  int midibotState = digitalRead(midipageBotPIN);

  if (midibotState == LOW && !midibuttonPreviousState) { // Detecta flanco de bajada (botón recién presionado)
    midibuttonPressed = true;
  } else {
    midibuttonPressed = false;
  }

  midibuttonPreviousState = (midibotState == LOW);    // Actualiza el estado anterior del botón de página

  if (midibuttonPressed) { // Si se presionó el botón, alterna el canal MIDI entre 0 y 9
    POT_CH = (POT_CH == 0) ? 9 : 0;
    BOT_CH = (BOT_CH == 0) ? 9 : 0; 
    
    midiledState = !midiledState;                                      // Alterna el estado del LED de página
    digitalWrite(ledmidipagePIN, midiledState ? HIGH : LOW);          // Enciende o apaga el LED de página
  }
  
  // ---- ENVÍO DE NOTAS MIDI POR BOTONES ----
  for(int i = 0; i < (N_BOT); i++) {

    if (botState[i] == LOW && !swPreviousState[i]) {  // Detecta flanco de bajada (botón recién presionado)
      swPressed[i] = true;
    } else {
      swPressed[i] = false;
    }

    swPreviousState[i] = (botState[i] == LOW);  // Actualiza el estado anterior de cada botón

    if(swPressed[i]) {        // Al presionar: envía Note On y parpadea el LED de encendido
      
      noteOn (BOT_CH, botnote[i], 127);
      MidiUSB.flush();

      //swledState[i] = !swledState[i];
      digitalWrite(onLedPIN, HIGH);
      delay(50);
      digitalWrite(onLedPIN, LOW);
      delay(10);
    }
    else {                    // Al soltar: envía Note Off
      noteOff (BOT_CH, botnote[i], 127);
      MidiUSB.flush();
    } 
  }


  // ---- LECTURA Y ENVÍO DE CONTROL CHANGE POR POTENCIÓMETROS ----
  for (int i = 0; i < N_POTS; i++) {

    potReading[i] = analogRead(potPin[i]);          // Lee el valor crudo del potenciómetro
    responsivePot[i].update(potReading[i]);          // Actualiza el suavizado del potenciómetro
    potState[i] = responsivePot[i].getValue();       // Obtiene el valor suavizado
    midiState[i] = map(potState[i], 0, 1023, 0, 128); // Mapea el valor al rango MIDI (0-127)

    int potVar = abs(potState[i] - potPState[i]);   // Calcula el cambio absoluto del potenciómetro

    if (potVar > potThreshold) {
      pPotTime[i] = millis();  // Registra el momento del último cambio significativo
    }

    potTimer[i] = millis() - pPotTime[i];  // Calcula el tiempo transcurrido desde el último cambio

    if (potTimer[i] < POT_TIMEOUT) {
      if (midiState[i] != midiPState[i]) {
        
        controlChange(POT_CH, potCC[i], midiState[i]);  // Envía el mensaje MIDI Control Change (canal, CC, valor)
        MidiUSB.flush();                                 // Vacía el buffer de salida MIDI

        // Imprime en el monitor serie para depuración
        Serial.print("Pot ");
        Serial.print(i);
        Serial.print(" | ");
        Serial.print("PotState: ");
        Serial.print(potState[i]);
        Serial.print(" -  midiState: ");
        Serial.println(midiState[i]);

        midiPState[i] = midiState[i];  // Actualiza el estado MIDI anterior
      }
      potPState[i] = potState[i];  // Actualiza el valor anterior del potenciómetro
    }
  }
}


// ---- FUNCIONES MIDI PARA ARDUINO (PRO) MICRO CON LIBRERÍA MIDIUSB ----

void noteOn(byte channel, byte pitch, byte velocity) {
  // Envía un mensaje Note On: canal, nota, velocidad
  midiEventPacket_t noteOn = { 0x09, 0x90 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  // Envía un mensaje Note Off: canal, nota, velocidad
  midiEventPacket_t noteOff = { 0x08, 0x80 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOff);
}

void controlChange(byte channel, byte control, byte value) {
  // Envía un mensaje Control Change: canal, número de control, valor
  midiEventPacket_t event = { 0x0B, 0xB0 | channel, control, value };
  MidiUSB.sendMIDI(event);
}
