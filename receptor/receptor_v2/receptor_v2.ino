/* Librerías */
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <pgmspace.h>

/* Definiciones */
HardwareSerial SIM800L(1);

//const char* SSID = "SKLA-H90";
//const char* PASSWORD = "PagaTuWifi";
const char* SSID = "IZZI-53E3";
const char* PASSWORD = "F0AF853B53E3";
const String TOKEN = "1fbb3d99ca08eedc1322ceefb678eb7ae3f6063459c39621b88a4ec83dc810eb";
const String SERVER = "https://app-rastreo-backend-1.onrender.com";
const String URL = SERVER + "/api/tracker/";
const String number = "+525545464585";

const int LED = 2;

/* Certificado raíz HTTPS */
const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n" \
"VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n" \
"A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n" \
"WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n" \
"IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n" \
"AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n" \
"QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n" \
"HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n" \
"BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n" \
"9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n" \
"p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n" \
"-----END CERTIFICATE-----\n";

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

bool despertarServidor() {
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  HTTPClient http;

  http.begin(client, URL + "/despertar-servidor");
  int httpCode = http.GET();
  http.end();

  return httpCode > 0;
}

/* Enviar POST HTTPS */
bool enviarPostRequest(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no conectado");
    return false;
  }


  Serial.println("Enviando peticion a: " + URL);

  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
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
    return true;
  } else {
    Serial.println("Error en POST");
    http.end();
    return false;
  }
}

bool enviarEncendido(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no conectado");
    return false;
  }


  Serial.println("Enviando peticion a: " + URL + "/tracker-on");

  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  HTTPClient http;

  http.setTimeout(90000);
  http.begin(client, URL + "/tracker-on");
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
    return true;
  } else {
    Serial.println("Error en POST");
    http.end();
    return false;
  }
}

bool enviarRastreoActivo(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no conectado");
    return false;
  }


  Serial.println("Enviando peticion a: " + URL + "/rastreo-on");

  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  HTTPClient http;

  http.setTimeout(90000);
  http.begin(client, URL + "/rastreo-on");
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
    return true;
  } else {
    Serial.println("Error en POST");
    http.end();
    return false;
  }
}

/* Enviar GET HTTPS */
String checkServerEstatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no conectado");
    return "WiFi no conectado";
  }

  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  HTTPClient http;

  http.setTimeout(90000);
  // Construimos la URL con los parámetros necesarios
  String urlWithParams = URL + "/despertar-servidor";
  http.begin(client, urlWithParams);
  http.addHeader("Authorization", "Bearer " + TOKEN);

  Serial.println("Enviando GET a: " + urlWithParams);
  int httpCode = http.GET(); // Cambiamos a GET
  String response = String(httpCode) + " - Respuesta servidor: " + http.getString();
  Serial.println("HTTP Response: " + response);
  http.end();
  return response;
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
    // Datos de rastreo
    despertarServidor();
    delay(5000);
    const int maxRetries = 3;
    for (int i = 0; i < maxRetries; i++) {
      if (enviarPostRequest(message)) {
        SIM800L.println("AT+CMGD=" + String(index));
        delay(2000);
        break;
      }
      delay(10000); // espera 10 segundos antes de reintentar
    }
  } else if (message == "BORRAR*") {
    // Comando para limpiar buffer
    borrarTodosMensajes();
    enviarMensajeRecibido("Mensajes eliminados");
  } else if (message == "ESTATUS?") {
    // Comando para saber estatus del receptor
    SIM800L.println("AT");
    delay(500);
    enviarMensajeRecibido(SIM800L.readString());
  } else if (message == "SERVER?") {
    // Comando para revisar conexion con el servidor backend
    despertarServidor();
    delay(5000);
    String response = checkServerEstatus();
    delay(500);
    enviarMensajeRecibido(response);
  } else if (message.indexOf("Rastreador:") != -1) {
    despertarServidor();
    delay(5000);
    const int maxRetries = 3;
    for (int i = 0; i < maxRetries; i++) {
      if (enviarEncendido(message)) {
        SIM800L.println("AT+CMGD=" + String(index));
        delay(2000);
        break;
      }
      delay(10000); // espera 10 segundos antes de reintentar
    }
  } else if (message.indexOf("Rastreo Continuo ACTIVADO") != -1 || message.indexOf("Rastreo Modo Ahorro ACTIVADO") != -1) {
    despertarServidor();
    delay(5000);
    const int maxRetries = 3;
    for (int i = 0; i < maxRetries; i++) {
      if (enviarRastreoActivo(message)) {
        SIM800L.println("AT+CMGD=" + String(index));
        delay(2000);
        break;
      }
      delay(10000); // espera 10 segundos antes de reintentar
    }
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

  checkServerEstatus();
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
    digitalWrite(LED, HIGH);
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
          despertarServidor();
          delay(5000);
          const int maxRetries = 3;
          for (int i = 0; i < maxRetries; i++) {
            if (enviarPostRequest(message)) {
              enviarMensajeRecibido("Mensaje enviado al servidor");
              delay(2000);
              break;
            }else{
              enviarMensajeRecibido("Ocurrio un error al enviar al servidor");
            }
            delay(10000); // espera 10 segundos antes de reintentar
          }
  
          String payload = splitMessage(message);
          Serial.println("payload: " + payload);
          delay(2000);
          enviarMensajeRecibido(payload);
          //delay(3000);
          //enviarMensajeRecibido("Test de envío SIM800L");
      }else if (message.indexOf("Rastreador:") != -1) {
          despertarServidor();
          delay(5000);
          const int maxRetries = 3;
          for (int i = 0; i < maxRetries; i++) {
            if (enviarEncendido(message)) {
              enviarMensajeRecibido("Rastreador encendido. Mensaje enviado al servidor");
              delay(2000);
              break;
            }else{
              enviarMensajeRecibido("Ocurrio un error al enviar al servidor");
            }
            delay(10000); // espera 10 segundos antes de reintentar
          }
  
          String payload = message;
          delay(2000);
          enviarMensajeRecibido("encendido: "+payload);
          //delay(3000);
          //enviarMensajeRecibido("Test de envío SIM800L");
      } else if (message.indexOf("Rastreo Continuo ACTIVADO") != -1 || message.indexOf("Rastreo Modo Ahorro ACTIVADO") != -1) {
          despertarServidor();
          delay(5000);
          const int maxRetries = 3;
          for (int i = 0; i < maxRetries; i++) {
            if (enviarRastreoActivo(message)) {
              enviarMensajeRecibido("Rastreador encendido. Mensaje enviado al servidor");
              delay(2000);
              break;
            }else{
              enviarMensajeRecibido("Ocurrio un error al enviar al servidor");
            }
            delay(10000); // espera 10 segundos antes de reintentar
          }
  
          String payload = message;
          delay(2000);
          enviarMensajeRecibido("encendido: "+payload);
          //delay(3000);
          //enviarMensajeRecibido("Test de envío SIM800L");
      }else if (message.indexOf("BORRAR*") != -1) {
        Serial.println("Solicitando borrar mensajes en buffer");
        borrarTodosMensajes();
        enviarMensajeRecibido("Mensajes eliminados");
      } else if (message.indexOf("ESTATUS?") != -1) {
        Serial.println("Solicitando estatus de receptor");
        SIM800L.println("AT");
        delay(500);
        enviarMensajeRecibido(SIM800L.readString());
      } else if (message.indexOf("SERVER?")) {
        despertarServidor();
        delay(5000);
        String response = checkServerEstatus();
        delay(500);
        enviarMensajeRecibido(response);
      } else {
        Serial.println("Mensaje no reconocido: " + message);
      }
      digitalWrite(LED, LOW);
    }
  } else {
    digitalWrite(LED, LOW);
  }
}
