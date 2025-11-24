#include <Wire.h>
#include <HardwareSerial.h>
#include <RTClib.h>

const int STM_LED = PC13;
int _timeout;
String _buffer;
String debugQueue[10];
int debugWrite = 0;
int debugRead = 0;
String rxBuffer = "";


HardwareSerial A7670SA(PA3, PA2);

void enviarComando(String comando, int espera = 1000) {
    A7670SA.println(comando);
    delay(espera);
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

void enviarSMS(String SMS, String number = "+525545464585")
{
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

void borrarSMS(int index)
{
    enviarComando("AT+CMGD=" + String(index), 1000);
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

    String SMS = "El rastreador esta encendido.";
    enviarSMS(SMS, "+525545464585");

    delay(2000);
}

void setup() {
    // Inicializar puertos seriales
    Wire.begin();
    _buffer.reserve(50);
    A7670SA.begin(115200);

    /* Configuracion de puertos */
    pinMode(STM_LED, OUTPUT);

    digitalWrite(STM_LED, LOW);
    
    // Iniciar A7670SA
    iniciarA7670SA();

    delay(5000);

    enviarComando("AT+CMGF=1",1000); // modo texto

    // enviarComando("AT+CNMI=1,2,0,0,0", 1000); // notificaciones automáticas

    notificarEncendido();
    digitalWrite(STM_LED,HIGH);
}

String leerSMSCompleto() {
    String buffer = "";
    unsigned long t0 = millis();

    while (millis() - t0 < 3000) { // espera hasta 3 segundos
        while (A7670SA.available()) {
            char c = A7670SA.read();
            buffer += c;

            // Fin del SMS detectado
            if (buffer.endsWith("\r\n\r\n")) {
                return buffer;
            }
        }
    }

    return buffer; // Lo que se tenga si no llegó completo
}

void actualizarBuffer() {
    while (A7670SA.available()) {
        char c = A7670SA.read();
        rxBuffer += c;
    }
}

bool smsCompletoDisponible() {
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


void loop() {
    actualizarBuffer();
    if (smsCompletoDisponible()) {
        String mensaje = obtenerSMS();
        enviarSMS("entrada1: " + mensaje);
    }
}
