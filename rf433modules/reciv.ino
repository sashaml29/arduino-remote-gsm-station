#include <LiquidCrystal.h>

#include <VirtualWire.h>
//LiquidCrystal lcd(2, 3, 4, 5, 6, 7);  // КОНТАКТЫ В ДИСПЛЕЕ RS E D4 D5 D6 D7
LiquidCrystal lcd(14, 15, 16, 17, 18, 19);  // КОНТАКТЫ В ДИСПЛЕЕ RS E D4 D5 D6 D7
//VSS-питание VDD-земля  V0-контрастность  RW-чтение или запись, замкнуть на зелмлю A- плюс подсветки K- минус подсветки
const int receive_pin = 2; // пин приемника
int s = 0;
long int mm, msec;
struct structdat
{
  char device_id[2];
  bool present = false;
  long int lastms = 0;
  int order = 0;
  int vcc = -1000; //значения -1000 означают что данных этого параметра нет
  bool batlow = false;
  int temp = -1000;
  int hum = -1000;


};

//structdat dat[10];




structdat decodedata(uint8_t* s, int len)
{

  structdat d;
  d.device_id[0] = s[2];
  d.device_id[1] = s[3];

  for (int i = 4; i < len; i = i + 2) // данные идут по два байта, первый - ид параметра, второй - значение
  {
    switch (s[i])
    {
      case 't':
        d.temp = s[i + 1];
      case 'h':
        d.hum = s[i + 1];
      case 'B':
        d.vcc = s[i + 1];
      case 'b':
        d.vcc = s[i + 1];
        d. batlow = true;
    }
  }
  return d;
}



void setup() {
  lcd.begin(16, 2); // устанавливаем кол-во столбцов и строк
  pinMode(13, OUTPUT);
  pinMode(receive_pin, INPUT);
  Serial.begin(9600);
  vw_set_rx_pin(receive_pin);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);
  vw_rx_start();
  lcd.print("Started");
}

void loop()

{
   mm = micros() - msec;
  Serial.println(mm);
  msec = micros();
  int t = -100;
  int h = -100;
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN; // обязательно присвоить перед вызовом vw_get_message, иначе переменная меняется
  if (vw_get_message(buf, &buflen)) // Non-blocking
  {

    int i;
    digitalWrite(13, true);
    delay(200);
    digitalWrite(13, false); // мигнем диодом если хоть что-то приняли
    Serial.print("Got: ");

    for (i = 0; i < buflen; i++)
    {
      Serial.print(buf[i], DEC);
      Serial.print(" ");
    }
    Serial.println("");
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
      lcd.print("V");

      //      lcd.print(device_id);
      //      // lcd.print(":") ;
      //      lcd.print(t);
      //      lcd.print("C ");
      //      lcd.print(h);
      //      lcd.print("%");
      //      Serial.print("Dtvice ID: ");
      //      Serial.print(device_id);
      //      Serial.print("   Temperature: ");
      //      Serial.print(t);
      //      Serial.print("   Humidity: ");
      //      Serial.println(h);

    }
  }
}

