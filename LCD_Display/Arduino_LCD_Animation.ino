#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <SoftwareSerial.h>

hd44780_I2Cexp lcd;
SoftwareSerial espSerial(2, 3);

String receivedMessage = "";
String lastDisplayedMessage = "\xff";

String projectTitle = "Motor Health v1";

void setup()
{
  Serial.begin(9600);
  espSerial.begin(9600);

  lcd.begin(16, 2);
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print(projectTitle);
}

void loop()
{

  if (espSerial.available() > 0)
  {
    receivedMessage = espSerial.readStringUntil('\n');
    receivedMessage.trim();
    Serial.println(receivedMessage);
  }

  if (receivedMessage != lastDisplayedMessage)
  {
    lastDisplayedMessage = receivedMessage;

    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print(projectTitle);

    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);

    if (receivedMessage.startsWith("STATUS") ||
        receivedMessage.startsWith("FAULT") ||
        receivedMessage.startsWith("WIFI") ||
        receivedMessage.startsWith("BLYNK"))
    {
      lcd.print(receivedMessage);
    }
    else if (receivedMessage.startsWith("MAINT"))
    {
      lcd.print("MAINTENANCE!");
    }
    else if (receivedMessage.length() > 0)
    {
      lcd.print(receivedMessage);
    }
    else
    {
      lcd.print("SYSTEM READY");
    }
  }

  delay(100);
}
