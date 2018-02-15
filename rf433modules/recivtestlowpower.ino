//#include <avr/wdt.h>  // watchdog
#include "DHT.h"
#include <LowPower.h>
#include <VirtualWire.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Arduino Uno
// MOSI is LCD DIN - this is pin 11 on an Arduino Uno
// pin 10 - Data/Command select (D/C)
// pin 9 - LCD chip select (CS)
// pin 19 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(10, 9, 19);
// Note with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer.  Be careful sharing these pins!
//#include <OneWire.h>
SoftwareSerial SoftSerial(5, 6); // RX, TX
#define HC12SET 7  // управление режимом hc12
SoftwareSerial SoftSerialHC(15, 16); // RX, TX
//int Temp; //температура, считанная с 18dsb20
#define receive_pin  2 // пин приемника
#define POWERRECIVPIN 3 // пин питания приемника он же питание дисплея
#define POWERESP 4 // пин питания ESP8266 (через транзистор)
#define BEEPPIN 18 // пин пищалки
int s = 0;
long int mm, msec;
float vcc;
//String strout;
//String strout_tmp;
uint8_t buf[VW_MAX_MESSAGE_LEN]; //принятое неразобранное сообщение
uint8_t buflen = VW_MAX_MESSAGE_LEN; //его длина
byte msg[20]; //принятое расшифрованное сообщение 8 байт служ данных и  9 байт датчиков максимум
byte len; // его длина
byte datalen; //  длина данных - количество значений данных
byte  device_id; // идентификатор (номер) датчика,
byte data[5]; // принятые данные от сенсоров датчика
byte  device_type ; // тип датчика
byte testmode = 0;

//OneWire ds(17); // датчик 18ds20 на 17 пине
#define DHTPIN 17
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
// Переменные, создаваемые процессом сборки,
// когда компилируется скетч

// для функции memoryfree
extern int __bss_end;
extern void *__brkval;


struct record  // строка записи данных в массиве датчиков
{
  byte vcc = 0; //напр батареи, умноженное на 10
  byte device_type = 0; //тип датчика
  byte datalen = 0; //количество байт дананнх от датчика
  byte data[5]; //сами данные надо 10
};

record md[10];

//float vbat; // напряжение батареи
float temp1; // временное значение температуры
float Temperature;
float hum = 0;
//int watchdogenabled = 0;

void setup() {
  pinMode(HC12SET, OUTPUT);
  digitalWrite(HC12SET, HIGH); //hc 12 в нормальном режиме
  delay(100);
  pinMode(POWERRECIVPIN, OUTPUT);
  pinMode(POWERESP, OUTPUT);
  pinMode(BEEPPIN, OUTPUT);
  digitalWrite(BEEPPIN, LOW);
  digitalWrite(POWERESP, LOW);
  digitalWrite(POWERRECIVPIN, HIGH);
  display.begin();
  display.setContrast(60);
  display.display(); // show splashscreen
  delay(500);
  display.clearDisplay();
  display.println("Strt");
  display.display();
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  delay (50);
  digitalWrite(13, LOW);
  delay (50);
  digitalWrite(13, HIGH);
  delay (50);
  digitalWrite(13, LOW);
  delay (50);
  digitalWrite(13, HIGH);
  delay (50);
  digitalWrite(13, LOW);
  pinMode(receive_pin, INPUT);
  Serial.begin(9600);
  vw_set_rx_pin(receive_pin);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2400);
  vw_rx_start();
  Serial.println("Strt");
  SoftSerial.begin(9600);
  SoftSerialHC.begin(2400);
  // переход в FU2 надо делать только один раз
  /*  SoftSerialHC.setTimeout(100);
    digitalWrite(HC12SET, LOW); //hc 12 в настроечном
    delay(100); //время чтобы войти в настроечный режим
    SoftSerialHC.println("AT+FU1");
    delay(100); //время для того чтобы принять команду
    digitalWrite(HC12SET, HIGH); //hc 12 в нормальном режиме
  */
  vcc = readVcc();
  char ch = ' ';
  while (Serial.available()) // чистим буфер приема
  {
    ch = Serial.read();
    delay(5);
  }
  delay(2000);
  Serial.print("t");
  delay(4000);
  ch = Serial.read(); //если rx и tx замкнуты или с консоли в течениие четырех секунд послали букву t - войдем в режим тестировани- непрерывное чтение данных без передачи и перехода в режим сна
  // ch = 't';// принудительно в testmode

  Serial.print(ch);
  if (ch == 't')
  {
    testmode = 1;
    digitalWrite(POWERRECIVPIN, HIGH);
    digitalWrite(POWERESP, HIGH);
    display.clearDisplay();
    display.println("tst:");
    Serial.println("tst:");
    //  strout = "";
    readlocaldata();
    //    strout = strout + "&T1=" + String(Temperature) + "&H1=" + String(hum);
    display.display();
    while (1 == 1) readsensors();
  }
}

void loop()
{
  //   delay(5000);
  //   digitalWrite(POWERRECIVPIN, LOW);
  //   LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
  //    digitalWrite(POWERRECIVPIN, HIGH);
  digitalWrite(HC12SET, LOW); //hc 12 в настроечном
  delay(100);
  digitalWrite(HC12SET, HIGH); //hc 12 в нормальном режиме - дернем ногу чтоб модуль проснулся
  delay(100);// время для просыпания
  digitalWrite(POWERRECIVPIN, HIGH);
  //  strout = "";
  readlocaldata();
  // strout = strout + "&T1=" + String(Temperature) + "&H1=" + String(hum);
  readsensors();
  display.clearDisplay();
  display.println("Send:");
  display.display();
  digitalWrite(POWERESP, HIGH);
  delay(1000); // всего задержка перед сатртом esp 4 сек, 1 сек стартуем чтобы померить напряжение питания
  vcc = readVcc();
  //    Serial.println(strout_tmp);
  //      Serial.println(strout);
  // strout = strout + "&U0=" + String(vcc);
  //     Serial.println(strout_tmp);
  //     Serial.println(strout);

  display.display();

  //3.2 и 0.1  работает - нижний предел при котором возможна передача
  //оставляем 4 и 0.5 итого 4.5 сек не передачу данных от ESP
  delay(3000);
  Serial.println("Send:");
  displaymd(1);
  // Serial.println(strout);
  // SoftSerial.println(strout);
  digitalWrite(HC12SET, LOW); //hc 12 в настроечном
  delay(100); //время чтобы войти в настроечный режим
  SoftSerialHC.println("AT+SLEEP");// усыпим hc12
  delay(100); //время для того чтобы принять команду спячкм
  digitalWrite(HC12SET, HIGH); //hc 12 в нормальном режиме для спячки
  delay(500);
  Serial.println("E sle");
  display.clearDisplay();
  display.println("E sle");
  display.display();
  delay(100);
  digitalWrite(POWERESP, LOW);
  digitalWrite(POWERRECIVPIN, LOW);
  digitalWrite(13, LOW);
  digitalWrite(11, LOW);
  digitalWrite(10, LOW);
  digitalWrite(9, LOW);
  digitalWrite(19, LOW);
  digitalWrite(BEEPPIN, LOW);
  //LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

  for (int  i = 0; i < 6; i++) // сколько минут
    {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    //64 секунды примерно минута
    }
  


}


void readsensors()
{

  display.begin();
  display.setContrast(60);
  display.clearDisplay();
  display.println("sens:");
  // display.println(strout);
  // display.println(strout_tmp);
  display.display();


  long int starttime = millis();
  long int totaltime = 0;

  for (int i = 0; i < 10; i++)  // чистим массив основных данных
  {
    md[i].datalen = 0;
    md[i].vcc = 0;

  }
  Serial.println("sens:");
  while (totaltime < 40000)
  {
    totaltime = millis() - starttime;
    if (totaltime < 0) totaltime = -totaltime;
    if (SoftSerialHC.available()) //если модуль что-то послал
    {
      buflen = SoftSerialHC.readBytes(buf, 40); //прочитаем не более 100 символов
      Serial.print("HC: ");
      for (int i = 0; i < buflen; i++)
      {
        Serial.print(buf[i], DEC);
        Serial.print(" ");
      }
      readremotedata();
      //   beep();
    }
    buflen = VW_MAX_MESSAGE_LEN; // обязательно присвоить перед вызовом vw_get_message, иначе переменная меняется
    if (vw_get_message(buf, &buflen)) // Non-blocking
    {
      int i;
      digitalWrite(13, true);
      delay(200);
      digitalWrite(13, false); // мигнем диодом если хоть что-то приняли
      Serial.println("");
      Serial.print("Got: ");
      for (i = 0; i < buflen; i++)
      {
        Serial.print(buf[i], DEC);
        Serial.print(" ");
      }
      readremotedata();
    }
  }
}

float readVcc()
{
  long int result;
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(75); // Wait for Vref to settle // без задержки неверно меряет только первый раз после стартаб потом нрмально
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring
  result = ADCL;
  result |= ADCH << 8;
  result =  1125300L / result; // (Kvcc * 1023.0 * 1000) (in mV)
  float r = result;
  float rr = r / 10;
  rr = round(rr);
  rr = rr / 100; // округление до сотых
  return rr;  // в вольтах
}


void readremotedata()
{
  if (((buflen % 2) > 0) || (buflen < 4)) // если длина сообщения нечетная или меньше 45 байт -оно не наше - покажем данные и вернемся
  {
    Serial.print("NV: ");
    displaydata();
    return;
  }
  len = buflen / 2;
  for (int i = 0; i < len; i++)
  {
    if ((buf[i]) == (buf[len * 2 - i - 1])) //проверяем сообщение - данные в нем дублируются дважды, второй раз - в обоатном порядке
    {
      msg[i] = buf[i];
    }
    else
    {
      Serial.print("Era: ");
      displaydata();
      return;
    }
  }
  if (buf[0] > 200)
  {
    Serial.print("Erd: ");
    Serial.println(buf[0]);
    displaydata();
    return;
  }
  if (buf[0] < 100) // если номер датчика меньше 100, то это короткая посылка, иначе длинная - разные обработчики
  {
    readshortdata();
  }
  else
  {
    readfulldata();
  }
  displaymd(0); // не только показыват , но и формирует глобальные строки  strd   strdf
}

void displaymd(byte flagesp) //показывает на экране массив  данных, здесь еще формируются строки для посылки на внешний сервер, flagesp =1 значит передавать данные через esp8266
{
  String strout;
  Serial.println("");

  display.clearDisplay();
  delay(30);  // мигнем пустым дисплеем, чтобы показать, что программа работает
  display.display();

  strout = "&U0=" + String(vcc) + "&T01=" + String(Temperature) + "&H01=" + String(hum);
  Serial.print(strout);
  display.print(strout);
  if (flagesp == 1)
  {
    SoftSerial.print(strout);
  }

  //  strout_tmp = "";
  for (int i = 0; i < 10; i++)
  {
    // Serial.println(i);
    //Serial.println(millis());
    //Serial.println(md[i].time);

    if (md[i].datalen > 0) // датчик активен если число данных больше 0
    {
      float vbat;
      vbat = md[i].vcc; // преобразовать тип
      vbat = vbat / 10;
      if (vbat != 0)
      {
        // strout_tmp = strout_tmp + "&U" + String(i) + "=" + String(vbat);
          strout = "&U" + String(i) + "=" + String(vbat);
          Serial.print(strout);
          display.print(strout);
          if (flagesp == 1)
          {
            SoftSerial.print(strout);
          }       
      }
      for (int j = 0;  j < md[i].datalen; j++) // перебираем все значения данных
      {
        temp1 = md[i].data[j];

        device_type = md[i].device_type;
        /*  типы датчиков
          0-все однобайтные сенсоры температуры, точность 0,5 градуса, в байте данных содержится температура, умноженная на 2
          1-первый сенор влажности, остальные - однобайтные температуры
          2-первый и второй сенсор влажности, остальные- однобайтная температура
        */
        if ((device_type == 0) || ((device_type == 1) && (j > 0)) || ((device_type == 2) && (j > 1)))
        {
          if (temp1 > 127) // старший бит=1 , значит орицательная температура
          {
            temp1 = temp1 - 256;
          }
          temp1 = temp1 / 2; // было передано удвоенное значение температуры
          // strout_tmp = strout_tmp + "&T" + String(i) + String(j + 1) + "=" + String(temp1);

          strout = "&T" + String(i) + String(j + 1) + "=" + String(temp1);
          Serial.print(strout);
          display.print(strout);
          if (flagesp == 1)
          {
            SoftSerial.print(strout);
          }

        }
        if  (((device_type == 1) && (j < 1)) || ((device_type == 2) && (j < 2)))
        {
          // strout_tmp = strout_tmp + "&H" + String(i) + String(j + 1) + "=" + String(md[i].data[j]); // влажность передается в одном байте как есть
          strout = "&H" + String(i) + String(j + 1) + "=" + String(md[i].data[j]);
          Serial.print(strout);
          display.print(strout);
          if (flagesp == 1)
          {
            SoftSerial.print(strout);
          }
        }
      }
    }

  }

  //  Serial.println(strout);
  //  Serial.println(strout_tmp);

  if (testmode == 1)
  {
    beep();
  }
  //display.println(strout);
  //display.println(strout_tmp);
  display.display();
  Serial.println("");

  if (flagesp == 1)
  {
    SoftSerial.println("");
  }
}

void beep()
{
  for (int i = 0; i < 300; i++)
  {
    digitalWrite(BEEPPIN, HIGH);
    delayMicroseconds(100);
    digitalWrite(BEEPPIN, LOW);
    delayMicroseconds(100);
  }
}


void readshortdata()
{
  int did_type = msg[0]; // идентификатор (номер) датчика и тип
  device_id = did_type % 10;// остаток от деления на 10, младший знак это номер датчика
  device_type = did_type / 10; // старший знак
  datalen = len - 1; // 1 байт с 0 по 0 служебных данных
  for (int i = 1; i < len; i++) data[i - 1] = msg[i]; // принятые данные от сенсоров датчика
  parsedata() ; // разбор сформированного масива данных data
}

void readfulldata()
{
  //Serial.print("full: ");
  int did_type = msg[0] - 100; // идентификатор (номер) датчика в длинной посылке увеличен на 100,
  device_id = did_type % 10;// остаток от деления на 10, младший знак это номер датчика
  device_type = did_type / 10;
  md[device_id].vcc = msg[7];
  datalen = len - 8; // 8 байт с 0 по 7 служебных данных
  for (int i = 8; i < len; i++)
  {
    data[i - 8] = msg[i]; // принятые данные от сенсоров датчика
  }
  parsedata() ; // разбор сформированног масива данных data
}


void parsedata() //обрабатываем массив данных от датчиков, данные в массиве data[0], количество данных datalen, пишем нужные данные в массив md
{

  md[device_id].device_type = device_type;
  md[device_id].datalen = datalen;
  for (int i = 0; i < datalen; i++)
  {
    md[device_id].data[i] = data[i];
  }
}



void displaydata() //показывает все пришедшие данные по радиоканалу в сначала в числовом, потом в текстовом виде,  больше нужно для отладки, вызывается, если не удалось идентифицировать посылку
{
  int i;
  digitalWrite(13, true);
  delay(200);
  digitalWrite(13, false); // мигнем диодом если хоть что-то приняли
  String tmp = "";
  Serial.print("RF: ");
  display.clearDisplay();
  display.display();
  delay(100);  // мигнем пустым дисплеем, чтобы показать, что программа работает
  for (i = 0; i < buflen; i++) //выведем принятое в числовом виде для отладки
  {
    Serial.print(buf[i], DEC);
    Serial.print(" ");
    display.print(buf[i], DEC);
    display.print(" ");
    if (buf[i] == 0)
    {
      tmp = tmp + "/0";
    }
    else
    {
      tmp = tmp + char(buf[i]);
    }
  }
  display.println("");
  display.print(tmp);  //выведем принятое в символьном виде
  Serial.println("");
  Serial.println(tmp);
  display.display();
}




void readlocaldata()
{


  /* блок для 18ds20
    byte data[2];
    ds.reset();
    ds.write(0xCC);
    ds.write(0x44); // посылаем команду начала преобразования
    // датчику надо дать секунду с вкл питанием чтобы инициализироваться
    delay(1000);
    //  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);// поспим секунду вместо вместо delay(1000) - все таки 10 миллиампер пторебления,если разрешение термодатчика 9 bit, время преобразования будет около 0.1 сек, остальное в время датчик тока почти не потребляет
    if ( watchdogenabled == 1) wdt_enable (WDTO_8S);
    ds.reset();
    ds.write(0xCC);
    ds.write(0xBE);
    data[0] = ds.read();
    data[1] = ds.read();
    // Temp = (data[1] << 8) + data[0];
    Temperature =  (data[0] | (data[1] << 8)) / 16.0;
    //Temp = Temp >> 4; // если сделать так, то в Temp окажется целочисленное значение температуры
    //  Temp = Temp >> 3; // // если сделать так, то в Temp окажется целочисленное значение температуры, умноженное на 2, так как точность преобразования выставили на сенсоре 0,5 градуса,
    //Serial.println(Temp);
  */
  // блок для am2320
  Temperature = dht.readTemperature();
  hum = dht.readHumidity();


}

int memoryFree()
{
  int freeValue;
  if ((int)__brkval == 0)
    freeValue = ((int)&freeValue) - ((int)&__bss_end);
  else
    freeValue = ((int)&freeValue) - ((int)__brkval);
  return freeValue;
}

/* расчет энергосбережения если 1 мин читаем данные 6 сек посылаем и 5 мин спим
  // расчет в ма/сек
  60 сек*10ма+6сек*80ма+300сек*0,01ма=600+480+6=1086ма/сек= 0,3 ма/ч за 5 ми цикл (0,08часа)
  2000ма/ч/0.3 ма/ч = 6700 циклов *0,08 =533 часа =22 суток

  если 40 сек читаем, 6 сек  посылаем 10 мин спим
  40*10+6*80+600*0,01=886 ма/сек = 0,246 ма/ч за 10 мин цикл (0,167 часа)
  2000/0,246=8130 циклов * 0,167=1357 часов = 56 суток

  если 30 сек читаем 8 сек посылаем 20 мин спим
  30*10+8*80+1200*0,01=1072 ма/сек=0,298 ма/сек за 20 мин цикл (0,33 часа)
  2000/0,298*0,33=2214 час = 92 дня

  если 60 сек читаем 8 сек посылаем 20 мин спим
  600+640+12 =1252 ма/сек /3600 = 0,348 ма/ч
  2000/0,348*0,33=1896 часов - 80 дней


  если 40 сек читаем 4,5 сек посылаем 20 мин спим
  400+360+12 =772 ма/сек /3600 = 0,214 ма/ч
  2000/0.214*0,33=3084 часов - 128 дней или 4 мес, на 2-3 мес на холоде можно рассчитывать


  грубо если передача каждые 5 мин - то 20 дней, пропорционально увеличиваем интервал - увеличаться дни , если каждый час, то 60/5*20= 240 дней на одной батарейке
  время чтения можно оставить 60 сек, основной вклад (2/3) - время передачи

  если сократим время передачи до 2 сек, то получим 40 дней при 5 минутном цикле
  пока сделал на 40сек, потр 10 ма, раз в 6 мин, должно хватить на месяц
*/
