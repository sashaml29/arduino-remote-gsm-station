#include <avr/wdt.h>  // watchdog
#include "DHTmy.h" // в библиотеке изменены 2 строчки: 
////delay(250);  //закомментирована только эта строка, так как заранее до вызова библиотеки в взводим ногу в хай
//  delay(2); // здесь было 20 миллисек но по даташиту dht22 достаточно полмилисекунды
#include <VirtualWire.h>
#include <LowPower.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <SoftwareSerial.h>
SoftwareSerial SoftSerialHC(18, 17); // RX, TX
#define HC12SET 16
#define POWERTRANSMITTERPIN 6 // с этой ноги подается питание на передатчик
#define LEDPIN 13 // нога индикаторного диода
#define TRANSMITTERDATAPIN 5 // передатчик на 5 пине
int watchdogenabled = 0; // 1 или 0 - будет ли использоваться в скетче watchdog, нормально работает только при заливке скетча стандартным образом и наличии правильного optiboot, при заливке через usbasp - не работает, при срабатывании watchdog получается bootloop

// блок для eeprom
/*  типы датчиков
    0-все однобайтные сенсоры температуры на ногах 3, 8, 7, точность 0,5 градуса, в байте темп умноженная на 2
    1-первый сенор влажности на ноге 3, остальные - однобайтные температуры
    2-первый и второй сенсор влажности, остальные- однобайтная температура
*/
byte  device_id = 2; // идентификатор датчика, потом прочитаем из EEPROM
byte  device_type = 1;
char device_name[4] = {'d', 'a', 't', '2'};
byte  vbat_corr = 100; // коррекция измеренного напр питания
int dscount = 1;
int dhtcount = 1;



int Temp1; //температура, считанная с 18dsb20
byte data_temp1; // посылаемый байт температуры, целое - температура*2, на приемной стороне поделим на 2 и получим температуру с точностью до 0,5 градуса
int Temp2; //температура, считанная с 18dsb20
byte data_temp2; // посылаемый байт температуры, целое - температура*2, на приемной стороне поделим на 2 и получим температуру с точностью до 0,5 градуса
int Temp3; //температура, считанная с 18dsb20
byte data_temp3; // посылаемый байт температуры, целое - температура*2, на приемной стороне поделим на 2 и получим температуру с точностью до 0,5 градуса
int TempDHT1; // температурв с dht1
int hDHT1; //влажность с dht1
byte data_temp_dht1;
byte data_h_dht1;
int TempDHT2; // температурв с dht1
int hDHT2; //влажность с dht1
byte data_temp_dht2;
byte data_h_dht2;

byte msg[40]; //массив для отсылки данных
int j = 0; //счетчик основного цикла
OneWire ds1(3); // датчик 18ds20 на 3 пине
OneWire ds2(8); //
OneWire ds3(7); //
DHT dht1(14, DHT22); // сенсоры am2320 на ногах 14 и 15
DHT dht2(15, DHT22);


String info;


void setup() {
  wdt_disable();

  //writefirstdataeeprom(); //раскомментировать для однократного запуска, обязательно должны быть фьюзы для eesave, иначе при перепрограммировании еепром стирается
  if (  (EEPROM.read(0) > 0) && ((EEPROM.read(0) < 255))  ) // если в еепром не 0 и  не 255 значит читаем из нее данные, иначе- она была затерта, данные не читаем, используем по умолчанию
  {
    device_id = EEPROM.read(0); // в ячейке 0 хранится ид датичка
    device_type = EEPROM.read(1);
    //  device_type = 0; //может и ненадо хранить епром тип датчика, алгоритмы для каждого типа свои
    device_name[0] = EEPROM.read(2);
    device_name[1] = EEPROM.read(3);
    device_name[2] = EEPROM.read(4);
    device_name[3] = EEPROM.read(5);
    vbat_corr = EEPROM.read(6);
    dscount = EEPROM.read(7);
    dhtcount = EEPROM.read(8);
  }
  dscount = 1;
  dhtcount = 1;
  pinMode(LEDPIN, OUTPUT);
  pinMode(19, INPUT);
  digitalWrite(19, HIGH); //подтягиваем вход к плюсу

  pinMode(POWERTRANSMITTERPIN, OUTPUT);
  pinMode(HC12SET, OUTPUT);
  digitalWrite(HC12SET, HIGH); //hc 12 в нормальном режиме
  SoftSerialHC.begin(2400);
  delay(100);
  digitalWrite(HC12SET, LOW); //hc 12 в настроечном
  delay(100); //время чтобы войти в настроечный режим
  SoftSerialHC.println("AT+FU1");
  delay(100); //время для того чтобы принять команду
  digitalWrite(HC12SET, HIGH); //hc 12 в нормальном режиме
  delay(100);
  Serial.begin(9600);

  vw_set_tx_pin(TRANSMITTERDATAPIN);
  //vw_set_ptt_inverted(true); // Required for DR3100
  vw_set_ptt_inverted(false); //непонятно что правильнее, работает то и другое
  vw_setup(2400);
  readdata();
  for (int i = 0; i <= 10; i++)
  { // помигаем диодом часто, делая полную посылку данных, чтобы показать что скетч стартует
    //    sendfull();
    digitalWrite(LEDPIN, true); // включим светодиод на время передачи
    delay(100);
    digitalWrite(LEDPIN, false); // включим светодиод на время передачи
    delay(100);
  }
  //установим разрешение датчика на точность 0,5 градуса, время на конвертацию будет меньше
  if (dscount > 0)
  {
    ds1.reset();
    ds1.write(0xCC); // пропуск rom - работа со всеми датчиками на шине
    ds1.write(0x4E); // команда записи следующих трех байт в оперативную память датчика
    ds1.write(0x00); // ненужный байт
    ds1.write(0x00); // ненужный байт
    ds1.write(0x1F); // байт конфигурации 0x1F (00011111) -разрешение 9 bit , 0x7F (00111111) -10 бит,  0x5F (01011111)-11 бит 0x7F (01111111) -12 бит,  важна установка 5 и 6 бита, остальные не используются, потому в разных примерах hex значения могут отличаться
    delay(15);
    ds1.reset();
    ds1.write(0x48); // // команда записи в eprom записи конфигурации (иначе после выкл питания состояние датчика не запоминается), в некоторых примерах пишут, что запись в епром должна быть  только после  ds.reset()
  }

  if (dscount > 1)
  {
    ds2.reset();
    ds2.write(0xCC); // пропуск rom - работа со всеми датчиками на шине
    ds2.write(0x4E); // команда записи следующих трех байт в оперативную память датчика
    ds2.write(0x00); // ненужный байт
    ds2.write(0x00); // ненужный байт
    ds2.write(0x1F); // байт конфигурации 0x1F (00011111) -разрешение 9 bit , 0x7F (00111111) -10 бит,  0x5F (01011111)-11 бит 0x7F (01111111) -12 бит,  важна установка 5 и 6 бита, остальные не используются, потому в разных примерах hex значения могут отличаться
    delay(15);
    ds2.reset();
    ds2.write(0x48); // // команда записи в eprom записи конфигурации (иначе после выкл питания состояние датчика не запоминается), в некоторых примерах пишут, что запись в епром должна быть  только после  ds.reset()
  }

  if (dscount > 2)
  {
    ds3.reset();
    ds3.write(0xCC); // пропуск rom - работа со всеми датчиками на шине
    ds3.write(0x4E); // команда записи следующих трех байт в оперативную память датчика
    ds3.write(0x00); // ненужный байт
    ds3.write(0x00); // ненужный байт
    ds3.write(0x1F); // байт конфигурации 0x1F (00011111) -разрешение 9 bit , 0x7F (00111111) -10 бит,  0x5F (01011111)-11 бит 0x7F (01111111) -12 бит,  важна установка 5 и 6 бита, остальные не используются, потому в разных примерах hex значения могут отличаться
    delay(15);
    ds3.reset();
    ds3.write(0x48); // // команда записи в eprom записи конфигурации (иначе после выкл питания состояние датчика не запоминается), в некоторых примерах пишут, что запись в епром должна быть  только после  ds.reset()
  }
  dht1.begin();
  dht2.begin();
  delay(15);
  if ( watchdogenabled == 1) wdt_enable (WDTO_8S); // watchdog на 8 секунд, родной загрузчик ардуино должен быть обязательно заменен на otiboot, иначе будет циклическая перезагрузка
  byte normalmode = digitalRead(19);
  if (normalmode == 0) // режим тестовой посылки каждую секунду
  {
    digitalWrite(LEDPIN, true);
    delay(4000); // покажем что вошли в тестовый режим
    digitalWrite(LEDPIN, false);
    while (1 == 1)
    {
      wdt_reset();
      readdata();
      sendshort();
    }
  }
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

  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF); //спим нужное количество времени
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  if ( watchdogenabled == 1) wdt_enable (WDTO_8S); // powerDown отключает watchdogenabled, надо его снова активировать
  //  delay(10000);
  //тест ватчдога, если раскомментировать, то будет перезагрузка
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

int readVcc()  // Измерение напряжения питания
{
  long int result;
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  //delay(75); // Wait for Vref to settle // без задержки неверно меряет только первый раз после стартаб потом нормально
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring
  result = ADCL;
  result |= ADCH << 8;
  result =  1125300L / result; // (Kvcc * 1023.0 * 1000) (in mV)
  if ((vbat_corr > 60) && (vbat_corr < 140)) // значение, считанное еепром для корректировки батареи лежит в разумных пределах +- 40%
  {
    result = (result * vbat_corr) / 100; // корректируем напр питания
  }
  return result;
}

void readdata()
{
  byte data[2];
  ds1.reset();
  ds1.write(0xCC);
  ds1.write(0x44); // посылаем команду начала преобразования
  ds2.reset();
  ds2.write(0xCC);
  ds2.write(0x44); // посылаем команду начала преобразования
  ds3.reset();
  ds3.write(0xCC);
  ds3.write(0x44); // посылаем команду начала преобразования
  // датчику надо дать секунду с вкл питанием чтобы инициализироваться
  digitalWrite(14, HIGH); // подаем высокий уровень на ноги датчиков dht
  digitalWrite(15, HIGH);
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);// поспим секунду вместо вместо delay(1000) - все таки 10 миллиампер пторебления,если разрешение термодатчика 9 bit, время преобразования будет около 0.1 сек, остальное в время датчик тока почти не потребляет
  if ( watchdogenabled == 1) wdt_enable (WDTO_8S);
  ds1.reset();
  ds1.write(0xCC);
  ds1.write(0xBE);
  ds2.reset();
  ds2.write(0xCC);
  ds2.write(0xBE);
  ds3.reset();
  ds3.write(0xCC);
  ds3.write(0xBE);


  data[0] = ds1.read();
  data[1] = ds1.read();
  Temp1 = (data[1] << 8) + data[0];
  //Temp = Temp >> 4; // если сделать так, то в Temp окажется целочисленное значение температуры
  Temp1 = Temp1 >> 3; // // если сделать так, то в Temp окажется целочисленное значение температуры, умноженное на 2, так как точность преобразования выставили на сенсоре 0,5 градуса,
  //Serial.println(Temp);
  data_temp1 = Temp1;

  data[0] = ds2.read();
  data[1] = ds2.read();
  Temp2 = (data[1] << 8) + data[0];
  //Temp = Temp >> 4; // если сделать так, то в Temp окажется целочисленное значение температуры
  Temp2 = Temp2 >> 3; // // если сделать так, то в Temp окажется целочисленное значение температуры, умноженное на 2, так как точность преобразования выставили на сенсоре 0,5 градуса,
  //Serial.println(Temp);
  data_temp2 = Temp2;

  data[0] = ds3.read();
  data[1] = ds3.read();
  Temp3 = (data[1] << 8) + data[0];
  //Temp = Temp >> 4; // если сделать так, то в Temp окажется целочисленное значение температуры
  Temp3 = Temp3 >> 3; // // если сделать так, то в Temp окажется целочисленное значение температуры, умноженное на 2, так как точность преобразования выставили на сенсоре 0,5 градуса,
  //Serial.println(Temp);
  data_temp3 = Temp3;


  float tt = dht1.readTemperature(false, true); // первый параметр- градусы цельсия, второй -форсировать чтение новых данных, иначе библиотека думет что прошло мало времени
  float hh = dht1.readHumidity();
  tt = tt * 2;
  TempDHT1 = tt;
  hDHT1 = hh;
  data_temp_dht1 = TempDHT1;
  data_h_dht1 = hDHT1;

  tt = dht2.readTemperature(false, true); // первый параметр- градусы цельсия, второй -форсировать чтение новых данных, иначе библиотека думет что прошло мало времени
  hh = dht2.readHumidity();
  tt = tt * 2;
  TempDHT2 = tt;
  hDHT2 = hh;
  data_temp_dht2 = TempDHT1;
  data_h_dht2 = hDHT2;

}


void sendshort() // передача данных в сокращенном виде - только номердатчика и сами данные
{
  digitalWrite(POWERTRANSMITTERPIN, HIGH); // включим питание передатчика
  digitalWrite(LEDPIN, true); // включим светодиод на время передачи
  msg[0] = 10 * device_type + device_id; //в первом байте тип датчика и номер датчика, десятки- тип датчика, единицы- номер датчика
  if (device_type == 0)
  {
    msg[1] = data_temp1;
    msg[2] = data_temp2;
    msg[3] = data_temp3;
  }
  if (device_type == 1)
  {
    msg[1] = data_h_dht1;
    msg[2] = data_temp_dht1;
    msg[3] = data_temp1;
    msg[4] = data_temp2;
    msg[5] = data_temp3;
  }
  if (device_type == 2)
  {
    msg[1] = data_h_dht1;
    msg[2] = data_h_dht2;
    msg[3] = data_temp_dht1;
    msg[4] = data_temp_dht2;
    msg[5] = data_temp1;
    msg[6] = data_temp2;
    msg[7] = data_temp3;
  }


  encrypt(1 + dscount + dhtcount * 2); // так шифруем передачу данные дублируются в обратном порядке, на приемной стороне так убедимся, что это наша передача и заодно удостоверимся в корректности данных, посылаем вдвое больше символов сообщения, чем было исходных
  vw_send((uint8_t *)msg, (1 + dscount + dhtcount * 2) * 2);
  vw_wait_tx(); // ждем  икончания передачи
  // sendhc12((1 + dscount) * 2);
  digitalWrite(POWERTRANSMITTERPIN, LOW); //выключим питание передатчика
  digitalWrite(LEDPIN, false); //гасим диод
}

void sendfull() // полная посылка данных с именем датчика, типом, и состоянием батареи
{
  char sbatt = 'B';
  int vcc = readVcc() / 100;
  vcc = readVcc() / 100; //передадим напряжение питания с точностью только до десятых вольта
  char symbolvcc = vcc;
  if (vcc < 31) sbatt = 'b'; // если питание меньше 3.1 в имя параметра батарейки - маленькая b
  digitalWrite(POWERTRANSMITTERPIN, HIGH); // включим питание передатчика, если померить напряжение при включенном передатчике оно может быть меньше
  digitalWrite(LEDPIN, true); // включим светодиод на время передачи
  msg[0] = 100 + 10 * device_type + device_id; // номер датчика больше 100, признак того что пошла полная посылка, на приемной стороне вычтем 100, чтобы получить номер датчика
  msg[1] = device_type; // тип датчика
  msg[2] = device_name[0]; // имя датчика
  msg[3] = device_name[1];
  msg[4] = device_name[2];
  msg[5] = device_name[3];
  msg[6] = sbatt; // символ разряжена батарея (b) или нет (B)
  msg[7] = symbolvcc; // напряжение питания, умноженное на 10
  if (device_type == 0)
  {
    msg[8] = data_temp1; // сами данные
    msg[9] = data_temp2; // сами данные
    msg[10] = data_temp3; // сами данные
  }
  if (device_type == 1)
  {
    msg[8] = data_h_dht1;
    msg[9] = data_temp_dht1;
    msg[10] = data_temp1;
    msg[11] = data_temp2;
    msg[12] = data_temp3;
  }
  if (device_type == 2)
  {
    msg[8] = data_h_dht1;
    msg[9] = data_h_dht2;
    msg[10] = data_temp_dht1;
    msg[11] = data_temp_dht2;
    msg[12] = data_temp1;
    msg[13] = data_temp2;
    msg[14] = data_temp3;
  }

  encrypt(8 + dscount + dhtcount * 2);
  vw_send((uint8_t *)msg, (8 + dscount + dhtcount * 2) * 2);
  vw_wait_tx(); // ждем  икончания передачи
  // sendhc12((8 + dscount) * 2);
  digitalWrite(POWERTRANSMITTERPIN, LOW); //выключим питание передатчика
  digitalWrite(LEDPIN, false); //гасим диод
}

void encrypt(int len) //шифруем массив msg длиной len, доавляя в конец столько же символов, но в обратном порядке и формируем строку msgprint для передачи через hc12
{
  for (int i = 0; i < len; i++)
  {
    msg[len + i] = msg[len - i - 1];
  }
}

void writefirstdataeeprom()
{
  EEPROM.write(0, device_id); //ид датчика
  EEPROM.write(1, device_type); //тип датчика
  EEPROM.write(2, device_name[0]); //1 символ имени датчика
  EEPROM.write(3, device_name[1]); //2 символ имени датчика
  EEPROM.write(4, device_name[2]); //3 символ имени датчика
  EEPROM.write(5, device_name[3]); //4 символ имени датчика
  EEPROM.write(6, 100); //значение коррекции измерения напряжения питания батареи, индивидуальное для каждого микроконтроллера, в процентах, 100% -коррекции нет
  EEPROM.write(7, dscount); // кол-во датчиков ds
  EEPROM.write(8, dhtcount); //кол-во датчиков dht
}


void sendhc12(int len) //посылает байты из глобального массива msg длиной len
{
  digitalWrite(HC12SET, LOW); //hc 12 в настроечном
  digitalWrite(HC12SET, HIGH); //hc 12 в нормальном режиме - дернем ногу чтоб модуль проснулся
  delay(50);// время для просыпания
  SoftSerialHC.write(msg, len);
  SoftSerialHC.flush();
  delay(50); //время для передачи данных радиомодулем
  digitalWrite(HC12SET, LOW); //hc 12 в настроечном
  delay(50); //время чтобы войти в настроечный режим
  SoftSerialHC.println("AT+SLEEP");
  delay(50); //время для того чтобы принять команду спячкм
  digitalWrite(HC12SET, HIGH); //hc 12 в нормальном режиме для спячки
  digitalWrite(POWERTRANSMITTERPIN, LOW); //выключим питание передатчика
  digitalWrite(LEDPIN, false); //гасим диод
}
/*
   замечяния по времени работы датчика
   на 16 мгц в среднем чтение датчик- 2 мсек, посылка данных 110 мсек, всего 0.11 с потреблением 20 ма и 1 сек с включенным питанием питанием датчика 80 мка
  потребление в режиме ожидания 22 мка-если питание датчика через диод от ноги, если напрямую от пллюса - то потребление всей схемы в спячке 100 мка
   если посылка каждую минуту то средний ток за минуту (0,11*20+1*0,08)/60+0,022=(2,2+0,08)/60+0,022=2,3/60+0,022=0,038+0,022=0,06=60 мка
   на светодиод стоит резистор 2.2 к , потребление при 4в 2 ма, светит только когда передатчик работает, на светодиод уходит около 10 процентов энергии

  есл  посылка каждые 8 сек:
      данные на 8 Мгц и 18ds20 3 аккумулятора  итого 4 вольта питания, ток в спячке 7мка 8 сек, ток преобразования датчика 0.7 ма в течении 1 сек, ток в режимме передачи 20 ма
   грубо 0.01 ма в спячке, 1 ма за  1 сек преобразование, 20ма за 0.1 сек передача, за цикл 8 сек 8*0,01+1*1+20*0,1 =0,08+1+2=3ма*сек, средний ток 3/8 =0.4 ма 1000
   1000ма/0,4=2500 часов   2500/24 =100 суток или 3 месяца минимум
   если  разрядность датчика 9 bit то время преобразования 0,1 сек, 8*0,01 +1*0,1+2=2,2 ма/сек, в полтора раза меньше (время работы около 5 мес)
  если посылка каждые 64 сек то 64*0,01++1*1=20*0,1=4ма*сек, средний ток 4/64=0,06 ма, 1000ма/0,06=17000 часов или 690 суток или 2 года, реже не имеет смысла-саморазряд батареи
   ниже 8 мгц запустить не удалось датчик на onewire, низкая частота нужна не только для малого энергопотребления, но и для пониженног питания, для 8 мгц это 3.3 вольта, надо 3 аккумулятора
   на 2 может не работать, но передатчику для мощности нужно больше вольтажа, потму пусть будет 3

   для модуля hc12 аремя передачи 0.2с с током 50 ма , если раз в 16 сек то 16*0,01+1*0,1+ 50*0,2= 11ма/сек, средний ток 11/16=0,7 ма, 2000/0,7=2800 часов =119 дней, 3 месяца,
   раз в пол минуты - полгода, раз в минуту - год

*/
