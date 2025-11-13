#include <Wire.h>
#include <HardwareSerial.h>
#include <RTClib.h>

const int STM_LED = PC13;
int _timeout;
String _buffer;

HardwareSerial A7670SA(PA3, PA2);

struct SMS {
    bool exito;
    String remitente;
    String fecha;
    String cuerpo;
    String estado; // "REC READ", "REC UNREAD", etc.
};

/* ============================================
   FUNCIÓN DE LECTURA CORREGIDA
   ============================================ */

/* ============================================
   OTRAS FUNCIONES (sin cambios necesarios)
   ============================================ */

/* Limpiar buffer */
void limpiarBufferA7670SA() {
    while (A7670SA.available()) {
        A7670SA.read();
    }
    delay(50);
}

void flushA7670SA() {
    limpiarBufferA7670SA();
}

void enviarComando(const char* comando, int espera = 1000) {
    A7670SA.println(comando);
    delay(espera);
}

void iniciarA7670SA() {
    enviarComando("AT", 1000);
    enviarComando("AT+CNMP=2", 2000);
    enviarComando("AT+CSQ", 1000);
    enviarComando("AT+CREG?", 1000);
}

void enviarSMS(String SMS, String number = "+525545464585") {
    limpiarBufferA7670SA();
    enviarComando("AT+CMGF=1", 1000);
    enviarComando(("AT+CMGS=\"" + number + "\"").c_str(), 3000);
    
    A7670SA.println(SMS);
    delay(500);
    A7670SA.write(26); // CTRL+Z
    delay(2000);
    
    limpiarBufferA7670SA();
}

void borrarSMS(int index) {
    limpiarBufferA7670SA();
    A7670SA.print("AT+CMGD=");
    A7670SA.println(index);
    delay(500);
    limpiarBufferA7670SA();
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

String leerRespuestaA7670SA(unsigned long timeout = 3000) {
    String response = "";
    unsigned long startTime = millis();
    unsigned long ultimoCaracter = millis();
    const unsigned long TIEMPO_SIN_DATOS = 150; // Esperar 150ms sin datos
    
    while (millis() - startTime < timeout) {
        while (A7670SA.available()) {
        char c = A7670SA.read();
        response += c;
        ultimoCaracter = millis(); // Actualizar tiempo del último carácter
        
        // Detectar fin de respuesta AT+CMGR (termina con OK)
        if (response.endsWith("\r\nOK\r\n") || 
            response.endsWith("\nOK\n") ||
            response.endsWith("\r\nERROR\r\n")) {
            delay(50); // Pequeña espera adicional por si acaso
            while (A7670SA.available()) {
            response += (char)A7670SA.read();
            }
            return response;
        }
        }
        
        // Si no hay datos por 150ms y ya tenemos respuesta, salir
        if (response.length() > 0 && (millis() - ultimoCaracter) > TIEMPO_SIN_DATOS) {
        break;
        }
        
        delay(10);
    }
    
    return response;
}

/* ============================================
   FUNCIÓN MEJORADA para leer SMS por índice
   ============================================ */

SMS leerSMSPorIndice(int index) {
    SMS resultado;
    resultado.exito = false;
    
    // Limpiar buffer antes de enviar comando
    limpiarBufferA7670SA();
    
    // Solicitar SMS específico
    A7670SA.print("AT+CMGR=");
    A7670SA.println(index);
    
    // Esperar respuesta completa
    delay(500);
    String respuesta = leerRespuestaA7670SA(3000);
    
    // Parsear respuesta
    // Formato esperado:
    // +CMGR: "REC UNREAD","+525620577634",,"24/11/10,14:32:00-24"
    // PIN=589649;STATUS
    // 
    // OK
    
    if (respuesta.indexOf("+CMGR:") == -1) {
        return resultado; // No hay mensaje
    }
    
    // Extraer línea del header
    int posHeader = respuesta.indexOf("+CMGR:");
    int finHeader = respuesta.indexOf('\n', posHeader);
    
    if (finHeader == -1) {
        return resultado;
    }
    
    String header = respuesta.substring(posHeader, finHeader);
    
    // Extraer estado: "REC UNREAD" o "REC READ"
    int primerComilla = header.indexOf('"');
    int segundaComilla = header.indexOf('"', primerComilla + 1);
    if (primerComilla != -1 && segundaComilla != -1) {
        resultado.estado = header.substring(primerComilla + 1, segundaComilla);
    }
    
    // Extraer número de teléfono
    int tercerComilla = header.indexOf('"', segundaComilla + 1);
    int cuartaComilla = header.indexOf('"', tercerComilla + 1);
    if (tercerComilla != -1 && cuartaComilla != -1) {
        resultado.remitente = header.substring(tercerComilla + 1, cuartaComilla);
    }
    
    // Extraer cuerpo del mensaje (siguiente línea después del header)
    int inicioCuerpo = finHeader + 1;
    int finCuerpo = respuesta.indexOf("\r\n\r\nOK", inicioCuerpo);
    if (finCuerpo == -1) {
        finCuerpo = respuesta.indexOf("\n\nOK", inicioCuerpo);
    }
    if (finCuerpo == -1) {
        finCuerpo = respuesta.indexOf("\nOK", inicioCuerpo);
    }
    if (finCuerpo == -1) {
        finCuerpo = respuesta.length();
    }
    
    resultado.cuerpo = respuesta.substring(inicioCuerpo, finCuerpo);
    resultado.cuerpo.trim();
    
    resultado.exito = (resultado.cuerpo.length() > 0);
    
    return resultado;
}

/* ============================================
   VERSIÓN SIMPLIFICADA - Solo extraer cuerpo
   ============================================ */

String leerCuerpoSMS(int index) {
    limpiarBufferA7670SA();
    
    A7670SA.print("AT+CMGR=");
    A7670SA.println(index);
    
    delay(500);
    String respuesta = leerRespuestaA7670SA(3000);
    
    // Buscar +CMGR:
    int pos = respuesta.indexOf("+CMGR:");
    if (pos == -1) {
        return "ERROR: No se encontró mensaje";
    }
    
    // Saltar a la siguiente línea (el cuerpo)
    int salto = respuesta.indexOf('\n', pos);
    if (salto == -1) {
        return "ERROR: No se encontró cuerpo";
    }
    
    // Extraer hasta el OK final
    int finOK = respuesta.indexOf("\nOK", salto);
    if (finOK == -1) {
        finOK = respuesta.length();
    }
    
    String cuerpo = respuesta.substring(salto + 1, finOK);
    cuerpo.trim();
    
    return cuerpo;
}

/* ============================================
   SETUP
   ============================================ */

void setup() {
    Wire.begin();
    _buffer.reserve(50);
    A7670SA.begin(115200);
    
    pinMode(STM_LED, OUTPUT);
    digitalWrite(STM_LED, LOW);
    
    // Configurar modo texto
    enviarComando("AT+CMGF=1", 1000);
    
    // Habilitar notificaciones de SMS
    enviarComando("AT+CNMI=2,1,0,0,0", 1000);
    
    iniciarA7670SA();
    
    delay(5000);
    
    // Notificar encendido
    digitalWrite(STM_LED, HIGH);
    delay(500);
    digitalWrite(STM_LED, LOW);
    delay(500);
    digitalWrite(STM_LED, HIGH);
    
    enviarSMS("✅ Rastreador encendido", "+525545464585");
    
    delay(2000);
    digitalWrite(STM_LED, HIGH);
}

/* ============================================
   LOOP MEJORADO
   ============================================ */

void loop() {
    // Verificar si hay notificación de SMS nuevo
    if (A7670SA.available()) {
        digitalWrite(STM_LED, LOW);
        
        String entrada = A7670SA.readString();
        entrada.trim();
        
        // Extraer índice del SMS
        int index = extraerIndiceCMTI(entrada);
        
        if (index != -1) {
        // Parpadeo para indicar procesamiento
        for(int i = 0; i < 3; i++) {
            digitalWrite(STM_LED, LOW);
            delay(100);
            digitalWrite(STM_LED, HIGH);
            delay(100);
        }
        
        // MÉTODO 1: Usar la estructura completa
        SMS mensaje = leerSMSPorIndice(index);
        
        if (mensaje.exito) {
            String info = "📩 SMS de: " + mensaje.remitente + "\n";
            info += "Estado: " + mensaje.estado + "\n";
            info += "Mensaje: " + mensaje.cuerpo;
            
            enviarSMS(info, "+525545464585");
            delay(2000);
            
            // Aquí puedes procesar el comando
            if (mensaje.cuerpo.indexOf("PIN=") != -1) {
            enviarSMS("🔐 Comando detectado: " + mensaje.cuerpo, "+525545464585");
            // procesarComando(mensaje.cuerpo, mensaje.remitente);
            }
        } else {
            enviarSMS("⚠️ Error al leer SMS índice " + String(index), "+525545464585");
        }
        
        // MÉTODO 2: Solo el cuerpo (más simple)
        // String cuerpo = leerCuerpoSMS(index);
        // enviarSMS("📩 SMS: " + cuerpo, "+525545464585");
        
        // Borrar mensaje después de procesarlo
        delay(1000);
        borrarSMS(index);
        }
        
        digitalWrite(STM_LED, HIGH);
    }
    
    delay(100);
}

/* ============================================
   VERSIÓN ALTERNATIVA DEL LOOP
   Para revisar SMS periódicamente sin esperar notificación
   ============================================ */

// void loopAlternativo() {
//   static unsigned long ultimaRevision = 0;
//   unsigned long ahora = millis();
  
//   // Revisar cada 10 segundos
//   if (ahora - ultimaRevision >= 10000) {
//     ultimaRevision = ahora;
    
//     digitalWrite(STM_LED, LOW);
//     delay(50);
//     digitalWrite(STM_LED, HIGH);
    
//     // Listar mensajes no leídos
//     limpiarBufferA7670SA();
//     A7670SA.println("AT+CMGL=\"REC UNREAD\"");
//     delay(1000);
    
//     String respuesta = leerRespuestaA7670SA(3000);
    
//     if (respuesta.indexOf("+CMGL:") != -1) {
//       // Hay mensajes sin leer
//       // Extraer el primer índice
//       int pos = respuesta.indexOf("+CMGL:");
//       if (pos != -1) {
//         int coma = respuesta.indexOf(',', pos);
//         if (coma != -1) {
//           int index = respuesta.substring(pos + 7, coma).toInt();
          
//           // Leer y procesar
//           SMS mensaje = leerSMSPorIndice(index);
//           if (mensaje.exito) {
//             enviarSMS("📩 " + mensaje.remitente + ": " + mensaje.cuerpo, "+525545464585");
//             delay(2000);
//             borrarSMS(index);
//           }
//         }
//       }
//     }
//   }
  
//   delay(100);
// }
