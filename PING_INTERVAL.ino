#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Ping.h>
#include <WiFi.h>
#include <time.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// กำหนดพินสำหรับ RGB LED
#define RED_PIN 25
#define GREEN_PIN 26
#define BLUE_PIN 27

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "101-IOT";
const char* password = "10101010";
const char* remote_host = "1.1.1.1";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;  // GMT+7
const int daylightOffset_sec = 0;

// ตัวแปรสำหรับเก็บค่า ping และคุณภาพ
float lastPingTime = 0;
String lastQuality = "";
unsigned long lastPingCheck = 0;
const unsigned long PING_INTERVAL = 5000; // 5 วินาที

// ตัวแปรสำหรับคำนวณค่าเฉลี่ย
const int MAX_SAMPLES = 12;  // จำนวนตัวอย่างใน 1 นาที (12 ครั้ง จาก ping ทุก 5 วินาที)
float pingSamples[MAX_SAMPLES];
int sampleIndex = 0;
float avgPingTime = 0;

// ตัวแปรสำหรับสลับหน้าจอ
bool showStats = false;
unsigned long screenToggleTime = 0;
const unsigned long SCREEN_TOGGLE_INTERVAL = 3000; // สลับหน้าจอทุก 3 วินาที

String getLocalDateTime()
{
  struct tm timeinfo;
  char dateTimeStringBuff[50];
  
  if(!getLocalTime(&timeinfo)){
    return "Failed to get time";
  }
  
  strftime(dateTimeStringBuff, sizeof(dateTimeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(dateTimeStringBuff);
}

void updateLED() {
  // ปิดไฟ LED ทั้งหมดก่อน
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
  
  if(lastPingTime < 50) {
    digitalWrite(GREEN_PIN, HIGH);  // สีเขียว เมื่อ ping < 50ms
  } else if(lastPingTime >= 200 && lastPingTime < 300) {
    digitalWrite(BLUE_PIN, HIGH);   // สีน้ำเงิน เมื่อ ping 200-299ms
  } else if(lastPingTime >= 300) {
    digitalWrite(RED_PIN, HIGH);    // สีแดง เมื่อ ping >= 300ms
  }
}

void updatePingAverage() {
  float sum = 0;
  int validSamples = 0;
  
  for(int i = 0; i < MAX_SAMPLES; i++) {
    if(pingSamples[i] > 0) {
      sum += pingSamples[i];
      validSamples++;
    }
  }
  
  if(validSamples > 0) {
    avgPingTime = sum / validSamples;
  } else {
    avgPingTime = 0;
  }
}

void displayDateTime() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Ping: " + String(lastPingTime, 1) + " ms");
  display.println("Quality: " + lastQuality);
  display.setCursor(0,20);
  display.println(getLocalDateTime());
  display.display();
}

void displayStats() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("1-Min Statistics:");
  display.println("Avg: " + String(avgPingTime, 1) + " ms");
  display.println("Last: " + String(lastPingTime, 1) + " ms");
  display.display();
}

void checkPing() {
  if(Ping.ping(remote_host)) {
    lastPingTime = Ping.averageTime();
    
    // บันทึกค่าลงในอาเรย์
    pingSamples[sampleIndex] = lastPingTime;
    sampleIndex = (sampleIndex + 1) % MAX_SAMPLES;
    
    // คำนวณค่าเฉลี่ยใหม่
    updatePingAverage();
    
    if(lastPingTime < 20) {
      lastQuality = "Excellent";
    } else if(lastPingTime < 50) {
      lastQuality = "Good";
    } else if(lastPingTime < 100) {
      lastQuality = "Fair";
    } else {
      lastQuality = "Poor";
    }
    
    // อัพเดทสถานะ LED
    updateLED();
    
    Serial.println("Ping: " + String(lastPingTime, 1) + " ms");
    Serial.println("Avg Ping: " + String(avgPingTime, 1) + " ms");
  } else {
    lastPingTime = 0;
    lastQuality = "Failed";
    Serial.println("Ping failed");
    
    // กระพริบไฟแดงเมื่อ ping ล้มเหลว
    digitalWrite(RED_PIN, HIGH);
    delay(100);
    digitalWrite(RED_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);

  // กำหนดค่า LED pins เป็น OUTPUT
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // ปิดไฟ LED ทั้งหมดเมื่อเริ่มต้น
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);

  // เริ่มต้นค่า pingSamples ด้วย 0
  for(int i = 0; i < MAX_SAMPLES; i++) {
    pingSamples[i] = 0;
  }

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connecting to WiFi...");
    display.display();
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  checkPing();
}

void loop() {
  unsigned long currentMillis = millis();
  
  if(WiFi.status() == WL_CONNECTED) {
    // สลับหน้าจอทุก 3 วินาที
    if(currentMillis - screenToggleTime >= SCREEN_TOGGLE_INTERVAL) {
      showStats = !showStats;
      screenToggleTime = currentMillis;
    }
    
    // แสดงผลตามหน้าที่เลือก
    if(showStats) {
      displayStats();
    } else {
      displayDateTime();
    }
    
    // ตรวจสอบ ping ทุก 5 วินาที
    if(currentMillis - lastPingCheck >= PING_INTERVAL) {
      checkPing();
      lastPingCheck = currentMillis;
    }
  } else {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("WiFi disconnected");
    display.display();
    
    // กระพริบไฟน้ำเงินเมื่อ WiFi หลุด
    digitalWrite(BLUE_PIN, HIGH);
    delay(500);
    digitalWrite(BLUE_PIN, LOW);
    delay(500);
  }
  
  delay(1000);
}
