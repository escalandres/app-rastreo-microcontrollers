#include <TinyGPS.h> // Incluimos TinyGPS

TinyGPS gps; // Declaramos el objeto gps

#define PUSH_BTN 9
int contador = 0;

const String number = "+525620577634";

float latitude, longitude;
// Declaramos las variables para la obtención de datos
int year;
byte month, day, hour, minute, second, hundredths;
unsigned long chars;
unsigned short sentences, failed_checksum;

void setup()
{
  Serial.begin(9600);       // Serial principal (PC)
  Serial1.begin(115200);    // SIM800L (TX1=18, RX1=19)
  Serial2.begin(9600);      // GPS (TX2=16, RX2=17)

  pinMode(PUSH_BTN, INPUT);

  // Imprimimos información inicial
  Serial.println("");
  Serial.println("GPS GY-GPS6MV2 Leantec");
  Serial.println(" ---Buscando senal--- ");
  Serial.println("");
}

void loop()
{
  while (Serial2.available()) // Leer datos del GPS
  {
    int c = Serial2.read();

    if (gps.encode(c)) // Si se recibe una sentencia válida
    {
      gps.f_get_position(&latitude, &longitude);
      Serial.print("Latitud/Longitud: ");
      Serial.print(latitude, 5);
      Serial.print(", ");
      Serial.println(longitude, 5);

      gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths);
      Serial.print("Fecha: ");
      Serial.print(day, DEC);
      Serial.print("/");
      Serial.print(month, DEC);
      Serial.print("/");
      Serial.print(year);
      Serial.print(" Hora: ");
      Serial.print(hour, DEC);
      Serial.print(":");
      Serial.print(minute, DEC);
      Serial.print(":");
      Serial.print(second, DEC);
      Serial.print(".");
      Serial.println(hundredths, DEC);
      Serial.print("Altitud (metros): ");
      Serial.println(gps.f_altitude());
      Serial.print("Rumbo (grados): ");
      Serial.println(gps.f_course());
      Serial.print("Velocidad (km/h): ");
      Serial.println(gps.f_speed_kmph());
      Serial.print("Satelites: ");
      Serial.println(gps.satellites());
      Serial.println();
      gps.stats(&chars, &sentences, &failed_checksum);
    }
  }

  if (digitalRead(PUSH_BTN) == HIGH && (contador == 0))
  {
    Serial.println("Configurando SIM800L");
    Serial.println("Enviando SMS...");

    // Configuración del SIM800L
    Serial1.println("AT+CMGF=1"); // Configura el SIM800L en modo texto
    delay(200);

    Serial1.println("AT+CMGS=\"" + number + "\"\r"); // Número de teléfono
    delay(200);

    // Formatear mensaje SMS
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "\"lat\":\"%.6f\",\"lon\":\"%.6f\"", latitude, longitude);
    String SMS = String(buffer);

    Serial1.println(SMS); // Envía el mensaje
    delay(100);
    Serial1.write((char)26); // Código ASCII de CTRL+Z
    delay(200);
    Serial.println("Mensaje Enviado!");
  }
}
