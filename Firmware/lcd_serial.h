#ifndef LCD_SERIAL_H
#define LCD_SERIAL_H

extern String lastLCDmsg;

inline void sendLCD(String msg)
{
    static unsigned long lastSent = 0;
    if(msg != lastLCDmsg || millis() - lastSent >= LCD_HEARTBEAT_MS)
    {
        Serial2.println(msg);
        lastLCDmsg = msg;
        lastSent = millis();
    }
}

#endif
