#ifndef SCHEDULER_H
#define SCHEDULER_H

#define SAMPLE 10        // scheda 8128 misure ambientali
#define CAMPIONI 10      // scheda 6814 CO, NH3, NO2
#define MISURE 10       // scheda pm 10 e pm 2.5

typedef enum
{
    p_baro = 1,
    t_amb,
    h_amb,
    CO_2,
    NH_3,
    CO,
    NO_2,
    PM25,
    PM10
}ElencoInquinanti;

ElencoInquinanti elemento;

typedef struct          // misure ambientali scheda 8128 lettura ogni 10 s
{
   float p_atm;
   float temp;
   float hum;
   float co_2; 
}ambientale;

ambientale ambientali[SAMPLE];

typedef struct          // scheda cjmcu6814 lettura ogni 15 s
{
    float _CO;
    float _NH3;
    float _NO2;
}inquinanti;

inquinanti elementi[CAMPIONI];

typedef struct          // scheda SDS011 lettura ogni 5 s
{
    float _PM25;
    float _PM10;
}polveri;

polveri particolato[MISURE];


#endif