#include <TinyGPS.h> // Librería TinyGPS
#include <TinyGPSPlus.h> // Librería TinyGPS++

TinyGPSPlus gps1; // Objeto GPS

TinyGPS gps; // Objeto GPS

float latitude, longitude;
int year;
byte month, day, hour, minute, second, hundredths;
unsigned long chars;
unsigned short sentences, failed_checksum;

float latitude1, longitude1;
int year1;
byte month1, day1, hour1, minute1, second1, hundredths1;
unsigned long chars1;
unsigned short sentences1, failed_checksum1;


void setup()
{
  Serial.begin(9600);       // Comunicación con el monitor serial
  Serial2.begin(9600);      // GPS (TX2 = 16, RX2 = 17)
  Serial1.begin(9600);
  Serial.println("Inicializando GPS...");
}

void loop()
{
  while (Serial1.available()) {
    char c = Serial1.read(); // Leer carácter del GPS
    gps1.encode(c);           // Procesar el carácter
    
    if (gps1.location.isValid()) { // Si hay una ubicación válida
      Serial.println(".......... Leyendo NEO-M8N ..............");

      Serial.print("📍 Latitud: ");
      Serial.println(gps1.location.lat(), 6);
      Serial.print("📍 Longitud: ");
      Serial.println(gps1.location.lng(), 6);

      Serial.print("📅 Fecha: ");
      Serial.print(gps1.date.day());
      Serial.print("/");
      Serial.print(gps1.date.month());
      Serial.print("/");
      Serial.println(gps1.date.year());

      Serial.print("🕒 Hora (UTC): ");
      Serial.print(gps1.time.hour());
      Serial.print(":");
      Serial.print(gps1.time.minute());
      Serial.print(":");
      Serial.println(gps1.time.second());

      Serial.print("📡 Satélites conectados: ");
      Serial.println(gps1.satellites.value());

      Serial.print("🚗 Velocidad (km/h): ");
      Serial.println(gps1.speed.kmph());

      Serial.print("🧭 Rumbo (grados): ");
      Serial.println(gps1.course.deg());

      Serial.println("----------------------------------");
    }

  }

  while (Serial2.available()) {
    char c = Serial2.read();     // Leer caracter del GPS
    if (gps.encode(c)) {         // Si se recibió una sentencia completa y válida
      Serial.println(".......... Leyendo NEO-6M ..............");
      gps.f_get_position(&latitude, &longitude);

      if (latitude == 1000.0 && longitude == 1000.0) {
        Serial.println("⛔ No hay señal GPS todavía. Esperando...");
        return;
      }

      // Información disponible: mostrarla
      gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths);

      Serial.println("✅ Señal GPS detectada:");
      Serial.print("📍 Latitud: ");
      Serial.println(latitude, 6);
      Serial.print("📍 Longitud: ");
      Serial.println(longitude, 6);

      Serial.print("📅 Fecha: ");
      Serial.print(day);
      Serial.print("/");
      Serial.print(month);
      Serial.print("/");
      Serial.println(year);

      Serial.print("🕒 Hora (UTC): ");
      Serial.print(hour);
      Serial.print(":");
      Serial.print(minute);
      Serial.print(":");
      Serial.println(second);

      Serial.print("📡 Satélites conectados: ");
      Serial.println(gps.satellites());

      Serial.print("🚗 Velocidad (km/h): ");
      Serial.println(gps.f_speed_kmph());

      Serial.print("🧭 Rumbo (grados): ");
      Serial.println(gps.f_course());

      Serial.println("----------------------------------");
    }
  }

  delay(500); // Esperar un poco antes del siguiente chequeo
}
