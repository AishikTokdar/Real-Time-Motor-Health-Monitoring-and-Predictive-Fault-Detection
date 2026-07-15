#ifndef LCD_SERIAL_H
#define LCD_SERIAL_H

String lastLCDmsg = "";

void sendLCD(String msg)
{
    if(msg != lastLCDmsg)
    {
        Serial2.println(msg);
        lastLCDmsg = msg;
    }
}

#endif
