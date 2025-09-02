#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
// ====== USER CONFIG ======
const char* WIFI_SSID     = "BITEK";
const char* WIFI_PASS     = "bitek@2025";
const char* BOT_TOKEN     = "8382833862:AAEY6iC4yoprH9iU9JBnFf-ueAlOAo1LW4Y"; // from BotFather
// Pins
constexpr int PIR_PIN     = 13; // Connect PIR output directly to this pin
constexpr int BUZZER_PIN  = 12; // Connect buzzer via 100Ω-1kΩ resistor
constexpr int LED_PIN     = 14; // Connect LED via 220Ω-1kΩ resistor
// Behavior
bool ARMED = true;                  // start armed
unsigned long ALERT_COOLDOWN_MS = 15000;  // 15s anti-spam
unsigned long SIREN_MS = 2000;            // motion siren duration
// ====== INTERNALS ======
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long lastAlertMs = 0;
int lastPirState = LOW;
unsigned long bot_lasttime = 0;
const unsigned long BOT_POLL_MS = 1000;
// If you don't know your chat ID yet, leave this empty "".
// On first message, we'll print the chat ID in Serial.
String OWNER_CHAT_ID = "";  // e.g., "123456789"
// Forward decl
void handleNewMessages(int numNewMessages);
void ensureWiFi();
void setSiren(bool on);
void sendStatus(const String& chat_id);
void sendWelcome(const String& chat_id);
void sendControlHint(const String& chat_id);
// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  secured_client.setInsecure(); // easiest for Telegram SSL
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.println("Open Telegram, message your bot with /start, then watch Serial for your chat ID.");
}
// ====== LOOP ======
void loop() {
  ensureWiFi();
  // 1) Poll Telegram for commands
  if (millis() - bot_lasttime > BOT_POLL_MS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
  // 2) Read motion sensor
  int pirVal = digitalRead(PIR_PIN);
  // Rising edge = newly detected motion
  if (pirVal == HIGH && lastPirState == LOW) {
    if (ARMED) {
      unsigned long now = millis();
      if (now - lastAlertMs > ALERT_COOLDOWN_MS) {
        // Trigger alert actions
        digitalWrite(LED_PIN, HIGH);
        setSiren(true);
        delay(SIREN_MS);
        setSiren(false);
        lastAlertMs = now;
        // Notify via Telegram
        String msg = "🚨 Motion Detected!\n";
        msg += "Time: " + String((unsigned long)(now / 1000)) + "s since boot\n";
        if (OWNER_CHAT_ID.length() > 0) {
          bot.sendMessage(OWNER_CHAT_ID, msg, "");
        } else {
          Serial.println("[WARN] OWNER_CHAT_ID not set. Printing message only:");
          Serial.println(msg);
        }
      }
    } else {
      // Disarmed: just blink LED briefly
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
    }
  }
  // LED reflects armed state when idle
  if (pirVal == LOW && !ARMED) {
    digitalWrite(LED_PIN, LOW);
  } else if (pirVal == LOW && ARMED) {
    digitalWrite(LED_PIN, LOW);
  }
  lastPirState = pirVal;
}
// ====== HELPERS ======
void ensureWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Reconnected");
    } else {
      Serial.println("[WiFi] Failed to reconnect");
    }
  }
}
void setSiren(bool on) {
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}
void sendWelcome(const String& chat_id) {
  String txt = "👋 *ESP32 Security Bot*\n\n";
  txt += "Commands:\n";
  txt += "/status – Show system status\n";
  txt += "/arm – Arm the motion alarm\n";
  txt += "/disarm – Disarm the motion alarm\n";
  txt += "/siren_on – Turn buzzer ON\n";
  txt += "/siren_off – Turn buzzer OFF\n";
  txt += "/test – Send a test alert\n";
  txt += "/ping – Bot health check\n";
  bot.sendMessage(chat_id, txt, "Markdown");
}
void sendControlHint(const String& chat_id) {
  String txt = "🔧 Quick control:\n";
  txt += "Use /arm, /disarm, /status, /siren_on, /siren_off, /test.";
  bot.sendMessage(chat_id, txt, "");
}
void sendStatus(const String& chat_id) {
  String txt = "📊 *Status*\n";
  txt += String("Armed: ") + (ARMED ? "YES" : "NO") + "\n";
  txt += "Cooldown: " + String(ALERT_COOLDOWN_MS / 1000) + "s\n";
  txt += "WiFi: " + String(WiFi.SSID()) + " (" + WiFi.localIP().toString() + ")\n";
  bot.sendMessage(chat_id, txt, "Markdown");
}
bool isOwner(const String& chat_id) {
  // First contact? Print chat_id so you can paste it into OWNER_CHAT_ID
  if (OWNER_CHAT_ID.length() == 0) {
    Serial.print("[INFO] First chat detected. Your chat_id is: ");
    Serial.println(chat_id);
    Serial.println("Paste this into OWNER_CHAT_ID and re-upload to lock the bot.");
    return true; // allow temporarily
  }
  return chat_id == OWNER_CHAT_ID;
}
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text    = bot.messages[i].text;
    String user    = bot.messages[i].from_name;
    // Log chat_id once for setup
    Serial.print("[TG] From ");
    Serial.print(user);
    Serial.print(" (chat_id=");
    Serial.print(chat_id);
    Serial.print("): ");
    Serial.println(text);
    if (!isOwner(chat_id)) {
      bot.sendMessage(chat_id, "⛔ Not authorized.", "");
      continue;
    }
    if (text == "/start") {
      sendWelcome(chat_id);
      sendControlHint(chat_id);
    }
    else if (text == "/help") {
      sendWelcome(chat_id);
    }
    else if (text == "/status") {
      sendStatus(chat_id);
    }
    else if (text == "/arm") {
      ARMED = true;
      bot.sendMessage(chat_id, "🟢 System *ARMED*.", "Markdown");
    }
    else if (text == "/disarm") {
      ARMED = false;
      setSiren(false);
      digitalWrite(LED_PIN, LOW);
      bot.sendMessage(chat_id, "🔴 System *DISARMED*.", "Markdown");
    }
    else if (text == "/siren_on") {
      setSiren(true);
      bot.sendMessage(chat_id, "📢 Siren ON.", "");
    }
    else if (text == "/siren_off") {
      setSiren(false);
      bot.sendMessage(chat_id, "🔇 Siren OFF.", "");
    }
    else if (text == "/test") {
      bot.sendMessage(chat_id, "🧪 Test alert: system reachable.", "");
    }
    else if (text == "/ping") {
      bot.sendMessage(chat_id, "🏓 pong", "");
    }
    else {
      bot.sendMessage(chat_id, "❓ Unknown command. Try /help", "");
    }
  }
}
