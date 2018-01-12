//
// Проект на github
//  https://github.com/sashaml29/arduino-remote-gsm-station

// GSM - Version: Latest
//https://www.tinyosshop.com/arduino-gsm-shield

//cхема
//https://easyeda.com/sasha_ml/Arduino_Mega-15a761a8028f4398b58b41b1c01d44cf

//этот код
//https://create.arduino.cc/editor/sasha_ml/d271f25d-d158-4372-87bb-69d778120403/preview
//--
//


/*
  SD card read/write

  This example shows how to read and write data to and from an SD card file
  The circuit:
   SD card attached to SPI bus as follows Arduino Uno:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK -  pin 13
 ** CS - pin 4 (for MKRZero SD: SDCARD_SS_PIN)

  arduino Mega
  gnd -- gnd
  3.3v -- 3.3 v
  5v -- 5v
  cs -- 53
  mosi -- 51
  sck -- 52
  miso -- 50
  gnd -- gnd



*/

#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>



// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
// MKRZero SD: SDCARD_SS_PIN
//Arduino Mega 2650 pin 53
const int chipSelect = 53 ;


/* подключение часов DS3231

  http://blog.rchip.ru/podklyuchenie-chasov-realnogo-vremeni-rtc-ds3231-k-arduino/

  http://iarduino.ru/file/235.html  библиотека <iarduino_RTC.h>

  подключение к мега2560
  SCL 21 (scl)
  SCA 20 (sca)
  VCC  +5v
  GND gnd
  time.settime(0,51,21,27,10,15,2);  Устанавливаем время: 0 сек, 51 мин, 21 час, 27, октября, 2015 года, вторник
*/
#include <iarduino_RTC.h>
iarduino_RTC time(RTC_DS3231);
int year;
int month;
int date;
int hour;
int min;
int sec;


#include <VirtualWire.h>
const int receive_pin = 22; // пин приемника
int s = 0;
long int mm, msec;

#include <avr/wdt.h>  // watchdog

// display nokia 5510
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
// pin 32 - Serial clock out (SCLK)
// pin 29 - Serial data out (DIN)
// pin 28 - Data/Command select (D/C)
// pin 31 - LCD chip select (CE) (CS)
// pin 30 - LCD reset (RST)
//pin vcc - +3.3V
//pin GND - GND
// pin BL +3.3v or non connected это подсветка
//Adafruit_PCD8544 display = Adafruit_PCD8544(SCLK, DIN, D/C, CS, RST);
//Adafruit_PCD8544 display = Adafruit_PCD8544(52, 49, 48, 51, 50); было на родном spi, нужно освободить интерфейс для других устройств
Adafruit_PCD8544 display = Adafruit_PCD8544(32, 29, 28, 31, 30);

#include <SPI.h>
#include <RF22.h>

#define gsmport Serial1

String textsms, number, txt, strd, strdf;
String mynumber;
String temp;
int debugstatus = 1;
unsigned long predtime1, nexttime1;
unsigned long predtime2, nexttime2;
unsigned long predtime3, nexttime3,  tektime; // счетчики времени для вызова периодических событий
String val = ""; //глобальная переменная где храним строку с модема, чотбы не создавать лишний string в вызываемых процедурах
int enableset;
uint8_t buf[VW_MAX_MESSAGE_LEN]; //принятое неразобранное сообщение
uint8_t buflen = VW_MAX_MESSAGE_LEN; //его длина
byte msg[30]; //принятое расшифрованное сообщение
int len; // его длина
int datalen; //  длина данных - количество значений данных
int  device_id; // идентификатор (номер) датчика,
byte data[10]; // принятые данные от сенсоров датчика
int  device_type ; // тип датчика
int vcc; // напряжение батареи умноженное на 10
char symbolvcc; // b или B - символ разряда батареи
float vbat; // напряжение батареи
float temp1;
String dname = "test"; // имя датчика

struct record  // строка записи данных в массиве датчиков
{
  byte active = 0; // если датчик астивен, то 1, если нет то 0
  byte device_type = 1; //тип датчика
  byte vcc = 0; //напр батареи, умноженное на 10
  char symbolvcc = 0; // b или B - символ разряда батареи
  char dname[5] = "xxxx"; //имя датчика из 4 символов, 6 байт зарезервировано
  byte sort = 0; // значение сортировки, возможно использовать для сортировки вывода
  long int time = 0; //время последнего прихода данных в миллисекундах 4 байта
  byte datalen; //количество дананных
  float data[10]; //сами данные 4*10=40 байт
  //итого 54 байта занимает запись
};
record md[10]; //массив главных данных (maindata) от датчиков, 540 байт памяти, в элементе с индексом 0 будем хранить данные основного блока, индексы 1-9 - данные удаленных датчиков, у которого может быть до 10 сенсоров
//максимальный номер датчика 9, максимальный номер данных сенсора 9, можно кодировать данные двызначным числом, например 23 - данные третьего сенсора от второго датчика

struct recordalert  // строка записи данных в массиве предупреждений
{
  byte alert_active = 0; // если  активен, то 1, если нет то 0
  int numdat = 0; // номер датчика (0-9), 
  int numsens = 0; // номер сенсора в датчике (0-9)
  char typealert = "u"; //u при превышении порога, d при понижении порога 
  float triggerdata; // порог - значение данны при превышении или понижении которого будет выдаваться оповещенеи
  long int lasttimeonsent = 0; //время последней отправки оповещения, в глобальных миллисекундах
};
recordalert al[10]; // массив предупреждений всего 9, от 1 до 9, 0 не будем задейсттвовать
// будем посылать смс вида #al925u35 - предупреждение номер 9, датчик 2 сенсор 3, послать сообщение  при превышении данных выше 35    
//#al3* - дективировать предупреждение с номером 3
//#al* - деактивировать все предупреждения

long int alarmtimeout=3600000; // период в милиссекундах, через которы будут посылаться сообщения при превышении порога датчика
String stringactivealert; // строка обратного ответа о всех активных предупреждениях



void setup() {
  wdt_disable();
  Serial.begin(9600);
  delay(500);
  EEPROM.get(100, al); // считаем содержимое массива уведомлений из еепром
  for (int i = 1; i <= 9 ; i++) 
   {
    al[i].lasttimeonsent=0; // после рестарата содержимое милис обнуляется, потому обнулим данные о последнем времени посылке предупреждения
   }
    
  printactivealert(); // процедура пишет в переменную stringactivealert содержимое активных предупреждений
  inf(stringactivealert);
  stringactivealert=""; 
  display.begin();
  display.setContrast(65);
  time.begin();
  String tmpt = time.gettime("d-m-Y, H:i:s");
  inf("Init..\r\n" + tmpt);

  if (!SD.begin(chipSelect))
  {
    Serial.println("Card failed, or not present");
    inf("SD card initialization failed");
  }
  else
  {
    inf("SD card - correct");
  }

  dbgprint("Start sketch");
  gsmport.begin(115200);
  gsmport.setTimeout(500); //будем ждать ответа модема по полсекунды в строках gsmport.readString();
  delay(500);
  /*
    //Включаем GPRS Shield, эмулируя нажатие кнопки POWER
    pinMode(9, OUTPUT);
    digitalWrite(9, HIGH);    // Подаем High на пин 9
    delay(3000);              // на 3 секунды
    digitalWrite(9, LOW);     // и отпускаем в Low.
    delay(5000);              // Ждём 5 секунд для старта шилда
  */
  //Настраиваем приём сообщений с других устройств и печатаем ответы модема
  gsmport.print("AT+CMGF=1\r"); // модем будет посылать смс в текстовом формате
  printstrfrommodem();
  gsmport.print("AT+IFC=1, 1\r");  //— устанавливает программный контроль потоком передачи данных
  printstrfrommodem();
  gsmport.print("AT+CPBS=\"SM\"\r"); //— открывает доступ к данным телефонной книги SIM-карты AT+CPBS="SM" выбрать как основную память сим-карту
  printstrfrommodem();
  gsmport.print("AT+CNMI=1,2,2,1,0\r"); // — включает оповещение о новых сообщениях, новые сообщения приходят в следующем формате: +CMT: "<номер телефона>", "", "<дата, время>", а на следующей строчке с первого символа идёт содержимое сообщения
  printstrfrommodem();
  gsmport.println("AT+CMGD=1,4"); // удалить все сообщения в памяти
  printstrfrommodem();
  mynumber = ReadMasterNum (); // считывает запись с индексом 1 c сим карты
  inf("Master num:\r\n" + mynumber);
  delay(5000);
  //mynumber="+792100000000"; // можно напрямую присвоить мастер номер
  pinMode(13, OUTPUT); //для идикаторного диода
  dbgprint("Setup done");
  vw_set_rx_pin(receive_pin);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);
  vw_rx_start();
  predtime1 = 0;
  nexttime1 = 0;
  predtime2 = 0; 
  nexttime2 = 3000000; // второе событие вызовется первый раз через пять минут после старта (для записи  лог - когда данные с датчиков придут)
  predtime3 = 0;
  nexttime3 = 0;
  wdt_enable (WDTO_8S); // watchdog на 8 секунд, родной загрузчик ардуино должен быть обязательно заменен на otiboot, иначе будет циклическая перезагрузка
}


void loop()
{
  wdt_reset();  // вызываемые процедуры из loop() суммарно не должны длиться более 8 секунд (либо в них должен стоять еще wdt_reset(); ) , иначе перезагруга
  tektime = millis();
  if ((nexttime1 < tektime) || (predtime1 > tektime)) event1(); //если время следующего события пришло или время предыдущего события больше текущего времени, что бывает раз в 49 дней при переполнении millis(), то вызываем основное событие
  if (StrFromSerial(txt) > 0) //ждем команды вручную с порта и если что-то есть - посылаем в модем и печатаем ответ
  {
    // результат чтения с порта в переменной txt
    if (txt.substring(0, 2) == "AT") // если строка начинается с AT, то все что пришло с порта посылаем модему
    {
      gsmport.print(txt);
      temp = gsmport.readString();
      /*Serial.print(temp);
        display.print(temp);*/
      inf(temp);
    }
    else if (txt.substring(0, 3) == "log") // выводим в сериал весь логфайл с нужным именем,послали команду например log abc, значит буде выводить файл abc.csv
    {
      String filename = txt.substring(4);
      filename.trim();
      filename = filename + ".csv";
      Serial.print("log: ");
      Serial.println(filename);
      File dataFile = SD.open(filename);
      if (dataFile)
      {
        while (dataFile.available())
        {
          Serial.write(dataFile.read());
        }
        dataFile.close();
      }

      else
      {

        Serial.print("error opening ");
        Serial.println(filename);
      }

    }

    else if (txt.substring(0, 6) == "dellog") //послали команду например log abc, значит буде выводить файл abc.csv
    {
      String filename = txt.substring(7);
      filename.trim();
      filename = filename + ".csv";
      Serial.print("delete: ");
      Serial.println(filename);
      if (SD.remove(filename))
      {
        Serial.println("deleted ");
      }
      else
      {
        Serial.println("Not deleted!");
      }
    }
    else if (txt.substring(0, 3) == "#al") 
    {
      setalert(txt);
    }
    else if (txt.substring(0, 6) == "listmd") //показ содержимого основного массива
    {
      listmd();
    }
  };
  checkforsms(); // проверяем буфер порта модема  на наличе данных и смс в нем

  buflen = VW_MAX_MESSAGE_LEN; // в библиотеке это 30 обязательно присвоить перед вызовом vw_get_message, иначе переменная меняется
  if (vw_get_message(buf, &buflen)) // Non-blocking
  {
    digitalWrite(13, true);
    readremotedata();
    digitalWrite(13, false);
  }
}


void checkforsms()
{
  if (gsmport.available()) //если модуль что-то послал
  {
    val = gsmport.readString(); //буфер порта только 64байта, смс может обрезать до 14 символов, команды делать лучше длиной до 10 символов
    if (val.indexOf("+CMT") > -1)
    {
      Serial.println("Incoming SMS:");
      Serial.println(val);
      gsmport.println("AT+CMGD=1,4"); // удалить все сообщения в памяти чтобы не забивать память модема
      gsmport.readString(); // игнорируем ответ- читаем в никуда
      String Numtel = substrPoNomeru(1);
      if ((Numtel.indexOf(mynumber) > -1) || (enableset == 1) ) // если это свой номер или настройки разрешены
      {
        Serial.println("This mynumber");
        if (val.indexOf("#info") > -1) sendinfo();
        if (val.indexOf("#inff") > -1) sendinfof();
        if (val.indexOf("#settime") > -1) settimedatefromsms();
        if (val.indexOf("#setmynum") > -1) setmynumber();
        if (val.indexOf("#balans") > -1) sendbalans();
        if (val.indexOf("#al") > -1) setalert(val);
      }
    }
    else
    {
      Serial.println("Modem send other - not sms");
      Serial.println(val);
    }
  }
}


//здесь пошли процедуры исполнения команд, принятых в смс
void sendinfo()
{
  textsms = strd;
  sendmessage(mynumber, textsms);
  inf("comamnd #sendinfo");
}


void sendinfof()
{
  textsms = strdf;
  sendmessage(mynumber, textsms);
  inf("comamnd #sendinfo");
}

void settimedatefromsms()
{
  // надо делать только при зажатой кнопке
  inf("comamnd #settime");
  String DateTime = substrPoNomeru(3);
  String Command = "AT+CCLK=" + '"' + DateTime + '"'; //часы в модеме ужасно убегают, лучше не пользоваться
  gsmport.println(Command);
  temp = gsmport.readString();
  Serial.println(temp);
  gsmport.println(Command);
  gsmport.println("AT+CCLK?");
  // Datetime в таком формате 18/01/08,02:54:11+12
  year = DateTime.substring(0, 2).toInt();
  month = DateTime.substring(3, 5).toInt();
  date = DateTime.substring(6, 8).toInt();
  hour = DateTime.substring(9, 11).toInt();
  min = DateTime.substring(12, 14).toInt();
  sec = DateTime.substring(15, 17).toInt();

  /*Serial.println(year);
    Serial.println(month);
    Serial.println(year);
    Serial.println(date);
    Serial.println(hour);
    Serial.println(min);
    Serial.println(sec);


    Serial.println(DateTime.substring(0,2));
    Serial.println(DateTime.substring(3,5));
    Serial.println(DateTime.substring(6,8));
    Serial.println(DateTime.substring(9,11));
    Serial.println(DateTime.substring(12,14));
    Serial.println(DateTime.substring(15,17));
  */
  temp = gsmport.readString();
  Serial.println(temp);
  time.settime(sec, min, hour, date, month, year);
  String tmpt = time.gettime("d-m-Y, H:i:s");
  inf("Time:\r\n" + tmpt);
}


void setmynumber ()
{
  inf("command #setmynum");
  String Numtel = substrPoNomeru(1);
  String Command = "AT+CPBW=1,\"" + Numtel + "\",145,\"Master\"";
  gsmport.println(Command);
  String Mynum = substrPoNomeru(1);
  printstrfrommodem();
  inf(Mynum)  ;
}


void sendbalans ()
{
  inf("command #balans");
  //тут поставим кнопку при зажатии которой можно записывать мастер номер
  String Command = "AT+CUSD=1," + String('"') + String('*') + "100#" + '"';
  //AT+CUSD=1,"*100#"
  Serial.println(Command);
  gsmport.println(Command);
  wdt_reset();
  delay(5000);
  wdt_reset();
  delay(5000);
  wdt_reset();
  temp = gsmport.readString();
  String text = temp.substring(37);
  inf(text);
  sendmessage(mynumber, text);
}

void sendmessage(String telnum, String text)
{
  if (telnum.length() > 4)
  {
    if (text.length() < 160)
    {
      delay(100);
      gsmport.println("AT"); // пошлем на всякий случай пустую команду, если в буфере модема неисполненные симыолы
      delay(100);
      gsmport.print("AT+CMGF=1\r");
      delay(200);
      gsmport.println("AT+CMGS=\"" + telnum + "\"");
      delay(200);
      gsmport.println(text);
      delay(200);
      gsmport.println((char)26);
      delay(200);
      inf("Sending sms:\r\n" + text);
      gsmport.readString(); // чистим буфер
    }
    else
    {
      inf("symbols in sms more 160:");
      Serial.println(text.length());
    }
  }
}



int StrFromSerial(String &val)  // val - передача параметра поссылке, ее нужно менять
{ //функция читает данные с порта 0 без пауз и записывает в переданную переменную val
  int i = 0;
  if (Serial.available()) // что-то послалано в порт
  {
    char ch = ' ';
    val = "";
    while (Serial.available())
    {
      ch = Serial.read();
      val += char(ch); //собираем принятые символы в строку
      delay(5);
    }
    i = 1;
  }
  return i;
}


void dbgprint(String text)
{
  if (debugstatus = 1)  Serial.println(text);
}


void inf(String text)
{ //вывод промежуточнонй информации в порт на дисплей или еще куда
  wdt_reset();
  display.clearDisplay();
  Serial.println(text);
  display.println(text);
  display.display();
  delay(2000);
  wdt_reset();
}


//процедуры разбора строки данные идут в кавычках
//возвращает подстроку между кавычками по номеру данных
String substrPoNomeru(int num)
{ // val - глобальная переменная, где содержится строка для разбора
  String strret = "";
  char q = '"'; // будем искать символ кавычку
  int poz2 = -1; // позиция 0 это первая позиция в строке
  int poz1 = -1;
  int k = 0;
  if (val.length() == 0) return "";
  for (int i = 0; i <= val.length() - 1 ; i++)
  {
    if (val[i] == q)
    {
      k = k + 1;
      if (k == num * 2 - 1) poz1 = i;
      if (k == num * 2) poz2 = i;
    };
  };
  if ((poz1 > -1) & (poz2 > -1))  strret = val.substring(poz1 + 1, poz2);
  return strret;
}


void printstrfrommodem() // печатает ответ модема в порт
{ val = gsmport.readString();
  Serial.print("GSM modem send> ");
  Serial.println(val);
}


String ReadMasterNum () // возвращает из первой ячейки смс номер телефона
{
  gsmport.println("AT");
  gsmport.readString();
  gsmport.println("AT+CPBR=1");
  val = gsmport.readString();
  String Numtel = substrPoNomeru(1);
  return Numtel;
}





void refreshdisplay()
{
  display.clearDisplay();
  display.display();
  delay(100);  // мигнем пустым дисплеем, чтобы показать, что программа работает
  display.println("Main event 10s\r\n" + String(int(tektime / 1000)) + " sec");
  display.display();
}


void readremotedata()
{
  if (((buflen % 2) > 0) || (buflen < 4)) // если длина сообщения нечетная или меньше 45 байт -оно не наше - покажем данные и вернемся
  {
    Serial.print("Not valid data: ");
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
      Serial.print("Error in data: ");
      displaydata();
      return;
    }
  }
  if (buf[0] > 200)
  {
    Serial.print("Error number device: ");
    Serial.println(buf[0]);
    displaydata();
    return;
  }
  if (buf[0] < 100) // если номер датчика меньше 100, то это короткая посылка, иначе длинная - разные обработчики
  {
    if (buf[0] > 9) // максимальный номер датчика 9
    {
      Serial.print("Error number device: ");
      Serial.println(buf[0]);
      displaydata();
    }
    readshortdata();
  }
  else
  {
    if (buf[0] > 109) // максимальный номер датчика 9
    {
      Serial.print("Error number device: ");
      Serial.println(buf[0]);
      displaydata();
    }
    readfulldata();
  }
  displaymd(); // не только показыват , но и формирует глобальные строки  strd   strdf 
}


void displaymd() //показывает на экране массив  данных, здесь еще формируются строки для посылки смс 
{
  strd = ""; //в коротком виде
  strdf = ""; // в полном виде
  for (int i = 0; i < 10; i++)
  {
    // Serial.println(i);
    //Serial.println(millis());
    //Serial.println(md[i].time);
    if ((millis() - md[i].time) > 60000)
    {
      md[i].active = 0; // если данных от датчика не было определенное количество миллисекунд, сделаем его неактивным
      //Serial.print("inactive ");
      //Serial.println(i);
    }
    if (md[i].active == 1)
    {
      vbat = md[i].vcc; // преобразовать тип
      vbat = vbat / 10;
      long int told = (millis() - md[i].time) / 1000;      
      strd = strd + md[i].dname;
      strdf = strdf + String(i) + " " + md[i].dname + " Ty" + String(md[i].device_type) + " " + md[i].symbolvcc + String(vbat) + "\r\n";
      for (int j = 0;  j < md[i].datalen; j++) // перебираем все значения данных
      {
        strd = strd + formattemp(md[i].data[j]);
        strdf = strdf + String(md[i].data[j]);
      }
      strd = strd + "\r\n";
      strdf = strdf + "\r\n";
      strdf = strdf + "old " + String(told) + " sec\r\n";

    }
  }
  //  Serial.print(strdf);
  String tmpt = time.gettime("d-m-y H:i");
  strd = tmpt + "\r\n" + strd; // для отладки покажем время
 // Serial.print(strd);
  // Serial.println("");
  display.clearDisplay();
  display.display();
  delay(30);  // мигнем пустым дисплеем, чтобы показать, что программа работает
  display.println(strd);
  display.display();
}

void readshortdata()
{
  //Serial.print("short: ");
  device_id = msg[0]; // идентификатор (номер) датчика в длинной посылке увеличен на 100,
  datalen = len - 1; // 1 байт с 0 по 0 служебных данных
  for (int i = 1; i < len; i++) data[i - 1] = msg[i]; // принятые данные от сенсоров датчика
  parsedata() ; // разбор сформированного масива данных data
}

void readfulldata()
{
  //Serial.print("full: ");
  device_id = msg[0] - 100; // идентификатор (номер) датчика в длинной посылке увеличен на 100,
  device_type = msg[1] ; // тип датчика
  md[device_id].device_type = device_type;
  dname[0] = msg[2] ; // имя датчика
  dname[1] = msg[3];
  dname[2] = msg[4];
  dname[3] = msg[5];
  md[device_id].dname[0] = msg[2];
  md[device_id].dname[1] = msg[3];
  md[device_id].dname[2] = msg[4];
  md[device_id].dname[3] = msg[5];
  symbolvcc = msg[6]; // b или B - символ разряда батаре
  md[device_id].symbolvcc = symbolvcc;
  vcc = msg[7];
  md[device_id].vcc = vcc;
  vbat = vcc; // преобразовать тип
  vbat = vbat / 10;
  datalen = len - 8; // 8 байт с 0 по 7 служебных данных
  for (int i = 8; i < len; i++)
  {
    data[i - 8] = msg[i]; // принятые данные от сенсоров датчика
  }
  parsedata() ; // разбор сформированног масива данных data
}


void parsedata() //обрабатываем массив данных от датчиков, данные в массиве data[0], количество данных datalen
{
  int flagmod = 0; // флаг модификации, если данные изменилиси с предыдущим приходом, то флаг взведем
  md[device_id].datalen = datalen;
  md[device_id].active = 1;
  md[device_id].time = millis();
  if (device_type < 10) // датчики с 0 по 9 типа - температурные, все данные записываем как температуру
  {
    for (int i = 0; i < datalen; i++)
    {
      temp1 = data[i];
      if (temp1 > 127) // старший бит=1 , значит орицательная температура
      {
        temp1 = temp1 - 256;
      }
      temp1 = temp1 / 2; // было передано удвоенное значение температуры
      if (abs(md[device_id].data[i] - temp1) > 0.5) //данные изменились с прошлой передачи больше чем на 0.5 градуса
      {
        md[device_id].data[i] = temp1;
        flagmod = 1;
      }
    }
  }
  if (flagmod == 1) // если есть изменения в данных
  {
    writelognumdat(device_id); // запишем данные в лог
  }
}


void displaydata() //показывает все пришедшие данные по радиоканалу в сначала в числовом, потом в текстовом виде,  больше нужно для отладки, вызывается, если не удалось идентифицировать посылку
{
  int i;
  digitalWrite(13, true);
  delay(200);
  digitalWrite(13, false); // мигнем диодом если хоть что-то приняли
  String tmp = "";
  Serial.print("Got: ");
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


String formattemp(float ftemp) // форматирует и возвращает целую температуру со знаком + или - 3 символа
{
  int itemp = round(ftemp);
  String strtemp;
  strtemp = String(itemp);
  strtemp.trim(); // удалим пробелы
  if (itemp > 0)
  {
    strtemp = "+" + strtemp; // у минусовой температуры минус уже есть
  }
  strtemp = "   " + strtemp;
  strtemp = strtemp.substring(strtemp.length() - 3); // оставим три символа справа
  return strtemp;
}


void event1() // основное событие раз в 10 секунд, отсюда вызываются другие более редкие события
{
  //здесь вероятно будем обновлять дисплей
// Serial.println("event1");
  checkalerts(); // проверим датчики на срабатывание по порогу
  // refreshdisplay();
  predtime1 = millis(); //запоминаем время прошедшего события
  nexttime1 = predtime1 + 20000; // назначаем время следующего основного события через 20 сек

  tektime = millis();
  if ((nexttime2 < tektime) || (predtime2 > tektime)) event2(); // вызовем если нужно второе собыитие

  tektime = millis();
  if ((nexttime3 < tektime) || (predtime3 > tektime)) event3(); // вызываем третье событие  раз в сутки обычно
  // эту проверку можно было сделать в основном loop, но не будем грузить основной цикл лишним кодом, чтобы дать процессору больше времени на проверки приходящих данных
}


void event2() // второе  событие вызывается реже, обычно раз в час отсюда вызываются другие более редкие события
{
  Serial.println("event2");
  predtime2 = millis(); //запоминаем время прошедшего события
  nexttime2 = predtime2 + 3600000; // назначаем время следующего  события 2 через час (3600000 msec час)

  writelogdatat(); // пишем в лог с именем файла- номер датчика+"t" данные каждй интервал времени, когда вызвано событе event2
}

void event3() // второе  событие вызывается реже, обычно раз в час отсюда вызываются другие более редкие события
{
  Serial.println("event3");
  predtime3 = millis(); //запоминаем время прошедшего события
  nexttime3 = predtime3 + 86400000; // назначаем время следующего  события 3 через это время (86400000 msec сутки)
}


void writelogdatat() // эта запись в лог будет вызываться по таймеру, пишем все активные датчики
{
  for (int i = 0; i < 10; i++)
  {

    if (md[i].active == 1)
    {
      String strlog = ""; //строка для записи в лог
      String filename = "";
      vbat = md[i].vcc; // преобразовать тип
      vbat = vbat / 10; // пока не будем писать напр батареи
      filename = String(i);
      filename.trim();
      filename = "t" + filename + ".csv"; // имя файла совпадает с именем датчика, буква t  в начале имени означает что это лого по таймеру (раз в минуту час,день)
      String tmpt = time.gettime("d-m H:i:s");
      strlog = tmpt;
      for (int j = 0;  j < md[i].datalen; j++) // перебираем все значения данных
      {
        strlog = strlog + ";" + String(md[i].data[j]);
      }

      File dataFile = SD.open(filename, FILE_WRITE);
      // if the file is available, write to it:
      if (dataFile)
      {
        dataFile.println(strlog);
        dataFile.close();
        // print to the serial port too:
        Serial.println("writing log on timer: " + filename);
        Serial.println(strlog);
      }
      // if the file isn't open, pop up an error:
      else
      {
        Serial.println("error opening log for write");
      }
    }
  }
}


void writelognumdat(int i) // эта запись в лог пишет датчик с номером i
{

  String strlog = "";
  String filename = "";
  vbat = md[i].vcc; // преобразовать тип
  vbat = vbat / 10;
  filename = String(i);
  filename.trim();
  filename = filename + ".csv"; // имя файла совпадает с именем датчика,
  String tmpt = time.gettime("d-m H:i:s");
  strlog = tmpt;
  for (int j = 0;  j < md[i].datalen; j++) // перебираем все значения данных
  {
    int itemp = round(md[i].data[j]); // в этом температуру округлим, так ка пишем в него когда темп измениться больше чем на полградуса
    strlog = strlog + ";" + String(itemp);
  }

  File dataFile = SD.open(filename, FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile)
  {
    dataFile.println(strlog);
    dataFile.close();
    // print to the serial port too:
    Serial.println("writing log on modify data: " + filename);
    Serial.println(strlog);
  }
  // if the file isn't open, pop up an error:
  else
  {
    Serial.println("error opening log for write");
  }
}



// будем посылать смс вида #al925u35 - предупреждение номер 9, датчик 2 сенсор 3, послать сообщение  при превышении данных выше 35    
//#al3* - дективировать предупреждение с номером 3
//#al* - деактивировать все предупреждения
//#all - вывести список всех предупреждениий
void setalert(String instring)
{
 String outstring="";
 Serial.println("Set alert");
 int n=instring.indexOf("#al");
 if (n > -1)
 {
   instring=instring.substring(n);
 }
 Serial.println(instring);
 if (instring.substring(3,4)=="*")
 {
 Serial.println("deactivate all alert");   
 for (int i = 1; i <= 9 ; i++) 
   {
   al[i].alert_active=0;
   }
  EEPROM.put(100, al);
  printactivealert();
  Serial.println(stringactivealert);
  sendmessage(mynumber, stringactivealert);
  stringactivealert=""; 
 return;
 }
  if (instring.substring(3,4)=="l")
 {
 Serial.println("list all alert");   
  printactivealert();
  Serial.println(stringactivealert);
  sendmessage(mynumber, stringactivealert);
  stringactivealert=""; 
  
 return;
 }
 int alertnum=instring.substring(3,4).toInt();
 Serial.print("alertnum: ");
  Serial.println(alertnum);
  if ((alertnum>9) || (alertnum<1)) 
  {
    Serial.print("wrong alertnum");
    return;
  }
 if (instring.substring(4,5)=="*")
 {
 Serial.println("deactivate alert");
 Serial.println(alertnum); 
 al[alertnum].alert_active=0;
  EEPROM.put(100, al);
  printactivealert();
  Serial.println(stringactivealert);
  sendmessage(mynumber, stringactivealert);
  stringactivealert=""; 
 return;
 }
 int datnum=instring.substring(4,5).toInt();
   if ((alertnum>9) || (alertnum<0))
   {
    Serial.print("wrong datnum");
    Serial.println(datnum); 
    return;
  }
  int sensnum=instring.substring(5,6).toInt();
   if ((sensnum>9) || (sensnum<0))
   {
    Serial.print("wrong sensnum");
    Serial.println(sensnum); 
    return;
  }
 String typealert=instring.substring(6,7);
  if (!((typealert=="d") || (typealert=="u")))
   {
    Serial.print("wrong typealert");
    Serial.println(typealert); 
    return;
  }
    float triggerdata=instring.substring(7).toFloat();
   if ((triggerdata>200) || (triggerdata<-200))
   {
    Serial.print("wrong triggerdata");
    Serial.println(triggerdata); 
    return;
  }
   Serial.print("Setting alert: ");
   String stringsetalert;
   String stringalertnum=String(alertnum);
   stringalertnum.trim();
   String stringdatnum=String(datnum);
   stringdatnum.trim();
   String stringsensnum=String(sensnum);
   stringsensnum.trim();
   String stringtriggerdata=String(triggerdata);
   stringtriggerdata.trim();
   stringsetalert="#al"+stringalertnum+stringdatnum+stringsensnum+typealert+stringtriggerdata;
   Serial.println(stringsetalert);
   al[alertnum].alert_active=1;
   al[alertnum].numdat=datnum;
   al[alertnum].numsens=sensnum;
   al[alertnum].typealert=typealert[0];
   al[alertnum].triggerdata=triggerdata;
   al[alertnum].lasttimeonsent=0;
 EEPROM.put(100, al);
  printactivealert();
  Serial.println(stringactivealert);
  sendmessage(mynumber, stringactivealert);
  stringactivealert=""; 
   
}

void printactivealert()
{
 stringactivealert="";
   for (int i = 1; i <= 9 ; i++) 
   {
   if (al[i].alert_active==1)
   {
   String stringsetalert;
   String stringalertnum=String(i);
   stringalertnum.trim();
   String stringdatnum=String(al[i].numdat);
   stringdatnum.trim();
   String stringsensnum=String(al[i].numsens);
   stringsensnum.trim();
   String stringtriggerdata=String(al[i].triggerdata);
   stringtriggerdata.trim();
   stringsetalert="#al"+stringalertnum+stringdatnum+stringsensnum+al[i].typealert+stringtriggerdata;
   stringactivealert=stringactivealert+stringsetalert+"\r\n";
   }
     
   }
   if (stringactivealert=="")
   {
     stringactivealert="no active alert";
   }
   stringactivealert="Active alert: \r\n"+stringactivealert;
   
}


void checkalerts() // проверяет значения датчиков на превышение или понижение относительно порога и отправляет смс
{
   
   
   for (int i = 1; i <= 9 ; i++) 
   {
    int numdat=al[i].numdat;
    int numsens=al[i].numsens;
    float triggerdata=al[i].triggerdata;
    int alert_active= al[i].alert_active;
    char typealert=al[i].typealert;
    long int lasttimeonsent = al[i].lasttimeonsent;
    long int currtime=millis();
    
     int needsend=0; // нужно или нет послать сообщение
     if (alert_active==1)
     {
      /* Serial.println(numdat);
       Serial.println(numsens);
       Serial.println(md[numdat].data[numsens]);
       Serial.println(triggerdata); */
       if (typealert=='u' ) // превышение порога
       {
        if (md[numdat].data[numsens]>triggerdata)
        {
          needsend=1;
        }
       
       }
       if (typealert=='d' ) // понижение порога
       {
       if (md[numdat].data[numsens]<triggerdata)
        {
          needsend=1;
        }
       }
       long int needtime=lasttimeonsent+alarmtimeout;
      // Serial.println(needtime);
      // Serial.println(currtime);
       if ((needtime>currtime) &&( lasttimeonsent!=0)) // чтобы не посылать предуперждения слишком часто , задаим период посылки
       {
         needsend=0; // если предупреждение сработало, но  время еще не подошло и не было ребута , то не посылаем
       }
       if (needsend==1)
       {
         String textalarm="Alarm on device_id:"+ String(numdat)+"  "+md[numdat].dname+"\r\n"+"sensor:" + String(numsens) +"\r\n"+"data: "+ String (md[numdat].data[numsens]) +"\r\n"+"trigger:"+String(triggerdata);
         inf (textalarm);
        sendmessage(mynumber, textalarm);
         al[i].lasttimeonsent=currtime;
         
         
       }
      }
   }
}

void listmd() //выводит в консоль основной массив данных md
{
 for (int i = 0; i <= 9 ; i++) 
   { 
     Serial.print("Device N:");
     Serial.print(i);
     Serial.print(" name:");
     Serial.print(md[i].dname);
     Serial.print(" type:");
     Serial.println(md[i].device_type);
     Serial.print("Active:");
     Serial.println(md[i].device_type);
     long int curtime=millis();
     long int tmold=(curtime-md[i].time)/1000;
     Serial.print("time old ");
     Serial.print(tmold);
     Serial.println(" sec");
     float vccbatt=float(md[i].vcc)/10;
     Serial.print("Vcc ");
     Serial.print(vccbatt);
     Serial.print(" ");
     Serial.println(md[i].symbolvcc);
     Serial.print("Data len:");
     Serial.print(md[i].datalen);
     Serial.println(" ,  data:");
     for (int j = 0; j < datalen ; j++) 
     { 
       Serial.print(md[i].data[j]);
       Serial.print(" ");
      }
   Serial.println("");
      Serial.println("");
    }
}
