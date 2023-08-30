#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Токен бота.
#define BOTtoken "token"

//Имя и пароль сети.
char ssid[] = "name";
char password[] = "password";
// Список устройств для управления.
String buttons[] = {"Socket"};
// Номер вывода, к которому подключено реле (возможно добавление нескольких устройств).
int pin[] = {5};

// Права доступа: 0 - доступно всем пользователям, 1 - доступ по Chat ID, если оно внесено в accessChatID.
bool protection = 1;
// Chat ID, которым разрешен доступ. Игнорируется, если protection = 0.
int accessChatID[] = {0000000000};

// Индикатор включенного состояния.
String onIndicator = "🟢 ";
// Индикатор выключенного состояния.
String offIndicator = "🔴 ";

// Объект для безопасного соединения с API.
WiFiClientSecure client;
// Инициализация бота.
UniversalTelegramBot bot(BOTtoken, client);

// Количество устройств.
int quantity;
// Время в мс между опросом бота или получением новых сообщений.
int botMs = 3000;
// Время последнего опроса бота.
long botLasttime;
// Клавиатура бота.
String keyboardJson = "";
// Переменная для хранения идентификатора последнего сообщения "/control".
String lastControlMessageId = "";

// Обработка новых сообщений.
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String m_id = String(bot.messages[i].message_id);
    if (bot.messages[i].type == "callback_query") {
      String statusMessage;
      for (int j = 0; j < quantity; j++) {
        if (bot.messages[i].text == buttons[j]) {
          // Изменение состояния реле.
          digitalWrite (pin[j], !digitalRead(pin[j]));
        }
        // Выбор индикатора состояния.
        digitalRead(pin[j]) ? statusMessage += onIndicator : statusMessage += offIndicator;
        statusMessage += buttons[j]; 
        statusMessage += '\n';
      }
      // Удаление сообщения и отправка нового с обновленным индикатором состояния.
      bot.deleteMessage(bot.messages[i].chat_id, bot.messages[i].message_id);
      bot.sendMessageWithInlineKeyboard(bot.messages[i].chat_id, statusMessage, "", keyboardJson);
      
    } else {
      String text = bot.messages[i].text;
      Serial.println(m_id);
      String from_name = bot.messages[i].from_name;
      // Если имя не указано, обращаться как к гостю.
      if (from_name == "") from_name = "Гость";
      bool commandRecognized = false;
      int k = 0;
      do {
        if (!protection || String(accessChatID[k]) == chat_id) {
          // Сообщение с кнопками управления.
          if (text == "/control") {
            if (lastControlMessageId != "") {
              // Если уже есть активное сообщение с кнопками управления, отправляем сообщение об этом.
              bot.sendMessage(chat_id, "Вы уже вызвали сообщение для управления.", "");
            } else {
              String statusMessage;
              for (int i = 0; i < quantity; i++) {
                digitalRead(pin[i]) ? statusMessage += onIndicator : statusMessage += offIndicator;
                statusMessage += buttons[i];
                statusMessage += '\n';
              }
              // Удаление предыдущего сообщения "/control".
              if (lastControlMessageId != "") {
                bot.deleteMessage(chat_id, lastControlMessageId);
              }
              // Сохранение идентификатора текущего сообщения.
              lastControlMessageId = m_id;
              // Отправка нового сообщения с обновленным статусом устройств.
              bot.sendMessageWithInlineKeyboard(chat_id, statusMessage, "", keyboardJson);
            }
            commandRecognized = true;
          }

          // Приветственное сообщение.
          if (text == "/start") {
            String welcome = "Здравствуйте, " + from_name + ".\nЭто умный выключатель на микроконтроллере ESP8266, управляемый через Telegram.\n\n/help - получить справочное сообщение.\n/control - перейти к управлению.\n";
            String keyboardStart = "[[{ \"text\" : \"GitHub разработчика\", \"url\" : \"https://github.com/Annanas555\" }]]";
            // Отправка сообщения с клавиатурой.
            bot.sendMessageWithInlineKeyboard(chat_id, welcome, "", keyboardStart);
            commandRecognized = true;
          }

          // Справочное сообщение.
          if (text == "/help") {
            String helpMessage = "Название Вашего устройства отображено на кнопке.\n🟢 - индикатор включенного состояния.\n🔴 - индикатор выключенного состояния.\nЧтобы начать управлять выключателем, пришлите команду /control";
            bot.sendMessage(chat_id, helpMessage, "");
            commandRecognized = true;
          }
          break;
        } else {
          // Если Chat ID не внесен в список для получения доступа.
          if (k == ((sizeof(accessChatID) / sizeof(int)) - 1) && text == "/start" || (sizeof(accessChatID) / sizeof(int)) == 0 && text == "/start") {
            bot.sendMessage(chat_id, "Нет доступа. Внесите Chat ID в список разрешенных.\nChat ID: " + chat_id, "");
            commandRecognized = true;
          }
        }

        k++;
      }
      
      while (k < (sizeof(accessChatID) / sizeof(int)));
      
      if (!commandRecognized) {
        // Отправка сообщения о неизвестной команде.
        bot.sendMessage(chat_id, "Неизвестная команда!", "");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  quantity = sizeof(pin) / sizeof(int);
  for (int i = 0; i < quantity; i++) {
    pinMode(pin[i], OUTPUT);
  }

  // Генерация кнопок на клавиатуре для управления устройствами
  for (int i = 0; i < quantity; i++) {
    if (i == 0) keyboardJson += "[";
    keyboardJson += "[{ \"text\" : \"";
    keyboardJson += buttons[i];
    keyboardJson += "\", \"callback_data\" : \"";
    keyboardJson += buttons[i];
    keyboardJson += "\" }]";
    if (i == quantity - 1) {
      keyboardJson += "]";
    }
    else {
      keyboardJson += ",";
    }
  }
  delay(10);
  client.setInsecure();
}

void loop() {
  if (millis() > botLasttime + botMs)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    botLasttime = millis();
  }
}
