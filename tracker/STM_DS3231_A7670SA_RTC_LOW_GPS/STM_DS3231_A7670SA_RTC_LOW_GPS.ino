/* Declaracion de librerias */
#include <Wire.h>
#include <HardwareSerial.h>
#include <RTClib.h>
#include <low_power.h>
#include <STM32LowPower.h>
#include <TinyGPSPlus.h>
#include <EEPROM.h>
#include <IWatchdog.h>

/* Declaracion de variables globales */
volatile bool alarmFired = false;
RTC_DS3231 rtc; // Objeto Reloj de precision RTC
TinyGPSPlus gps1; // Objeto GPS

/* Declaracion de puertos del STM32F103C8T6 */
const int SLEEP_PIN = PB1;
const int SQW_PIN = PB0;
const int STM_LED = PC13;
const int BATERIA = PA0;
bool rtc_sano = false;
bool resetPorWatchdog = false;

/* Constantes y Variables Globales */
struct HoraRedISO {
  DateTime local;
  DateTime utc;
  String localISO;
  String utcISO;
  bool ok;
};

enum ModoOperacion : uint8_t {
  MODO_OFF = 0,
  MODO_AHORRO = 1,
  MODO_CONTINUO = 2
};

// struct Config {
//   uint32_t firma;           // ← debe ser 0xCAFEBABE
//   int idRastreador;         // ID unico del rastreador
//   char receptor[16];           // Numero de telefono del receptoristrador
//   char numUsuario[16];      // Numero de usuario que recibe los SMS;
//   int intervaloSegundos;    // Intervalo de envio de datos en segundos
//   int intervaloMinutos;     // Intervalo de envio de datos en minutos
//   int intervaloHoras;       // Intervalo de envio de datos en horas
//   int intervaloDias;        // Intervalo de envio de datos en dias
//   bool modoAhorro;          // Modo ahorro de energia (true/false) 
//   char pin[8];              // PIN para aceptar comandos SMS
//   bool configurado;         // Indica si el rastreador ha sido configurado
//   bool rastreoActivo;       // Indica si el rastreo está activo o no
// };

struct Config {
  uint32_t firma;           // ← debe ser 0xCAFEBABE
  uint16_t idRastreador;         // ID unico del rastreador
  char receptor[16];           // Numero de telefono del receptoristrador
  char numUsuario[16];      // Numero de usuario que recibe los SMS;
  uint16_t intervaloSegundos;    // Intervalo de envio de datos en segundos
  uint16_t intervaloMinutos;     // Intervalo de envio de datos en minutos
  uint16_t intervaloHoras;       // Intervalo de envio de datos en horas
  uint16_t intervaloDias;        // Intervalo de envio de datos en dias
  ModoOperacion modo;          // OFF / AHORRO / CONTINUO
  char pin[8];              // PIN para aceptar comandos SMS
  bool configurado;         // Indica si el rastreador ha sido configurado
};

Config config;
// Dirección en EEPROM para guardar la configuración
const uint16_t CONFIG_ADDRESS = 0;

String latitude, longitude;

int _timeout;
String _buffer;
String rxBuffer = "";

// Definir el puerto serial A7670SA
HardwareSerial A7670SA(PA3, PA2);
HardwareSerial NEO8M(PA10, PA9);

void encenderLED() {
    digitalWrite(STM_LED, LOW);
}

void apagarLED() {
    digitalWrite(STM_LED, HIGH);
}

// -------- Funciones de EEPROM --------

/* Función para guardar configuración en EEPROM */
void guardarConfigEEPROM() {
  config.firma = 0xCAFEBABE;  // Marca de validación
  EEPROM.put(CONFIG_ADDRESS, config);
  // NO necesitas commit() con EEPROM.h estándar
}

/* Función para leer configuración desde EEPROM */
bool leerConfigEEPROM() {
  Config tempConfig;
  EEPROM.get(CONFIG_ADDRESS, tempConfig);
  
  // Verificar si la configuración es válida
  if (tempConfig.firma == 0xCAFEBABE && tempConfig.configurado) {
    config = tempConfig;
    return true;
  }
  return false;
}

/* Función para cargar valores por defecto */
void cargarConfiguracionPorDefecto() {
  config.firma = 0xCAFEBABE;
  config.idRastreador = 48273619;
  strcpy(config.receptor, "+525620577634");
  strcpy(config.numUsuario, "");
  config.intervaloSegundos = 0;
  config.intervaloMinutos = 5;
  config.intervaloHoras = 0;
  config.intervaloDias = 0;
  config.modoAhorro = false;
  strcpy(config.pin, "589649");
  config.configurado = false;  // Marcar como configurado
  config.rastreoActivo = false;
  
  guardarConfigEEPROM();
}

/* Función opcional para resetear EEPROM */
void resetearEEPROM() {
  // Útil para debugging o comando SMS
  config.firma = 0;
  config.configurado = false;
  EEPROM.put(CONFIG_ADDRESS, config);
  // Reiniciar el dispositivo después
}

// -------- Funciones de alarma RTC --------

void setAlarmFired() {
  alarmFired = true;
}

void configurarAlarma(int dias = 0, int horas = 0, int minutos = 5, int segundos = 0) {
  // Limpiar alarmas anteriores
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);

  // SQW en modo alarma (no onda cuadrada)
  rtc.writeSqwPinMode(DS3231_OFF);
  DateTime now = rtc.now();

  if(segundos > 0){
    DateTime nextAlarm = now + TimeSpan(0, 0, 0, segundos);

    //Set Alarm to be trigged in X seconds
    rtc.setAlarm1(nextAlarm, DS3231_A1_Second);  // this mode triggers the alarm when the seconds match.
  }else{
    DateTime nextAlarm = now + TimeSpan(dias, horas, minutos, 0);

    //Set Alarm to be trigged in X
    rtc.setAlarm2(nextAlarm, DS3231_A2_Minute);  // this mode triggers the alarm when the seconds match.
  }

  alarmFired = false;
}

// -------- Funciones de Bateria --------
float leerVoltaje(int pin) {
  const float R1 = 51000.0;
  const float R2 = 20000.0;
  const float Vref = 3.3;
  const float factorDivisor = (R1 + R2) / R2;  // ≈ 3.55

  // --- Lectura "dummy" obligatoria con divisores de alta impedancia ---
  analogRead(pin);  
  delayMicroseconds(200);  // pequeño delay para estabilizar

  // --- Promediar varias lecturas ---
  float suma = 0;
  for (int i = 0; i < 10; i++) {
    suma += analogRead(pin);
    delay(5);  // con divisores altos no conviene muestrear demasiado rápido
  }

  float lecturaADC = suma / 10.0;

  // Convertir a voltaje
  float voltajeADC = (lecturaADC / 4095.0) * Vref;
  float voltajeBateria = voltajeADC * factorDivisor;

  return voltajeBateria;
}

// int calcularNivelBateria(float v) {
//   if (v >= 4.15) return 100;
//   else if (v >= 4.12) return 98;
//   else if (v >= 4.10) return 95;
//   else if (v >= 4.07) return 92;
//   else if (v >= 4.04) return 90;
//   else if (v >= 4.00) return 85;
//   else if (v >= 3.96) return 80;
//   else if (v >= 3.92) return 75;
//   else if (v >= 3.88) return 70;
//   else if (v >= 3.84) return 65;
//   else if (v >= 3.80) return 60;
//   else if (v >= 3.76) return 55;
//   else if (v >= 3.72) return 50;
//   else if (v >= 3.68) return 45;
//   else if (v >= 3.64) return 40;
//   else if (v >= 3.60) return 35;
//   else if (v >= 3.55) return 25;
//   else if (v >= 3.50) return 15;
//   else if (v >= 3.45) return 10;
//   else if (v >= 3.40) return 5;
//   else if (v >= 3.35) return 3;
//   else if (v >= 3.30) return 1;
//   else return 0;
// }

int calcularNivelBateria(float v) {
  if (v >= 4.20) return 100;
  else if (v >= 3.70) {
      // entre 4.20 y 3.70 V → 100% a 50%
      return map(v * 100, 370, 420, 50, 100);
  } else if (v >= 3.40) {
      // entre 3.70 y 3.40 V → 50% a 10%
      return map(v * 100, 340, 370, 10, 50);
  } else {
      return 0;
  }
}

String obtenerVoltajeBateria() {
  float voltaje = leerVoltaje(BATERIA);
  // enviarSMS("Voltaje: " + String(voltaje));
  int nivelBateria = calcularNivelBateria(voltaje);
  String sms = "bat:" + String(nivelBateria);
  return sms;
}

// ---------- Funciones del A7670SA ----------

void enviarComando(const char* comando, int espera = 1000) {
  A7670SA.println(comando);
  delay(espera);
}

String enviarComandoConRetorno(const char* comando, unsigned long timeout = 1000) {
  // 1. Limpiar buffer antes de enviar
  while (A7670SA.available()) {
    A7670SA.read();
  }

  // 2. Enviar comando
  A7670SA.println(comando);

  // 3. Leer respuesta con timeout real
  String resp = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (A7670SA.available()) {
      char c = A7670SA.read();
      resp += c;
      start = millis(); // reinicia timeout si siguen llegando datos
    }
  }

  return resp;
}

void iniciarA7670SA() {
  enviarComando("AT", 500); // Probar comunicación AT

  enviarComando("AT+CTZU=1", 500); // ⚠️ ANTES de registrarse
  enviarComando("AT+CTZR=1", 500); // opcional (debug)

  enviarComando("AT+CNMP=2", 1000); // Establecer modo LTE
  // Confirmar nivel de señal y registro otra vez
  enviarComando("AT+CSQ", 500);
  enviarComando("AT+CREG?", 1000);
  enviarComando("AT+CMGF=1", 1000);
}


void dormirA7670SA() {
  digitalWrite(SLEEP_PIN, LOW);  // LOW despierta el módulo
  delay(300);
  enviarComando("AT+CSCLK=0");
  enviarComando("AT");
}

void despertarA7670SA() {
  enviarComando("AT+CSCLK=1");
  // digitalWrite(SLEEP_PIN, LOW);   // LOW despierta el módulo
  digitalWrite(SLEEP_PIN, HIGH);   // HIGH permite que el módulo entre en sleep
  delay(300);
}

void limpiarBufferA7670SA() {
  while (A7670SA.available()) {
    A7670SA.read();
  }
  delay(50);
}

String leerRespuestaA7670SA(unsigned long timeout = 2000) {
  String response = "";
  unsigned long startTime = millis();
  unsigned long ultimoCaracter = millis();
  const unsigned long TIEMPO_SIN_DATOS = 200; // Si no llegan datos por 100ms, asumir que terminó
  
  while (millis() - startTime < timeout) {
    while (A7670SA.available()) {
      char c = A7670SA.read();
      response += c;
      ultimoCaracter = millis(); // Actualizar tiempo del último carácter recibido
    }
    
    // Si han pasado 100ms sin recibir nada y ya hay respuesta, salir
    if (response.length() > 0 && (millis() - ultimoCaracter) > TIEMPO_SIN_DATOS) {
      break;
    }
    
    // Pequeña pausa para no saturar el CPU
    delay(10);
  }
  
  return response;
}

String _readSerial(unsigned long timeout = 5000) {
  String resp = "";
  unsigned long start = millis();

  while (millis() - start < timeout) {
    while (A7670SA.available()) {
      char c = A7670SA.read();
      resp += c;
      start = millis(); // reinicia timeout si siguen llegando datos
    }
  }

  return resp;
}

void flushA7670SA() {
    unsigned long startTime = millis();
    while (A7670SA.available()) {
        A7670SA.read();
        if (millis() - startTime > 500) { // Máximo 500 ms de espera
            break;
        }
    }
}

void enviarSMS(String SMS, String number = config.receptor)
{
  // Limpiar buffer antes de enviar
  limpiarBufferA7670SA();
  delay(1000);

  // Configurar modo texto
  enviarComando("AT+CMGF=1",500);  

  enviarComando(("AT+CMGS=\"" + number + "\"").c_str(), 2000); //Mobile phone number to send message

  A7670SA.println(SMS);
  delay(500);
  A7670SA.println((char)26);// ASCII code of CTRL+Z
  delay(500);
  _buffer = _readSerial();

  delay(1000);
}

bool estaRegistradoEnRed() {
    String resp = enviarComandoConRetorno("AT+CREG?", 2000);
    resp.trim();

    // Buscar la línea con +CREG:
    int idx = resp.indexOf("+CREG:");
    if (idx == -1) return false;

    // Si contiene ,1 (registrado en red local) o ,5 (roaming)
    if (resp.indexOf(",1", idx) != -1 || resp.indexOf(",5", idx) != -1) {
        return true;
    }
    return false;
}

int nivelSenal() {
    String resp = enviarComandoConRetorno("AT+CSQ", 2000);
    resp.trim();

    int idx = resp.indexOf("+CSQ:");
    if (idx == -1) return -1;

    int startIdx = resp.indexOf(":", idx) + 1;
    int endIdx   = resp.indexOf(",", startIdx);
    if (startIdx == 0 || endIdx == -1) return -1;

    return resp.substring(startIdx, endIdx).toInt();
}

HoraRedISO obtenerHoraRedISO(int fallbackTZQuarters = -24) {
  HoraRed out = { DateTime((uint32_t)0), DateTime((uint32_t)0), "", "", false };

  String resp = enviarComandoConRetorno("AT+CCLK?", 2000);

  int idx = resp.indexOf("+CCLK:");
  if (idx == -1) return out;

  int q1 = resp.indexOf('"', idx);
  int q2 = resp.indexOf('"', q1 + 1);
  if (q1 == -1 || q2 == -1) return out;

  String s = resp.substring(q1 + 1, q2); // Ej: "26/01/05,19:01:42-24"
  if (s.length() < 19) return out;

  // Parse estricto de campos
  int yy = s.substring(0, 2).toInt();
  int MM = s.substring(3, 5).toInt();
  int dd = s.substring(6, 8).toInt();
  int hh = s.substring(9, 11).toInt();
  int mm = s.substring(12, 14).toInt();
  int ss = s.substring(15, 17).toInt();

  // tz puede incluir signo; cuartos de hora
  int tz = s.substring(18).toInt(); // ej: -24
  int tzEffective = (tz == 0 ? fallbackTZQuarters : tz);

  int year = 2000 + yy;

  // Validaciones
  if (year < 2020 || year > 2035) return out;
  if (MM < 1 || MM > 12) return out;
  if (dd < 1 || dd > 31) return out;
  if (hh < 0 || hh > 23) return out;
  if (mm < 0 || mm > 59) return out;
  if (ss < 0 || ss > 59) return out;

  DateTime localTime(year, MM, dd, hh, mm, ss);

  // UTC = local - (tzEffective * 15 min)
  // int offsetSeconds = tzEffective * 15 * 60;
  // DateTime utcTime = localTime - TimeSpan(offsetSeconds);
  DateTime utcTime = localTime + TimeSpan(6 * 3600);

  char bufLocal[21];
  char bufUTC[21];
  sprintf(bufLocal, "%04d-%02d-%02dT%02d:%02d:%02d",
          localTime.year(), localTime.month(), localTime.day(),
          localTime.hour(), localTime.minute(), localTime.second());
  sprintf(bufUTC, "%04d-%02d-%02dT%02d:%02d:%02d",
          utcTime.year(), utcTime.month(), utcTime.day(),
          utcTime.hour(), utcTime.minute(), utcTime.second());

  out.local = localTime;
  out.utc = utcTime;
  out.localISO = String(bufLocal);
  out.utcISO = String(bufUTC);
  out.ok = true;
  return out;
}

void configurarModoAhorroEnergia() {

  // =====================================================
  // 1️⃣ WATCHDOG – respaldo absoluto si el RTC falla
  // =====================================================
  if (!IWatchdog.isEnabled()) {

    unsigned long intervaloMs =
      ((unsigned long)config.intervaloDias * 86400UL +
       (unsigned long)config.intervaloHoras * 3600UL +
       (unsigned long)config.intervaloMinutos * 60UL +
       (unsigned long)config.intervaloSegundos) * 1000UL;

    // Margen de seguridad: +15 minutos
    unsigned long WDG_TIMEOUT_MS = intervaloMs + (15UL * 60UL * 1000UL);

    IWatchdog.begin(WDG_TIMEOUT_MS);
  }


  // =====================================================
  // 2️⃣ INTENTAR CORREGIR RTC ANTES DE PROGRAMAR ALARMA
  // =====================================================
  if (!rtcValido(rtc.now())) {
    corregirRTC();   // GPS → Red → nada
  }

  // ⚠️ Volver a leer después de corregir
  DateTime now = rtc.now();

  // Si sigue inválido, NO dependemos del RTC
  bool rtcConfiable = rtcValido(now);


  // =====================================================
  // 3️⃣ SIM / MODEM
  // =====================================================
  enviarComando("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000);
  enviarComando("AT+CNMI=2,1,0,0,0", 1000);


  // =====================================================
  // 4️⃣ AHORRO DE ENERGÍA MCU
  // =====================================================
  pinMode(STM_LED, INPUT);   // LED off real
  alarmFired = false;


  // =====================================================
  // 5️⃣ RTC ALARMA (solo si es confiable)
  // =====================================================
  if (rtcConfiable) {
    configurarAlarma(
      config.intervaloDias,
      config.intervaloHoras,
      config.intervaloMinutos,
      config.intervaloSegundos
    );
  } else {
    // RTC no confiable → el watchdog será quien despierte
    // (no programar alarma para evitar quedar dormido)
  }

  // =====================================================
  // 6️⃣ PERIFÉRICOS A SLEEP
  // =====================================================
  dormirA7670SA();
  // configureGPS(NEO8M); // si aplica

  // =====================================================
  // 7️⃣ DEEP SLEEP
  // =====================================================
  LowPower.begin();

  LowPower.attachInterruptWakeup(
    digitalPinToInterrupt(SQW_PIN),
    setAlarmFired,
    FALLING,
    DEEP_SLEEP_MODE
  );

  delay(200);
  LowPower.deepSleep();
}

void configurarRastreoContinuo(unsigned int segundos = 45) {
  // Configurar almacenamiento en memoria interna
  enviarComando("AT+CPMS=\"ME\",\"ME\",\"ME\"", 1000);

  // CNMI para SMS en vivo
  enviarComando("AT+CNMI=1,2,0,0,0");
  // RTC alarma cada X segundos
  configurarAlarma(0,0,0,segundos);

  // Activar interrupción en FALLING
  attachInterrupt(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING);
}

void sincronizarRTCconRed(int margenSegundos = 60) {
  HoraRedISO horaRed = obtenerHoraRedISO();
  if (horaRed.ok == false) {
      Serial.println("Error: no se pudo obtener hora de la red");
      return;
  }

  DateTime horaRTC = rtc.now();

  // Diferencia en segundos
  long diff = horaRTC.unixtime() - horaRed.utc.unixtime();
  if (diff < 0) diff = -diff; // valor absoluto

  if (diff > margenSegundos) {
      rtc.adjust(horaRed.utc);
      if(config.numUsuario != ""){
        enviarSMS("RTC sincronizado con red. Diferencia era de " + String(diff) + " segundos.", String(config.numUsuario));
      }
  } else {
    // RTC ya está sincronizado dentro del margen
  }
}

void procesarComando(String mensaje) {
    mensaje.trim();
    mensaje.toUpperCase(); // Para evitar problemas con mayúsculas/minúsculas

    // --- Verificar formato PIN=xxxxxx; ---
    if (!mensaje.startsWith("PIN=")) {
      if(String(config.numUsuario) =! ""){
        enviarSMS(">:( Falta el prefijo PIN en el comando.", String(config.numUsuario));
      }
      return;
    }

    int igual = mensaje.indexOf('=');
    int separador = mensaje.indexOf(';');
    if (separador == -1) {
      if(String(config.numUsuario) =! ""){
        enviarSMS(">:( Formato inválido. Use: PIN=xxxxxx;COMANDO", String(config.numUsuario));
      }
      return;
    }

    String pinIngresado = mensaje.substring(igual + 1, separador);
    pinIngresado.trim();
    // Validar PIN
    if (pinIngresado != config.pin) {
      if(config.numUsuario != ""){
        enviarSMS(">:( PIN incorrecto.", String(config.numUsuario));
      }
      return;
    }

    // Extraer comando real después del ;
    String comando = mensaje.substring(separador + 1);
    comando.trim();
    comando.toUpperCase();

  // ========== COMANDOS ==========
  // enviarSMS("Procesando comando: " + comando);
  // --- RASTREAR ON/OFF ---
  if (comando.indexOf("SET#MODO=") != -1) {
    if (comando.indexOf("AHORRO") != -1) {
      // Si ya está activado el rastreo, seguir.
      if(config.modo == MODO_CONTINUO) return;

      config.modo = MODO_AHORRO;
      config.firma = 0xCAFEBABE; // Asegurar firma válida
      guardarConfigEEPROM();
      // Rastreo con modo ahorro
      String intervalo = String(config.intervaloDias) + "D" +
                String(config.intervaloHoras) + "H" +
                String(config.intervaloMinutos) + "M" +
                String(config.intervaloSegundos) + "S";
      enviarSMS("Rastreo con Modo Ahorro ACTIVADO. Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC() + ". INT: " + intervalo, String(config.receptor));
      if(config.numUsuario != ""){
        enviarSMS("^_^ Rastreo con Modo Ahorro ACTIVADO.\nIntervalo de activacion: " + intervalo, String(config.numUsuario));
      }
      configurarModoAhorroEnergia();
    } 

    else if(comando.indexOf("CONTINUO") != -1) {
      // Si ya está activado el rastreo, seguir.
      if (config.modo == MODO_CONTINUO) return;

      config.modo = MODO_CONTINUO;
      config.firma = 0xCAFEBABE; // Asegurar firma válida
      guardarConfigEEPROM();

      // Rastreo sin modo ahorro
      configurarRastreoContinuo(59); // Cada 59 segundos
      enviarSMS("Rastreo Continuo ACTIVADO. Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC(), String(config.receptor));

      if(config.numUsuario != ""){
        enviarSMS("^_^ Rastreo Continuo ACTIVADO.\nIntervalo de activacion: 59 segundos", String(config.numUsuario));
      }
    }
    
    else if (comando.indexOf("OFF") != -1) {
      // Si ya está desactivado el rastreo, seguir.
      if(config.modo == MODO_OFF) {
        if(config.numUsuario != ""){
          enviarSMS("-_- Rastreo ya estaba DESACTIVADO.", String(config.numUsuario));
        }
      }

      config.modo = MODO_OFF;
      config.firma = 0xCAFEBABE;
      // Leer mensajes desde la memoria interna
      enviarComando("AT+CPMS=\"ME\",\"ME\",\"ME\"", 1000);
      enviarComando("AT+CNMI=1,2,0,0,0"); // Configurar notificaciones SMS en vivo
      guardarConfigEEPROM();
      enviarSMS("Rastreo DESACTIVADO. Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC(), String(config.receptor));
      if(config.numUsuario != ""){
        enviarSMS("-_- Rastreo DESACTIVADO.", String(config.numUsuario));
      }
    }
  }

  // else if (comando.indexOf("RASTREAR") != -1) {
  //   if (comando.indexOf("ON") != -1) {
  //     // Si ya está activado el rastreo, seguir.
  //     if(config.rastreoActivo) return;

  //     config.rastreoActivo = true;
  //     config.firma = 0xCAFEBABE; // Asegurar firma válida
  //     guardarConfigEEPROM();
  //     if(!config.modoAhorro){
  //       // Rastreo sin modo ahorro
  //       configurarRastreoContinuo(59); // Cada 59 segundos
  //       enviarSMS("Rastreo Continuo ACTIVADO. Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC(), String(config.receptor));

  //       if(config.numUsuario != ""){
  //         enviarSMS("^_^ Rastreo Continuo ACTIVADO.\nIntervalo de activacion: 59 segundos", String(config.numUsuario));
  //       }
  //     }else{
  //       // Rastreo con modo ahorro
  //       String intervalo = String(config.intervaloDias) + "D" +
  //                 String(config.intervaloHoras) + "H" +
  //                 String(config.intervaloMinutos) + "M" +
  //                 String(config.intervaloSegundos) + "S";
  //       enviarSMS("Rastreo con Modo Ahorro ACTIVADO. Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC() + ". INT: " + intervalo, String(config.receptor));
  //       if(config.numUsuario != ""){
  //         enviarSMS("^_^ Rastreo con Modo Ahorro ACTIVADO.\nIntervalo de activacion: " + intervalo, String(config.numUsuario));
  //       }
  //       configurarModoAhorroEnergia();
  //     }
  //   } else if (comando.indexOf("OFF") != -1) {
  //     // Si ya está desactivado el rastreo, seguir.
  //     if(!config.rastreoActivo) return;

  //     config.rastreoActivo = false;
  //     config.modoAhorro = false;
  //     config.firma = 0xCAFEBABE;
  //     // Leer mensajes desde la memoria interna
  //     enviarComando("AT+CPMS=\"ME\",\"ME\",\"ME\"", 1000);
  //     enviarComando("AT+CNMI=1,2,0,0,0"); // Configurar notificaciones SMS en vivo
  //     guardarConfigEEPROM();
  //     enviarSMS("Rastreo DESACTIVADO. Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC(), String(config.receptor));
  //     if(config.numUsuario != ""){
  //       enviarSMS("-_- Rastreo DESACTIVADO.", String(config.numUsuario));
  //     }
  //   }
  // }
  
  // // --- MODO AHORRO ---
  // else if (comando.indexOf("MODOAHORRO=") != -1) {
  //   if (comando.indexOf("ON") != -1) {
  //     // Si ya está activado el modo ahorro, seguir.
  //     if(config.modoAhorro) return;
  //     // Activar modo ahorro
  //     config.modoAhorro = true;
  //   } else if (comando.indexOf("OFF") != -1) {
  //     // Si ya está desactivado el modo ahorro, seguir.
  //     if(!config.modoAhorro) return;
  //     // Desactivar modo ahorro
  //     config.modoAhorro = false;
  //   } else {
  //     if(config.numUsuario != ""){
  //       enviarSMS("Use: MODOAHORRO=ON o OFF");
  //     }
  //     return;
  //   }
    
  //   config.firma = 0xCAFEBABE;
  //   guardarConfigEEPROM();
    
  //   String msg = "Modo ahorro: ";
  //   msg += config.modoAhorro ? "ON" : "OFF";
  //   msg += ". Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC();
  //   // enviarSMS(msg, String(config.receptor));
  //   if(config.numUsuario != ""){
  //     enviarSMS(msg, String(config.numUsuario));
  //   }
  // }
  
  // --- INTERVALO ---
  else if (comando.indexOf("SET#INTERVALO=") != -1) {
    String valor = comando.substring(10);
    valor.trim();
    
    if (valor.length() == 0) {
      if(String(config.numUsuario) != ""){
        enviarSMS(">:( Formato: INTERVALO=5M o 1H30M", String(config.numUsuario));
      }
      return;
    }
    
    // Resetear intervalos
    config.intervaloSegundos = 0;
    config.intervaloMinutos = 0;
    config.intervaloHoras = 0;
    config.intervaloDias = 0;
    
    // Parsear formato: 1D2H30M15S
    int i = 0;
    bool formatoValido = true;
    
    while (i < valor.length() && formatoValido) {
      String numero = "";
      
      // Extraer número
      while (i < valor.length() && isDigit(valor[i])) {
        numero += valor[i];
        i++;
      }
      
      if (numero.length() == 0) {
        formatoValido = false;
        break;
      }
      
      int cantidad = numero.toInt();
      
      // Extraer sufijo
      if (i >= valor.length()) {
        formatoValido = false;
        break;
      }
      
      char sufijo = valor[i];
      i++;
      
      switch(sufijo) {
        case 'S': config.intervaloSegundos += cantidad; break;
        case 'M': config.intervaloMinutos += cantidad; break;
        case 'H': config.intervaloHoras += cantidad; break;
        case 'D': config.intervaloDias += cantidad; break;
        default:
          formatoValido = false;
      }
    }
    
    if (!formatoValido) {
      if(config.numUsuario != ""){
        enviarSMS(">:( Formato inválido. Use: 5M, 1H30M, 1D2H", String(config.numUsuario));
      }
      return;
    }
    
    // Validar que no sea todo cero
    if (config.intervaloSegundos == 0 && config.intervaloMinutos == 0 &&
        config.intervaloHoras == 0 && config.intervaloDias == 0) {
      if(config.numUsuario != ""){
        enviarSMS(">:( Intervalo no puede ser 0", String(config.numUsuario));
      }
      return;
    }
    
    config.firma = 0xCAFEBABE;
    guardarConfigEEPROM();
    
    String resumen = "Intervalo configurado: ";
    if (config.intervaloDias > 0) resumen += String(config.intervaloDias) + "D ";
    if (config.intervaloHoras > 0) resumen += String(config.intervaloHoras) + "H ";
    if (config.intervaloMinutos > 0) resumen += String(config.intervaloMinutos) + "M ";
    if (config.intervaloSegundos > 0) resumen += String(config.intervaloSegundos) + "S";
    if(config.numUsuario != ""){
      enviarSMS(";-) " + resumen, String(config.numUsuario));
    }
  }
  
  // --- SETNUM (solo receptor) ---
  else if (comando.indexOf("SET#NUM=") != -1) {
    String nuevoNumero = comando.substring(7);
    nuevoNumero.trim();
    
    // Validar formato
    if (!nuevoNumero.startsWith("+") || nuevoNumero.length() < 11) {
      if(config.numUsuario != ""){
        enviarSMS(">:( Formato: +52XXXXXXXXXX", String(config.numUsuario));
      }
      return;
    }
    
    // Validar solo dígitos después del +
    bool valido = true;
    for (int i = 1; i < nuevoNumero.length(); i++) {
      if (!isDigit(nuevoNumero[i])) {
        valido = false;
        break;
      }
    }
    
    int digitos = nuevoNumero.length() - 1;
    if (!valido || digitos < 10 || digitos > 15) {
      if(config.numUsuario != ""){
        enviarSMS(">:( Número inválido (10-15 dígitos)", String(config.numUsuario));
      }
      return;
    }
    
    strcpy(config.numUsuario, nuevoNumero.c_str());
    config.configurado = true;
    config.firma = 0xCAFEBABE;
    guardarConfigEEPROM();
    
    delay(500);
    if(config.numUsuario != ""){
      enviarSMS(";) Numero guardado: " + nuevoNumero, String(config.numUsuario));
    }
  }
  
  // --- Config ---
  else if (comando.indexOf("GET#CONFIG") != -1) {
    String info = "OK#CONFIG\n";

    // ID
    info += "ID:" + String(config.idRastreador) + ";";

    // Numero receptor
    info += "NUM:" + String(config.receptor) + ";";

    // MODO
    info += "MODO:";
    switch (config.modo) {
      case MODO_OFF:      info += "OFF"; break;
      case MODO_AHORRO:   info += "AHORRO"; break;
      case MODO_CONTINUO: info += "CONTINUO"; break;
      default:            info += "DESCONOCIDO"; break;
    }
    info += ";";

    // INTERVALO
    info += "INTERVALO:";

    info += String(config.intervaloDias) + "D ";
    info += String(config.intervaloHoras) + "H ";
    info += String(config.intervaloMinutos) + "M ";
    info += String(config.intervaloSegundos) + "S;";

    // Numero de usuario
    info += "NUM:";
    if (strlen(config.numUsuario) > 0) {
      info += String(config.numUsuario);
    } else {
      info += "NO_CONFIG";
    }
    info += ";";

    // Enviar respuesta
    if (strlen(config.numUsuario) > 0) {
      enviarSMS(info, String(config.numUsuario));
    }
  }

  // --- STATUS ---
  else if (comando.indexOf("GET#STATUS") != -1) {
    String info = "OK#STATUS:\n";
    info += "ID: " + String(config.idRastreador) + ";";
    // MODO
    info += "MODO:";
    switch (config.modo) {
      case MODO_OFF:      info += "OFF"; break;
      case MODO_AHORRO:   info += "AHORRO"; break;
      case MODO_CONTINUO: info += "CONTINUO"; break;
      default:            info += "DESCONOCIDO"; break;
    }
    info += ";";
    
    // TIME
    info += "TIME: " + obtenerTiempoRTC() + ";";

    // GPS
    String datosGPS = leerYGuardarGPS();
    info += "GPS: " + datosGPS + ";";

    // BATTERY
    String batteryCharge = "";
    batteryCharge = obtenerVoltajeBateria();
    
    info += batteryCharge + "%;";
    if(config.numUsuario != ""){
      info.toUpperCase();
      enviarSMS(info, String(config.numUsuario));
    }
  }
  
  // --- LOCATION ---
  else if (comando.indexOf("GET#LOCATION") != -1) {
    String ubicacion = "Ubicacion obtenida:";
    String datosGPS = leerYGuardarGPS();

    ubicacion += "\nGPS: " + datosGPS;

    String cellInfo = obtenerTorreCelular();
    
    ubicacion += ".\nCT: " + cellInfo;

    if(config.numUsuario != ""){
      enviarSMS(ubicacion, String(config.numUsuario));
    }
  }

  else if (comando.indexOf("GET#TIMECELL") != -1) {
    // String fechaHoraLocal = obtenerFechaHoraRedISO("LOCAL");
    // String fechaHoraUTC = obtenerFechaHoraRedISO("UTC");
    HoraRedISO horaRed = obtenerHoraRedISO();
    String horaRTC = obtenerTiempoRTC();
    String mensaje;

    if (horaRed.localISO != "") {
      mensaje = "Tiempo de la red: " + horaRed.localISO;
      mensaje += " (LOCAL)\n" + horaRed.utcISO + " (UTC+00)";
      mensaje += "\nRTC: " + horaRTC;
    } else {
      mensaje = "Error: no se pudo leer hora de red";
    }

    if (config.numUsuario != "") {
      enviarSMS(mensaje, String(config.numUsuario));
    }
  }

  else if (comando.indexOf("GET#BATERIA") != -1) {
    String batteryCharge = obtenerVoltajeBateria();
    if(config.numUsuario != ""){
      enviarSMS("Nivel de bateria: " + batteryCharge, String(config.numUsuario));
    }
  }

  else if (comando.indexOf("RESET") != -1) {
    if(config.numUsuario != ""){
      enviarSMS("!_! Reiniciando y reseteando configuración a valores de fábrica.", String(config.numUsuario));
    }

    resetearEEPROM();
  }
  
  // --- COMANDO NO RECONOCIDO ---
  else {
    if(config.numUsuario != ""){
      enviarSMS(">:( Comando desconocido: " + comando, String(config.numUsuario));
    }
  }
}

void actualizarBuffer() {
    while (A7670SA.available()) {
        char c = A7670SA.read();
        rxBuffer += c;
    }
}

bool smsCompletoDisponible() {
  // Normalizar saltos de línea
  rxBuffer.replace("\r\n", "\n");

  // Recepción en vivo (CNMI con +CMT:)
  int idxCMT = rxBuffer.indexOf("+CMT:");
  if (idxCMT != -1) {
    int firstNL = rxBuffer.indexOf("\n", idxCMT);
    if (firstNL == -1) return false;

    // Verifica que haya texto después del encabezado
    if (rxBuffer.length() > firstNL + 1) {
      return true;
    }
  }

  // Lectura desde memoria (CMGL o CMGR)
  int idxMem = rxBuffer.indexOf("+CMGL:");
  if (idxMem == -1) idxMem = rxBuffer.indexOf("+CMGR:");
  if (idxMem != -1) {
    int firstNL = rxBuffer.indexOf("\n", idxMem);
    if (firstNL == -1) return false;

    if (rxBuffer.length() > firstNL + 1) {
      return true;
    }
  }

  // Mensajes inesperados (debug defensivo)
  rxBuffer.trim();
  if (rxBuffer != "" &&
      rxBuffer.indexOf("+CNMI") == -1 &&
      rxBuffer.indexOf("AT+CPMS") == -1 &&
      rxBuffer.indexOf("OK") == -1) {
    String mensaje = "rxBuffer: " + rxBuffer;
    if (mensaje.length() > 160) {
      mensaje = mensaje.substring(0, 160);
    }
    enviarSMS(mensaje, String(config.numUsuario));
  }

  return false;
}

String obtenerSMS() {
  // Normalizar saltos de línea
  rxBuffer.replace("\r\n", "\n");

  // Buscar encabezado +CMT
  int idx = rxBuffer.indexOf("+CMT:");
  if (idx != -1) {
    int start = rxBuffer.indexOf("\n", idx) + 1;
    int end   = rxBuffer.indexOf("\n", start);
    if (end == -1) end = rxBuffer.length();

    String sms = rxBuffer.substring(start, end);
    sms.trim();

    // Limpiar lo consumido
    rxBuffer = rxBuffer.substring(end);
    return sms;
  }

  // Buscar encabezado +CMGL o +CMGR
  idx = rxBuffer.indexOf("+CMGL:");
  if (idx == -1) idx = rxBuffer.indexOf("+CMGR:");
  if (idx != -1) {
    int start = rxBuffer.indexOf("\n", idx) + 1;
    int end   = rxBuffer.indexOf("\n", start);
    if (end == -1) end = rxBuffer.length();

    String sms = rxBuffer.substring(start, end);
    sms.trim();

    // Limpiar lo consumido
    rxBuffer = rxBuffer.substring(end);
    return sms;
  }

  return "";
}

// ---------- Funciones del rastreador ----------

String obtenerTiempoRTC() {
    DateTime now = rtc.now();

    char buffer[20];
    sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());

    return String(buffer);
}

String crearMensaje(String datosGPS, String cellTowerInfo, String batteryCharge){
  
  //Verificar si el RTC tiene la hora y fecha correcta
  if (!rtcValido(rtc.now())) {
    // Intentar corregir desde GPS y red celular
    corregirRTC();
  }

  bool rtcEsConfiable = rtcValido(rtc.now());
  String currentTime = "";

  if(!rtcEsConfiable){
    // Fecha y hora no confiable, incluso despues de corregir
    currentTime = "INVALID";
  }else{
    currentTime = obtenerTiempoRTC();
  }

  String output = "id:" + String(config.idRastreador) + ",";
    output += "time:" + currentTime + ",";
    output += cellTowerInfo + ",";
    output += batteryCharge + ",";
    output += datosGPS;
    output += ",rtc_fix:" + String(rtcEsConfiable ? "1" : "0");
  return output;
}

void enviarDatosRastreador(String datosGPS)
{
  digitalWrite(STM_LED, LOW);

  String cellTowerInfo = "";
  cellTowerInfo = obtenerTorreCelular();

  String batteryCharge = "";
  batteryCharge = obtenerVoltajeBateria();

  String SMS = crearMensaje(datosGPS, cellTowerInfo, batteryCharge);
  enviarSMS(SMS, String(config.receptor));

  if(String(config.numUsuario) != ""){
    enviarSMS("Hay novedades de tu rastreador: " + String(config.idRastreador) + "!", String(config.numUsuario));
  }

  delay(500);
  digitalWrite(STM_LED,HIGH);

}

void notificarEncendido()
{
  digitalWrite(STM_LED, HIGH);
  delay(200);
  digitalWrite(STM_LED, LOW);
  delay(200);
  digitalWrite(STM_LED, HIGH);
  delay(200);
  digitalWrite(STM_LED, LOW);
  delay(200);
  digitalWrite(STM_LED, HIGH);
  delay(200);
  digitalWrite(STM_LED, LOW);

  // String currentTime = obtenerTiempoRTC();
  // HoraRedISO horaRed = obtenerFechaHoraRedISO("LOCAL");
  HoraRedISO horaRed = obtenerHoraRedISO();

  String SMS = "El rastreador: " + String(config.idRastreador) + ",";
  SMS += " esta encendido. Hora red: " + horaRed.localISO + ".";
  enviarSMS(SMS, String(config.receptor));

  if(String(config.numUsuario) != ""){
    enviarSMS(SMS, String(config.numUsuario));
  }

  delay(500);
}

void debugEEPROMporSMS() {
  String debug = "EEPROM:\n";
  debug += "id:" + String(config.idRastreador) + ",";
  debug += "ad:" + String(config.receptor) + ",";
  debug += "us:" + String(config.numUsuario) + ",";
  debug += "mod:" + String(config.modoAhorro ? "ON" : "OFF") + ",";
  debug += "pin:" + String(config.pin);
  debug += ",num:" + String(config.numUsuario);
  enviarSMS(debug, "+525620577634");
}

String leerYGuardarGPS() {
  String nuevaLat = "";
  String nuevaLon = "";
  bool ubicacionActualizada = false;
  unsigned long startTime = millis();
  int intentos = 0;

  while ((millis() - startTime) < 10000 && intentos < 30 && !ubicacionActualizada) {
      while (NEO8M.available()) {
          char c = NEO8M.read();
          gps1.encode(c);

          if (gps1.location.isUpdated() && gps1.location.isValid() && gps1.satellites.value() > 0) {
              nuevaLat = String(gps1.location.lat(), 6);
              nuevaLon = String(gps1.location.lng(), 6);

              latitude = nuevaLat;
              longitude = nuevaLon;
              ubicacionActualizada = true;
              break;
          }
      }
      delay(50);
      intentos++;
  }

  // Caso 1: nunca hubo fix → mandar 0.0
  if (!ubicacionActualizada && (latitude == "" || longitude == "")) {
      latitude = "0.0";
      longitude = "0.0";
  }
  // Caso 2: ya hubo fix antes → conservar última coordenada válida
  // No pisar con 0.0 aunque momentáneamente satélites sea 0

  return "lat:" + latitude + ",lon:" + longitude + ",gps_fix:" + (ubicacionActualizada ? "1" : "0");
}

bool rtcValido(DateTime t) {
  if (t.year() < 2023 || t.year() > 2035) return false;
  if (t.month() < 1 || t.month() > 12) return false;
  if (t.day() < 1 || t.day() > 31) return false;
  if (t.hour() > 23) return false;
  if (t.minute() > 59) return false;
  if (t.second() > 59) return false;
  return true;
}

bool obtenerHoraRed(DateTime &netTime) {
  String resp = enviarComandoConRetorno("AT+CCLK?", 1500);

  int idx = resp.indexOf("+CCLK:");
  if (idx == -1) return false;

  int q1 = resp.indexOf('"', idx);
  int q2 = resp.indexOf('"', q1 + 1);
  if (q1 == -1 || q2 == -1) return false;

  String s = resp.substring(q1 + 1, q2);
  // Ejemplo: 26/01/02,18:25:30-24

  if (s.length() < 17) return false;

  int yy = s.substring(0, 2).toInt();
  int MM = s.substring(3, 5).toInt();
  int dd = s.substring(6, 8).toInt();
  int hh = s.substring(9, 11).toInt();
  int mm = s.substring(12, 14).toInt();
  int ss = s.substring(15, 17).toInt();

  int tz = s.substring(18).toInt(); // cuartos de hora respecto a UTC

  int year = 2000 + yy;

  DateTime localTime(year, MM, dd, hh, mm, ss);

  // tz está en cuartos de hora → convertir a segundos
  // Ejemplo: -24 → -6 horas
  int offsetSeconds = tz * 15 * 60;

  // Para obtener UTC: localTime - offset
  netTime = localTime - TimeSpan(offsetSeconds);

  return true;
}

void corregirRTC() {
    DateTime now = rtc.now();
    DateTime newTime;
    bool timeValida = false;
    String fuente = "";

    // 1️⃣ GPS (fuente primaria)
    if (gps1.date.isValid() && gps1.time.isValid()) {
        int gpsYear   = gps1.date.year();
        int gpsMonth  = gps1.date.month();
        int gpsDay    = gps1.date.day();
        int gpsHour   = gps1.time.hour();
        int gpsMinute = gps1.time.minute();
        int gpsSecond = gps1.time.second();

        newTime = DateTime(gpsYear, gpsMonth, gpsDay, gpsHour, gpsMinute, gpsSecond);
        timeValida = rtcValido(newTime);
        fuente = "GPS";

        // Ajustar RTC si es necesario (igual que antes)
        if (timeValida) {
            long diff = labs((long)now.unixtime() - (long)newTime.unixtime());
            if (diff > 2) {
                rtc.adjust(newTime);

                static unsigned long ultimaCorreccion = 0;
                unsigned long tiempoActual = millis();

                if (tiempoActual - ultimaCorreccion > 3600000UL || ultimaCorreccion == 0) {
                    ultimaCorreccion = tiempoActual;
                    String mensaje = "RTC ajustado con hora " + fuente + " (diferencia " + String(diff) + "s)";
                    if(String(config.numUsuario) != ""){
                      enviarSMS(mensaje, config.numUsuario);
                    }
                }
            }
        }
    }

    // 2️⃣ Red celular (respaldo)
    else {
        // Aquí ya no usamos obtenerHoraRed directamente,
        // sino la función sincronizarRTCconRed con margen de 60s
        sincronizarRTCconRed(60);  
        // Nota: sincronizarRTCconRed ya hace la comparación y ajuste,
    }
}

String obtenerTorreCelular() {
    String lac = "";
    String cellId = "";
    String mcc = "";
    String mnc = "";
    String red = "";

    // Solicitar información de la red con A7670SA
    flushA7670SA();

    enviarComando("AT+CPSI?",2000);
    String cpsiResponse = leerRespuestaA7670SA();

    // Extraer datos de la respuesta de AT+CPSI?
    int startIndex = cpsiResponse.indexOf("CPSI:");

    if (startIndex != -1) {
        startIndex += 6; // Mover el índice después de "CPSI: "

        // Verificar si es LTE o GSM
        if (cpsiResponse.startsWith("LTE", startIndex)) {
            red = "lte";
            startIndex += 4; // Mover el índice después de "LTE,"
        } else if (cpsiResponse.startsWith("GSM", startIndex)) {
            red = "gsm";
            startIndex += 4; // Mover el índice después de "GSM,"
        } else {
            return "{}"; // Si no es ni GSM ni LTE, devolver JSON vacío
        }

        // Extraer MCC-MNC correctamente
        int mccMncStart = cpsiResponse.indexOf(",", startIndex) + 1;
        int mccMncEnd = cpsiResponse.indexOf(",", mccMncStart);
        String mccMncRaw = cpsiResponse.substring(mccMncStart, mccMncEnd);

        // Separar MCC y MNC
        int separatorIndex = mccMncRaw.indexOf("-");
        if (separatorIndex != -1) {
            mcc = mccMncRaw.substring(0, separatorIndex);  // MCC
            mnc = mccMncRaw.substring(separatorIndex + 1); // MNC
        }

        // Extraer LAC
        int lacStart = mccMncEnd + 1;
        int lacEnd = cpsiResponse.indexOf(",", lacStart);
        lac = cpsiResponse.substring(lacStart, lacEnd);

        // Extraer Cell ID
        int cellIdStart = lacEnd + 1;
        int cellIdEnd = cpsiResponse.indexOf(",", cellIdStart);
        cellId = cpsiResponse.substring(cellIdStart, cellIdEnd);
    }

    // Convertir de Hex a Decimal
    lac = hexToDec(lac);

    String json = "red:" + red + ",";
    json += "mcc:" + mcc + ",";
    json += "mnc:" + mnc + ",";
    json += "lac:" + lac + ",";
    json += "cid:" + cellId;
    //enviarSMS(json);
    return json;
}

String hexToDec(String hexStr) {
  long decVal = strtol(hexStr.c_str(), NULL, 16);
  return String(decVal);
}

void setup() {
  // Inicializar puertos seriales
  Wire.begin();
  _buffer.reserve(50);
  A7670SA.begin(115200);
  NEO8M.begin(9600);

  /* Configuracion de puertos */
  pinMode(SLEEP_PIN, OUTPUT);
  pinMode(SQW_PIN, INPUT_PULLUP);
  pinMode(STM_LED, OUTPUT);
  analogReadResolution(12);

  digitalWrite(STM_LED, LOW);

  // Detectar si el MCU reinició por Watchdog
  if (IWatchdog.isReset()) {
    resetPorWatchdog = true;
    IWatchdog.clearReset();
  }

  if (!rtc.begin()) {        // si falla la inicializacion del modulo
    //Serial.println("Modulo RTC no encontrado !");  // muestra mensaje de error
    while (1);         // bucle infinito que detiene ejecucion del programa
  }

  if(rtc.lostPower()) {
      DateTime localTime(__DATE__, __TIME__);
      DateTime utcTime = localTime + TimeSpan(6 * 3600); // Convertir a UTC sumando 6 horas
      // Ajustar el RTC a la fecha y hora de compilación en UTC
      rtc.adjust(utcTime);
  }

  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_OFF);

  // Configuración de EEPROM para STM32
  EEPROM.begin();

  // Intentar leer configuración guardada
  if (!leerConfigEEPROM()) {
    // Si no hay configuración válida, cargar defaults
    cargarConfiguracionPorDefecto();
  }

  // Iniciar A7670SA
  iniciarA7670SA();

  delay(5000);

  enviarComando("AT+CMGF=1",1000); // modo texto

  // Esperar un poco a que la red registre
  delay(2000);

  if (resetPorWatchdog) {
    corregirRTC();
    resetPorWatchdog = false;  // ya atendido
  }

  notificarEncendido();

  sincronizarRTCconRed(60); // margen de 60 segundos

  if(config.modo == MODO_AHORRO){
    configurarModoAhorroEnergia();
  }else if(config.modo == MODO_CONTINUO){
    configurarRastreoContinuo(45); // Cada 45 segundos
  }else{
    enviarComando("AT+CNMI=1,2,0,0,0"); // Configurar notificaciones SMS en vivo
  }

  digitalWrite(STM_LED,HIGH);
}

void procesarSMS(String resp, String banco) {
  int pos = 0;
  while ((pos = resp.indexOf("+CMGL:", pos)) != -1) {
    int fin = resp.indexOf("\r\n", pos);
    if (fin == -1) break;
    String header = resp.substring(pos, fin);
    pos = fin + 2;

    int bodyEnd = resp.indexOf("\r\n", pos);
    if (bodyEnd == -1) break;
    String body = resp.substring(pos, bodyEnd);
    pos = bodyEnd + 2;

    // enviarSMS("[" + banco + "] " + body, String(config.numUsuario));

    // Borrar SMS procesado
    int idxStart = header.indexOf(":");
    int idxEnd = header.indexOf(",");
    if (idxStart != -1 && idxEnd != -1) {
      int smsIndex = header.substring(idxStart + 1, idxEnd).toInt();
      enviarComando(("AT+CMGD=" + String(smsIndex)).c_str(), 1000);
    }

    // Procesar comando en cuerpo del SMS
    procesarComando(body);
  }
}

void leerSMSPendientes() {
  limpiarBufferA7670SA();
  rxBuffer = "";

  // 1. Leer en ME
  enviarComando("AT+CPMS=\"ME\",\"ME\",\"ME\"", 1000);
  String respME = enviarComandoConRetorno("AT+CMGL=\"REC UNREAD\"", 7000);
  // enviarSMS("ME: " + respME, String(config.numUsuario));
  if (respME.indexOf("+CMGL:") != -1) {
    procesarSMS(respME, "ME");
  } else {
    respME = enviarComandoConRetorno("AT+CMGL=\"ALL\"", 7000);
    // enviarSMS("MEAL: " + respME, String(config.numUsuario));
    if (respME.indexOf("+CMGL:") != -1) {
      procesarSMS(respME, "ME");
    }
  }

  // 2. Leer en SM
  enviarComando("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000);
  String respSM = enviarComandoConRetorno("AT+CMGL=\"REC UNREAD\"", 7000);
  // enviarSMS("SM: " + respSM, String(config.numUsuario));
  if (respSM.indexOf("+CMGL:") != -1) {
    procesarSMS(respSM, "SM");
  } else {
    respSM = enviarComandoConRetorno("AT+CMGL=\"ALL\"", 7000);
    // enviarSMS("SMAL: " + respSM, String(config.numUsuario));
    if (respSM.indexOf("+CMGL:") != -1) {
      procesarSMS(respSM, "SM");
    }
  }
}

void loop() {
  // Siempre escuchar fragmentos entrantes
  actualizarBuffer();

  // Si el watchdog está activo, recargarlo para evitar reinicios
  if (IWatchdog.isEnabled()) {
    IWatchdog.reload();
  }

  if (config.modo == MODO_OFF) {
    // Solo escuchar SMS
    if (smsCompletoDisponible()) {
      encenderLED();
      String mensaje = obtenerSMS();
      procesarComando(mensaje);
      apagarLED();
    }

    // Espera defensiva para acumular datos
    unsigned long t0 = millis();
    while (millis() - t0 < 2000) actualizarBuffer();
  }
  else if (config.modo == MODO_CONTINUO) {
    // Rastreo continuo: usar RTC o millis para intervalos
    // Primero revisar SMS pendientes
    if (smsCompletoDisponible()) {
      encenderLED();
      String mensaje = obtenerSMS();
      procesarComando(mensaje);
      apagarLED();
    }
    // Luego revisar si es tiempo de rastrear
    if (alarmFired) {
      encenderLED();
      alarmFired = false; // reset bandera
      String datosGPS = leerYGuardarGPS();
      enviarDatosRastreador(datosGPS);
      configurarRastreoContinuo();
      apagarLED();
    }
  }
  else if (config.modo == MODO_AHORRO && alarmFired) {
    // Rastreo con modo ahorro
    alarmFired = false; // reset bandera
    // limpiar flags de alarma
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);

    pinMode(STM_LED, OUTPUT);
    delay(200);

    encenderLED();

    // Despertar módem
    despertarA7670SA();
    iniciarA7670SA();

    // Configurar almacenamiento en SIM
    enviarComando("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000);

    // Reconfigurar CNMI para modo ahorro
    enviarComando("AT+CNMI=2,1,0,0,0", 1000);

    // Delay inicial para que la pila de SMS esté lista
    delay(1000);

    leerSMSPendientes();

    corregirRTC();

    // Leer GPS y enviar datos
    String datosGPS = leerYGuardarGPS();
    enviarDatosRastreador(datosGPS);

    corregirRTC();

    apagarLED();
    alarmFired = false;

    // Volver a dormir
    configurarModoAhorroEnergia();
  }
  else {
    // Espera defensiva para acumular datos
    unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
      actualizarBuffer();
    }
  }
}


// void setUpdateRate(Stream &gps) {
//   // UBX-config-RATE: set update rate to 10000 ms (0.1 Hz)
//   byte rateconfig[] = {
//     0xB5, 0x62,       // Sync chars
//     0x06, 0x08,       // Class = config, idRastreador = RATE
//     0x06, 0x00,       // Length = 6
//     0x10, 0x27,       // measRate = 10000 ms (0x2710)
//     0x01, 0x00,       // navRate = 1
//     0x01, 0x00        // timeRef = 1 (GPS time)
//   };
//   gps.write(rateconfig, sizeof(rateconfig));
// }

// void disableNMEAMessages(Stream &gps) {
//   // Formato: UBX-config-MSG (Clase 0xF0 = NMEA, idRastreador = tipo de mensaje)
//   byte msgs[][9] = {
//     {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x00, 0x00}, // GxGGA
//     {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x01, 0x00}, // GxGLL
//     {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x02, 0x00}, // GxGSA
//     {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x03, 0x00}, // GxGSV
//     {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x05, 0x00}, // GxVTG
//     {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x06, 0x00}  // GxGRS
//   };

//   for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++) {
//     gps.write(msgs[i], sizeof(msgs[i]));
//     delay(50); // Pequeña pausa entre comandos
//   }
// }

// void configureGPS(Stream &gps){
//   delay(500); // Esperar a que el GPS esté listo tras el power-up
//   setUpdateRate(gps);
//   delay(500); // Pausa corta entre comandos
//   disableNMEAMessages(gps);
// }
