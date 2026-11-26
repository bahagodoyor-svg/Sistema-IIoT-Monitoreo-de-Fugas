
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>

// =====================================================
// 1. PINES DEL ESP32 DEVKIT V1
// =====================================================

const int PIN_FLUJO  = 5;
const int PIN_RELE   = 16;
const int PIN_LLUVIA = 17;

// =====================================================
// 2. CONFIGURACION WIFI
// =====================================================

// Colocar las credenciales de la red local
// antes de utilizar el sistema.

const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASSWORD = "TU_CONTRASENA";

// =====================================================
// 3. CONFIGURACION MQTT
// =====================================================

// Direccion IP del broker MQTT.
// Configurar con la direccion del servidor local.

const char* MQTT_SERVER = "IP_DEL_BROKER";

const int MQTT_PORT = 1883;

// Credenciales MQTT opcionales.

const char* MQTT_USER = "";
const char* MQTT_PASSWORD = "";

const char* MQTT_TOPIC = "iiot/fugas";
const char* MQTT_CLIENT_ID = "ESP32_IIOT_01";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// =====================================================
// 4. CONFIGURACION DE SENSORES
// =====================================================

const float HZ_POR_LITRO_MIN = 7.5;

const bool LLUVIA_ACTIVA_LOW = true;
const bool RELE_ACTIVO_LOW = true;

const float UMBRAL_FUGA_L_MIN = 0.20;
const unsigned long TIEMPO_FUGA_MS = 5000;

const unsigned long INTERVALO_MS = 1000;

// true = genera valores simulados.
// false = utiliza lecturas reales de sensores.

const bool MODO_PRUEBA = true;

// =====================================================
// 5. VARIABLES
// =====================================================

volatile uint32_t pulsosFlujo = 0;

uint32_t pulsosPrevios = 0;

unsigned long tiempoAnterior = 0;
unsigned long ultimoEnvio = 0;
unsigned long inicioFlujo = 0;

uint32_t numeroRegistro = 0;

float frecuenciaHz = 0.0;
float caudalLMin = 0.0;
float litrosTotales = 0.0;

bool bombaEncendida = false;
bool fugaSospechada = false;

// =====================================================
// 6. INTERRUPCION DEL SENSOR DE FLUJO
// =====================================================

void IRAM_ATTR contarPulso() {
  pulsosFlujo++;
}

// =====================================================
// 7. CONTROL DE BOMBA
// =====================================================

void controlarBomba(bool encender) {

  bombaEncendida = encender;

  if (RELE_ACTIVO_LOW) {
    digitalWrite(PIN_RELE, encender ? LOW : HIGH);
  } else {
    digitalWrite(PIN_RELE, encender ? HIGH : LOW);
  }
}

// =====================================================
// 8. COMANDOS DESDE MONITOR SERIAL
// =====================================================

void leerComandos() {

  if (!Serial.available()) return;

  String comando = Serial.readStringUntil('\n');

  comando.trim();
  comando.toUpperCase();

  if (comando == "ON") {

    controlarBomba(true);
    Serial.println("BOMBA ENCENDIDA");

  } else if (comando == "OFF") {

    controlarBomba(false);
    Serial.println("BOMBA APAGADA");
  }
}

// =====================================================
// 9. CONEXION WIFI
// =====================================================

void conectarWiFi() {

  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("Conectando a WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WIFI CONECTADO");

  Serial.print("IP DEL ESP32: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// 10. CONEXION MQTT
// =====================================================

void conectarMQTT() {

  if (mqtt.connected()) return;

  while (!mqtt.connected()) {

    Serial.print("Conectando al broker MQTT...");

    bool conectado;

    if (strlen(MQTT_USER) > 0) {

      conectado = mqtt.connect(
        MQTT_CLIENT_ID,
        MQTT_USER,
        MQTT_PASSWORD
      );

    } else {

      conectado = mqtt.connect(MQTT_CLIENT_ID);
    }

    if (conectado) {

      Serial.println(" CONECTADO");

    } else {

      Serial.print(" ERROR: ");
      Serial.println(mqtt.state());

      delay(3000);
    }
  }
}

// =====================================================
// 11. CALCULO DEL CAUDAL REAL
// =====================================================

void actualizarCaudal() {

  unsigned long ahora = millis();

  unsigned long dt = ahora - tiempoAnterior;

  if (dt < INTERVALO_MS) return;

  uint32_t total;

  noInterrupts();
  total = pulsosFlujo;
  interrupts();

  uint32_t nuevos = total - pulsosPrevios;

  frecuenciaHz = nuevos * 1000.0 / dt;

  caudalLMin = frecuenciaHz / HZ_POR_LITRO_MIN;

  litrosTotales += caudalLMin * (dt / 60000.0);

  pulsosPrevios = total;
  tiempoAnterior = ahora;
}

// =====================================================
// 12. LECTURA SENSOR DE LLUVIA
// =====================================================

bool leerSensorLluvia() {

  int estado = digitalRead(PIN_LLUVIA);

  if (LLUVIA_ACTIVA_LOW) {
    return estado == LOW;
  }

  return estado == HIGH;
}

// =====================================================
// 13. DETECCION DE FUGAS
// =====================================================

void detectarFuga(float caudalActual) {

  unsigned long ahora = millis();

  if (!bombaEncendida &&
      caudalActual > UMBRAL_FUGA_L_MIN) {

    if (inicioFlujo == 0) {
      inicioFlujo = ahora;
    }

    if (ahora - inicioFlujo >= TIEMPO_FUGA_MS) {
      fugaSospechada = true;
    }

  } else {

    inicioFlujo = 0;
    fugaSospechada = false;
  }
}

// =====================================================
// 14. GENERACION DE DATOS DE PRUEBA
// =====================================================

void generarDatosPrueba(
  float &frecuencia,
  float &caudal,
  float &litros,
  bool &lluvia
) {

  float t = millis() / 1000.0;

  caudal = 1.5 + 0.8 * sin(t * 0.4);

  frecuencia = caudal * HZ_POR_LITRO_MIN;

  litrosTotales += caudal / 60.0;

  lluvia = ((int)(t / 10) % 2 == 0);
}

// =====================================================
// 15. ENVIO DE DATOS MEDIANTE MQTT
// =====================================================

void enviarDatos() {

  unsigned long ahora = millis();

  if (ahora - ultimoEnvio < INTERVALO_MS) return;

  ultimoEnvio = ahora;

  float frecuenciaEnviar = frecuenciaHz;
  float caudalEnviar = caudalLMin;
  float litrosEnviar = litrosTotales;

  bool aguaDetectada = leerSensorLluvia();

  if (MODO_PRUEBA) {

    generarDatosPrueba(
      frecuenciaEnviar,
      caudalEnviar,
      litrosEnviar,
      aguaDetectada
    );
  }

  detectarFuga(caudalEnviar);

  numeroRegistro++;

  String mensaje = "{";

  mensaje += "\"registro\":" + String(numeroRegistro) + ",";
  mensaje += "\"tiempo_ms\":" + String(ahora) + ",";
  mensaje += "\"frecuencia_Hz\":" + String(frecuenciaEnviar, 2) + ",";
  mensaje += "\"caudal_L_min\":" + String(caudalEnviar, 3) + ",";
  mensaje += "\"litros_totales\":" + String(litrosEnviar, 3) + ",";
  mensaje += "\"agua_detectada\":" + String(aguaDetectada ? 1 : 0) + ",";
  mensaje += "\"bomba_on\":" + String(bombaEncendida ? 1 : 0) + ",";
  mensaje += "\"fuga_sospechada\":" + String(fugaSospechada ? 1 : 0);

  mensaje += "}";

  if (mqtt.connected()) {

    bool enviado = mqtt.publish(
      MQTT_TOPIC,
      mensaje.c_str()
    );

    if (enviado) {

      Serial.print("MQTT ENVIADO: ");
      Serial.println(mensaje);

    } else {

      Serial.println("ERROR AL PUBLICAR MQTT");
    }

  } else {

    Serial.println("MQTT DESCONECTADO");
  }
}

// =====================================================
// 16. CONFIGURACION INICIAL
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(PIN_RELE, OUTPUT);

  digitalWrite(
    PIN_RELE,
    RELE_ACTIVO_LOW ? HIGH : LOW
  );

  controlarBomba(false);

  pinMode(PIN_FLUJO, INPUT_PULLUP);

  attachInterrupt(
    digitalPinToInterrupt(PIN_FLUJO),
    contarPulso,
    RISING
  );

  pinMode(PIN_LLUVIA, INPUT_PULLUP);

  Serial.println();
  Serial.println("SISTEMA IIOT INICIADO");

  conectarWiFi();

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);

  mqtt.setBufferSize(512);

  conectarMQTT();

  tiempoAnterior = millis();
  ultimoEnvio = millis();

  Serial.println("PUBLICACION MQTT ACTIVADA");
  Serial.println("TOPIC: iiot/fugas");
}

// =====================================================
// 17. BUCLE PRINCIPAL
// =====================================================

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  if (!mqtt.connected()) {
    conectarMQTT();
  }

  mqtt.loop();

  leerComandos();

  if (!MODO_PRUEBA) {
    actualizarCaudal();
  }

  enviarDatos();
}
