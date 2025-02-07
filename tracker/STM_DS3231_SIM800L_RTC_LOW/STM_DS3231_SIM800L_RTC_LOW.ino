/* Declaracion de librerias */
#include <Wire.h>
#include <HardwareSerial.h>
#include <RTClib.h>
#include <ArduinoJson.h>
#include <low_power.h>
#include <STM32LowPower.h>

volatile bool alarmFired = false;
RTC_DS3231 rtc;

/* Declaracion de puertos del STM32F103C8T6 */
const int SQW_PIN = PA0;
const int STM_LED = PC13;
const int LED = PA6;
const int PUSH_BTN = PB0;

/* Constantes y Variables Globales */
const String ID = "48273619";
int _timeout;
String _buffer;
//String number = "+525620577634";
String number = "+525554743913";
// Estructura para representar los límites geográficos de cada estado
struct Estado {
  float latMin, latMax;
  float lonMin, lonMax;
};

// Definir los límites de los estados del centro de México
Estado estadosCentroMexico[] = {
  // CDMX
  {19.27, 19.46, -99.17, -98.88},  
  // Estado de México
  {18.83, 19.75, -99.92, -98.94},  
  // Hidalgo
  {19.48, 21.14, -99.68, -98.56},
  // Puebla
  {18.05, 19.64, -98.49, -97.06},
  // Morelos
  {18.25, 19.40, -99.53, -98.72},
  // Tlaxcala
  {19.20, 19.62, -98.25, -97.75},
  // Querétaro
  {20.19, 21.27, -100.64, -99.63}
};

// Definir el puerto serial SIM800L
HardwareSerial SIM800L(PA3, PA2);


void setAlarmFired() {
  alarmFired = true;
}

void configureAlarm(){
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  // turn off alarm 2 (in case it isn't off already)
  // again, this isn't done at reboot, so a previously set alarm could easily go overlooked
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);
  
  // stop oscillating signals at SQW Pin
  // otherwise setAlarm1 will fail
  //rtc.writeSqwPinMode(DS3231_OFF);

  //Set Alarm to be trigged in X 
  rtc.setAlarm1( rtc.now() + TimeSpan(50), DS3231_A1_Second); // this mode triggers the alarm when the seconds match.

  alarmFired = false;
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  // Iniciar el puerto serial a una velocidad de 9600 baudios
  Serial.begin(9600);
  _buffer.reserve(50);
  SIM800L.begin(9600);
  
  /* COnfiguracion de puertos */
  pinMode(SQW_PIN, INPUT_PULLUP);
  pinMode(PUSH_BTN, INPUT);
  pinMode(STM_LED, OUTPUT);
  pinMode(LED, OUTPUT);

  digitalWrite(STM_LED, LOW);
  digitalWrite(LED, HIGH);

  if (! rtc.begin()) {        // si falla la inicializacion del modulo
    Serial.println("Modulo RTC no encontrado !");  // muestra mensaje de error
    while (1);         // bucle infinito que detiene ejecucion del programa
  }

  if(rtc.lostPower()) {
        // this will adjust to the date and time at compilation
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

   
    rtc.adjust(DateTime(__DATE__, __TIME__));  // funcion que permite establecer fecha y horario
              // al momento de la compilacion. Comentar esta linea
             // y volver a subir para normal operacion

    //rtc.adjust(DateTime(2025, 1, 31, 16, 30, 0));  // año, mes, día, hora, minuto, segundo
    rtc.disable32K();
    
    // stop oscillating signals at SQW Pin
    // otherwise setAlarm1 will fail
    rtc.writeSqwPinMode(DS3231_OFF);

    //Set Alarm to be trigged in X 
    //rtc.setAlarm1( rtc.now() + TimeSpan(50), DS3231_A1_Second); // this mode triggers the alarm when the seconds match.
    configureAlarm();

    //Create Trigger
    //attachInterrupt(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING);

    delay(2000);
    digitalWrite(STM_LED,HIGH);
    digitalWrite(LED,LOW);

    // Configure low power
    LowPower.begin();
    // Attach a wakeup interrupt on pin, calling repetitionsIncrease when the device is woken up
    // Last parameter (LowPowerMode) should match with the low power state used
    //LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, IDLE_MODE);
    //LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, SLEEP_MODE);
    LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, DEEP_SLEEP_MODE);
    //LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, SHUTDOWN_MODE);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(digitalRead(PUSH_BTN) == HIGH){
    SendMessage();
  }

  if(alarmFired){
    SendMessage();
    configureAlarm();
    //LowPower.idle();
    //LowPower.sleep();
    LowPower.deepSleep();
    //LowPower.shutdown();
  }
  if (SIM800L.available() > 0){
    Serial.write(SIM800L.read());
  }
}


void SendMessage()
{
  digitalWrite(STM_LED, LOW);
  digitalWrite(LED, HIGH);
  
  //Serial.println ("Sending Message");
  SIM800L.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
  delay(200);
  //Serial.println ("Set SMS Number");
  SIM800L.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
  delay(200);
  String SMS = createMeesageToSend();
  SIM800L.println(SMS);
  delay(100);
  SIM800L.println((char)26);// ASCII code of CTRL+Z
  delay(200);
  _buffer = _readSerial();

  delay(2000);
  digitalWrite(STM_LED,HIGH);
  digitalWrite(LED,LOW);
}
void RecieveMessage()
{
  Serial.println ("SIM800L Read an SMS");
  SIM800L.println("AT+CMGF=1");
  delay (200);
  SIM800L.println("AT+CNMI=1,2,0,0,0"); // AT Command to receive a live SMS
  delay(200);
  Serial.write ("Unread Message done");
}
String _readSerial() {
  _timeout = 0;
  while  (!SIM800L.available() && _timeout < 12000  )
  {
    delay(13);
    _timeout++;
  }
  if (SIM800L.available()) {
    return SIM800L.readString();
  }
}


String createMeesageToSend(){
  //Crear la semilla para el numero aleatorio
  randomSeed(millis());

  // Elegir un estado al azar de los definidos (por ejemplo, 0 = CDMX)
  int estadoIndex = random(0, 7);  // De 0 a 6 (7 estados)
  
  // Variables para almacenar las coordenadas
  float lat, lon;
  
  // Generar las coordenadas aleatorias para el estado elegido
  generarCoordenadasAleatorias(estadoIndex, &lat, &lon);

  DateTime now = rtc.now();
  //char format[20] = "yyyy-MM-dd hh:mm:ss";
  //String currentTime = _now.toString(format);
  //String currentTime = _now.toString(format);
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", 
          now.year(), now.month(), now.day(), 
          now.hour(), now.minute(), now.second());
  
  String currentTime = String(buffer);


  // Crear un documento JSON
  StaticJsonDocument<200> doc;

  // Rellenar los valores
  doc["id"] = ID;
  doc["lat"] = lat;
  doc["lon"] = lon;
  doc["time"] = currentTime;
 
  // Serializar el documento JSON a un string
  String output;
  serializeJson(doc, output);
  
  return output;
}

// Función para generar coordenadas aleatorias dentro de un estado específico
void generarCoordenadasAleatorias(int estadoIndex, float* lat, float* lon) {
  Estado estado = estadosCentroMexico[estadoIndex];
  
  // Generar latitud y longitud aleatorias dentro del rango del estado
  *lat = random(estado.latMin * 100, estado.latMax * 100) / 100.0;
  *lon = random(estado.lonMin * 100, estado.lonMax * 100) / 100.0;
}
