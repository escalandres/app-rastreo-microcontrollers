#include <Wire.h>
#include <HardwareSerial.h>
#include <RTClib.h>

const int STM_LED = PC13;
int _timeout;
String _buffer;

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

void borrarSMS(int index)
{
    enviarComando("AT+CMGD=" + String(index), 1000);
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

    enviarComando("AT+CMGF=1",1000);
    delay(500);

    notificarEncendido();
    digitalWrite(STM_LED,HIGH);
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

int extraerIndiceCMTI(String linea) {
    linea.trim();
    if (linea.startsWith("+CMTI:")) {
        int comaIndex = linea.lastIndexOf(',');
        if (comaIndex != -1 && comaIndex < linea.length() - 1) {
        return linea.substring(comaIndex + 1).toInt();
        }
    }
    return -1;
}

String leerMensajeCompleto(int index, unsigned long timeout = 10000) {
    String respuesta = "";
    unsigned long inicio = millis();
    bool mensajeCompleto = false;

    while (millis() - inicio < timeout) {
        while (A7670SA.available()) {
            char c = A7670SA.read();
            respuesta += c;

            // Si llega "OK" o "ERROR" probablemente ya terminó
            if (respuesta.indexOf("\r\nOK\r\n") != -1 || respuesta.indexOf("\r\nERROR\r\n") != -1) {
                mensajeCompleto = true;
                break;
            }
        }

        if (mensajeCompleto) break;
        delay(10); // da tiempo al buffer UART
    }

    return respuesta;
}

void leerMensajeViejo(int index) {
    A7670SA.println("AT+CMGR=" + String(index));
    delay(1000); // pequeña pausa antes de leer
    String contenido = leerMensajeCompleto(index);
    enviarSMS("Mensaje recibido Viejo:\n" + contenido);
}

void loop() {
    if (A7670SA.available()) {
        digitalWrite(STM_LED,LOW);
        A7670SA.println("AT+CMGF=1");
        delay(500);
        String entrada = A7670SA.readString();
        entrada.trim();
        enviarSMS("Notificacion recibida: " + entrada);
        int index = extraerIndiceCMTI(entrada);
        enviarSMS("Notificacion Indice: " + String(index));
        if (index != -1) {
            leerMensajeViejo(index);
            delay(1000);
            borrarSMS(index);
        }
        digitalWrite(STM_LED,HIGH);
    }
}
