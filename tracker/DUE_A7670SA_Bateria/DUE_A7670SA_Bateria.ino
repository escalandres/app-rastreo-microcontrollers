
const int pinBateria = A0; // Pin analógico donde está conectado la batería
const float voltajeMax = 4.2; // Voltaje máximo de la batería (por ejemplo, Li-Ion)
const float voltajeMin = 3.0; // Voltaje mínimo antes de considerarla descargada
const String number = "+525554743913"; //Telcel
const int LED = 9;
const int PUSH = 8;
void setup() {
    Serial.begin(9600); // Para la comunicación con el monitor serie
    Serial2.begin(115200); // Velocidad de baudios para el módulo
    delay(1000);
    pinMode(LED,OUTPUT);
    pinMode(PUSH,INPUT);
    Serial.println("--------------------------");
    Serial.println("Inicializando el módulo...");
    enviarComando("AT"); // Verifica comunicación
    enviarComando("AT+CGATT=1"); // Adjuntar a la red
    enviarComando("AT+CMGF=1"); // Configurar SMS en modo texto

}

void loop() {
    if (Serial2.available()) {
        Serial.write(Serial2.read()); // Muestra la respuesta del módulo
    }
    if(digitalRead(PUSH) == 1){
      obtenerVoltajeBateria();
      String sms = "¡Hola desde el A7670SA y Arduino! ";
      enviarSMS(number, sms);
      delay(1000);
    }
    
}

void enviarSMS(String numero, String mensaje) {
    digitalWrite(LED,HIGH);
    Serial.println("Enviando SMS...");
    enviarComando("AT+CMGF=1");
//    enviarComando("AT+CREG?");
//    enviarComando("AT+COPS?");
    enviarComando("AT+CSCA=\"" + numero + "\"");
    delay(1000);
    Serial2.print(mensaje);
    delay(500);
    Serial2.write(0x1A); // Código ASCII para finalizar el SMS
    delay(500);
    Serial.println("Mensaje enviado!");
    digitalWrite(LED,LOW);
}


void enviarComando(String comando) {
    Serial2.println(comando);
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
