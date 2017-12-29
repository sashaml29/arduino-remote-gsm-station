#include <VirtualWire.h>
// передатчик на 11 пине это PB3 MOSI/OC2  НОГА 17 МИКРОСХЕМЫ
#include <LowPower.h>

#include "DHTmy.h"
//#include "DHT.h"

#include <OneWire.h>
#define POWERDHTPIN 7
#define POWERTRANSMITTERPIN 6
#define DHTPIN 2
#define DHTTYPE DHT22
char device_id[] = "K1"; // идентификатор датчика из двух символов
DHT dht(DHTPIN, DHTTYPE);
OneWire ds(3);
String info;

void sendtext(String texttosend)  //посылает текстовую произвольную строку для отладки
{
  String sendstring = "DT" + texttosend;
  char msg[255];
  sendstring.toCharArray(msg, 255);
  digitalWrite(POWERTRANSMITTERPIN, HIGH);
  vw_send((uint8_t *)msg, strlen(msg));
  vw_wait_tx();
  digitalWrite(POWERTRANSMITTERPIN, LOW);
}


// Измерение напряжения питания
int readVcc()
{
  int result;
 ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  //delay(75); // Wait for Vref to settle // без задержки неверно меряет только первый раз после стартаб потом нрмально
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring
  result = ADCL;
  result |= ADCH << 8;
  result =  1125300L / result; // (Kvcc * 1023.0 * 1000) (in mV); у у меня эта фрмула завышает на 4%
  return result;
}


void setup() {
  // initialize digital pin 13 as an output.
  pinMode(13, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(POWERTRANSMITTERPIN, OUTPUT);
  pinMode(0, OUTPUT);
  Serial.begin(9600);
  dht.begin();
  vw_set_tx_pin(5);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);
}


void loop()

{
 // часть функции readvcc
  digitalWrite(DHTPIN, HIGH);
  digitalWrite(POWERDHTPIN, HIGH); // питание датчика включаем

  byte data[2];
  ds.reset();
  ds.write(0xCC);
  ds.write(0x44);



  int t = 0;
  int h = 0;

  float msec;
  msec = millis();
  // датчику надо дать секунду с вкл питанием чтобы инициализироваться
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);// поспим секунду вместо вместо delay(1000) - все таки 10 миллиампер пторебления
  digitalWrite(13, true); // Flash a light to show transmitting
  digitalWrite(0, true);

  ds.reset();
  ds.write(0xCC);
  ds.write(0xBE);
  data[0] = ds.read();
  data[1] = ds.read();
  int Temp = (data[1] << 8) + data[0];
  Temp = Temp >> 4;
  //Serial.println(Temp);
  info = Temp;
 // sendtext(info);

  float tt = dht.readTemperature(false, true); // первый параметр- градусы цельсия, второй -форсировать чтение новых данных, иначе библиотека думет что прошло мало времени
  float hh = dht.readHumidity();

  if (isnan(hh) || isnan(tt))
  {
    t = 99;
    h = 99;
  }
  else
  {
    t = tt;
    h = hh;
  };

  //  Serial.print("Temperature: ");
  //  Serial.print(t);
  //  Serial.print("   Humidity: ");
  //  Serial.println(h);

  String strMsg = "D "; //начало посылки два символа признак что данные с датчика
  char symbold1 = device_id[0];
  char symbold2 = device_id[1];
  char symbolt = t;
  char symbolh = h;
  char sbatt = 'B';
  int vcc = readVcc() / 100; //передадим напряжение питания с точностью только до десятых вольта
  char symbolvcc = vcc;
  if (vcc < 35) sbatt = 'b'; // если питание меньше 3.5 в имя параметра батарейки - маленькая b

  strMsg = strMsg + symbold1 + symbold2 + "t" + symbolt + "h" + symbolh + sbatt + symbolvcc; // передаем символ признака данных потом ид датчика потом название параметра потом значение параметра
  //strMsg = strMsg + symbold1+symbold2 +"t"+ symbolt;
 
  char msg[255];
  strMsg.toCharArray(msg, 255);
  digitalWrite(POWERTRANSMITTERPIN, HIGH);
  long int ms = millis();
  // while (millis()<ms+100000) vw_send((uint8_t *)msg, strlen(msg)); // отладочная посыдка длинная
  vw_send((uint8_t *)msg, strlen(msg));
  vw_wait_tx(); // Wait until the whole message is gone - ждать не будем так как это должна быть единственная передача в скетче
// время, нужное на передачу - 50 миллисек+ 6 милсек на каждый символ msg
  digitalWrite(POWERTRANSMITTERPIN, LOW);
  
  digitalWrite(13, false);
  digitalWrite(0, false);
  digitalWrite(POWERDHTPIN, LOW);// выкл питание  dht22, надо питание с ноги  пускать через диод, иначе толку нет
  digitalWrite(DHTPIN, LOW);// ногу данных в 0, иначе через нее течет ток 80 мка
  msec = millis() - msec;
  info = msec;
  //sendtext(info);
  // на 16 мгц в среднем чтение датчик- 2 мсек, посылка данных 110 мсек, всего 0.11 с потреблением 20 ма и 1 сек с включенным питанием питанием датчика 80 мка
  //потребление в режиме ожидания 22 мка-если питание датчика через диод от ноги, если напрямую от пллюса - то потребление всей схемы в спячке 100 мка
  // если посылка каждую минуту то средний ток за минуту (0,11*20+1*0,08)/60+0,022=(2,2+0,08)/60+0,022=2,3/60+0,022=0,038+0,022=0,06=60 мка
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}



