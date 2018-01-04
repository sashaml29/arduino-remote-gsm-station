#include <VirtualWire.h>
#include <LowPower.h>

#include "DHTmy.h"
//#include "DHT.h"

#include <OneWire.h>
#define POWERDHTPIN 7
#define POWERTRANSMITTERPIN 6
#define DHTPIN 2
#define DHTTYPE DHT22
#define  device_id 1 // идентификатор датчика 
DHT dht(DHTPIN, DHTTYPE);
OneWire ds(3);
String info;

void sendtext(String texttosend)  //посылает текстовую произвольную строку для отладки если в строке есть байт со значением 0, то на нем строка оборвется
{
  String sendstring = "DT" + texttosend;
  char msg[255];
  sendstring.toCharArray(msg, 255);
  digitalWrite(POWERTRANSMITTERPIN, HIGH);
  vw_send((uint8_t *)msg, strlen(msg));
  vw_wait_tx();
  digitalWrite(POWERTRANSMITTERPIN, LOW);
}

void sendarray (char msg[30],int len)  //посылает массив байт длиной len не более 30 символов
{
  digitalWrite(POWERTRANSMITTERPIN, HIGH);
  vw_send((uint8_t *)msg, len);
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
  vw_set_tx_pin(5); // передатчик на 5 пине
  //vw_set_ptt_inverted(true); // Required for DR3100
  vw_set_ptt_inverted(false); 
  vw_setup(2000);
  for (int i=0; i <= 10; i++) { // помигаем диодом, чтобы показать что скетч стартует   
  delay(50);
  digitalWrite(13, true); 
  digitalWrite(0, true);
  delay(50);
  digitalWrite(13, false); 
  digitalWrite(0, false);
  }
  //установим разрешение датчика на точность 0,5 градуса, время на конвертацию будет меньше
  ds.reset();
  ds.write(0xCC); // пропуск rom - работа со всеми датчиками на шине
  ds.write(0x4E); // команда записи следующих трех байт в оперативную память датчика
  ds.write(0x00); // ненужный байт
  ds.write(0x00); // ненужный байт
  ds.write(0x1F); // байт конфигурации 0x1F (00011111) -разрешение 9 bit , 0x7F (00111111) -10 бит,  0x5F (01011111)-11 бит 0x7F (01111111) -12 бит,  важна установка 5 и 6 бита, остальные не используются, потому в разных примерах hex значения могут отличаться
  delay(15);
  ds.reset();
  ds.write(0x48); // // команда записи в eprom записи конфигурации (иначе после выкл питания состояние датчика не запоминается), в некоторых примерах пишут, что запись в епром должна быть  только после  ds.reset()
  delay(15);
}


void loop()

{

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
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);// поспим секунду вместо вместо delay(1000) - все таки 10 миллиампер пторебления,если разрешение термодатчика 9 bit, время преобразования будет около 0.1 сек, остальное в время датчик тока почти не потребляет
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
  byte tempdht=Temp;
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
  char symbolds = Temp;
  char symbold = device_id;
  char symbolt = tempdht;
  char symbolh = h;
  char sbatt = 'B';
  int vcc = readVcc() / 100;
   vcc = readVcc() / 100; //передадим напряжение питания с точностью только до десятых вольта
  char symbolvcc = vcc;
  if (vcc < 35) sbatt = 'b'; // если питание меньше 3.5 в имя параметра батарейки - маленькая b

  strMsg = strMsg + symbold + "t" + symbolt + "h" + symbolh + sbatt + symbolvcc; // передаем символ признака данных потом ид датчика потом название параметра потом значение параметра
  //strMsg = strMsg + symbold1+symbold2 +"t"+ symbolt;
  //strMsg = "jhgh"+symbold+symbolds+"j";
  //strMsg="dsds";

  // strMsg=String(info);
  byte msg[30];
 
 msg[0]=symbold;
 msg[1]=symbolds;
 msg[2]=symbolvcc;

    
  
 // strMsg.toCharArray(msg, 255);
  digitalWrite(POWERTRANSMITTERPIN, HIGH);
  long int ms = millis();
  //while (millis()<ms+5000) vw_send((uint8_t *)msg, 3); // отладочная посыдка длинная 10  сек
 // vw_send((uint8_t *)msg, strlen(msg));
  vw_send((uint8_t *)msg, 3);
  vw_wait_tx(); // Wait until the whole message is gone - ждать не будем так как это должна быть единственная передача в скетче

 // msg[0]=symbold+100;
 
//  msg[10]=symbolds;
  
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
  // на светодиод стоит резистор 2.2 к , потребление при 4в 2 ма, светит только когда передатчик работает, на светодиод уходит около 10 процентов энергии

  //  данные на 8 Мгц и 18ds20 3 аккумулятора  итого 4 вольта питания, ток в спячке 7мка 8 сек, ток преобразования датчика 0.7 ма в течении 1 сек, ток в режимме передачи 20 ма
  // грубо 0.01 ма в спячке, 1 ма за  1 сек преобразование, 20ма за 0.1 сек передача, за цикл 8 сек 8*0,01+1*1+20*0,1 =0,08+1+2=3ма*сек, средний ток 3/8 =0.4 ма 1000
  // 1000ма/0,4=2500 часов   2500/24 =100 суток или 3 месяца минимум
  // если  разрядность датчика 9 bit то время преобразования 0,1 сек, 8*0,01 +1*0,1+2=2,2 ма/сек, в полтора раза меньше (время работы около 5 мес)
  //если посылка каждый 8 цикл спячки то 64*0,01++1*1=20*0,1=4ма*сек, средний ток 4/64=0,06 ма, 1000ма/0,06=17000 часов или 690 суток или 2 года, реже не имеет смысла-саморазряд батареи
  // ниже 8 мгц запустить не удалось датчик на onewire, низкая частота нужна не только для малого энергопотребления, но и для пониженног питания, для 8 мгц это 3.3 вольта, надо 3 аккумулятора
  // на 2 может не работать, но передатчику для мощности нужно больше вольтажа, потму пусть будет 3
  

  //LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);

}

