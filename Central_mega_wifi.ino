/* 
Arduino utilizado: Mega
Compilacao: Processador 2560
Rede com 3 arduinos: this: node 00 (Master)

                       Mega: Node 00

    Uno (caixa): Node 01         Nano (Cisterna): Node 02
*/

#include <Keypad.h>
//#include <TTP229.h> 
#include <LiquidCrystal.h>
#include <DS3231.h>
//#include <Wire.h
//#include "RTClib.h"
#include <RF24.h>
#include <RF24Network.h>
#include <SPI.h>
#include <NewPing.h>
#include <LiquidCrystal.h>

#define AbrirTorneira 1
#define FecharTorneira 0
#define TorneiraAberta 1
#define TorneiraFechada 0
#define Automatico 1
#define Manual 0
#define loop_time 500
#define tempoLCD  4000 // em ms - tempo que aparecerá informaçao de nivel no LCD
#define tempoTX 2000
#define hora_prog 20
#define minuto_prog 00
#define tempo_prog 8


//========================================================================================//
// --- Mapeamento de Hardware ---
#define RELE1 7  // pino de controle do rele
#define PIN_INT 2
//#define LED_TEST 17




//========================================================================================//
// --- Variávei Globais ---
Time t;                                     // Init a Time-data structure, hora atual
Time hora_programada;                       // Armaneza a hora programada pelo usuário
Time tempo_programado;                                 // Armazena o tempo programado pelo usuário
// Variáveis do Teclado
byte pinosLinhas[]  = {30, 32, 34, 36};
byte pinosColunas[] = {31, 33, 35, 37}; 
char teclas[4][4] = {
  {'A', 'B', 'C', 'D'},
  {'3', '6', '9', '#'},
  {'2', '5', '8', '0'},
  {'1', '4', '7', '*'}
}; 

char tecla_pressionada;                   // Armazena a tecla pressionada no teclado
char hora[3] = {'h', 'h'};                // Armazena os 2 digitos da hora em que a torneira ira abrir
char minutos[3] = {'m', 'm'};             // Armazena os 2 digitos dos minutos em que a torneira ira abrir
char tempo[3] = {'m', 'm'};               // Armazena os dois digitos do tempo em que torneira irá ficar aberta
bool StatusTorneira;
bool ModoOperacao;
int i;
int j;
boolean sucessoTX_Caixa, sucessoTX_Cisterna, sucessoTX;
long tempoInicio;
//int tempoTX;
bool ConfigIrrig;

//========================================================================================//
// --- Declaracao de Estrutura --- //
struct estruturaDadosRF
{
  int nivel = 0;
  int nivel_percent = 0;
};
typedef struct estruturaDadosRF tipoDadosRF;

tipoDadosRF dadosRecebidos, ultimoDadoRecebidoCaixa, ultimoDadoRecebidoCisterna;



//========================================================================================//
// --- Declaracao de Objetos --- //
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);                                          // Indica as portas digitais utilizadas: lcd(<pino RS>, <pino enable>, <pino D4>, <pino D5>, <pino D6>, <pino D7>)
DS3231  rtc(SDA, SCL);                                                            // Init the DS3231 using the hardware interface
Keypad teclado1 = Keypad( makeKeymap(teclas), pinosLinhas, pinosColunas, 4, 4);
RF24 radio(6, 5);                                                                 // CE,CSN = CHIP ENABLE E CHIP SELECT
//byte enderecos[][6] = {"1node","2node"};
RF24Network network(radio);      // Include the radio in the network
const uint16_t this_node = 00;   // Address of this node in Octal format ( 04,031, etc)
const uint16_t node01 = 01;
const uint16_t node02 = 02;
//TTP229 ttp229(SCL, SDA);

//========================================================================================//
// --- Protótipo das Funções ---
void configHora();
void configTempo();
void modo_manual();
void config_RELE(bool x);
void config_modo_oper(bool x);
void mostrar_hora_now();
void mostrar_hora_config();
void mostra_modo_oper();
void mostra_status_torneira();
void inicia_hora_config();
void le_nivel_caixa();
void le_nivel_cisterna();
void mostra_nivel_caixa();
void mostra_nivel_cisterna();
void inicializa_torneira_fechada();
//void pisca_led();

//========================================================================================//
// --- Configuraçoes Iniciais --- //
void setup() {

  Serial.begin(9600);

  rtc.begin();
  //rtc.setDOW(FRIDAY);             // Set Day-of-Week to SUNDAY
  //rtc.setTime(16, 16, 0);         // Set the time to 12:00:00 (24hr format)
  //rtc.setDate(15, 9, 2017);       // Set the date to January 1st, 2014
  t = rtc.getTime(); // Get data from the DS3231

  inicializa_torneira_fechada();

  lcd.begin(16, 2);
  lcd.print("Setup");
  delay(tempoLCD / 2);

  pinMode (RELE1, OUTPUT);
  config_RELE(FecharTorneira);

  inicia_hora_config();             // inicializa as variaveis hora_programa e tempo_programado
  config_modo_oper(Automatico);
  //mostra_status_torneira();
  mostrar_hora_now();
  mostra_modo_oper();
  mostra_status_torneira();
  mostrar_hora_config();
  ConfigIrrig = false;

  pinMode(PIN_INT, INPUT_PULLUP);
  //digitalWrite(PIN_INT, HIGH);     //internall pull-up active
  attachInterrupt(digitalPinToInterrupt(PIN_INT), rotina_int, FALLING);

  // rede wifi
  SPI.begin();
  radio.begin();
  network.begin(90, this_node);  //(channel, node address)
  //tempoTX = tempoLCD;

  i = 0;
  j = 0;
  delay(tempoLCD);

} //end setup



///========================================================================================//
// --- Loop Infinito --- //
void loop() {

  t = rtc.getTime(); // Get data from the DS3231

  if (ConfigIrrig == true) {

    Serial.println("Mostrando Irrig: ");
    Serial.println(tempoLCD / 1000);
    Serial.println(" s");

    mostrar_hora_now();
    mostra_modo_oper();
    mostra_status_torneira();
    mostrar_hora_config();
    
    tempoInicio = millis();
    while (millis() - tempoInicio < tempoLCD)
    {

      Serial.print("Esperando tecla ");
      Serial.print(tempoLCD / 1000);
      Serial.println(" s");

      tecla_pressionada = teclado1.getKey();      //Verifica se alguma tecla foi pressionada

      if (tecla_pressionada)                      //Será verdadeiro se alguma tecla for pressionada
      {

        lcd.clear();
        lcd.print("Config Hora -> A");
        lcd.setCursor(0, 1);
        lcd.print("Modo Manual -> B");

        tecla_pressionada = teclado1.waitForKey();  // Bloqueia o programa até que a alguma tecla seja acionada

        if (tecla_pressionada == 'A')
        {
          configHora();                           // Usuário informa hora (hh:mm) em que a torneira deve abrir
          configTempo();                          // Usuário informa o tempo (mm) que a torneira deve ficar aberta
          config_modo_oper(Automatico);

        }
        if (tecla_pressionada == 'B') {
          modo_manual();
          config_modo_oper(Manual);
        }

        if (tecla_pressionada != 'A' && tecla_pressionada != 'B') {
          lcd.clear();
        }

        i = i + 1;
        Serial.print("If1: ");
        Serial.println(i);
      }
    }
     ConfigIrrig = false;
  }

  if (ModoOperacao == Automatico)
  {
    Serial.println("Verifica se modo Automático");
    Serial.print("hora atual: ");
    Serial.print(t.hour);
    Serial.print(" / minuto atual: ");
    Serial.print(t.min);
    Serial.print(" / hora prog: ");
    Serial.print(hora_programada.hour);
    Serial.print(" / min prog: ");
    Serial.println(hora_programada.min);

    if (t.hour == hora_programada.hour and t.min == hora_programada.min)    // Verifica se hora (hh:mm) atual é igual à configurada pelo usuario
    {
      Serial.println("Primeiro IF");
      if (t.min - hora_programada.min < tempo_programado.min )            // Verifica se o tempo decorrido (em min) após a hora programada é menor que o tempo programado para a torneira ficar aberta
      {
        Serial.println("Segundo IF");
        if ( StatusTorneira == TorneiraFechada )                         // Se a torneira estiver fechada, deve ser aberta
        {
          Serial.println("Terceiro IF");
          config_RELE(AbrirTorneira);
        }
      }
    }


    else if (StatusTorneira == TorneiraAberta)                              // Verifica se a torneira realmente deveria estar aberta
    {
      if (t.min - hora_programada.min >= tempo_programado.min )             //  só deve estar aberta se o delta t for menor que o tempo programado
      {
        config_RELE(FecharTorneira);
      }
    }
  }


  tempoInicio = millis();

  Serial.print("Ouvindo cisterna por: ");
  Serial.print(tempoTX / 1000);
  Serial.println(" s");

  sucessoTX_Cisterna = 0;
  network.update();

  while ( millis() - tempoInicio < tempoTX)
  {
    if (sucessoTX = network.available())
    {
      RF24NetworkHeader header;
      network.read(header, &dadosRecebidos, sizeof(tipoDadosRF) ) ;

      if (header.from_node == node02)       // If data comes from Node Cisterna
      {
        sucessoTX_Cisterna = 1;
        le_nivel_cisterna();
        Serial.println("Recebido: Cisterna");
      }
    }

  }

  mostra_nivel_cisterna();

  Serial.print("Ouvindo caixa por: ");
  Serial.print(tempoTX / 1000);
  Serial.println(" s");

  tempoInicio = millis();

  sucessoTX_Caixa = 0;
  network.update();

  while ( millis() - tempoInicio < tempoTX)
  {
    if (sucessoTX = network.available())
    {
      RF24NetworkHeader header;
      network.read(header, &dadosRecebidos, sizeof(tipoDadosRF) ) ;

      if (header.from_node == node01)       // If data comes from Node CAIXA
      {
        sucessoTX_Caixa = 1;
        le_nivel_caixa();
        Serial.println("Recebido: Caixa");
      }
    }

  }

  mostra_nivel_caixa();

  j = j + 1;
  Serial.print("Loop: ");
  Serial.println(j);
  delay (loop_time);



} //end loop


void rotina_int()
{

    ConfigIrrig = true;
    Serial.println("Interrupcao");

}

//========================================================================================//
// --- Desenvolvimento das Funções ---

//========================================================================================//
// --- Configuraçao da Hora em que a torneira vai abrir ---
void configHora() {

  lcd.clear();
  lcd.print("Hora (hh:mm)");
  lcd.setCursor(0, 1);
  lcd.blink();
  for (int n = 1; n < 5; n++) {
    //
    tecla_pressionada = teclado1.waitForKey();

    if (n == 1) {
      if ( tecla_pressionada >= 48 and tecla_pressionada <= 50) {
        hora[0] = tecla_pressionada;
        lcd.write(hora[0]);

      } else {
        n = 0;
        lcd.clear();
        lcd.print("Hora (hh:mm)");
        lcd.setCursor(0, 1);
        lcd.blink();
      }
    }
    if (n == 2) {

      if (tecla_pressionada >= 48 and tecla_pressionada <= 57) {
        hora[1] = tecla_pressionada;
        lcd.write(hora[1]);
        lcd.write(byte(58)); // escreve ':

      } else {
        n = 0;
        lcd.clear();
        lcd.print("Hora (hh:mm)");
        lcd.setCursor(0, 1);
        lcd.blink();

      }

    }

    if (n == 3) {
      if (tecla_pressionada >= 48 and tecla_pressionada <= 53) {
        minutos[0] = tecla_pressionada;
        lcd.write(minutos[0]);

      }
      else {
        n = 0;
        lcd.clear();
        lcd.print("Hora (hh:mm)");
        lcd.setCursor(0, 1);
        lcd.blink();

      }
    }
    if (n == 4) {
      if (tecla_pressionada >= 48 and tecla_pressionada <= 57) {
        minutos[1] = tecla_pressionada;
        lcd.write(minutos[1]);

      } else {
        n = 0;
        lcd.clear();
        lcd.print("Hora (hh:mm)");
        lcd.setCursor(0, 1);
        lcd.blink();
      }
    }
    Serial.print("for: ");
    Serial.println(n);
  }

  hora_programada.hour = atoi(hora);
  hora_programada.min = atoi(minutos);
  lcd.noBlink();
  return;
}

//========================================================================================//
// --- Configuraçao do tempo que a torneira vai ficar aberta ---
void configTempo() {

  lcd.clear();
  lcd.print("Tempo (mm)");
  lcd.setCursor(0, 1);
  lcd.blink();

  for (int n = 1; n < 3; n++) {
    //
    tecla_pressionada = teclado1.waitForKey();

    if (n == 1) {
      if ( tecla_pressionada >= 48 and tecla_pressionada <= 57) {
        tempo[0] = tecla_pressionada;
        lcd.write(tempo[0]);
      } else {
        n = 0;
        lcd.clear();
        lcd.print("Tempo (mm)");
        lcd.setCursor(0, 1);
        lcd.blink();
      }
    }
    if (n == 2) {

      if (tecla_pressionada >= 48 and tecla_pressionada <= 57) {
        tempo[1] = tecla_pressionada;
        lcd.write(tempo[1]);
        lcd.print(" minutos");
      } else {
        n = 0;
        lcd.clear();
        lcd.print("Tempo (mm)");
        lcd.setCursor(0, 1);
        lcd.blink();
      }
    }
  }

  tempo_programado.min = atoi(tempo);
  lcd.noBlink();
  return;
}

//========================================================================================//
// --- Configuraçao do modo de operacao manual ---
void modo_manual() {

  lcd.clear();
  lcd.print("ABRIR (A)");
  lcd.setCursor(0, 1);
  lcd.print("FECHAR (B)");
  lcd.blink();
  //lcd.print("Modo Manual");
  tecla_pressionada = teclado1.waitForKey(); // Bloqueia o programa até que a alguma tecla seja acionada
  lcd.clear();
  if (tecla_pressionada == 65)  { // caracter 'A'
    config_RELE(AbrirTorneira);
  }

  if (tecla_pressionada == 66)  { // caracter 'B'
    config_RELE(FecharTorneira);
  }

  lcd.noBlink();
  return;
}

//========================================================================================//
// --- Configuraçao da ação do RELE ---

void config_RELE(bool x) {

  if (x == AbrirTorneira and StatusTorneira == TorneiraFechada) {
    Serial.println("Abrindo Torneira");
    digitalWrite(RELE1, LOW);
    delay(loop_time);
    StatusTorneira = TorneiraAberta;
    lcd.begin(16, 2);
    lcd.print("abrindo...");
    delay(5 * loop_time);
  }

  if (x == FecharTorneira and StatusTorneira == TorneiraAberta) {
    Serial.println("Fechando Torneira");
    digitalWrite(RELE1, HIGH);
    delay(loop_time);
    StatusTorneira = TorneiraFechada;
    lcd.begin(16, 2);
    lcd.print("fechando...");
    delay(5 * loop_time);
  }

}

//========================================================================================//
// --- Configuraçao do modo de operaçao de algoritmo --- //
void config_modo_oper(bool x) {

  if (x == Automatico) {
    ModoOperacao = Automatico;
  }

  if (x == Manual) {
    ModoOperacao = Manual;
  }
}

//========================================================================================//
// --- Rotinas Auxiliares - Apresentaçao --- //

// --- Mostra no LCD a hora agora (hh:mm) --- //
void mostrar_hora_now() {
  //t = rtc.getTime(); // Get data from the DS3231
  hora_programada.year = t.year;
  hora_programada.mon = t.mon;
  hora_programada.date = t.date;
  hora_programada.sec = t.sec;
  lcd.clear();
  lcd.setCursor (0, 0);
  lcd.print("Agora:");
  lcd.print(t.hour, DEC);

  if (t.min > 9 )
  {
    lcd.print(":");
    lcd.print(t.min, DEC);
  }
  else
  {
    lcd.print(":0");
    lcd.print(t.min, DEC);
  }

}


//========================================================================================//
// --- Mostra no LCD a hora programada (hh:mm) em que a torneira vai abrir --- //
void mostrar_hora_config() {
  //t = rtc.getTime(); // Get data from the DS3231
  lcd.setCursor (0, 1);
  //lcd.print("Abrir");
  lcd.print(hora_programada.hour, DEC);
  //lcd.print(":");

  if (hora_programada.min > 9 )
  {
    lcd.print(":");
    lcd.print(hora_programada.min, DEC);
  }
  else
  {
    lcd.print(":0");
    lcd.print(hora_programada.min, DEC);
  }
  //lcd.print(hora_programada.min, DEC);
  lcd.print(" ");
  lcd.print(tempo_programado.min, DEC);
  lcd.print("min");

}

// --- Mostra no LCD o modo de operaçao (aut/man) --- //

void mostra_modo_oper() {
  lcd.setCursor (13, 0);
  if (ModoOperacao == Automatico) {
    lcd.print("Aut");
  }
  if (ModoOperacao == Manual) {
    lcd.print("Man");
  }

}

//========================================================================================//
// --- Mostra no LCD o status da torneira (aber/fech) --- //

void mostra_status_torneira() {
  lcd.setCursor (12, 1);
  if (StatusTorneira == TorneiraAberta) {
    lcd.print("Aber");
  }
  if (StatusTorneira == TorneiraFechada) {
    lcd.print("Fech");
  }

}

/*
  void pisca_led() {

  digitalWrite(LED_TEST, !digitalRead(LED_TEST));
  //digitalWrite(LED_TEST,HIGH);
  delay(loop_time);
  }

*/
//========================================================================================//
// --- recebe nivel por wifi do arduino na caixa e mostra no LCD --- //
//========================================================================================//
void mostra_nivel_cisterna() {

  if (sucessoTX_Cisterna == 1) {
    //lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print("Ci:");
    lcd.print(ultimoDadoRecebidoCisterna.nivel);
    lcd.setCursor(6, 0);
    lcd.print("cm");
    lcd.setCursor(10, 0);
    lcd.print(ultimoDadoRecebidoCisterna.nivel_percent);
    lcd.setCursor(13, 0);
    lcd.print("%");
    lcd.setCursor(15, 0);
    lcd.print("S");
  }
  else if (ultimoDadoRecebidoCisterna.nivel != 0 ) {
    //lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("                "); // limpa linha 0 do LCD
    lcd.setCursor(0, 0);
    lcd.print("Ci:");
    lcd.print(ultimoDadoRecebidoCisterna.nivel);
    lcd.setCursor(6, 0);
    lcd.print("cm");
    lcd.setCursor(10, 0);
    lcd.print(ultimoDadoRecebidoCisterna.nivel_percent);
    lcd.setCursor(13, 0);
    lcd.print("%");
    lcd.setCursor(15, 0);
    lcd.print("F");
  }

  else
  {
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(15, 0);
    lcd.print("F");

  }

}

void le_nivel_cisterna() {

  ultimoDadoRecebidoCisterna.nivel = dadosRecebidos.nivel;
  ultimoDadoRecebidoCisterna.nivel_percent = dadosRecebidos.nivel_percent;
  Serial.print("Le cisterna:");
  Serial.println(dadosRecebidos.nivel);
 

}

void le_nivel_caixa() {

  ultimoDadoRecebidoCaixa.nivel = dadosRecebidos.nivel;
  ultimoDadoRecebidoCaixa.nivel_percent = dadosRecebidos.nivel_percent;
  Serial.print("Le caixa:");
  Serial.println(dadosRecebidos.nivel);
  
}

void mostra_nivel_caixa() {

  if (sucessoTX_Caixa == 1) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print("Ca:");
    lcd.print(ultimoDadoRecebidoCaixa.nivel);
    lcd.setCursor(6, 1);
    lcd.print("cm");
    lcd.setCursor(10, 1);
    lcd.print(ultimoDadoRecebidoCaixa.nivel_percent);
    lcd.setCursor(13, 1);
    lcd.print("%");
    lcd.setCursor(15, 1);
    lcd.print("S");
  }
  else if (ultimoDadoRecebidoCaixa.nivel != 0 )
  {
    lcd.setCursor(0, 1);
    lcd.print("                "); // limpa linha 1 do LCD
    lcd.setCursor(0, 1);
    lcd.print("Ca:");
    lcd.print(ultimoDadoRecebidoCaixa.nivel);
    lcd.setCursor(6, 1);
    lcd.print("cm");
    lcd.setCursor(10, 1);
    lcd.print(ultimoDadoRecebidoCaixa.nivel_percent);
    lcd.setCursor(13, 1);
    lcd.print("%");
    lcd.setCursor(15, 1);
    lcd.print("F");
  }

  else
  {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(15, 1);
    lcd.print("F");
  }

}



// --- Rotinas Auxiliares - Inicializaçao --- //
// --- Inicializa as variaveis hora_programada e tempo_programado --- //
void inicia_hora_config() {

  hora_programada.hour = hora_prog;
  hora_programada.min = minuto_prog;
  tempo_programado.min = tempo_prog;
}

void inicializa_torneira_fechada() {
  Serial.println("Fechando Torneira");
  digitalWrite(RELE1, HIGH);
  delay(loop_time);
  StatusTorneira = TorneiraFechada;
}




