/* Declaracion de librerias */
#include <HardwareSerial.h> // Librería para comunicación serial
#include <WiFi.h> // Librería para conexión WiFi
#include <HTTPClient.h> // Librería para hacer peticiones HTTP
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <NetworkClientSecure.h>
#include <pgmspace.h>


// Definir el puerto serial SIM800L
HardwareSerial SIM800L(1);

// Datos de la red WiFi
const char* SSID = "XXXXX";
const char* PASSWORD = "XXXXXXXXXXXXXXXXXXXXX";
const String TOKEN = "XXXXXXXXXXXXXXXXXXXX";
/* Declaracion de puertos del ESP */
const int LED = 2;
const int READ_BTN = 4;

// Servidor al cual se hará la petición POST
//const String SERVER = "http://192.168.0.4:5322";
const String SERVER = "https://app-rastreo-backend.onrender.com";
//const String URL = SERVER + "/api/tracker/post-test";
const String URL = SERVER + "/api/tracker/upload-data";

int _timeout;
String _buffer;

//const char rootCACertificate[] PROGMEM = R"EOF(
//-----BEGIN CERTIFICATE-----
//MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD
//VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG
//A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw
//WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz
//IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi
//AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi
//QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR
//HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW
//BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D
//9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8
//p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD
//-----END CERTIFICATE-----
//)EOF";

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

// Objeto WiFiClient para manejar la conexión
// WiFiClient client;
// WiFiClientSecure client; // Cliente seguro para HTTPS

void setup() {
  Serial.begin(9600); // Serial para debug
  SIM800L.begin(9600, SERIAL_8N1, 16, 17);
  // Configurar el pin del LED como salida
  //pinMode(READ_BTN, INPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  // Conectar a la red WiFi
  Serial.println("Conectando a WiFi...");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando...");
  }
  Serial.println("Conectado a WiFi.");
  IPAddress ip = WiFi.localIP();
  Serial.println("IP: " + ip);
  // client.setCACert(test_root_ca);
  Serial.println("----------------");
  // Configuración del SIM800L
  SIM800L.println("AT");
  delay(100);
  SIM800L.println("AT+CMGF=1"); // Modo texto
  delay(100);
  // Leer el mensaje
  SIM800L.println("AT+CMGR=1"); // Leer el primer mensaje
  delay(1000); // Esperar respuesta
  borrarTodosMensajes();
  digitalWrite(LED, LOW);
}
void loop() {
  if (Serial.available() > 0) {
    String message = Serial.readString();
    Serial.println("Ahol: " + message);
    if(message == "r"){
      RecieveMessage();
    }else{
      enviarMensaje(Serial.readString());
    }
  }
  if (SIM800L.available()) {
    Serial.println("Llegó algo");
    //String message = SIM800L.readString();
    String message = "";
    char incomingChar;
    // Bucle para leer todos los caracteres disponibles
    while (SIM800L.available()) {
      incomingChar = SIM800L.read(); // Leer carácter por carácter
      message += incomingChar; // Agregar carácter al mensaje
      delay(10); // Pequeña pausa para asegurar la lectura completa
    }
    message = cleanString(message);
    Serial.println("Leido: " + message);
    int index = validarFormatoCMTI(message);
    leerMensaje(index);
  }
}

String cleanString(String message) {
  message.trim();
  message.replace("\n","");
  message.replace("\r","");
  message.replace("\"", "'");
  return message;
}

bool enviarMensaje(String message) {
  digitalWrite(LED, HIGH); // Encender LED para indicar que hay un mensaje que se va a enviar
  message = cleanString(message);
  Serial.println("Mensaje enviado: " + message); // Imprimir mensaje en el puerto serie para verificación
  bool isTrue = sendPostRequest(message); // Enviar el mensaje por HTTP POST
  digitalWrite(LED, LOW); // Apagar el LED después de enviar el mensaje
  return isTrue;
}

bool sendPostRequest(String message) {
  // NetworkClientSecure *client = new NetworkClientSecure;
  bool answer = false;
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure *client = new WiFiClientSecure;
    // set secure client with certificate
    client->setCACert(rootCACertificate);
    HTTPClient http;
    Serial.println("Enviando mensaje");
    // Iniciar la conexión con el objeto WiFiClient y la URL
    http.begin(*client, URL);
    http.addHeader("Content-Type", "application/json"); // Header de la petición
    http.addHeader("Host", "app-rastreo-backend.onrender.com"); // Cabecera HOST
    http.addHeader("Authorization", "Bearer " + TOKEN); // Añadir el token de au
    // String jsonData = "{\"datos\":\"" + message + "\"}";
    // jsonData = 
    // Crear un documento JSON
    StaticJsonDocument<256> doc;

    // Cadena con formato correcto usando comillas dobles escapadas
    String datos = message;
    
    // Agregar el campo "datos" al JSON
    doc["datos"] = datos;

    // Convertir a string JSON
    String jsonString;
    serializeJson(doc, jsonString);

    // Mostrar el JSON generado
    Serial.println("json: " + jsonString);
    int httpResponseCode = http.POST(jsonString);
    Serial.print("Codigo:");
    Serial.println(httpResponseCode);
    if (httpResponseCode > 0) {
      String response = http.getString(); // Obtener respuesta del servidor
      Serial.println("Respuesta del servidor: " + response);
      answer = true;
    } else {
      Serial.println("Error en la petición POST");
      Serial.println(http.getString());
    }
    http.end(); // Terminar la conexión
    delay(2000);
  } else {
    Serial.println("Error de conexión WiFi");
  }
  return answer;
}

void RecieveMessage() { 
  Serial.println("\n......................"); 
  Serial.println("Leyendo mensajes....."); 
  SIM800L.println("AT+CMGF=1"); 
  delay(500); 
  SIM800L.println("AT+CNMI=1,2,0,0,0"); // AT Command to receive a live SMS 
  delay(500); 
  Serial.println("Unread Message done"); 
  Serial.println("......................\n"); 
}

String _readSerial() { 
  Serial.println("Leyendo buffer"); 
  _timeout = 0; 
  while (!SIM800L.available() && _timeout < 12000) { 
    if(SIM800L.available())Serial.println("Hy algo"); 
    delay(13); 
    _timeout++; 
    Serial.println("."); 
  }
  if (SIM800L.available()) { 
    return SIM800L.readString(); 
  } 
  else { 
    return ""; // Agrega un valor de retorno predeterminado aquí 
  } 
}

void leerMensaje(const int &index) { 
  Serial.println("Leyendo mensaje en la ubicación: " + String(index)); // Enviar comando AT para leer el mensaje 
  SIM800L.println("AT+CMGR=" + String(index)); 
  delay(100); // Esperar respuesta del módulo 
  String message = SIM800L.readString(); // Mostrar la respuesta completa 
  Serial.println("Respuesta hexadecimal del módulo:"); 
  Serial.println(message); 
  bool isTrue = enviarMensaje(message); 
  if(isTrue){ borrar1Mensaje(index); 
  } 
}

int validarFormatoCMTI(String message) { 
  message.replace("+CMTI: 'SM',",""); 
  Serial.println("Mensaje extraido: "+message); 
  return message.toInt(); 
}

void borrar1Mensaje(int index){ // Borra el mensaje en la posición 1 
  Serial.println("Borrando mensaje: AT+CMGD="+index); 
  SIM800L.println("AT+CMGD="+index); // Borrar el primer mensaje 
  delay(1000); // Esperar respuesta 
  while (SIM800L.available()) { 
    String response = SIM800L.readString(); 
    Serial.println(response); // Imprimir la respuesta del módulo 
  }
  Serial.println("Mensaje borrado"); 
}

void borrarTodosMensajes(){ 
  Serial.println("Borrando todos los mensajes"); // Para borrar todos los mensajes 
  SIM800L.println("AT+CMGD=0"); // Borra todos los mensajes 
  delay(1000); // Esperar respuesta 
  SIM800L.println("AT+CMGDA=\"DEL ALL\""); // Borra todos los mensajes
  delay(1000);
  SIM800L.println("AT+CMGDA=\"DEL UNREAD\""); // Borra todos los mensajes
  delay(1000);
  while (SIM800L.available()) { 
    String response = SIM800L.readString(); 
    Serial.println(response); // Imprimir la respuesta del módulo 
  }
  Serial.println("Mensajes borrados"); 
}

void encenderLed(){
  digitalWrite(LED, HIGH);
  delay(2000);
  digitalWrite(LED, LOW);
  delay(2000);
  digitalWrite(LED, HIGH);
  delay(2000);
  digitalWrite(LED, LOW);
}
