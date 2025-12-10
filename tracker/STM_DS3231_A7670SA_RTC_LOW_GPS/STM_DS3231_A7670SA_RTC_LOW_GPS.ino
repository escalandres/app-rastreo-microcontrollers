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
  uint32_t firma;           // ← debe ser 0xCAFEBABE
  int idRastreador;         // ID unico del rastreador
  char receptor[16];           // Numero de telefono del receptoristrador
  char numUsuario[16];      // Numero de usuario que recibe los SMS;
  int intervaloSegundos;    // Intervalo de envio de datos en segundos
  int intervaloMinutos;     // Intervalo de envio de datos en minutos
  int intervaloHoras;       // Intervalo de envio de datos en horas
  int intervaloDias;        // Intervalo de envio de datos en dias
  bool modoAhorro;          // Modo ahorro de energia (true/false) 
  char pin[8];              // PIN para aceptar comandos SMS
  bool configurado;         // Indica si el rastreador ha sido configurado
  bool rastreoActivo;       // Indica si el rastreo está activo o no
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

  //Set Alarm to be trigged in X
  rtc.setAlarm1(rtc.now() + TimeSpan(dias, horas, minutos, segundos), DS3231_A1_Second);  // this mode triggers the alarm when the seconds match.

  alarmFired = false;
}

void configurarModoAhorroEnergia() {
  // CNMI para guardar SMS en memoria
  enviarComando("AT+CNMI=2,1,0,0,0");

  // Configurar para modo ahorro de energia
  // Desactivar LED
  pinMode(STM_LED, INPUT); // Cambiar a entrada para reducir consumo

  alarmFired = false; // Resetear bandera

  // Configurar alarma RTC
  configurarAlarma(config.intervaloDias, config.intervaloHoras, config.intervaloMinutos, config.intervaloSegundos);

  // Configurar A7670SA para sleep automatico en idle
  dormirA7670SA();

  // Configurar GPS para modo bajo consumo (si es posible)
  //configureGPS(NEO8M);
  // Aquí podrías enviar comandos específicos al GPS si soporta modos de bajo consumo

  // Configure low power
  LowPower.begin();
  // Attach a wakeup interrupt on pin, calling repetitionsIncrease when the device is woken up
  // Last parameter (LowPowerMode) should match with the low power state used
  LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, DEEP_SLEEP_MODE); // SLEEP_MODE
  delay(400);
  LowPower.deepSleep();
}

void configurarRastreoContinuo(unsigned int segundos = 45) {
  // CNMI para SMS en vivo
  enviarComando("AT+CNMI=1,2,0,0,0");
  // RTC alarma cada X segundos
  configurarAlarma(0,0,0,segundos);

  // Activar interrupción en FALLING
  attachInterrupt(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING);
}

// ---------- Funciones del A7670SA ----------

void enviarComando(const char* comando, int espera = 1000) {
  A7670SA.println(comando);
  delay(espera);
}

void iniciarA7670SA(){
  //digitalWrite(LEFT_LED, HIGH);
  // Probar comunicación AT
  enviarComando("AT", 500);

  // Establecer modo LTE (opcional)
  enviarComando("AT+CNMP=2", 1000);

  // Confirmar nivel de señal y registro otra vez
  enviarComando("AT+CSQ", 500);
  enviarComando("AT+CREG?", 1000);

  enviarComando("AT+CMGF=1",1000); // modo texto

}

// void dormirA7670SA() {
//   if (dormir) {
//     enviarComando("AT+CSCLK=1");
//     delay(300);
//     digitalWrite(SLEEP_PIN, HIGH);  // HIGH permite que el módulo entre en sleep
//   } else {
//     digitalWrite(SLEEP_PIN, LOW);   // LOW despierta el módulo
//     delay(300);
//     enviarComando("AT+CSCLK=0");
//     enviarComando("AT");
//   }
// }
void dormirA7670SA() {
  digitalWrite(SLEEP_PIN, LOW);   // LOW despierta el módulo
  delay(300);
  enviarComando("AT+CSCLK=0");
  enviarComando("AT");
}

void despertarA7670SA() {
  enviarComando("AT+CSCLK=1");
  delay(300);
  digitalWrite(SLEEP_PIN, HIGH);  // HIGH permite que el módulo entre en sleep
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

void enviarMensaje(String mensaje, String number) {

  enviarComando("AT+CMGF=1",500);

  enviarComando(("AT+CMGS=\"" + number + "\"").c_str(), 2000); //Mobile phone number to send message

  A7670SA.println(mensaje);
  delay(500);
  A7670SA.println((char)26);// ASCII code of CTRL+Z
  delay(500);
  _buffer = _readSerial();

  delay(1000);
}

void enviarSMS(String SMS, String number = config.receptor)
{
  enviarComando("AT+CMGF=1",500);

  enviarComando(("AT+CMGS=\"" + number + "\"").c_str(), 2000); //Mobile phone number to send message

  A7670SA.println(mensaje);
  delay(500);
  A7670SA.println((char)26);// ASCII code of CTRL+Z
  delay(500);
  _buffer = _readSerial();

  delay(1000);
}

// bool esperarRegistroRed(unsigned long timeout = 30000) {
//   unsigned long start = millis();
//   int intentos = 0;
  
//   while (millis() - start < timeout) {
//     limpiarBufferA7670SA();
    
//     enviarComando("AT+CREG?\r");
//     delay(500);
    
//     String respuesta = leerRespuestaA7670SA(2000);
    
//     // +CREG: 0,1 = registrado en red local
//     // +CREG: 0,5 = registrado en roaming
//     if (respuesta.indexOf("+CREG: 0,1") != -1 || 
//         respuesta.indexOf("+CREG: 0,5") != -1) {
//       return true;
//     }
    
//     // Indicador visual cada 5 intentos
//     intentos++;
//     if (intentos % 5 == 0) {
//       digitalWrite(STM_LED, LOW);
//       delay(100);
//       digitalWrite(STM_LED, HIGH);
//     }
    
//     delay(2000);
//   }
  
//   return false;
// }

void procesarComando(String mensaje) {
    mensaje.trim();
    mensaje.toUpperCase(); // Para evitar problemas con mayúsculas/minúsculas

    // --- Verificar formato PIN=xxxxxx; ---
    if (!mensaje.startsWith("PIN=")) {
      enviarSMS(">:( Falta el prefijo PIN en el comando.", numeroRemitente);
      return;
    }

    int igual = mensaje.indexOf('=');
    int separador = mensaje.indexOf(';');
    if (separador == -1) {
      enviarSMS(">:( Formato inválido. Use: PIN=xxxxxx;COMANDO", numeroRemitente);
      return;
    }

    String pinIngresado = mensaje.substring(igual + 1, separador);
    pinIngresado.trim();
    // Validar PIN
    if (pinIngresado != config.pin) {
      if(config.numUsuario != ""){
        enviarSMS(">:( PIN incorrecto.", numeroRemitente);
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
  if (comando.indexOf("RASTREAR") != -1) {
    if (comando.indexOf("ON") != -1) {
      config.rastreoActivo = true;
      config.firma = 0xCAFEBABE; // Asegurar firma válida
      guardarConfigEEPROM();
      if(!config.modoAhorro){
        // Rastreo sin modo ahorro
        configurarRastreoContinuo(45); // Cada 45 segundos
        enviarSMS("Rastreo Continuo ACTIVADO. Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC(), String(config.receptor));

        if(config.numUsuario != ""){
          enviarSMS("^_^ Rastreo Continuo ACTIVADO.\nIntervalo de activacion: 45 segundos", String(config.numUsuario));
        }
      }else{
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
    } else if (comando.indexOf("OFF") != -1) {
      config.rastreoActivo = false;
      config.firma = 0xCAFEBABE;
      enviarComando("AT+CNMI=1,2,0,0,0"); // Configurar notificaciones SMS en vivo
      guardarConfigEEPROM();
      enviarSMS("Rastreo DESACTIVADO. Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC(), String(config.receptor));
      if(config.numUsuario != ""){
        enviarSMS("-_- Rastreo DESACTIVADO.", String(config.numUsuario));
      }
    }
  }
  
  // --- MODO AHORRO ---
  else if (comando.indexOf("MODOAHORRO=") != -1) {
    if (comando.indexOf("ON") != -1) {
      // Activar modo ahorro
      config.modoAhorro = true;
    } else if (comando.indexOf("OFF") != -1) {
      // Desactivar modo ahorro
      config.modoAhorro = false;
    } else {
      if(config.numUsuario != ""){
        enviarSMS("Use: MODOAHORRO=ON o OFF");
      }
      return;
    }
    
    config.firma = 0xCAFEBABE;
    guardarConfigEEPROM();
    
    String msg = "Modo ahorro: ";
    msg += config.modoAhorro ? "ON" : "OFF";
    msg += ". Rastreador: " + String(config.idRastreador) + ". Time: " + obtenerTiempoRTC();
    enviarSMS(msg, String(config.receptor));
    if(config.numUsuario != ""){
      enviarSMS(msg, String(config.numUsuario));
    }
  }
  
  // --- INTERVALO ---
  else if (comando.indexOf("INTERVALO=") != -1) {
    String valor = comando.substring(10);
    valor.trim();
    
    if (valor.length() == 0) {
      if(config.numUsuario != ""){
        enviarSMS(">:( Formato: INTERVALO=5M o 1H30M");
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
        enviarSMS(">:( Formato inválido. Use: 5M, 1H30M, 1D2H");
      }
      return;
    }
    
    // Validar que no sea todo cero
    if (config.intervaloSegundos == 0 && config.intervaloMinutos == 0 &&
        config.intervaloHoras == 0 && config.intervaloDias == 0) {
      if(config.numUsuario != ""){
        enviarSMS(">:( Intervalo no puede ser 0");
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
      enviarSMS(";-) " + resumen, numeroRemitente);
    }
  }
  
  // --- SETNUM (solo receptor) ---
  else if (comando.indexOf("SETNUM=") != -1) {
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
  
  // --- STATUS ---
  else if (comando.indexOf("STATUS") != -1) {
    String info = "STATUS:\n";
    info += "ID: " + String(config.idRastreador) + ";";
    info += "RASTREO: " + String(config.rastreoActivo ? "ON" : "OFF") + ";";
    info += "MODOAHORRO: " + String(config.modoAhorro ? "ON" : "OFF") + ";";
    info += "INTERVALO: ";

    if (config.intervaloDias > 0) info += String(config.intervaloDias) + "D ";
    if (config.intervaloHoras > 0) info += String(config.intervaloHoras) + "H ";
    if (config.intervaloMinutos > 0) info += String(config.intervaloMinutos) + "M ";
    if (config.intervaloSegundos > 0) info += String(config.intervaloSegundos) + "S";
    
    info += ";#NUM: " + String(strlen(config.numUsuario) > 0 ? String(config.numUsuario) + ";" : "No configurado;");

    String batteryCharge = "";
    batteryCharge = obtenerVoltajeBateria();
    
    info += batteryCharge + ";";
    if(config.numUsuario != ""){
      enviarSMS(info, String(config.numUsuario));
    }
  }
  
  // --- LOCATION ---
  else if (comando.indexOf("LOCATION") != -1) {
    String ubicacion = "Ubicación obtenida!";
    String datosGPS = leerYGuardarGPS();

    ubicacion += "\nGPS: " + datosGPS;

    String cellInfo = obtenerTorreCelular();
    
    if (cellInfo.length() > 0) {
      ubicacion += ".\nCT: " + cellInfo;
    }
    if(config.numUsuario != ""){
      enviarSMS(ubicacion, String(config.numUsuario));
    }
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
  // enviarSMS("smsCompletoDisponible", String(config.receptor));
  // enviarSMS("rxBuffer: "+rxBuffer, String(config.receptor));
  // Para recepción en vivo
  if (rxBuffer.indexOf("+CMT:") != -1) {
    // Debe tener encabezado +CMT
    int idx = rxBuffer.indexOf("+CMT:");
    if (idx == -1) return false;

    // Debe tener al menos dos saltos de línea después
    int firstNL  = rxBuffer.indexOf("\n", idx);
    if (firstNL == -1) return false;

    int secondNL = rxBuffer.indexOf("\n", firstNL + 1);
    if (secondNL == -1) return false;

    return true; // ya llegó encabezado + texto
  }
   // Para lectura por memoria (cuando hay sleep/PSM)
    if (rxBuffer.indexOf("+CMGL:") != -1 || rxBuffer.indexOf("+CMGR:") != -1) {
        // Busca al menos un salto después del encabezado
        int idx = rxBuffer.indexOf("+CMGL:");
        if (idx == -1) idx = rxBuffer.indexOf("+CMGR:");
        int firstNL = rxBuffer.indexOf("\n", idx);
        if (firstNL == -1) return false;

        // Todo lo que sigue hasta el próximo encabezado o FIN es el mensaje
        return true;
    }

    return false;
}

// bool smsCompletoDisponible() {
//     // enviarSMS("rxBuffer: "+rxBuffer, String(config.receptor));
//     if (rxBuffer.indexOf("+CMT:") != -1 ||
//         rxBuffer.indexOf("+CMGL:") != -1 ||
//         rxBuffer.indexOf("+CMGR:") != -1) {
//         // Basta con que haya un encabezado y al menos un salto
//         int idx = rxBuffer.indexOf("+CMT:");
//         if (idx == -1) idx = rxBuffer.indexOf("+CMGL:");
//         if (idx == -1) idx = rxBuffer.indexOf("+CMGR:");
//         int firstNL = rxBuffer.indexOf("\r\n", idx);
//         if (firstNL == -1) return false;
//         return true;
//     }
//     return false;
// }

String obtenerSMS() {
    int idx = rxBuffer.indexOf("+CMT:");
    int start = rxBuffer.indexOf("\n", idx) + 1;
    int end   = rxBuffer.indexOf("\n", start);

    String sms = rxBuffer.substring(start, end);
    sms.trim();

    // Limpiar lo consumido
    rxBuffer = rxBuffer.substring(end);

    return sms;
}

// String obtenerSMS() {
//     int hdr = rxBuffer.indexOf("+CMGL:");
//     if (hdr == -1) hdr = rxBuffer.indexOf("+CMGR:");
//     if (hdr == -1) hdr = rxBuffer.indexOf("+CMT:");
//     if (hdr == -1) return "";

//     int start = rxBuffer.indexOf("\r\n", hdr);
//     if (start == -1) return "";
//     start += 2; // saltar CRLF

//     int end = rxBuffer.indexOf("\r\nOK", start);
//     if (end == -1) end = rxBuffer.length();

//     String cuerpo = rxBuffer.substring(start, end);
//     cuerpo.trim();
//     return cuerpo;
// }

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
    enviarSMS("Hay novedades de tu rastreador!", String(config.numUsuario));
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
  // DateTime now = rtc.now();

  // char buffer[20];
  // sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d",
  //       now.year(), now.month(), now.day(),
  //       now.hour(), now.minute(), now.second());

  String currentTime = obtenerTiempoRTC();

  String SMS = "El rastreador: " + String(config.idRastreador) + ",";
  SMS += " esta encendido. Tiempo: " + currentTime + ", config: " + String(config.configurado ? "OK" : "NO") + ". " + "ModoAhorro: " + String(config.modoAhorro ? "ON" : "OFF") + "."
    + "Rastreo: " + String(config.rastreoActivo ? "ON" : "OFF") + ".";
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

String obtenerVoltajeBateria() {
  float voltaje = leerVoltaje(BATERIA);
  // enviarSMS("Voltaje: " + String(voltaje));
  int nivelBateria = calcularNivelBateria(voltaje);
  String sms = "bat:" + String(nivelBateria);
  return sms;
}

/* Configuración de puertos */

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

  delay(5000);

  enviarComando("AT+CMGF=1",1000); // modo texto

  notificarEncendido();

  // enviarComando("AT+CNM I=1,2,0,0,0"); // Configurar notificaciones SMS en vivo


  if(config.rastreoActivo && config.modoAhorro){
    configurarModoAhorroEnergia();
  }

  digitalWrite(STM_LED,HIGH);
}

// void loop() {
//   // Siempre escuchar fragmentos entrantes
//   actualizarBuffer();

//   if (config.rastreoActivo == true) {
//     if (alarmFired) {
//       pinMode(STM_LED, OUTPUT);
//       delay(200);

//       encenderLED();
//       if(config.modoAhorro){
//         // Encender A7670SA
//         despertarA7670SA();
//         iniciarA7670SA();

//       }
//       delay(200);
//       // enviarSMS("Hola", String(config.numUsuario));

//       rxBuffer = "";  // limpiar antes
//       // enviarComando("AT+CPMS=\"ME\",\"ME\",\"ME\"", 1000);
//       // delay(300);

//       enviarComando("AT+CPMS?", 1000);
//       unsigned long t0 = millis();
//       while (millis() - t0 < 1000) actualizarBuffer();
//       enviarSMS("rxBuffer 1:", rxBuffer);
//       delay(1000);
//       rxBuffer = "";
//       enviarComando("AT+CMGL=\"REC UNREAD\"", 1500);
//       t0 = millis();
//       while (millis() - t0 < 2000) actualizarBuffer();
//       enviarSMS("rxBuffer 2:", rxBuffer);

//       // Esperar y acumular toda la respuesta
//       t0 = millis();
//       while (millis() - t0 < 2000) {
//           actualizarBuffer();
//       }

//       // Revisar si hay mensajes SMS pendientes
//       if (smsCompletoDisponible()) {
//           String mensaje = obtenerSMS();
//           enviarSMS("Comando: " + mensaje, String(config.numUsuario));
//           procesarComando(mensaje, String(config.receptor));
//       }

//       // Leer GPS
//       String datosGPS = leerYGuardarGPS();

//       // Enviar datos de rastreo
//       enviarDatosRastreador(datosGPS);

//       // Espera sin perder data
//       t0 = millis();
//       while (millis() - t0 < 3000) {
//         actualizarBuffer();
//       }

//       apagarLED();

//       alarmFired = false;

//       // 1. Rastreo continuo (NO dormir)
//       if (!config.modoAhorro) {
//           configurarRastreoContinuo();
//       }
//       // 2. Rastreo con ahorro → dormir STM32 y A7670SA
//       else {
//           configurarModoAhorroEnergia();
//       }
//     }

//   } else {
//     // Rastreo apagado, pero revisar SMS
//     if (smsCompletoDisponible()) {
//         encenderLED();
//         String mensaje = obtenerSMS();
//         procesarComando(mensaje, String(config.receptor));
//         apagarLED();
//     }

//     // Espera sin perder paquetes
//     unsigned long t0 = millis();
//     while (millis() - t0 < 3000) {
//       actualizarBuffer();
//     }
//   }
// }

void loop() {
  // Siempre escuchar fragmentos entrantes
  actualizarBuffer();

  if (!config.rastreoActivo) {
    // Solo escuchar SMS
    if (smsCompletoDisponible()) {
      String mensaje = obtenerSMS();
      procesarComando(mensaje);
    }

    // Espera defensiva para acumular datos
    unsigned long t0 = millis();
    while (millis() - t0 < 2000) actualizarBuffer();
  }
  else if (config.rastreoActivo && !config.modoAhorro && alarmFired) {
    // Rastreo continuo: usar RTC o millis para intervalos
    if (alarmFired) {
      alarmFired = false; // reset bandera
      if (smsCompletoDisponible()) {
        String mensaje = obtenerSMS();
        procesarComando(mensaje);
      }
      String datosGPS = leerYGuardarGPS();
      enviarDatosRastreador(datosGPS);
      configurarRastreoContinuo();
    }
  }
  else if (config.rastreoActivo && config.modoAhorro && alarmFired) {
    alarmFired = false; // reset bandera

    pinMode(STM_LED, OUTPUT);
    delay(200);

    encenderLED();

    // Despertar módem
    encenderA7670SA();
    iniciarA7670SA();

    // Leer SMS pendientes desde memoria
    rxBuffer = "";
    enviarComando("AT+CPMS?", 1000);
    enviarComando("AT+CMGL=\"REC UNREAD\"", 2000);

    unsigned long t0 = millis();
    while (millis() - t0 < 2000) actualizarBuffer();

    if (smsCompletoDisponible()) {
      String mensaje = obtenerSMS();
      procesarComando(mensaje);
    }

    // Leer GPS y enviar datos
    String datosGPS = leerYGuardarGPS();
    enviarDatosRastreador(datosGPS);

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
