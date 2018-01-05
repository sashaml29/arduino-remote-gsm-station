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

#define gsmport Serial1

String textsms, number, txt;
String mynumber;
String temp;
int debugstatus=1;
unsigned long predtime, nexttime, tektime; 
String val=""; //глобальная переменная где храним строку с модема, чотбы не создавать лишний string в вызываемых процедурах
int enableset;



void setup() {
wdt_disable();
Serial.begin(9600);
display.begin();
display.setContrast(65);


inf("Init ...");
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
mynumber=ReadMasterNum (); // считывает запись с индексом 1 c сим карты
inf("Master num:\r\n"+mynumber);
delay(5000);
//mynumber="+792100000000"; // можно напрямую присвоить мастер номер
pinMode(13, OUTPUT); //для идикаторного диода
dbgprint("Setup done");
 
 vw_set_rx_pin(receive_pin);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);
  vw_rx_start();

predtime=0;
nexttime=0;


wdt_enable (WDTO_8S); // watchdog на 8 секунд, родной загрузчик ардуино должен быть обязательно заменен на otiboot, иначе будет циклическая перезагрузка
}


void loop() 
{ 
  wdt_reset();  // вызываемые процедуры из loop() суммарно не должны длиться более 8 секунд (либо в них должен стоять еще wdt_reset(); ) , иначе перезагруга
  tektime=millis();
  if ((nexttime<tektime) || (predtime>tektime)) mainevent(); //если время следующего события пришло или время предыдущего события больше текущего времени, что бывает раз в 49 дней при переполнении millis(), то вызываем основное событие
  if (StrFromSerial(txt)>0)  //ждем команды вручную с порта и если что-то есть - посылаем в модем и печатаем ответ
  {
    gsmport.print(txt);
    temp=gsmport.readString();
    /*Serial.print(temp);
    display.print(temp);*/
    inf(temp);
  };
  checkforsms(); // проверяем буфер порта модема  на наличе данных и смс в нем
  
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN; // в библиотеке это 30 обязательно присвоить перед вызовом vw_get_message, иначе переменная меняется
  if (vw_get_message(buf, &buflen)) // Non-blocking
  {

    int i;
    digitalWrite(13, true);
    delay(200);
    digitalWrite(13, false); // мигнем диодом если хоть что-то приняли
    String tmp="";
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
      if (buf[i]==0)
       {
         tmp=tmp+"/0";
       }
       else 
        {
          tmp=tmp+char(buf[i]);
        }
      
    }
     display.println("");
     display.print(tmp);  //выведем принятое в символьном виде
    Serial.println("");
     Serial.println(tmp);
    
      
    display.display();
  }
    /*Serial.println("");
    digitalWrite(13, false);
    if ((buf[0] == 'D' ) && (buf[1] == 'T')) // текстовое сообщение начинается с DT
    {

      lcd.setCursor(0, 1);
      lcd.print("                ");//моргнем цифрами чтобы видеть что данные обновляются
      lcd.setCursor(0, 1);
      delay(200);
      for (int i = 2; i < buflen; i++)
      {
        lcd.print(char((buf[i])));
      }
    };

    if ((buf[0] == 68) && (buf[1] == 32)) // сообщение c данными начинается с D пробел
    {
      char device_id = buf[2];
      int t = buf[3];
      int h = buf[4];
      lcd.setCursor(0, 0);
      lcd.print("                ");//моргнем цифрами чтобы видеть что данные обновляются
      lcd.setCursor(0, 0);
      delay(200);
      structdat d = decodedata((uint8_t *)buf, buflen);
      Serial.println(char(d.device_id[0]));
      lcd.print(d.device_id[0]);
      lcd.print(d.device_id[1]);
      lcd.print(":") ;
      lcd.print(d.temp);
      lcd.print("C ");
      lcd.print(d.hum);
      lcd.print("% ");
      float vcc1 = d.vcc;
      float vcc = vcc1 / 10;
      lcd.print(vcc);
      lcd.print("V");*/
}
  

void checkforsms() 
 {
  if(gsmport.available()) //если модуль что-то послал
  {  
    val=gsmport.readString();  //буфер порта только 64байта, смс может обрезать до 14 символов, команды делать лучше длиной до 10 символов
    if (val.indexOf("+CMT") > -1)
    {
     Serial.println("Incoming SMS:");
     Serial.println(val);
     gsmport.println("AT+CMGD=1,4"); // удалить все сообщения в памяти чтобы не забивать память модема
     gsmport.readString(); // игнорируем ответ- читаем в никуда
     String Numtel=substrPoNomeru(1);
     if ((Numtel.indexOf(mynumber) > -1) || (enableset==1) ) // если это свой номер или настройки разрешены
     {
       Serial.println("This mynumber"); 
       if (val.indexOf("#info") > -1) sendinfo();
       if (val.indexOf("#settime") > -1) settimedatefromsms();
       if (val.indexOf("#setmynum") > -1) setmynumber();
       if (val.indexOf("#balans") > -1) sendbalans();
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
   textsms="info:";
   textsms=textsms+"\r"+"tepl 29 10:15";
   textsms=textsms+"\r"+"komn 20 21:30" ;
   textsms=textsms+"\r"+"mans 15";
   textsms=textsms+"\r"+"podah 29";
   textsms=textsms+"\r"+"obrat 25";
   sendmessage(mynumber, textsms);
   inf("comamnd #sendinfo");
  }
 
 
 void settimedatefromsms()
  {
   // надо делать только при зажатой кнопке
   inf("comamnd #settime");
   String DateTime=substrPoNomeru(3);
   String Command="AT+CCLK="+'"'+DateTime+'"'; 
   gsmport.println(Command);
   temp=gsmport.readString();
   Serial.println(temp);
   gsmport.println(Command);
   gsmport.println("AT+CCLK?"); 
   temp=gsmport.readString();
   Serial.println(temp);
  }


void setmynumber ()
{
  inf("command #setmynum");
  String Numtel=substrPoNomeru(1);
  String Command="AT+CPBW=1,\""+Numtel+"\",145,\"Master\"";
  gsmport.println(Command);
  String Mynum=substrPoNomeru(1);
  printstrfrommodem();
  inf(Mynum)  ;
  
}
  

void sendbalans ()
{
 inf("command #balans");
//тут поставим кнопку при зажатии которой можно записывать мастер номер
  String Command="AT+CUSD=1,"+String('"')+String('*')+"100#"+'"'; 
   //AT+CUSD=1,"*100#"
   Serial.println(Command); 
   gsmport.println(Command);
   wdt_reset(); 
   delay(5000);
   wdt_reset(); 
   delay(5000);
   wdt_reset(); 
   temp=gsmport.readString();
   String text=temp.substring(37);
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
     inf("Sending sms:\r\n"+text);
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
{  //функция читает данные с порта 0 без пауз и записывает в переданную переменную val 
  int i=0;
  if(Serial.available()) // что-то послалано в порт
  {  
    char ch = ' ';
    val = "";
    while(Serial.available()) 
    {  
     ch = Serial.read();
     val += char(ch); //собираем принятые символы в строку
     delay(5);
    }
    i=1;
  }
  return i;
}   


void dbgprint(String text) {
  if (debugstatus=1)  Serial.println(text); 
 }


void inf(String text)
  {  //вывод промежуточнонй информации в порт на дисплей или еще куда
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
  String strret="";
  char q='"'; // будем искать символ кавычку
  int poz2=-1; // позиция 0 это первая позиция в строке
  int poz1=-1; 
  int k=0;
  if (val.length()==0) return "";
  for (int i=0; i <= val.length()-1 ; i++)
  {
   if (val[i]==q)
    {
      k=k+1;
      if (k==num*2-1) poz1=i;
      if (k==num*2) poz2=i;
    };
  };
  if ((poz1>-1)&(poz2>-1))  strret=val.substring(poz1+1, poz2);
  return strret;
}


 void printstrfrommodem() // печатает ответ модема в порт
 {  val=gsmport.readString();
    Serial.print("GSM modem send> ");
    Serial.println(val);
 }   

String ReadMasterNum () // возвращает из первой ячейки смс номер телефона
{
  gsmport.println("AT");
  gsmport.readString();
  gsmport.println("AT+CPBR=1");
  val=gsmport.readString();
  String Numtel=substrPoNomeru(1);
  return Numtel;
}

void mainevent() // основное событие раз в 10 секунд, отсюда вызываются другие более редкие события
{
 /*  refreshdisplay(); 
   predtime=millis(); //запоминаем время прошедшего события
   nexttime=predtime+10000; // назначаем время следующего основного события*/
}

void refreshdisplay()
{ 
  display.clearDisplay();
  display.display();
  delay(100);  // мигнем пустым дисплеем, чтобы показать, что программа работает
  display.println("Main event 10s\r\n" +String(int(tektime/1000))+" sec");
  display.display();
}
