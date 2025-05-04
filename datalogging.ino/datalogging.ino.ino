#include <RTCZero.h>   // RTC alarm & standbyMode
#include <SdFat.h>     // Fast FAT32 I/O
#include <SPI.h>       // SPI bus for SD

RTCZero rtc;
SdFat SD;

const uint8_t SD_CS        = SDCARD_SS_PIN;  // MKRZero onboard CS pin :contentReference[oaicite:2]{index=2}
const uint8_t SD_PWR_PIN   = 7;              // GPIO → PMOS gate (pulls LOW to power ON)
const uint32_t WAKE_SEC    = 600;            // 10 minutes

void rtcWake(); // Forward declaration for ISR

void setup() {
  // Disable unused peripherals for low power
  USBDevice.detach();
  ADC->CTRLA.bit.ENABLE = 0;

  // Configure GPIOs
  pinMode(SD_PWR_PIN, OUTPUT);
  digitalWrite(SD_PWR_PIN, HIGH);          // SD off by default (gate pulled up)
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);               // deselect SD

  // Initialize RTC
  rtc.begin();
  rtc.setTime(12, 0, 0);           // Set arbitrary current time (HH,MM,SS)
  rtc.setDate(1, 5, 25);           // Set date (DD,MM,YY)
  rtc.attachInterrupt(rtcWake);
}

void loop() { // sleep, wake, log, repeat
  // Power on SD card via PMOS
  digitalWrite(SD_PWR_PIN, LOW);
  delay(10);                               // 10 ms for VCC to stabilize
  // Exception Catch
  if (!SD.begin(SD_CS)) {
    digitalWrite(SD_PWR_PIN, HIGH);        // power off SD on failure
    rtc.setAlarmEpoch(rtc.getEpoch() + WAKE_SEC); // Retry next cycle
    rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);
    rtc.standbyMode();                          
    return;
  }

  // timestamped filename: "YYYY-MM-DD_HH-MM-SS.csv"
  char fname[25];
  snprintf(fname, sizeof(fname), "%04u-%02u-%02u_%02u-%02u-%02u.csv",
           2000 + rtc.getYear(), rtc.getMonth(), rtc.getDay(), rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());

  // Open file & write 5 rows of data
  File logF = SD.open(fname, FILE_WRITE);
  if (logF) {
    for (int i = 0; i < 5; i++) {
      // Dummy Data
      logF.print(1.0 + i * 0.1f);
      logF.write(',');
      logF.print(2.0 + i * 0.2f);
      logF.write(',');
      logF.println(3.0 + i * 0.3f);
    }
    logF.close();
  }

  // Power off SD card
  SD.end();
  digitalWrite(SD_CS, HIGH);
  digitalWrite(SD_PWR_PIN, HIGH);
  SPI.end();

  // Configure next RTC alarm & enter standby
  uint32_t now = rtc.getEpoch();
  rtc.setAlarmEpoch(now + WAKE_SEC);       // schedule 10 min later
  rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);           // match seconds field
  rtc.standbyMode();                       // sets SLEEPDEEP and sleep-on-exit
}

// rtcWake(): RTC alarm ISR
void rtcWake() {
  rtc.clearAlarm();                        // clear flag for next cycle
  // Execution resumes immediately after __WFI(), returning to loop()
}
