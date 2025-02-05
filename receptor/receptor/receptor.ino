#include <SoftwareSerial.h> // Librería para comunicación serial
#include <ESP8266WiFi.h> // Librería para conexión WiFi
#include <ESP8266HTTPClient.h> // Librería para hacer peticiones HTTP
SoftwareSerial sim(D3,D2);
// Datos de la red WiFi
const char* ssid = "WIFI";
const char* password = "PASSWORD";
// Pin para el LED (indicador de mensaje)
const int ledPin = D8;
const int pushB = D6;
const int pushS = D7;
const int pushR = D4;
// Servidor al cual se hará la petición POST
const String server = "http://192.168.0.5:5322";
const String url = server + "/api/tracker/post-test";
int _timeout;
String _buffer;
const String number = "+525545464585";
//const String number = "+525554743913";
// Objeto WiFiClient para manejar la conexión
WiFiClient client;
void setup() {
Serial.begin(9600); // Serial para debug
sim.begin(9600);
// Configurar el pin del LED como salida
pinMode(pushB, INPUT);
pinMode(pushS, INPUT);
pinMode(pushR, INPUT);
pinMode(ledPin, OUTPUT);
digitalWrite(ledPin, HIGH);
// Conectar a la red WiFi
Serial.println("Conectando a WiFi...");
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(1000);
Serial.println("Conectando...");
}
Serial.println("Conectado a WiFi.");
Serial.println("----------------");
// Configuración del SIM800L
sim.println("AT");
delay(100);
sim.println("AT+CMGF=1"); // Modo texto
delay(100);
// Leer el mensaje
sim.println("AT+CMGR=1"); // Leer el primer mensaje
delay(1000); // Esperar respuesta
borrarTodosMensajes();
digitalWrite(ledPin, LOW);
}
void loop() {
if (digitalRead(pushB) == 1) {
enviarMensaje("Enviando informacion de los rastreadores al servidor");
}
if (digitalRead(pushR) == 1) {
Serial.println("lEYENDO mensajeS SMS a prueba");
RecieveMessage();
}
if (Serial.available() > 0) {
String message = Serial.readString();
Serial.println("Ahol: " + message);
if(message == "r"){
RecieveMessage();
}else{
  enviarMensaje(Serial.readString());
}
}
if (sim.available()) {
Serial.println("Llegó algo");
//String message = sim.readString();
String message = "";
char incomingChar;
// Bucle para leer todos los caracteres disponibles
while (sim.available()) {
incomingChar = sim.read(); // Leer carácter por carácter
message += incomingChar; // Agregar carácter al mensaje
delay(10); // Pequeña pausa para asegurar la lectura completa
}
message.trim(); // Construir el mensaje a partir de la lectura del puerto serial
message.replace("\n","");
message.replace("\r","");
message.replace("\"", "'");
Serial.println("Leido: " + message);
int index = validarFormatoCMTI(message);
leerMensaje(index);
}
}
bool enviarMensaje(String message) {
digitalWrite(ledPin, HIGH); // Encender LED para indicar que hay un mensaje que se va a enviar
message.trim(); // Construir el mensaje a partir de la lectura del puerto serial
message.replace("\n","");
message.replace("\r","");
message.replace("\"", "'");
Serial.println("Mensaje enviado: " + message); // Imprimir mensaje en el puerto serie para verificación
bool isTrue = sendPostRequest(message); // Enviar el mensaje por HTTP POST
digitalWrite(ledPin, LOW); // Apagar el LED después de enviar el mensaje
return isTrue;
}
bool sendPostRequest(String message) {
bool answer = false;
if (WiFi.status() == WL_CONNECTED) {
HTTPClient http;
Serial.println("Enviando mensaje");
// Iniciar la conexión con el objeto WiFiClient y la URL
http.begin(client, url);
http.addHeader("Content-Type", "application/json"); // Header de la petición
String jsonData = "{\"datos\":\"" + message + "\"}";
Serial.println("json: " + jsonData);
int httpResponseCode = http.POST(jsonData);
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
  sim.println("AT+CMGF=1"); 
  delay(500); 
  sim.println("AT+CNMI=1,2,0,0,0"); // AT Command to receive a live SMS 
  delay(500); 
  Serial.println("Unread Message done"); 
  Serial.println("......................\n"); 
  }
  
  String _readSerial() { 
    Serial.println("Leyendo buffer"); 
    _timeout = 0; 
    while (!sim.available() && _timeout < 12000) { 
      if(sim.available())Serial.println("Hy algo"); 
      delay(13); 
      _timeout++; 
      Serial.println("."); 
      }
      if (sim.available()) { 
        return sim.readString(); 
        } 
        else { 
          return ""; // Agrega un valor de retorno predeterminado aquí 
          } 
          }
          
          void leerMensaje(const int &index) { 
            Serial.println("Leyendo mensaje en la ubicación: " + String(index)); // Enviar comando AT para leer el mensaje 
            sim.println("AT+CMGR=" + String(index)); 
            delay(100); // Esperar respuesta del módulo 
            String message = sim.readString(); // Mostrar la respuesta completa 
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
              sim.println("AT+CMGD="+index); // Borrar el primer mensaje 
              delay(1000); // Esperar respuesta 
              while (sim.available()) { 
                String response = sim.readString(); 
                Serial.println(response); // Imprimir la respuesta del módulo 
                }
                Serial.println("Mensaje borrado"); 
                }void borrarTodosMensajes(){ 
                  Serial.println("Borrando todos los mensajes"); // Para borrar todos los mensajes 
                  sim.println("AT+CMGD=0"); // Borra todos los mensajes 
                  delay(1000); // Esperar respuesta 
                  while (sim.available()) { 
                    String response = sim.readString(); 
                    Serial.println(response); // Imprimir la respuesta del módulo 
                    
                    }Serial.println("Mensajes borrados"); 
                    }