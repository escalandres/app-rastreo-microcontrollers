/* Declaracion de librerias */
#include <Wire.h>
#include <HardwareSerial.h>
#include <RTClib.h>
#include <low_power.h>
#include <STM32LowPower.h>
#include <TinyGPSPlus.h>
#include <EEPROM.h>

/* Declaracion de variables globales */
volatile bool alarmFired = false;
RTC_DS3231 rtc; // Objeto Reloj de precision RTC
TinyGPSPlus gps1; // Objeto GPS

/* Declaracion de puertos del STM32F103C8T6 */
const int SLEEP_PIN = PB1;
const int SQW_PIN = PB0;
const int STM_LED = PC13;
const int BATERIA = PA0;

/* Constantes y Variables Globales */
struct Config {
  uint32_t firma; // ← debe ser 0xCAFEBABE
  int idRastreador;         // ID unico del rastreador
  char admin[16];       // Numero de telefono del administrador
  char numUsuario[16];  // Numero de usuario que recibe los SMS;
  int intervaloSegundos;            // Intervalo de envio de datos en segundos
  int intervaloMinutos;             // Intervalo de envio de datos en minutos
  int intervaloHoras;               // Intervalo de envio de datos en horas
  int intervaloDias;                // Intervalo de envio de datos en dias
  bool modoAhorro;              // Modo ahorro de energia (true/false) 
  char pin[8];                // PIN para aceptar comandos SMS
  bool configurado;
  bool rastreoActivo;          // Indica si el rastreo está activo o no
};

Config config;
// Dirección en EEPROM para guardar la configuración
const uint16_t CONFIG_ADDRESS = 0;

String latitude, longitude;

int _timeout;
String _buffer;

// Definir el puerto serial A7670SA
HardwareSerial A7670SA(PA3, PA2);
HardwareSerial NEO8M(PA10, PA9);

// -------- Funciones de alarma RTC --------

void setAlarmFired() {
  alarmFired = true;
}

void configurarAlarma(int dias = 0, int horas = 0, int minutos = 5, int segundos = 0) {
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);

  //Set Alarm to be trigged in X
  rtc.setAlarm1(rtc.now() + TimeSpan(dias, horas, minutos, segundos), DS3231_A1_Second);  // this mode triggers the alarm when the seconds match.

  alarmFired = false;
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
  strcpy(config.admin, "+525620577634");
  strcpy(config.numUsuario, "");
  config.intervaloSegundos = 0;
  config.intervaloMinutos = 5;
  config.intervaloHoras = 0;
  config.intervaloDias = 0;
  config.modoAhorro = false;
  strcpy(config.pin, "589649");
  config.configurado = true;  // Marcar como configurado
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

  delay(7000);
  
  notificarEncendido();
  debugEEPROMporSMS();
  // Esperar registro en red
  // if (esperarRegistroRed()) {
  //   notificarEncendido();
  // } 
  digitalWrite(STM_LED,HIGH);
}

void configurarModoAhorroEnergia(bool modoAhorro) {
  if (modoAhorro) {
    // Configurar para modo ahorro de energia
    // Desactivar LED
    pinMode(STM_LED, INPUT); // Cambiar a entrada para reducir consumo

    // Configurar alarma RTC
    configurarAlarma(config.intervaloDias, config.intervaloHoras, config.intervaloMinutos, config.intervaloSegundos);

    // Configurar A7670SA para sleep automatico en idle
    dormirA7670SA(true);

    // Configurar GPS para modo bajo consumo (si es posible)
    //configureGPS(NEO8M);
    // Aquí podrías enviar comandos específicos al GPS si soporta modos de bajo consumo

    // Configure low power
    LowPower.begin();
    // Attach a wakeup interrupt on pin, calling repetitionsIncrease when the device is woken up
    // Last parameter (LowPowerMode) should match with the low power state used
    LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, DEEP_SLEEP_MODE); // SLEEP_MODE

    LowPower.deepSleep();
  } else {
    // Configurar para modo normal
    pinMode(STM_LED, OUTPUT); // Cambiar a salida para usar el LED
    digitalWrite(STM_LED, HIGH); // Encender LED
    dormirA7670SA(false);
    // Configurar GPS para modo normal (si es posible)
    // Aquí podrías enviar comandos específicos al GPS si soporta modos normales
    iniciarA7670SA();
  }
}

void loop() {
  if(config.rastreoActivo == true){
    // Si el rastreo está desactivado, dormir por un tiempo y volver a checar
    if(alarmFired){
      pinMode(STM_LED, OUTPUT);
      // Enciende led del STM32
      digitalWrite(STM_LED,LOW);
      // Encender modulo A7670SA
      dormirA7670SA(false);
      iniciarA7670SA();
      delay(2000);
      // Revisar si hay mensajes SMS pendientes
      if (hayMensajesPendientes()) {
        leerMensajes();
      }
      // Leer GPS
      String datosGPS = leerYGuardarGPS();

      // Enviar datos de rastreo
      enviarDatosRastreador(datosGPS);
      delay(3000);
      // Apagar led del STM32
      digitalWrite(STM_LED,HIGH);
      
      // Limpiar bandera de alarma
      alarmFired = false;

      // Configurar modo ahorro de energia si está activado
      configurarModoAhorroEnergia(config.modoAhorro);
    }
  }
  else{
    // Si el rastreo está desactivado
    // Revisar si hay mensajes SMS pendientes
    if (hayMensajesPendientes()) {
      leerMensajes();
    }

    // dormir por un tiempo y volver a checar
    delay(20000);
  }
}

// ---------- Funciones del A7670SA ----------

void enviarComando(const char* comando, int espera = 1000) {
  A7670SA.println(comando);
  delay(espera);

  // while (A7670SA.available()) {
  //   Serial.write(A7670SA.read());
  // }
  //Serial.println();a
}

void iniciarA7670SA(){
  //digitalWrite(LEFT_LED, HIGH);
   // 1. Probar comunicación AT
  enviarComando("AT", 1000);

  // 5. Establecer modo LTE (opcional)
  enviarComando("AT+CNMP=2", 2000);

  // Confirmar nivel de señal y registro otra vez
  enviarComando("AT+CSQ", 1000);
  enviarComando("AT+CREG?", 1000);
}

void dormirA7670SA(bool dormir) {
  if (dormir) {
    enviarComando("AT+CSCLK=1");
    delay(100);
    digitalWrite(SLEEP_PIN, HIGH);  // HIGH permite que el módulo entre en sleep
  } else {
    digitalWrite(SLEEP_PIN, LOW);   // LOW despierta el módulo
    delay(100);
    enviarComando("AT+CSCLK=0");
    enviarComando("AT");
  }
}

String leerRespuestaA7670SA(unsigned long timeout = 2000) {
  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    while (A7670SA.available()) {
      char c = A7670SA.read();
      response += c;
    }
  }

  return response;
}

String _readSerial() {
  _timeout = 0;
  while  (!A7670SA.available() && _timeout < 12000  )
  {
    delay(13);
    _timeout++;
  }
  if (A7670SA.available()) {
    return A7670SA.readString();
  }
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

void enviarSMS(String SMS, String number = "+525620577634")
{
  iniciarA7670SA();
  enviarComando("AT+CREG?",1000);
  enviarComando("AT+CMGF=1",1000);

  //Serial.println ("Set SMS Number");
  enviarComando(("AT+CMGS=\"" + number + "\"").c_str(), 3000); //Mobile phone number to send message

  A7670SA.println(SMS);
  delay(500);
  A7670SA.println((char)26);// ASCII code of CTRL+Z
  delay(500);
  _buffer = _readSerial();

  delay(2000);
}

bool hayMensajesPendientes() {
    enviarComando("AT+CPMS=\"SM\"\r"); // Selecciona la memoria SIM
    delay(200);

    String respuesta = leerRespuestaA7670SA(1000); // Lee respuesta con timeout de 1s

    // Ejemplo de respuesta:
    // +CPMS: 1,30,1,30,1,30
    // Donde el primer número es la cantidad de SMS usados en la memoria

    // Busca el número de mensajes almacenados
    int index = respuesta.indexOf("+CPMS:");
    if (index != -1) {
        int coma = respuesta.indexOf(',', index);
        if (coma != -1) {
            int usados = respuesta.substring(index + 7, coma).toInt();
            if (usados > 0) {
                // Ahora verificamos si alguno está sin leer
                enviarComando("AT+CMGL=\"REC UNREAD\"\r");
                delay(500);
                String lista = leerRespuestaA7670SA(2000);

                if (lista.indexOf("+CMGL:") != -1) {
                    return true; // Hay mensajes sin leer
                }
            }
        }
    }

    return false;
}

void leerMensajes() {
    enviarComando("AT+CMGF=1\r"); // Modo texto
    delay(100);
    enviarComando("AT+CMGL=\"REC UNREAD\"\r"); // Lista mensajes no leídos
    delay(500);

    String respuesta = leerRespuestaA7670SA(4000);

    // Ejemplo de respuesta:
    // +CMGL: 1,"REC UNREAD","+521234567890",,"24/10/09,14:32:00+08"
    // MODOAHORRO=ON

    int index = 0;
    while ((index = respuesta.indexOf("+CMGL:", index)) != -1) {
        int id = respuesta.substring(index + 7, respuesta.indexOf(',', index)).toInt();

        // Busca el texto del mensaje
        int saltoLinea = respuesta.indexOf("\n", index);
        int finMsg = respuesta.indexOf("+CMGL:", saltoLinea);
        if (finMsg == -1) finMsg = respuesta.length();
        String mensaje = respuesta.substring(saltoLinea + 1, finMsg);
        mensaje.trim();

        procesarComando(mensaje); // Aquí llamas tu lógica según el contenido

        // Borra el mensaje después de procesarlo
        String cmd = "AT+CMGD=" + String(id) + "\r";
        enviarComando(cmd.c_str());
        delay(200);

        index = finMsg;
    }
}

bool esperarRegistroRed() {
  unsigned long start = millis();
  while (millis() - start < 10000) { // Esperar hasta 10 segundos
    enviarComando("AT+CREG?", 1000);
    String respuesta = leerRespuestaA7670SA();
    if (respuesta.indexOf("+CREG: 0,1") != -1 || respuesta.indexOf("+CREG: 0,5") != -1) {
      return true; // Registrado en red
    }
    delay(1000);
  }
  return false; // No se registró
}

void procesarComando(String mensaje) {
    mensaje.trim();
    mensaje.toUpperCase(); // Para evitar problemas con mayúsculas/minúsculas

    // --- Verificar formato PIN=xxxxxx; ---
    if (!mensaje.startsWith("PIN=")) {
        if(String(config.numUsuario) != ""){
          enviarSMS("Falta el prefijo PIN en el comando.", String(config.numUsuario));
        }
        return;
    }

    int igual = mensaje.indexOf('=');
    int separador = mensaje.indexOf(';');
    if (separador == -1) {
        if(String(config.numUsuario) != ""){
          enviarSMS("Formato inválido. Use: PIN=xxxxxx;COMANDO", String(config.numUsuario));
        }
        return;
    }

    String pinIngresado = mensaje.substring(igual + 1, separador);
    pinIngresado.trim();

    // Validar PIN
    if (pinIngresado != config.pin) {
        if(String(config.numUsuario) != ""){
          enviarSMS("🔒 PIN incorrecto.", String(config.numUsuario));
        }
        return;
    }

    // Extraer comando real después del ;
    String comando = mensaje.substring(separador + 1);
    comando.trim();
    comando.toUpperCase();

    // --- Activar o desactivar modo ahorro ---
    if (comando.indexOf("MODOAHORRO=")  != -1) {
        if (comando.endsWith("ON")) {
            config.modoAhorro = true;
            if(String(config.numUsuario) != ""){
              enviarSMS("Modo ahorro activado.", String(config.numUsuario));
            }
            enviarSMS("Modo ahorro activado.", String(config.admin));
        } else {
            config.modoAhorro = false;
            enviarSMS("Modo ahorro desactivado.", String(config.admin));
            if(String(config.numUsuario) != ""){
              enviarSMS("Modo ahorro desactivado.", String(config.numUsuario));
            }
        }
        guardarConfigEEPROM();
    }

    // --- Cambiar intervalo ---
    else if (comando.indexOf("INTERVALO=") != -1) {
      String valor = comando.substring(10);
      valor.trim();

      // Reiniciar todos los valores
      config.intervaloSegundos = 0;
      config.intervaloMinutos = 0;
      config.intervaloHoras = 0;
      config.intervaloDias = 0;

      int i = 0;
      while (i < valor.length()) {
          String numero = "";
          // Extraer número
          while (i < valor.length() && isDigit(valor[i])) {
              numero += valor[i];
              i++;
          }

          // Si no hay número, sal del bucle
          if (numero == "") break;

          int cantidad = numero.toInt();

          // Extraer sufijo
          if (i < valor.length()) {
              char sufijo = valor[i];
              i++;
              if (sufijo == 'S') config.intervaloSegundos += cantidad;
              else if (sufijo == 'M') config.intervaloMinutos += cantidad;
              else if (sufijo == 'H') config.intervaloHoras += cantidad;
              else if (sufijo == 'D') config.intervaloDias += cantidad;
              else {
                  enviarSMS("Unidad inválida: use S, M, H o D.", String(config.admin));
                  if (config.numUsuario != "") {
                      enviarSMS("Unidad inválida: use S, M, H o D.", String(config.numUsuario));
                  }
                  return;
              }
          }
      }

      guardarConfigEEPROM();

      String resumen = "Intervalo actualizado: ";
      if (config.intervaloDias > 0) resumen += String(config.intervaloDias) + "D";
      if (config.intervaloHoras > 0) resumen += String(config.intervaloHoras) + "H";
      if (config.intervaloMinutos > 0) resumen += String(config.intervaloMinutos) + "M";
      if (config.intervaloSegundos > 0) resumen += String(config.intervaloSegundos) + "S";

      if(String(config.numUsuario) != ""){
        enviarSMS(resumen, String(config.numUsuario));
      }
      enviarSMS(resumen, String(config.admin));
  }

    // --- Cambiar número de usuario ---
    else if (comando.indexOf("SETNUM=") != -1) {
      String nuevoNumero = comando.substring(7);
      nuevoNumero.trim();

      // --- Validar formato internacional ---
      if (!nuevoNumero.startsWith("+")) {
          enviarSMS("Número inválido. Debe iniciar con '+'.", String(config.admin));
          return;
      }

      // --- Verificar que solo tenga dígitos después del '+' ---
      bool formatoValido = true;
      for (int i = 1; i < nuevoNumero.length(); i++) {
          if (!isDigit(nuevoNumero[i])) {
              formatoValido = false;
              break;
          }
      }

      // --- Verificar longitud válida ---
      int digitos = nuevoNumero.length() - 1; // sin contar el '+'
      if (!formatoValido || digitos < 10 || digitos > 15) {
          enviarSMS("Número inválido. Debe incluir código de país y tener 10-15 dígitos.", String(config.admin));
          return;
      }

      // --- Guardar si es válido ---
      strcpy(config.numUsuario, nuevoNumero.c_str());
      config.configurado = true;  // 🔹 Marcar como configurado
      guardarConfigEEPROM();

      enviarSMS("✅ Número de destino actualizado: " + nuevoNumero, String(config.admin));
      enviarSMS("Número configurado correctamente.", String(config.numUsuario));
  }

    // --- Iniciar rastreo ---
    else if (comando.indexOf("RASTREAR ON") != -1) {
        config.rastreoActivo = true;
        guardarConfigEEPROM();
        enviarSMS("Rastreo activado.", String(config.admin));
    }

    // --- Detener rastreo ---
    else if (comando.indexOf("RASTREAR OFF") != -1) {
        config.rastreoActivo = false;
        guardarConfigEEPROM();
        enviarSMS("Rastreo detenido.", String(config.admin));
    }

    else if (comando.indexOf("STATUS") != -1) {
      String info = "ID:" + String(config.idRastreador) + "\n" +
                    "Ahorro: " + String(config.modoAhorro ? "ON" : "OFF") + "\n" +
                    "Int: " + String(config.intervaloMinutos) + " min\n" +
                    "Ras: " + String(config.rastreoActivo ? "ON" : "OFF");
      enviarSMS(info, String(config.admin));
    }

    else if (comando.indexOf("UBICACION") != -1) {
      String datosGPS = leerYGuardarGPS();
      String cellTowerInfo = obtenerTorreCelular();
      datosGPS += "," + cellTowerInfo;
      String smsUbicacion = "Ubicación actual:\n" + datosGPS;
      enviarSMS(smsUbicacion, String(config.admin));
      if(String(config.numUsuario) != ""){
        enviarSMS(smsUbicacion, String(config.numUsuario));
      }
    }

    // --- Comando desconocido ---
    else {
      enviarSMS("Comando no reconocido: " + comando, String(config.admin));
      if(String(config.numUsuario) != ""){
        enviarSMS("Comando no reconocido: " + comando, String(config.numUsuario));
      }
    }
}

// ---------- Funciones del rastreador ----------

void enviarDatosRastreador(String datosGPS)
{
  digitalWrite(STM_LED, LOW);

  String cellTowerInfo = "";
  cellTowerInfo = obtenerTorreCelular();

  String batteryCharge = "";
  batteryCharge = obtenerVoltajeBateria();

  String SMS = crearMensaje(datosGPS, cellTowerInfo, batteryCharge);
  enviarSMS(SMS);

  delay(2000);
  digitalWrite(STM_LED,HIGH);

}

String crearMensaje(String datosGPS, String cellTowerInfo, String batteryCharge){

  //Verificar si el RTC tiene la hora y fecha correcta
  corregirRTC();

  DateTime now = rtc.now();

  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());

  String currentTime = String(buffer);

  String output = "id:" + String(config.idRastreador) + ",";
    output += "time:" + currentTime + ",";
    output += cellTowerInfo + ",";
    output += batteryCharge + ",";
    output += datosGPS;
  return output;
}

void notificarEncendido()
{
  digitalWrite(STM_LED, HIGH);
  delay(500);
  digitalWrite(STM_LED, LOW);
  delay(500);
  digitalWrite(STM_LED, HIGH);
  delay(500);
  digitalWrite(STM_LED, LOW);
  delay(500);
  digitalWrite(STM_LED, HIGH);
  delay(500);
  digitalWrite(STM_LED, LOW);
  DateTime now = rtc.now();

  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second());

  String currentTime = String(buffer);

  String SMS = "El rastreador: " + String(config.idRastreador) + ",";
  SMS += " esta encendido. Tiempo: " + currentTime;
  enviarSMS(SMS, String(config.admin));

  if(String(config.numUsuario) != ""){
    enviarSMS(SMS, String(config.numUsuario));
  }

  delay(2000);
}

void debugEEPROMporSMS() {
  String debug = "EEPROM:\n";
  debug += "id:" + String(config.idRastreador) + ",";
  debug += "ad:" + String(config.admin) + ",";
  debug += "us:" + String(config.numUsuario) + ",";
  debug += "mod:" + String(config.modoAhorro ? "ON" : "OFF") + ",";
  debug += "pin:" + String(config.pin);
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

            // Verifica si la ubicación es válida y hay satélites disponibles
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

    // Si NO hay conexión con satélites, actualiza los valores a 0.0 en el STM32
    if (gps1.satellites.value() == 0 || latitude == "" || longitude == "") {
        latitude = "0.0";
        longitude = "0.0";
    }

    return "lat:" + latitude + ",lon:" + longitude;
}

void corregirRTC() {
    DateTime now = rtc.now();

    if (gps1.date.isValid() && gps1.time.isValid()) {
        int gpsYear   = gps1.date.year();
        int gpsMonth  = gps1.date.month();
        int gpsDay    = gps1.date.day();
        int gpsHour   = gps1.time.hour();
        int gpsMinute = gps1.time.minute();
        int gpsSecond = gps1.time.second();

        DateTime gpsTime(gpsYear, gpsMonth, gpsDay, gpsHour, gpsMinute, gpsSecond);

        // Diferencia en segundos
        long diff = (long)now.unixtime() - (long)gpsTime.unixtime();
        if (diff < 0) diff = -diff;

        // Solo ajustar si la diferencia es significativa
        if (diff > 2) {
            rtc.adjust(gpsTime);

            static unsigned long ultimaCorreccion = 0;
            unsigned long tiempoActual = millis();

            // Enviar SMS solo si ha pasado más de 1 hora desde la última corrección
            if (tiempoActual - ultimaCorreccion > 3600000UL || ultimaCorreccion == 0) {
                ultimaCorreccion = tiempoActual;

                String mensaje = "RTC ajustado con hora GPS (diferencia " + String(diff) + "s)";
                enviarSMS(mensaje);
            }
        }
    }else {
        // GPS aún no tiene hora válida
        // (opcional) podrías forzar sincronizar con la última hora conocida
        // enviarSMS("RTC no sincronizado: GPS sin datos válidos");
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

String obtenerVoltajeBateria() {
  float voltaje = leerVoltaje(BATERIA);
  // enviarSMS("Voltaje: " + String(voltaje));
  int nivelBateria = calcularNivelBateria(voltaje);
  String sms = "nb:" + String(nivelBateria);
  return sms;
}

float leerVoltaje(int pin) {
  // Configuracion divisor de voltaje
  const float R1 = 51000.0;
  const float R2 = 20000.0;
  const float Vref = 3.3;  // referencia ADC
  const float factorDivisor = (R1 + R2) / R2;  // ≈ 3.55

  // int lecturaADC = analogRead(pin);
  float suma = 0;
  for (int i = 0; i < 10; i++) {
    suma += analogRead(pin);
    delay(5);
  }

  int lecturaADC = suma / 10;
  // Convertir lectura ADC a voltaje real de la batería
  float voltajeADC = (lecturaADC / 4095.0) * Vref;
  float voltajeBateria = voltajeADC * factorDivisor;

  return voltajeBateria;
}

int calcularNivelBateria(float v) {
  if (v >= 4.10) return 100;
  else if (v >= 4.05) return 95;
  else if (v >= 4.00) return 90;
  else if (v >= 3.95) return 85;
  else if (v >= 3.90) return 80;
  else if (v >= 3.85) return 75;
  else if (v >= 3.80) return 70;
  else if (v >= 3.75) return 65;
  else if (v >= 3.70) return 55;
  else if (v >= 3.65) return 45;
  else if (v >= 3.60) return 35;
  else if (v >= 3.50) return 20;
  else if (v >= 3.40) return 10;
  else return 0;
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
