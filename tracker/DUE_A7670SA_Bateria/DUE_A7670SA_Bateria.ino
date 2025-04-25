
const int pinBateria = A0; // Pin analógico donde está conectado la batería
const float voltajeMax = 4.2; // Voltaje máximo de la batería (por ejemplo, Li-Ion)
const float voltajeMin = 3.0; // Voltaje mínimo antes de considerarla descargada
const String number = "+525554743913"; //Telcel
const int LED = 9;
const int PUSH = 8;
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
    pinMode(PUSH,INPUT);
    Serial.println("--------------------------");
    Serial.println("Inicializando el módulo...");
    Serial.println("Iniciando Diagnóstico A7670SA...");
  delay(3000);

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

  Serial.println("Diagnóstico terminado.");

//    enviarComando("AT+CGATT=1"); // Adjuntar a la red
//    enviarComando("AT+CMGF=1"); // Configurar SMS en modo texto

}




void loop() {
    // Permitir interactuar manualmente desde Serial Monitor
    if (Serial.available()) {
     Serial2.write(Serial.read());
    }
    if (Serial2.available()) {
      Serial.write(Serial2.read());
    }
    if(digitalRead(PUSH) == 1){
      obtenerVoltajeBateria();
      String sms = "Probando envio 123";
      enviarSMS(number, sms);
      delay(1000);
    }
    
}



void enviarSMS(String numero, String mensaje) {
    digitalWrite(LED,HIGH);
    Serial.println("Enviando SMS...");
    
    enviarComando("AT+CREG?",1000);
    enviarComando("AT+CMGF=1",1000);
    enviarComando("AT+CSCS?",1000);
    enviarComando("AT+CSCS=\"GSM\"",1000);
//    enviarComando("AT+COPS?");
    enviarComando(("AT+CMGS=\"" + numero + "\"").c_str(), 3000);
    //enviarComando("AT+CSCA=\"" + numero + "\"");
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
    Serial.println("\nProbando getNeighborCells...");
    String neighborCells = getNeighborCells();
    Serial.println(neighborCells);

    
enviarComando("AT+CELLINFO?");
enviarComando("AT+CCED=0,1");
enviarComando("AT+ZCELLINFO=2");
enviarComando("AT+ZCELLINFO=1");
enviarComando("AT+NUESTINFO");
    
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

    // Solicitar información de la red con A7670SA
    flushA7670SA();
    Serial2.println("AT+CPSI?");
    String cpsiResponse = readA7670SAResponse();

    // Extraer datos de la respuesta de AT+CPSI?
    int startIndex = cpsiResponse.indexOf("LTE,Online,");
    if (startIndex != -1) {
        int mccStart = startIndex + 11; // Después de "LTE,Online,"
        int mccEnd = cpsiResponse.indexOf("-", mccStart);
        mcc = cpsiResponse.substring(mccStart, mccEnd);

        int mncStart = mccEnd + 1;
        int mncEnd = cpsiResponse.indexOf(",", mncStart);
        mnc = cpsiResponse.substring(mncStart, mncEnd);

        // Formatear MNC a 3 dígitos (concatenar ceros si es necesario)
        while (mnc.length() < 3) {
            mnc = "0" + mnc;
        }

        int lacStart = mncEnd + 1;
        int lacEnd = cpsiResponse.indexOf(",", lacStart);
        lac = cpsiResponse.substring(lacStart, lacEnd);

        int cellIdStart = lacEnd + 1;
        int cellIdEnd = cpsiResponse.indexOf(",", cellIdStart);
        cellId = cpsiResponse.substring(cellIdStart, cellIdEnd);
    }

    // Convertir de Hex a Decimal
    lac = hexToDec(lac);

    // Construir JSON corregido
    String json = "{";
    json += "\"red\":\"LTE\",";
    json += "\"estado\":\"Online\",";
    json += "\"mcc\":\"" + mcc + "\",";
    json += "\"mnc\":\"" + mnc + "\",";
    json += "\"lac\":\"" + lac + "\",";
    json += "\"cid\":\"" + cellId + "\"";
    json += "}";

    return json;
}

String getNeighborCells() {
    String neighborCells = "{";

    // Solicitar información de torres celulares vecinas
    flushA7670SA();
    Serial2.println("AT+CGSCAN?");
    String scanResponse = readA7670SAResponse();

    // Procesar la respuesta
    int startIndex = scanResponse.indexOf("+CGSCAN:");
    if (startIndex != -1) {
        int currentIndex = startIndex;
        int cellCount = 0;

        while (cellCount < 5) { // Limitar a 5 torres vecinas
            int mccStart = scanResponse.indexOf(",", currentIndex) + 1;
            int mccEnd = scanResponse.indexOf("-", mccStart);
            String mcc = scanResponse.substring(mccStart, mccEnd);

            int mncStart = mccEnd + 1;
            int mncEnd = scanResponse.indexOf(",", mncStart);
            String mnc = scanResponse.substring(mncStart, mncEnd);

            while (mnc.length() < 3) {
                mnc = "0" + mnc;
            }

            int lacStart = mncEnd + 1;
            int lacEnd = scanResponse.indexOf(",", lacStart);
            String lac = scanResponse.substring(lacStart, lacEnd);

            int cellIdStart = lacEnd + 1;
            int cellIdEnd = scanResponse.indexOf(",", cellIdStart);
            String cellId = scanResponse.substring(cellIdStart, cellIdEnd);

            // Convertir LAC y Cell ID de HEX a DEC
            lac = hexToDec(lac);
            cellId = hexToDec(cellId);

            // Agregar al JSON
            neighborCells += "\"cell_" + String(cellCount) + "\":{";
            neighborCells += "\"mcc\":\"" + mcc + "\",";
            neighborCells += "\"mnc\":\"" + mnc + "\",";
            neighborCells += "\"lac\":\"" + lac + "\",";
            neighborCells += "\"cid\":\"" + cellId + "\"";
            neighborCells += "}";

            cellCount++;
            currentIndex = cellIdEnd + 1;

            if (scanResponse.indexOf("+CGSCAN:", currentIndex) == -1) {
                break;
            }
        }
    }

    neighborCells += "}";
    return neighborCells;
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

void obtenerVoltajeBateria(){
    float voltaje = leerVoltaje(pinBateria);
    int nivelBateria = calcularNivelBateria(voltaje);
    
    Serial.print("Voltaje: ");
    Serial.print(voltaje);
    Serial.print("V - Nivel de batería: ");
    Serial.print(nivelBateria);
    Serial.println("%");

    delay(1000);
}

float leerVoltaje(int pin) {
    int lecturaADC = analogRead(pin);
    float voltaje = (lecturaADC / 1023.0) * 5.0; // Convierte lectura ADC a voltaje (ajusta si usas divisor)
    return voltaje;
}

int calcularNivelBateria(float voltaje) {
    float porcentaje = ((voltaje - voltajeMin) / (voltajeMax - voltajeMin)) * 100;
    porcentaje = constrain(porcentaje, 0, 100); // Limita el porcentaje entre 0 y 100%
    return (int)porcentaje;
}
