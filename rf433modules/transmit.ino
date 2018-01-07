#include <avr/wdt.h>  // watchdog
#include <VirtualWire.h>
#include <LowPower.h>
#include <EEPROM.h>
#include "DHTmy.h"
#include <OneWire.h>
//#include "DHT.h"



#define POWERDHTPIN 7 // с этой ного подается питание на датчик dht22
#define DHTPIN 2 // нога данных датчика dht
#define POWERTRANSMITTERPIN 6 // с этой ноги подается питание на передатчик
#define LEDPIN 13 // нога индикаторного диода
#define TRANSMITTERDATAPIN 5 // передатчик на 5 пине
#define DHTTYPE DHT22

/*
    типы датчиков
    1-один сенсор температуры
    2-два сенсора температуры
    3-три сенсора температуры
    4-четыре сенсора температуры
    5-пять сенсоров температуры
    11 - сенсор температуры и влажности
*/
int watchdogenabled = 0; // 1 или 0 - будет ли использоваться в скетче watchdog, нормально работает только при заливке скетча стандартным образом и наличии правильного optiboot, при заливке через usbasp - не работает, при срабатывании watchdog получается bootloop
int  device_id = 1; // идентификатор датчика, потом прочитаем из EEPROM
int  device_type = 1;
int  vbat_corr = 100; // коррекция измеренного напр питания
char device_name[4] = {'t', 'e', 's', 't'};
int Temp; //температура, считанная с 18dsb20
byte data_temp; // посылаемый байт температуры, целое -температура*2, на приемной стороне поделим на 2 и получим температуру с точностью до 0,5 градуса
int t; // температурв с dht
int h; //влажность с dht
byte msg[30];
int j = 0; //счетчик основного цикла

DHT dht(DHTPIN, DHTTYPE);
OneWire ds(3);
String info;

void writefirstdataeeprom()
{
  EEPROM.write(0, 2); //ид датчика
  EEPROM.write(1, 1); //тип датчика
  EEPROM.write(2, 't'); //1 символ имени датчика
  EEPROM.write(3, 'e'); //2 символ имени датчика
  EEPROM.write(4, 's'); //3 символ имени датчика
  EEPROM.write(5, '2'); //4 символ имени датчика
  EEPROM.write(6, 100); //значение коррекции измерения напряжения питания батареи, индивидуальное для каждого микроконтроллера, в процентах, 100% -коррекции нет
}

void setup() {
  wdt_disable();
  // writefirstdataeeprom(); //раскомментировать для однократного запуска, обязательно должны быть фьюзы для eesave, иначе при перепрограммировании еепром стирается

  if (  (EEPROM.read(0) > 0) && ((EEPROM.read(0) < 255))  ) // если в еепром не 0 и  не 255 значит читаем из нее данные, иначе- она была затерта, данные не читаем, используем по умолчанию
  {
    device_id = EEPROM.read(0); // в ячейке 0 хранится ид датичка
    device_type = EEPROM.read(1);
    device_type = 1; //может и ненадо хранить епром тип датчика, алгоритмы для каждого типа свои
    device_name[0] = EEPROM.read(2);
    device_name[1] = EEPROM.read(3);
    device_name[2] = EEPROM.read(4);
    device_name[3] = EEPROM.read(5);
    vbat_corr = EEPROM.read(5);
  }

  pinMode(LEDPIN, OUTPUT);
  pinMode(POWERDHTPIN, OUTPUT);
  pinMode(POWERTRANSMITTERPIN, OUTPUT);
  
  Serial.begin(9600);
  dht.begin();

  vw_set_tx_pin(TRANSMITTERDATAPIN);
  //vw_set_ptt_inverted(true); // Required for DR3100
  vw_set_ptt_inverted(false); //непонятно что правильнее, работает то и другое

  vw_setup(2000);
  readdata();
  for (int i = 0; i <= 10; i++)
  { // помигаем диодом часто, делая полную посылку данных, чтобы показать что скетч стартует
    sendfull();
    delay(100);
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
  if ( watchdogenabled == 1) wdt_enable (WDTO_8S); // watchdog на 8 секунд, родной загрузчик ардуино должен быть обязательно заменен на otiboot, иначе будет циклическая перезагрузка
}




void loop()
{
  wdt_reset();  // вызываемые процедуры из loop() суммарно не должны длиться более 8 секунд (либо в них должен стоять еще wdt_reset(); ) , иначе перезагруга
  j = j + 1;
  readdata();
  if (j < 10)
  {
    sendshort();
   // sendfull(); //для отладки
  }
  else
  {
    sendfull();
    j = 0;
  }

  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF); //спим нужное количество времени
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  if ( watchdogenabled == 1) wdt_enable (WDTO_8S); // powerDown отключает watchdogenabled, надо его снова активировать

  //  delay(10000);
  //тест ватчдога, если раскомментировать, то будет перезагрузка

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
}

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

void sendarray (char msg[30], int len) //посылает массив байт длиной len не более 30 символов
{
  digitalWrite(POWERTRANSMITTERPIN, HIGH);
  vw_send((uint8_t *)msg, len);
  vw_wait_tx();
  digitalWrite(POWERTRANSMITTERPIN, LOW);
}

// Измерение напряжения питания
int readVcc()
{
  long int result;
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  //delay(75); // Wait for Vref to settle // без задержки неверно меряет только первый раз после стартаб потом нрмально
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring
  result = ADCL;
  result |= ADCH << 8;
  result =  1125300L / result; // (Kvcc * 1023.0 * 1000) (in mV) 
  if ((vbat_corr>60) && (vbat_corr<140)) // значение, считанное еепром для корректировки батареи лежит в разумных пределах +- 40%
  {
  result=(result*vbat_corr)/100; // корректируем напр питания   
  }
  return result;
}


void readdata()
{
  digitalWrite(DHTPIN, HIGH);
  digitalWrite(POWERDHTPIN, HIGH); // питание датчика включаем

  byte data[2];
  ds.reset();
  ds.write(0xCC);
  ds.write(0x44); // посылаем команду начала преобразования

  // датчику надо дать секунду с вкл питанием чтобы инициализироваться
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);// поспим секунду вместо вместо delay(1000) - все таки 10 миллиампер пторебления,если разрешение термодатчика 9 bit, время преобразования будет около 0.1 сек, остальное в время датчик тока почти не потребляет
  if ( watchdogenabled == 1) wdt_enable (WDTO_8S);


  ds.reset();
  ds.write(0xCC);
  ds.write(0xBE);
  data[0] = ds.read();
  data[1] = ds.read();
  Temp = (data[1] << 8) + data[0];
  //Temp = Temp >> 4; // если сделать так, то в Temp окажется целочисленное значение температуры
  Temp = Temp >> 3; // // если сделать так, то в Temp окажется целочисленное значение температуры, умноженное на 2, так как точность преобразования выставили на сенсоре 0,5 градуса, 
  
  //Serial.println(Temp);
 

  data_temp = Temp;

  int t = 99;
  int h = 99;
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

  digitalWrite(POWERDHTPIN, LOW);// выкл питание  dht22, надо питание с ноги  пускать через диод, иначе толку нет
  digitalWrite(DHTPIN, LOW);// ногу данных в 0, иначе через нее течет ток 80 мка
}


void sendshort() // передача данных в сокращенном виде - только номердатчика и сами данные
{
  digitalWrite(POWERTRANSMITTERPIN, HIGH); // включим питание передатчика
  digitalWrite(LEDPIN, true); // включим светодиод на время передачи

  msg[0] = device_id;
  msg[1] = data_temp;
  encrypt(2); // так шифруем передачу данные дублируются в обратном порядке, на приемной стороне так убедимся, что это наша передача и заодно удостоверимся в корректности данных
  //посылаем вдвое больше символов сообщения, чем было исходных
  vw_send((uint8_t *)msg, 4);

  vw_wait_tx(); // ждем  икончания передачи
  digitalWrite(POWERTRANSMITTERPIN, LOW); //выключим питание передатчика
  digitalWrite(LEDPIN, false); //гасим диод
}

void sendfull() // полная посылка данных с именем датчика, типом, и состоянием батареи
{
  char sbatt = 'B';
  int vcc = readVcc() / 100;
  vcc = readVcc() / 100; //передадим напряжение питания с точностью только до десятых вольта
  char symbolvcc = vcc;
  if (vcc < 35) sbatt = 'b'; // если питание меньше 3.5 в имя параметра батарейки - маленькая b
  
  digitalWrite(POWERTRANSMITTERPIN, HIGH); // включим питание передатчика, если померить напряжение при включенном передатчике оно может быть меньше
  digitalWrite(LEDPIN, true); // включим светодиод на время передачи

  
  msg[0] = 100 + device_id; // номер датчика больше 100, признак того что пошла полная посылка, на приемной стороне вычтем 100, чтобы получить номер датчика
  msg[1] = device_type; // тип датчика
  msg[2] = device_name[0]; // имя датчика
  msg[3] = device_name[1];
  msg[4] = device_name[2];
  msg[5] = device_name[3];
  msg[6] = sbatt; // символ разряжена батарея (b) или нет (B)
  msg[7] = symbolvcc; // напряжение питания, умноженное на 10
  msg[8] = data_temp; // сами данные
  encrypt(9);
  vw_send((uint8_t *)msg, 18);

  vw_wait_tx(); // ждем  икончания передачи
  digitalWrite(POWERTRANSMITTERPIN, LOW); //выключим питание передатчика
  digitalWrite(LEDPIN, false); //гасим диод

}

void encrypt(int len) //шифруем массив msg длиной len, доавляя в конец столько же символов, но в обратном порядке
{
  for (int i = 0; i < len; i++)
  {
    msg[len + i] = msg[len - i - 1];
  }
}
