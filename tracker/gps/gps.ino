#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// Configuración de los pines para el GPS y creación del objeto GPS
SoftwareSerial ss(4, 3); // RX, TX
TinyGPSPlus gps;

String leerGPS() {
  String datosGPS = "";

  // Esperar hasta que el GPS tenga datos disponibles
  while (ss.available() > 0) {
    gps.encode(ss.read());
  }

  if (gps.location.isUpdated()) {
    // Extraer datos relevantes
    float latitud = gps.location.lat();
    float longitud = gps.location.lng();
    float altitud = gps.altitude.meters();
    int satelites = gps.satellites.value();
    int hora = gps.time.hour();
    int minuto = gps.time.minute();
    int segundo = gps.time.second();

    // Formatear los datos como string
    datosGPS = "Hora: " + String(hora) + ":" + String(minuto) + ":" + String(segundo) +
               " | Latitud: " + String(latitud, 6) +
               " | Longitud: " + String(longitud, 6) +
               " | Altitud: " + String(altitud) + " m" +
               " | Satélites: " + String(satelites);
  }

  return datosGPS;
}

void setup() {
  // Inicializar la comunicación serie
  Serial.begin(9600);
  ss.begin(9600);
}

void loop() {
  String datosGPS = leerGPS();
  if (datosGPS != "") {
    // Imprimir los datos en la consola serie
    Serial.println(datosGPS);
  }
  delay(1000); // Esperar 1 segundo antes de leer de nuevo
}
