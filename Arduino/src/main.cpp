#include <Arduino.h>
#include <Wire.h>
#include <SparkFunCCS811.h>
#include <SparkFunBME280.h>
#include <ClosedCube_HDC1080.h>
#include <Adafruit_ADS1015.h>
#include <LiquidCrystal.h>
#include <Thread.h>
#include <ThreadController.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <Scheduler.h>
#include <SoftwareSerial.h>

// indirizzo scheda Display CCS811 e inizializzazione pin di controllo
#define CCS811_ADDR 0X5A
#define RS 12
#define EN 11
#define D4 5
#define D5 4
#define D6 9
#define D7 8

#define PERIOD_TIMER  50
#define DIM_FRAME 12  //lunghezza max stringa caratteri disponibili a display

// Creazione oggetti
CCS811 ccsBoard(CCS811_ADDR);   // sensore per la misura del CO2
ClosedCube_HDC1080 hdcBoard;    // sensore per la misura di temperatura e umidità
BME280 bmeBoard;                // sensore lettura barometrica
Adafruit_ADS1115 adsBoard;      // convertitore A/D 4 ch 16 bit
SoftwareSerial xbeeBoard(6, 7); // inizializzazione seriale di comunicazione con scheda XBee (6 Rx, 7 Tx)
SoftwareSerial sds(2, 3);       // pin 2 Rx, pin 3 Tx comunicazione seriale con polverimetro
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);  // inizializzazione display LCD

ThreadController ctr = ThreadController();  // inizializzazione controllo thread
// controllo lettura elementi
Thread analisi = Thread();
// controllo display
Thread dsp = Thread();    // aggiorna i valori letti a display
// controllo pulsanti
Thread pls = Thread();

ThreadController groupOfThreads = ThreadController();

// Variabili globali
float pAtm;
float temperatura;
float humidity;
float co2;
float CO2_Value;
float adc0_V;
float adc1_V;
float adc2_V;
float Rs_CO;
float Rs_NH3;
float Rs_NO2;
float ppm_CO;
float ppm_NH3;
float ppm_NO2;
float CO_Value;
float NH3_Value;
float NO2_Value;
float Ratio_CO;
float Ratio_NH3;
float Ratio_NO2;
float polveri2_5;
float polveri_10;
float V_COUNT = 1875E-7;
float _R0_CO = 280E3;
float _R0_NH3 = 88E3;
float _R0_NO2 = 150270;
float _I_CO = 1E-6;
float _I_NO2 = 100E-6;
float _I_NH3 = 1E-6;
float _CO_1 = 4.4638;
float _CO_2 = -1.177;
float _NH3_1 = 0.6151;
float _NH3_2 = -1.903;
float _NO2_1 = 0.1516;
float _NO2_2 = 0.9979;
float _MOL_CO = 28.01;
float _MOL_CO2 = 44.01;
float _MOL_NH3 = 17.031;
float _MOL_NO2 = 46.0055;
float correzione_barometrica = 20.8;
float correzione_temperatura = -5.5;
float correzione_humidity = -3.0;

bool statoMenu = false;
bool scansione = false;
bool disableRoutine = false;
String riga_1;
String riga_2;
char risultato[DIM_FRAME];
int step = 0;
int ciclo = 0;
int ciclo_2 = 0;
int ciclo_3 = 0;
int ciclo_4 = 0;
int16_t adc_0, adc_1, adc_2;

unsigned char frame[10];
uint8_t lunghezza;
bool sd = false;
uint16_t pm2_5;
uint16_t pm_10;

// visualizzazione a display caratteri ° e u
byte Grado[8] = {
  0b01100,
  0b01100,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

byte mu[8] = {
  0b00000,
  0b00000,
  0b10001,
  0b10001,
  0b10001,
  0b10011,
  0b01101,
  0b00000
};



void svuotaBuffer()
{
  // dati disponibili da polverimetro
  byte temp;
  while (sds.available())
  {
    temp = sds.read();
  }
}

bool startFrame(unsigned char *frame)
{
  bool _sd;
  unsigned char startDelimiter = frame[0];
  if (startDelimiter == 0xAA)
  {
    _sd = true;
  }
  else
  {
    _sd = false;
  }
  return _sd;
}

float emissioni2_5(unsigned char *frame)
{
  uint16_t _pm2_5;
  uint8_t _byte_1;
  uint8_t _byte_2;
  float _polveri2_5;

  _byte_1 = frame[2];
  _byte_2 = frame[3];
  _pm2_5 = _byte_1;
  _pm2_5 += (_byte_2 << 8);
  _polveri2_5 = ((float)_pm2_5)/10;
  return _polveri2_5;
}

float emissioni_10(unsigned char *frame)
{
  uint16_t _pm_10;
  uint8_t _byte_1;
  uint8_t _byte_2;
  float _polveri_10;

  _byte_1 = frame[4];
  _byte_2 = frame[5];
  _pm_10 = _byte_1;
  _pm_10 += (_byte_2 << 8);
  _polveri_10 = ((float)_pm_10)/10;
  return _polveri_10;
}

void Board_8128()
{
  Serial.println("Routine Board_8128");
    if(ccsBoard.dataAvailable())
    {
      ccsBoard.readAlgorithmResults();
      pAtm = ((bmeBoard.readFloatPressure())/100) + correzione_barometrica; //Lettura in hPa
      temperatura = hdcBoard.readTemperature() + correzione_temperatura;
      humidity = hdcBoard.readHumidity() + correzione_humidity;
      co2 = ccsBoard.getCO2();
      ccsBoard.setEnvironmentalData(hdcBoard.readHumidity(), hdcBoard.readTemperature());
      // Conversione ppm CO2 in g/m3
      CO2_Value = (0.0409 * co2 * _MOL_CO2)/1000;
      // Memorizzazione dati su struttura
      ambientali[ciclo].p_atm = pAtm;
      ambientali[ciclo].temp = temperatura;
      ambientali[ciclo].hum = humidity;
      ambientali[ciclo].co_2 = CO2_Value;
      // ************** Per verifica *****************
/*      Serial.print("P.Atm. [hPa] ");
      Serial.print(ambientali[ciclo].p_atm);
      Serial.print(" ; ");
      Serial.print("Temp. [°C] ");
      Serial.print(ambientali[ciclo].temp);
      Serial.print(" ; ");
      Serial.print("Umidità [%] ");
      Serial.print(ambientali[ciclo].hum);
      Serial.print(" ; ");
      Serial.print("CO2 [ppm] ");
      Serial.print(ambientali[ciclo].co_2);
      Serial.print(" ; ");
      Serial.print(ciclo);
      Serial.println(" "); */
      // *********************************************
      ciclo++;
      if(ciclo > SAMPLE-1)
      {
        ciclo = 0;
      }
    }


}

void display()
{  
  // in caso di verifica su singolo elemento da menu verrà indicato
  // il case da eseguire
  lcd.clear();
  switch (elemento)
  {
  case p_baro:
    memset(risultato, 0, DIM_FRAME);    // azzera la variabile
    dtostrf(pAtm,6,2,risultato);        // converte il float in stringa
    lcd.clear();                        // comandi LCD
    lcd.setCursor(0, 0);
    lcd.print("BAROMETRICA");
    lcd.setCursor(0, 1);
    lcd.print(risultato);
    lcd.print(" hPa");
    break;

  case t_amb:
    memset(risultato, 0, DIM_FRAME);
    dtostrf(temperatura, 5, 1, risultato);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("TEMPERATURA");
    lcd.setCursor(0,1);
    lcd.print(risultato);
    lcd.setCursor(6, 1);
    lcd.write(byte(0));
    lcd.print("C");
    break;

  case h_amb:
    memset(risultato, 0, DIM_FRAME);
    dtostrf(humidity, 5, 1, risultato);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("UMIDITA'");
    lcd.setCursor(0, 1);
    lcd.print(risultato);
    lcd.print(" %");

    break;

  case CO_2:
    memset(risultato, 0, DIM_FRAME);
    dtostrf(CO2_Value, 6, 1, risultato);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CO2");
    lcd.setCursor(0, 1);
    lcd.print(risultato);
    lcd.setCursor(8, 1);
    lcd.print("g/m3");

    break;      
  
  case NH_3:
    memset(risultato, 0, DIM_FRAME);
    dtostrf(NH3_Value, 4, 2, risultato);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NH3");
    lcd.setCursor(0, 1);
    lcd.print(risultato);
    lcd.setCursor(8, 1);
    lcd.write(byte(1)); // inserisce il simbolo micro (mu)
    lcd.print("g/m3");

    break;

  case CO:
    memset(risultato, 0, DIM_FRAME);
    dtostrf(CO_Value, 4, 2, risultato);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CO");
    lcd.setCursor(0, 1);
    lcd.print(risultato);
    lcd.print(" mg/m3");

    break;

  case NO_2:
    memset(risultato, 0, DIM_FRAME);
    dtostrf(NO2_Value, 4, 2, risultato);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NO2");
    lcd.setCursor(0, 1);
    lcd.print(risultato);
    lcd.setCursor(8, 1);
    lcd.write(byte(1));
    lcd.print("g/m3");

    break;

  case PM25:
    memset(risultato, 0, DIM_FRAME);
    dtostrf(polveri2_5, 6, 2, risultato);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PM 2.5");
    lcd.setCursor(0, 1);
    lcd.print(risultato);
    lcd.setCursor(8, 1);
    lcd.write(byte(1));
    lcd.print("g/m3");

    break;

  case PM10:
    memset(risultato, 0, DIM_FRAME);
    dtostrf(polveri_10, 6, 2, risultato);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PM 10");
    lcd.setCursor(0, 1);
    lcd.print(risultato);
    lcd.setCursor(8, 1);
    lcd.write(byte(1));
    lcd.print("g/m3");

    break;

  default:
    break;

  }
  scansione = false;
}



void menu()
{
  // analizzare lo stato dei pulsanti
  // se non ci sono eventi si procede 
  // con la normale visualizzazione
  if(statoMenu == false)    // menù non attivo
  {
    if(scansione == false)
    {

      if(step == 1)
      {
      elemento = p_baro;
      scansione = true;
      }
      if(step == 2)
      {
      elemento = t_amb;
      scansione = true;
      }
      if (step == 3)
      {
      elemento = h_amb;
      scansione = true;
      }
      if(step == 4)
      {
      elemento = CO_2;
      scansione = true;
      }
      if (step == 5)
      {
        elemento = NH_3;
        scansione = true;
      }
      if (step == 6)
      {
        elemento = CO;
        scansione = true;
      }
      if (step == 7)
      {
        elemento = NO_2;
        scansione = true;
      }
      if (step == 8)
      {
        elemento = PM25;
        scansione = true;
      }
      if (step == 9)
      {
        elemento = PM10;
        scansione = true;
      }

      step++;
      if(step>9)
      {
      step = 0;
      }
    }
  }

}

void board6814()
{
  Serial.println("Routine Board_6814");

  adc_0 = adsBoard.readADC_SingleEnded(0);    // Segnale in tensione CO Red Sensor
  adc_1 = adsBoard.readADC_SingleEnded(1);    // Segnale in tensione NH3
  adc_2 = adsBoard.readADC_SingleEnded(2);    // Segnale in tensione NO2

  adc0_V = V_COUNT * adc_0;
  adc1_V = V_COUNT * adc_1;
  adc2_V = V_COUNT * adc_2;
  /*
  Serial.print("V_CO = ");
  Serial.println(adc0_V);
  Serial.print("V_NH3 = ");
  Serial.println(adc1_V);
  Serial.print("V_NO2 = ");
  Serial.println(adc2_V);
  */
  // calcolo Rs

  Rs_CO = adc0_V/_I_CO;
  Rs_NH3 = adc1_V/_I_NH3;
  Rs_NO2 = adc2_V/_I_NO2;
  /*
  Serial.print("R_CO = ");
  Serial.println(Rs_CO);
  Serial.print("R_NH3 = ");
  Serial.println(Rs_NH3);
  Serial.print("R_NO2 = ");
  Serial.println(Rs_NO2);
  */
  // calcolo rapporto Rs/R0

  Ratio_CO = Rs_CO/_R0_CO;
  Ratio_NH3 = Rs_NH3/_R0_NH3;
  Ratio_NO2 = Rs_NO2/_R0_NO2;

  // calcolo ppm

  ppm_CO = _CO_1 * pow(Ratio_CO, _CO_2);
  ppm_NH3 = _NH3_1 * pow(Ratio_NH3, _NH3_2);
  ppm_NO2 = _NO2_1 * pow(Ratio_NO2, _NO2_2);

  // trasformo i ppm in microgrammi/m3

  CO_Value = (ppm_CO * 12.187 * _MOL_CO)/(273.15 + temperatura);    // in milligrammi/m3
  NH3_Value = ((ppm_NH3 * 1000) * 12.187 * _MOL_NH3)/(273.15 + temperatura);  // in microgrammi/m3
  NO2_Value = ((ppm_NO2 * 1000) * 12.187 * _MOL_NO2)/(273.15 + temperatura);  // in microgrammi/m3

  // ************** Per verifica *****************
/*  Serial.print("CO [ppm] ");
  Serial.print(CO_Value);
  Serial.print(" ; ");
  Serial.print("NH3 [ppm] ");
  Serial.print(NH3_Value);
  Serial.print(" ; ");
  Serial.print("NO2 [ppm] ");
  Serial.print(NO2_Value);
  Serial.print(" ; ");
  Serial.println(" "); */
  // *********************************************

  elementi[ciclo_2]._CO = CO_Value;
  elementi[ciclo_2]._NH3 = NH3_Value;
  elementi[ciclo_2]._NO2 = NO2_Value;

  ciclo_2++;
  if (ciclo_2 > CAMPIONI-1)
  {
    ciclo_2 = 0;
  }
}

void trx()
{
    // ogni minuto trasmette i valori registrati
    // esegue la media per ogni elemento misurato
    
    //Serial.println("Routine TRX");
    pAtm = 0;
    temperatura = 0;
    humidity = 0;
    CO2_Value = 0;
    CO_Value = 0;
    NH3_Value = 0;
    NO2_Value = 0;
    polveri2_5 = 0;
    polveri_10 = 0;

    // riutilizzare le variabili principali senza usarne nuove

    for(int i=0; i<SAMPLE;i++)
    {
      pAtm += ambientali[i].p_atm;
      temperatura += ambientali[i].temp;
      humidity += ambientali[i].hum;
      CO2_Value += ambientali[i].co_2;
    }
    pAtm = pAtm/SAMPLE;
    temperatura = temperatura/SAMPLE;
    humidity = humidity/SAMPLE;
    CO2_Value = CO2_Value/SAMPLE;
    // Trasformazione da ppm a microgrammi/m3
    //co2 = ((co2 * 1000) * 12.187 * _MOL_CO2)/(273.15 + temperatura);

      /*Serial.print("P.Atm. [hPa] ");
      Serial.print(pAtm);
      Serial.print(" ; ");
      Serial.print("Temp. [°C] ");
      Serial.print(temperatura);
      Serial.print(" ; ");
      Serial.print("Umidità [%] ");
      Serial.print(humidity);
      Serial.print(" ; ");
      Serial.print("CO2 [g/mc] ");
      Serial.print(CO2_Value);
      Serial.println(" ");*/

      for (int i = 0; i < CAMPIONI; i++)
      {
        CO_Value += elementi[i]._CO;
        NH3_Value += elementi[i]._NH3;
        NO2_Value += elementi[i]._NO2;
      }
      CO_Value = CO_Value/CAMPIONI;
      NH3_Value = NH3_Value/CAMPIONI;
      NO2_Value = NO2_Value/CAMPIONI;

      /*Serial.print("CO [g/m3] ");
      Serial.print(CO_Value);
      Serial.print(" ; ");
      Serial.print("NH3 [ug/m3] ");
      Serial.print(NH3_Value);
      Serial.print(" ; ");
      Serial.print("NO2 [ug/m3] ");
      Serial.print(NO2_Value);
      Serial.print(" ; ");
      Serial.println(" ");*/

      for (int i = 0; i < MISURE; i++)
      {
        polveri2_5 += particolato[i]._PM25;
        polveri_10 += particolato[i]._PM10;
      }
      polveri2_5 = polveri2_5 / MISURE;
      polveri_10 = polveri_10 / MISURE;

      /*Serial.print("PM 2.5 = ");
      Serial.print(polveri2_5);
      Serial.println(" ug/m3");
      Serial.print("PM 10 = ");
      Serial.print(polveri_10);
      Serial.println(" ug/m3");
      Serial.println(" ");*/

      // trasmissione radio dei rilievi
      for(int i = 0; i < 9; i++)
      {
        memset(risultato, 0, DIM_FRAME);

        switch (i)
        {
        case 0 :
          dtostrf(pAtm, 9, 2, risultato);
          risultato[9] = ';';
/*          risultato[11] = '0';
          risultato[12] = '>';*/
          break;
        case 1:
          dtostrf(temperatura, 9, 1, risultato);
          risultato[9] = ';';
/*          risultato[11] = '1';
          risultato[12] = '>';*/
          break;
        case 2:
          dtostrf(humidity, 9, 2, risultato);
          risultato[9] = ';';
/*          risultato[11] = '2';
          risultato[12] = '>';*/
          break;
        case 3:
          dtostrf(CO2_Value, 9, 2, risultato);
          risultato[9] = ';';
/*          risultato[11] = '3';
          risultato[12] = '>';*/
          break;
        case 4:
          dtostrf(CO_Value, 9, 2, risultato);
          risultato[9] = ';';
/*          risultato[11] = '4';
          risultato[12] = '>';*/
          break;
        case 5:
          dtostrf(NH3_Value, 9, 2, risultato);
          risultato[9] = ';';
/*          risultato[11] = '5';
          risultato[12] = '>';*/
          break;
        case 6:
          dtostrf(NO2_Value, 9, 2, risultato);
          risultato[9] = ';';
/*          risultato[11] = '6';
          risultato[12] = '>';*/
          break;
        case 7:
          dtostrf(polveri2_5, 9, 2, risultato);
          risultato[9] = ';';
/*          risultato[11] = '7';
          risultato[12] = '>';*/
          break;
        case 8:
          dtostrf(polveri_10, 9, 2, risultato);
          risultato[9] = ';';
/*          risultato[11] = '8';
          risultato[12] = '>';*/
          break;

        default:
          break;          
        }
      
       xbeeBoard.print(risultato);
       delay(10);
       //svuotaBuffer();
       Serial.print(risultato);
      }
      memset(risultato, 0, DIM_FRAME);
}

void pmRead()
{
  //Serial.println("Routine Board_pm");

  svuotaBuffer();
  delay(1000);
  if (sds.available() > 9)
  {

    memset(frame, 0, 10);
    for(int i=0; i<10; i++)
    {
      frame[i] = sds.read();
      delay(10);
    }
    sd = true;
  }

//  while (sd)
//  {
    sd = startFrame(frame);
    if(sd)
    {
      polveri_10 = emissioni_10(frame);
      polveri2_5 = emissioni2_5(frame);

      particolato[ciclo_3]._PM10 = polveri_10;
      particolato[ciclo_3]._PM25 = polveri2_5;
      ciclo_3++;
      if(ciclo_3 > MISURE-1)
      {
          ciclo_3 = 0;
      }
      /*Serial.print("PM 2.5 = ");
      Serial.print(polveri2_5);
      Serial.println(" ug/m3");
      Serial.print("PM 10 = ");
      Serial.print(polveri_10);
      Serial.println(" ug/m3");
      Serial.println(" "); */
    }
//    else
//    {
//      sd = false;
//    }
    sd = false;
//  }
  
}

void sampleboard()
{
    Serial.println(ciclo_4);
    Board_8128();
    board6814();
    pmRead();

  // ************** Per verifica *****************
  /*Serial.print("CO [ppm] ");
  Serial.print(CO_Value);
  Serial.print(" ; ");
  Serial.print("NH3 [ppm] ");
  Serial.print(NH3_Value);
  Serial.print(" ; ");
  Serial.print("NO2 [ppm] ");
  Serial.print(NO2_Value);
  Serial.print(" ; ");
  Serial.println(" ");
  Serial.print("PM 2.5 = ");
  Serial.print(polveri2_5);
  Serial.println(" ug/m3");
  Serial.print("PM 10 = ");
  Serial.print(polveri_10);
  Serial.println(" ug/m3");
  Serial.println(" ");
  Serial.print("ciclo = ");
  Serial.println(ciclo_4);*/

  // *********************************************

  if(ciclo_4 < 10)
  {
    ciclo_4 += 1;
  }
  else
  {
    ciclo_4 = 0;
    trx();
  }

}

void setup() {

  Serial.begin(9600);
  xbeeBoard.begin(9600);
  sds.begin(9600);

  lcd.begin(16, 2);
  lcd.createChar(0, Grado);
  lcd.createChar(1, mu);
  lcd.setCursor(0, 0);
  lcd.print("    MISURE");
  lcd.setCursor(0, 1);
  lcd.print("  AMBIENTALI");
  
  Wire.begin();
  Wire.setClock(400000);

  adsBoard.setGain(GAIN_TWOTHIRDS); // 2/3x gain +/- 6.144V   0.1875mV (default)
  adsBoard.begin();

  dsp.onRun(display);
  dsp.setInterval(3000);

  pls.onRun(menu);
  pls.setInterval(100);

  analisi.onRun(sampleboard);
  analisi.setInterval(60000);

  ctr.add(&pls);
  groupOfThreads.add(&dsp);
  groupOfThreads.add(&analisi);
  ctr.add(&groupOfThreads);

  // Configurazione scheda BME280
  bmeBoard.settings.commInterface = I2C_MODE;
  bmeBoard.setI2CAddress(0x76);
  bmeBoard.begin();
  bmeBoard.setFilter(1);
  bmeBoard.setStandbyTime(0);
  bmeBoard.setTempOverSample(5);
  bmeBoard.setPressureOverSample(5);
  bmeBoard.setHumidityOverSample(5);
  bmeBoard.setMode(MODE_NORMAL);

  hdcBoard.begin(0x40);

  ccsBoard.begin();

  delay(2000);

}


void loop() {

ctr.run();
}