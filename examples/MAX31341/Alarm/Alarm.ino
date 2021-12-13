#include <MaxEssentialToolkit.h>

MAX31341 rtc(&Wire, MAX31341_I2C_ADDRESS);

// Pin Number that connects to MAX31341 INTA pin
// Please update pin_inta depend on your target board connection
int pin_inta = 3;

char time_char_buffer[40];
struct tm rtc_ctime;

void alarm1_irq_handler(void) {
  
    rtc.get_time(&rtc_ctime);

    strftime(time_char_buffer, sizeof(time_char_buffer), "%Y-%m-%d %H:%M:%S", &rtc_ctime);
    Serial.println(time_char_buffer);
}

void setup() {
    int ret;

    Serial.begin(115200);
    Serial.println("---------------------");
    Serial.println("MAX31341 Alarm1 use case example:");
    Serial.println("MAX31341 will be configured to generate periodic alarm");
    Serial.println(" ");

    rtc.begin();
    
    rtc_ctime.tm_year = 121; // years since 1900
    rtc_ctime.tm_mon  = 10;  // 0-11
    rtc_ctime.tm_mday = 24;  // 1-31
    rtc_ctime.tm_hour = 15;  // 0-23
    rtc_ctime.tm_min  = 10;  // 0-59
    rtc_ctime.tm_sec  = 0;   // 0-61
    //
    rtc_ctime.tm_yday  = 0;  // 0-365
    rtc_ctime.tm_wday  = 0;  // 0-6
    rtc_ctime.tm_isdst = 0;  // Daylight saving flag
    
    ret = rtc.set_time(&rtc_ctime);
    if (ret) {
        Serial.println("Set time failed!");
    }

    // Set alarm pin as input
    pinMode(pin_inta, INPUT);

    ret = rtc.configure_inta_clkin_pin(MAX31341::CONFIGURE_PIN_AS_INTA);
    if (ret) {
        Serial.println("Configure inta failed!");
    }

    ret = rtc.set_alarm(MAX31341::ALARM1, &rtc_ctime, MAX31341::ALARM_PERIOD_EVERYSECOND);
    if (ret) {
        Serial.println("Set alarm failed!");
    }

    ret = rtc.irq_enable(MAX31341::INTR_ID_ALARM1);
    if (ret) {
        Serial.println("IRQ enable failed!");
    }

    rtc.clear_irq_flags();
}

void loop()  {

    //Serial.println("Tick");

    int pin_state = digitalRead(pin_inta);
    
    if (pin_state == LOW) {
        alarm1_irq_handler();
        rtc.clear_irq_flags();    
    }
}
