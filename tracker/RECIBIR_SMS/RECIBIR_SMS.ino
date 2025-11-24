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

// Lee toda la respuesta del módulo (incluye el cuerpo del mensaje)
String leerRespuestaCompleta(unsigned long timeout = 12000) {
    String respuesta = "";
    unsigned long inicio = millis();
    bool fin = false;

    while (millis() - inicio < timeout) {
        while (A7670SA.available()) {
            char c = A7670SA.read();
            respuesta += c;

            // Si ya llegó el final típico de una respuesta AT
            if (respuesta.indexOf("\r\nOK\r\n") != -1 ||
                respuesta.indexOf("\r\nERROR\r\n") != -1) {
                fin = true;
                break;
            }
        }

        if (fin) break;
        delay(10);
    }

    return respuesta;
}

// Extrae solo el texto del SMS de la respuesta de AT+CMGR
String extraerCuerpoSMS(String respuesta) {
    int idx = respuesta.indexOf("+CMGR:");
    if (idx == -1) return "";

    // Buscar salto de línea después del encabezado
    int inicioCuerpo = respuesta.indexOf("\r\n", idx);
    if (inicioCuerpo == -1) return "";

    // Buscar fin antes del OK
    int finCuerpo = respuesta.indexOf("\r\nOK", inicioCuerpo + 2);
    if (finCuerpo == -1) finCuerpo = respuesta.length();

    String cuerpo = respuesta.substring(inicioCuerpo + 2, finCuerpo);
    cuerpo.trim();
    return cuerpo;
}

void leerMensaje(int index) {
    // Formato de texto
    enviarComando("AT+CMGF=1", 1000);
    delay(500);
    // Leer mensaje por índice
    enviarComando(("AT+CMGR="+ String(index)).c_str(),1000);
    delay(500);

    String respuesta = leerRespuestaCompleta();
    String cuerpo = extraerCuerpoSMS(respuesta);

    if (cuerpo.length() == 0) {
        enviarSMS("No se pudo leer cuerpo SMS idx " + String(index) + "\nResp:\n" + respuesta);
    } else {
        enviarSMS("Mensaje recibido idx " + String(index) + ":\n" + cuerpo);
    }
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

// void loop() { 
//     if (A7670SA.available()) { 
//         digitalWrite(STM_LED,LOW); 

//         enviarComando("AT+CMGF=1",1000); // modo texto

//         String entrada = A7670SA.readString(); 
//         entrada.trim();

//         enviarSMS("Notificación recibida:\n" + entrada);

//         // Buscar índice solo si la notificación fue +CMTI
//         int index = extraerIndiceCMTI(entrada);
//         enviarSMS("Notificación Indice:\n" + index);

//         if (index != -1) {
//             delay(500);
//             leerMensaje(index);
//             delay(1000);
//             borrarSMS(index);
//         }
//         digitalWrite(STM_LED,HIGH); 
//     } 
// }

bool enviarSMS_Seguro(String texto, String number = "+525545464585") {

    // Asegura modo texto (rápido, NO reinicia modem)
    A7670SA.println("AT+CMGF=1");
    delay(200);

    A7670SA.print("AT+CMGS=\"");
    A7670SA.print(number);
    A7670SA.println("\"");
    delay(200);

    A7670SA.print(texto);
    delay(100);

    A7670SA.write(26); // CTRL+Z
    delay(3000);       // La red sí necesita esto

    return true;
}

// void agregarDebug(String txt) {
//     debugQueue[debugWrite] = txt;
//     debugWrite = (debugWrite + 1) % 10;
// }

// void procesarDebug() {
//     if (debugRead != debugWrite) {
//         enviarSMS_Seguro(debugQueue[debugRead]);
//         debugRead = (debugRead + 1) % 10;
//     }
// }

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
        enviarSMS("entrada1: " + entrada);

    }

    // 1. Ver si llegó algo
    // if (A7670SA.available()) {
    //     digitalWrite(STM_LED, LOW);
    //     enviarComando("AT+CMGF=1",1000); // modo texto

    //     // String entrada = A7670SA.readString();
    //     String entrada = leerSMSCompleto();
    //     entrada.trim();

    //     enviarSMS("entrada1: " + entrada);
    //     enviarSMS_Seguro("Llegó entrada: " + entrada);

    //     int index = extraerIndiceCMTI(entrada);
    //     enviarSMS("➡ Indice: " + String(index));
    //     if (index != -1) {
    //         leerMensaje(index);
    //         borrarSMS(index);
    //     }
    //     digitalWrite(STM_LED, HIGH);
    // }

    // // 2. Enviar mensajes de debug sin bloquear y sin romper nada
    // procesarDebug();
}
