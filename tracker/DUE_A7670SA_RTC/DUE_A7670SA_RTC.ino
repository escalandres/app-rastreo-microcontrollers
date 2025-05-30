
const String number = "+525554743913"; //Telcel
const int LED = 9;
const int PUSH = 8;
const int SLEEP_PIN = 7;
int _timeout;
String _buffer;


void enviarComando(const char* comando, int espera = 1000) {
 Serial2.println(comando);
  delay(espera);

  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
  Serial.println();
}

void setup() {
    Serial.begin(9600); // Para la comunicación con el monitor serie
    Serial2.begin(115200); // Velocidad de baudios para el módulo
    delay(1000);
    pinMode(LED,OUTPUT);
    pinMode(SLEEP_PIN,OUTPUT);
    pinMode(PUSH,INPUT);
    Serial.println("--------------------------");
    Serial.println("Inicializando el módulo...");
    Serial.println("Iniciando Diagnóstico A7670SA...");
    delay(3000);
}

void startA7670SA(){
   // 1. Probar comunicación AT
  Serial.println("Verificando comunicación...");
  enviarComando("AT", 1000);

  // 5. Establecer modo LTE (opcional)
  Serial.println("Estableciendo modo LTE (CNMP=38)...");
  enviarComando("AT+CNMP=2", 2000);

  // Confirmar nivel de señal y registro otra vez
  Serial.println("Confirmando señal...");
  enviarComando("AT+CSQ", 1000);
  Serial.println("Confirmando registro...");
  enviarComando("AT+CREG?", 1000);

}

void testConection(){
  // 1. Probar comunicación AT
  Serial.println("Verificando comunicación...");
  enviarComando("AT", 1000);

  // 2. Estado de la SIM
  Serial.println("Consultando estado de SIM...");
  enviarComando("AT+CPIN?", 1000);

  // 3. Nivel de señal
  Serial.println("Consultando nivel de señal...");
  enviarComando("AT+CSQ", 1000);

  // 4. Registro en red
  Serial.println("Consultando registro en red...");
  enviarComando("AT+CREG?", 1000);

  // 5. Establecer modo LTE (opcional)
  Serial.println("Estableciendo modo LTE (CNMP=38)...");
  enviarComando("AT+CNMP=2", 2000);

  // Confirmar nivel de señal y registro otra vez
  Serial.println("Confirmando señal...");
  enviarComando("AT+CSQ", 1000);
  Serial.println("Confirmando registro...");
  enviarComando("AT+CREG?", 1000);

  Serial.println("Red...");
  enviarComando("AT+CPSI?", 1000);

  Serial.println("ATI...");
  enviarComando("ATI", 1000);

  String cellInfo = getCellInfo();
  Serial.println("Cell Info...");
  Serial.println(cellInfo);

  Serial.println("Configurar zona horaria");
  enviarComando("AT+CTIMEZONE=-6");

  Serial.println("Obteniendo Hora de la red");
  enviarComando("AT+CCLK?");  

  Serial.println("Diagnóstico terminado.");
}

void gsm(){
  // Establecer modo GSM
  Serial.println("Establecer modo GSM...");
  enviarComando("AT+CNMP=13", 2000);
  
  // Registrar en la red
  Serial.println("Registrar en la red...");
  enviarComando("AT+CREG?", 2000);
  
  // Obtener información del operador
  Serial.println("Obtener información del operador...");
  enviarComando("AT+COPS?", 2000);
  
  // Consultar calidad de la señal
  Serial.println("Consultar calidad de la señal...");
  enviarComando("AT+CSQ", 2000);

  Serial.println("Red GSM...");
  enviarComando("AT+CPSI?", 1000);

  String cellInfo = getCellInfo();
  Serial.println("Cell Info...");
  Serial.println(cellInfo);

}

void loop() {
    // Permitir interactuar manualmente desde Serial Monitor
    if (Serial.available()) {
       String receivedData = Serial.readString();
       receivedData.trim();
       Serial.println("............................");
      Serial.println("Comando: " + receivedData);
      if (receivedData == "off") {
        Serial.println("Apagando A7670SA...");
          digitalWrite(LED,HIGH);
          controlarSleepA7670SA(true);
      }

      if (receivedData == "on") {
          //digitalWrite(SLEEP_PIN,HIGH);
          Serial.println("Encendiendo A7670SA...");
          delay(1000);
          controlarSleepA7670SA(false);
          digitalWrite(LED,LOW);
      }

      if (receivedData == "test") {
          Serial.println("Probando conexion A7670SA...");
          //startA7670SA();
          testConection();
          
      }

      if (receivedData == "gsm1") {
          Serial.println("Configurando GSM...");
          gsm();
      }
    }
    if (Serial2.available()) {
      Serial.write(Serial2.read());
    }
//    if(digitalRead(PUSH) == 1){
//      controlarSleepA7670SA(false);  // Despertarlo
//      delay(2000);
////      String nb = obtenerVoltajeBateria();
////      Serial.println("nb: "+nb);
//      String cellTower = getCellInfo();
//      String sms = cellTower;
//      enviarSMS(number, sms);
//      delay(1000);
//      controlarSleepA7670SA(true);   // Dormir el módulo
//    }
    
}

void enviarSMS(String numero, String mensaje) {
    digitalWrite(LED,HIGH);
    Serial.println("Enviando SMS...");
    
    enviarComando("AT+CREG?",1000);
    enviarComando("AT+CMGF=1",1000);
//    enviarComando("AT+CSCS?",1000);
//    enviarComando("AT+CSCS=\"GSM\"",1000);
    enviarComando(("AT+CMGS=\"" + numero + "\"").c_str(), 3000);
    delay(1000);
    Serial2.print(mensaje);
    delay(500);
    Serial2.println((char)26); // Código ASCII para finalizar el SMS
    delay(500);
    Serial.println("Mensaje enviado!");
    Serial.println("Consultar el modo de red actual...");
    enviarComando("AT+CNMP?");
    Serial.println("Verificar la tecnología de conexión...");
    enviarComando("AT+CPSI?");
    Serial.println("Probando getCellInfo...");
    String cellInfo = getCellInfo();
    Serial.println(cellInfo);
    enviarComando("AT+CELLINFO?");
    digitalWrite(LED,LOW);
}

void flushA7670SA() {
  while (Serial2.available()) {
    Serial2.read();
  }
}

String readA7670SAResponse(unsigned long timeout = 2000) {
  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    while (Serial2.available()) {
      char c = Serial2.read();
      response += c;
    }
  }

  return response;
}

String getCellInfo() {
    String lac = "";
    String cellId = "";
    String mcc = "";
    String mnc = "";
    String red = "";

    // Solicitar información de la red con A7670SA
    flushA7670SA();
    Serial2.println("AT+CPSI?");
    String cpsiResponse = readA7670SAResponse();

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

    // Construir JSON corregido
    String json = "{";
    json += "\"red\":\"" + red + "\",";
    json += "\"mcc\":\"" + mcc + "\",";
    json += "\"mnc\":\"" + mnc + "\",";
    json += "\"lac\":\"" + lac + "\",";
    json += "\"cid\":\"" + cellId + "\"";
    json += "}";

    return json;
}



String hexToDec(String hexStr) {
    long decVal = strtol(hexStr.c_str(), NULL, 16);
    return String(decVal);
}

String _readSerial() {
  _timeout = 0;
  while  (!Serial2.available() && _timeout < 12000  )
  {
    delay(13);
    _timeout++;
  }
  if (Serial2.available()) {
    return Serial2.readString();
  }
}

void sendATCommand(String command, const char* expectedResponse, unsigned long timeout) {
  Serial.print(command);
  Serial2.println(command); // Send the AT command
  long int time = millis();
  while ((time + timeout) > millis()) {
    while (Serial2.available()) {
      if (Serial2.find(const_cast<char*>(expectedResponse))) {
        Serial.println(command + ": SUCCESS");
         return;
      }
    }
  }
  Serial.println(command + ": FAILED");
  delay(1000);
}

void controlarSleepA7670SA(bool dormir) {
  if (dormir) {
    enviarComando("AT+CSCLK=1");    
    delay(100);
    digitalWrite(SLEEP_PIN, LOW);  // DTR HIGH -> permite sleep en idle
    Serial.println("A7670SA en modo Sleep (cuando idle).");
  } else {
    digitalWrite(SLEEP_PIN, HIGH);   // DTR LOW -> despierta módulo
    delay(100);                     // Tiempo para que despierte
    enviarComando("AT+CSCLK=0");   
    enviarComando("AT");          // Activar UART
    Serial.println("A7670SA Despierto y sin sleep automático.");
  }
}
