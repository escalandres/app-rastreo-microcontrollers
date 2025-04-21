// Variables para Telegram
const String ID = "48273619";
const String token = "7621148456:AAEp3MgQVO5qfpaf2T83QvnYm9QFSYs9yKI";
const String chat_id = "94303788";
const int RED_LED = 9;

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600);
    delay(3000);  // Esperar que el módulo esté listo
    pinMode(RED_LED,OUTPUT);
    Serial.println("Iniciando Serial2...");
    
    //iniciarGPRS();  // Configurar la conexión GPRS
}

void loop() {
    enviarMensajeTelegram("Hola desde Serial2");
    delay(60000);  // Esperar 60 segundos antes de enviar otro mensaje
}

// ✅ Función para activar la conexión GPRS
void iniciarGPRS() {
    Serial.println("Activando GPRS...");
//    Serial2.println("AT+CGATT=1");
//    delay(2000);
//    Serial2.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
//    delay(1000);
    Serial2.println("AT+SAPBR=3,1,\"APN\",\"CMNET\"");
    delay(1000);
    Serial2.println("AT+SAPBR=1,1");
    delay(2000);
    Serial2.println("AT+SAPBR=2,1");
    delay(1000);
    Serial.println("✅ GPRS Activado!");
}

// ✅ Función para enviar mensajes a Telegram usando la API
void enviarMensajeTelegram(String SMS) {
    digitalWrite(RED_LED, HIGH);
Serial.println("----------------------------------------------------");
    // Construir la URL de la API de Telegram
    String api_url = "https://api.telegram.org/bot" + token + "/sendMessage?chat_id=" + chat_id + "&text=" + urlencode(SMS);
    Serial.println(api_url);
    Serial2.println("AT+CGPSOUT=1"); //Cierro el flujo de datos que recibo del GPS
    delay(1000);      
    
    Serial.println();

    Serial.println("Ejecutando comandos para enviar datos a la API...");

    Serial2.println("AT");
    delay(1000);
  
    Serial2.println("AT+HTTPTERM"); //Finalizar sesión HTTP por si hay alguna abierta antes de iniciar una sesión.
    delay(2000);
  
    Serial2.println("AT+SAPBR=0,1"); // Contexto de portador desactivado por si hay alguno activo
    delay(2000);
  
    Serial2.println("AT+CGATT=1");
    delay(2000);
    Serial2.println("AT+CGATT=1");
    delay(5000);
    Serial2.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
    delay(5000);
    Serial2.println("AT+SAPBR=3,1,\"APN\",\"internet.itelcel.com\"");
    delay(5000);
    String respuesta1 = _readSerial();
    Serial.println("Respuesta del servidor1: " + respuesta1);
    Serial2.println("AT+SAPBR=1,1");
    delay(5000);
    Serial2.println("AT+SAPBR=2,1");
    delay(5000);
    Serial.println("✅ GPRS Activado!");
    // Inicializar HTTP en el Serial2
    Serial2.println("AT+HTTPINIT");
    delay(2000);
    Serial2.println("AT+HTTPPARA=\"CID\",1");
    delay(2000);
    String peticion = "AT+HTTPPARA=\"URL\",\"" + api_url + "\"";

    Serial2.println(peticion);
    delay(2000);
    Serial2.println("AT+HTTPACTION=0");
    delay(5000);  // Esperar la respuesta

    // Leer la respuesta del servidor para verificar si el mensaje se envió correctamente
    Serial2.println("AT+HTTPREAD");
    delay(200);
    String respuesta = _readSerial();
    Serial.println("Respuesta del servidor: " + respuesta);

    // Verificar si el mensaje fue enviado correctamente
    if (respuesta.indexOf("200") > 0) {
        Serial.println("✅ Mensaje enviado correctamente a Telegram");
    } else {
        Serial.println("⚠️ Error al enviar mensaje, revisa la URL o el estado de la conexión.");
    }

    // Finalizar conexión HTTP
    Serial2.println("AT+HTTPTERM");
    delay(200);
    Serial2.println("AT+SAPBR=0,1");
    delay(200);
    digitalWrite(RED_LED, LOW);
}

// ✅ Función auxiliar para leer respuesta del Serial2
String _readSerial() {
    String respuesta = "";
    while (Serial2.available()) {  // Si hay datos en el buffer del módulo
        char c = Serial2.read();   // Leer carácter por carácter
        respuesta += c;            // Guardar en la variable
        delay(10);                 // Pequeña pausa para evitar perder datos
    }
    return respuesta;  // Devolver el contenido recibido
}

// ✅ Función para codificar espacios en la URL
String urlencode(String str) {
    str.replace(" ", "%20");
    return str;
}
