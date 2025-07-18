/* Librerías */
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <pgmspace.h>

/* Definiciones */
HardwareSerial SIM800L(1);

const char* SSID = "IZZI-53E3";
const char* PASSWORD = "F0AF853B53E3";
const String TOKEN = "1fbb3d99ca08eedc1322ceefb678eb7ae3f6063459c39621b88a4ec83dc810eb";
const String SERVER = "http://192.168.0.9:5322";
const String URL = SERVER + "/api/tracker/upload-data";
const String number = "+525545464585";

const int LED = 2;

/* Variables globales */
int _timeout;
String _buffer;

/* Funciones auxiliares */
void borrarTodosMensajes() {
  Serial.println("Borrando mensajes...");
  SIM800L.println("AT+CMGD=0");
  delay(500);
  SIM800L.println("AT+CMGDA=\"DEL ALL\"");
  delay(500);
  while (SIM800L.available()) Serial.println(SIM800L.readString());
}

void enviarComando(const char* comando, int espera = 500) {
  SIM800L.println(comando);
  delay(espera);
}

String cleanString(String message) {
  message.trim();
  message.replace("\n", "");
  message.replace("\r", "");
  message.replace("\"", "'");
  return message;
}

String _readSerial() {
  _timeout = 0;
  while (!SIM800L.available() && _timeout < 12000) {
    delay(13);
    _timeout++;
  }
  return SIM800L.available() ? SIM800L.readString() : "";
}

int validarFormatoCMTI(String message) {
  message.replace("+CMTI: \"SM\",", "");
  return message.toInt();
}

/* Enviar SMS */
void enviarMensajeRecibido(String SMS) {
  enviarComando("AT+CREG?");
  enviarComando("AT+CMGF=1");
  enviarComando(("AT+CMGS=\"" + number + "\"").c_str(), 3000);
  SIM800L.println(SMS);
  delay(500);
  SIM800L.write(26);
  delay(500);
  _buffer = _readSerial();
}

/* Enviar POST HTTPS */
bool enviarPostRequest(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no conectado");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  http.setTimeout(90000);
  http.begin(client, URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + TOKEN);

  StaticJsonDocument<256> doc;
  doc["datos"] = cleanString(message);
  String payload;
  serializeJson(doc, payload);

  Serial.println("Enviando JSON: " + payload);
  int httpCode = http.POST(payload);
  Serial.println("HTTP Response: " + String(httpCode));

  if (httpCode > 0) {
    Serial.println("Respuesta servidor: " + http.getString());
    http.end();
    if(httpCode == 200) return true;

    return false;
  } else {
    Serial.println("Error en POST");
    http.end();
    return false;
  }
}

/* Leer SMS */
void leerMensaje(int index) {
  SIM800L.println("AT+CMGR=" + String(index));
  delay(500);
  String message = SIM800L.readString();
  Serial.println("Mensaje: " + message);

  if (message.indexOf("ERROR") != -1) {
    Serial.println("Mensaje inválido.");
    return;
  }

  if (message.indexOf("id:") != -1) {
    if (enviarPostRequest(message)) {
      SIM800L.println("AT+CMGD=" + String(index));
    }
  } else if (message == "BORRAR*") {
    borrarTodosMensajes();
    enviarMensajeRecibido("Mensajes eliminados");
  } else if (message == "ESTATUS?") {
    SIM800L.println("AT");
    delay(500);
    enviarMensajeRecibido(SIM800L.readString());
  }
}

/* Recibir mensajes en vivo */
void recibirMensajes() {
  enviarComando("AT+CMGF=1");
  enviarComando("AT+CNMI=1,2,0,0,0");
}

/* Setup */
void setup() {
  Serial.begin(9600);
  SIM800L.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  Serial.println("Conectando a WiFi...");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");

  SIM800L.println("AT");
  delay(500);
  enviarComando("AT+CMGF=1");
  borrarTodosMensajes();
  recibirMensajes();
  digitalWrite(LED, LOW);
}

String splitMessage(String input) {
    int separatorIndex = input.indexOf("id:");

    // Validar que se encontró la separación correcta
    if (separatorIndex == -1) {
        return "Error: No se encontró 'id:' en el mensaje.";
    }

    // Extraer ambas partes
    String messagePart = input.substring(0, separatorIndex);
    String dataPart = input.substring(separatorIndex);

    // Mostrar en el monitor serie
    Serial.println("Parte 1: " + messagePart);
    Serial.println("Parte 2: " + dataPart);

    //enviarMensajeRecibido(messagePart);
    return dataPart;
}

/* Loop principal */
void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "r") recibirMensajes();
    if (cmd == "borrar") borrarTodosMensajes();
  }

  if (SIM800L.available()) {
    String notif = SIM800L.readString();
    notif = cleanString(notif);
    Serial.println("Recibido: " + notif);

    if (notif.indexOf("CMTI") != -1) {
      Serial.println("Mensaje como notificacion");
      int index = validarFormatoCMTI(notif);
      leerMensaje(index);
    }

    // 📌 Nueva condición para mensajes en vivo con +CMT:
    if (notif.indexOf("+CMT:") != -1) {
      Serial.println("Mensaje en vivo");
      // Leer siguiente línea que contiene el mensaje
      // while (!SIM800L.available()) delay(10);
      // String message = SIM800L.readString();
      String message = cleanString(notif);
      Serial.println("Mensaje recibido en vivo: " + message);

      if (message.indexOf("id:") != -1) {
        if (enviarPostRequest(message)) {
          enviarMensajeRecibido("Mensaje enviado al servidor");
          
        }else{
          enviarMensajeRecibido("Ocurrio un error al enviar al servidor");
        }

        String payload = splitMessage(message);
        Serial.println("payload: " + payload);
        delay(2000);
        enviarMensajeRecibido(payload);
        delay(3000);
        enviarMensajeRecibido("Test de envío SIM800L");
      } else if (message.indexOf("BORRAR*") != -1) {
        Serial.println("Solicitando borrar mensajes en buffer");
        borrarTodosMensajes();
        enviarMensajeRecibido("Mensajes eliminados");
      } else if (message.indexOf("ESTATUS?") != -1) {
        Serial.println("Solicitando estatus de receptor");
        SIM800L.println("AT");
        delay(500);
        enviarMensajeRecibido(SIM800L.readString());
      }
    }
  }
}
