#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
// #include <ESP8266WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

static const char FRONTPAGE[] = R"EOT(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP Controller</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.2.1/dist/css/bootstrap.min.css" rel="stylesheet" integrity="sha384-iYQeCzEYFbKjA/T2uDLTpkwGzCiq6soy8tYaI1GyVh/UjpbCx/TYkiZhlZB6+fzT" crossorigin="anonymous">

</head>
<body>

    <div id="app">
        <div class="container">
            <div>
                <h4 class="mt-3">Acciones con la huella dactilar</h4>
                <div class="mb-3">
                    <label class="form-label">Correo electronico</label>
                    <input 
                        v-model="email"
                        type="email" 
                        class="form-control" 
                        placeholder="Correo"/>
                </div>
                <div class="d-flex justify-content-between">
                    <button 
                        @click="registrarHuella" 
                        class="btn btn-primary">
                        Registra
                    </button>
                    <button 
                        @click="eliminarHuella" 
                        class="btn btn-outline-danger">
                        Eliminar
                    </button>
                </div>
            </div>

            <div>
                <h4 class="mt-3">Acciones con la tarjeta RFID</h4>
                <div class="mb-3">
                    <label class="form-label">Correo electronico</label>
                    <input 
                        v-model="email"
                        type="email" 
                        class="form-control" 
                        placeholder="Correo"/>
                </div>
                <div class="d-flex justify-content-between">
                    <button 
                        @click="registrarTarjeta" 
                        class="btn btn-primary">
                        Registrar tarjeta
                    </button>
                    <button 
                        @click="eliminarTarjeta" 
                        class="btn btn-outline-danger">
                        Eliminar asociación
                    </button>
                </div>
            </div>

        </div>

    </div>


    <script src="//cdn.jsdelivr.net/npm/sweetalert2@11"></script>
    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.2.1/dist/js/bootstrap.bundle.min.js" integrity="sha384-u1OknCvxWvY5kfmNBILK2hRnQC3Pr17a+RTT6rIHI7NnikvbZlHgTPOOmMi466C8" crossorigin="anonymous"></script>
    <script src="https://unpkg.com/vue@3/dist/vue.global.js"></script>
    
    <script>
        const { createApp } = Vue
        createApp({
            mounted() {
                console.log("monda");
                this.conn = new WebSocket(`ws://${this.ip}/ws`);
                this.conn.onopen = this.onOpen;
                this.conn.onclose = this.onClose;
                this.conn.onmessage = this.onMessage;
                this.Toast = Swal.mixin({
                    toast: true,
                    position: 'top-end',
                    showConfirmButton: false,
                    timer: 5000,
                    timerProgressBar: true,
                    didOpen: (toast) => {
                        toast.addEventListener('mouseenter', Swal.stopTimer)
                        toast.addEventListener('mouseleave', Swal.resumeTimer)
                    }
                });
            },
            data() {
                return {
                    message: 'Hello Vue!',
                    ip: '192.168.0.30',
                    conn: null,
                    email: 'suarez@gmail.com',
                    Toast: null
                }
            },
            methods: {
                registrarHuella() {
                    let info = {
                        accion: 'registrar_huella',
                        email: this.email
                    }
                    this.conn.send(JSON.stringify(info));
                },
                eliminarHuella() {
                    let info = {
                        accion: 'eliminar_huella',
                        email: this.email
                    }
                    this.conn.send(JSON.stringify(info));
                },
                registrarTarjeta() {
                    let info = {
                        accion: 'registrar_tarjeta',
                        email: this.email
                    }
                    this.conn.send(JSON.stringify(info));
                },
                eliminarTarjeta() {
                    let info = {
                        accion: 'eliminar_tarjeta',
                        email: this.email
                    }
                    this.conn.send(JSON.stringify(info));
                },
                onOpen(e) {
                    console.log("Conectados al websocket");
                },
                onClose() {
                    console.log("Close websocket");
                },
                onMessage(event) {
                    console.log(event.data);
                    this.Toast.fire({
                        icon: 'success',
                        title: event.data,
                    })
                }
            }
        }).mount('#app')
    </script>
</body>
</html>
)EOT";

// Declaraciones de funciones
void peticionTest();
void authFinger();
void addHandlers();
void encenderLuces();
void apagarLuces();
int estadoLuces();
void abrirPuerta(String data);
void showMenssage(String msg);
void printHex(byte *buffer, byte bufferSize);
void printDec(byte *buffer, byte bufferSize);
void readCard();
void handleWebSocketText(AsyncWebSocketClient *client, String request);
uint8_t registrar_huella(uint32_t id);
uint32_t getIDUsiario(String email);
uint8_t deleteFingerprint(uint8_t id);
void registrar_tarjeta(uint32_t id);
void eliminar_tarjeta();

// Configuración del wifi
const char *SSID = "arduino";
const char *PASS = "arduino123456";
StaticJsonDocument<300> docUsuario;
String baseURL = "http://blooming-tundra-94814.herokuapp.com";

// pines a utilizar
const int PUERTA = 5;
const int LUCES = 4;
int FINGER_RX = 0; // RX
int FINGER_TX = 2; // TX

#define SS_PIN 15  // D8
#define RST_PIN 16 // D0

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

byte nuidPICC[4];

// Intervalos de tiempos
const unsigned long intervaloFinger = 500UL;
unsigned long eventoFinger = 500UL;

const unsigned long intervaloRFID = 1000UL;
unsigned long eventoRFID = 1000UL;

// objetos para trabajar con internet
ESP8266WiFiMulti WiFiMulti;
HTTPClient http;
WiFiClient wifiClient;

bool registrarHuella = false;
bool eliminarHuella = false;
bool registrarTarjeta = false;
bool eliminarTarjeta = false;
// Servidor web
// ESP8266WebServer server(80);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  String msg = "";

  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    
    if (info->final && info->index == 0 && info->len == len)
    {
      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < info->len; i++)
        {
          msg += (char)data[i];
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < info->len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          msg += buff;
        }
      }
      if (info->opcode == WS_TEXT)
        //Serial.println(msg);
        handleWebSocketText(client, msg);
    }
    else
    {
      // message is comprised of multiple frames or the frame is split into multiple packets
      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < len; i++)
        {
          msg += (char)data[i];
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          msg += buff;
        }
      }
      Serial.printf("%s\n", msg.c_str());
      if ((info->index + len) == info->len)
      {
        if (info->final)
        {
          if (info->message_opcode == WS_TEXT)
            //Serial.println(msg);
            handleWebSocketText(client, msg);
        }
      }
    }
    break;
  }
}

StaticJsonDocument<500> docWebSocket;
String correo;
void handleWebSocketText(AsyncWebSocketClient *client, String request) {
    
  DeserializationError err = deserializeJson(docWebSocket, request);
  if (err) {
    Serial.println("Error al deserializar el json");
    return;
  }

  String accion = docWebSocket["accion"];
  if (accion == "registrar_huella") {
    
    String correol = docWebSocket["email"];
    correo = correol;
    Serial.printf("Registro de huella: %s\n", correo.c_str());
    registrarHuella = true;  
  }

  if (accion == "eliminar_huella") {
    String correol = docWebSocket["email"];
    correo = correol;
    Serial.printf("Registro de huella: %s\n", correo.c_str());
    eliminarHuella = true;
  }

  if (accion == "registrar_tarjeta") {
    String correol = docWebSocket["email"];
    correo = correol;
    Serial.printf("Registro de tarjeta RFID: %s\n", correo.c_str());
    registrarTarjeta = true;
  }

  if (accion == "eliminar_tarjeta") {
    String correol = docWebSocket["email"];
    correo = correol;
    eliminarTarjeta = true;
  }
  
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Configuración de display oled
#define SCREEN_WIDTH 128 // ancho pantalla OLED
#define SCREEN_HEIGHT 32 // alto pantalla OLED
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// configuración para el fingerprint
SoftwareSerial mySerial(FINGER_RX, FINGER_TX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);

  // Definiendo puertos de salida
  pinMode(PUERTA, OUTPUT);
  pinMode(LUCES, OUTPUT);

  // Estableciendo valores por defecto
  digitalWrite(LUCES, HIGH);
  digitalWrite(PUERTA, HIGH);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("Falla al localizar el oled");
    for (;;)
      ;
  }

  display.display();
  delay(2000);

  display.drawPixel(10, 10, SSD1306_WHITE);
  display.display();
  delay(2000);

  showMenssage("Pantalla funcionando");

  // RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("[RFID]: Reader: ");
  rfid.PCD_DumpVersionToSerial();

  for (byte i = 0; i < 6; i++)
  {
    key.keyByte[i] = 0xFF;
  }

  Serial.println("Este codigo escanea");
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE);

  // // Inciamos el fingerprint
  finger.begin(57600);
  delay(10);
  if (finger.verifyPassword())
  {
    Serial.println("[FINGERPRINT]: Conexión correcta");
  }
  else
  {
    Serial.println("[FINGERPRINT]: Conexión fallida");
  }

  // configuraciones del ssid y password
  WiFiMulti.addAP(SSID, PASS);

  while (WiFiMulti.run() != WL_CONNECTED)
  {
    delay(100);
  }

  Serial.println("Datos de coneción");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());

  // addHandlers();
  // // iniciar servidor web
  // server.begin();

  initWebSocket();
  server.on(
    "/", 
    HTTP_GET, 
    [](AsyncWebServerRequest *request)
    { 
      request->send_P(200, "text/html", FRONTPAGE); 
    });

  server.begin();
}

void loop()
{
  unsigned long actual = millis();
  
  ws.cleanupClients();
  

  // Acciones por parte del usuario.
  if (registrarHuella) {
    ws.textAll("Registrando al usuario " + correo);
    uint32_t my_id = getIDUsiario(correo);
    Serial.printf("ID usuario: %d\n", my_id);
    registrar_huella(my_id);
    registrarHuella = false;
  }

  if (eliminarHuella) {
    ws.textAll("Eliminando huella");
    uint32_t my_id = getIDUsiario(correo);
    Serial.printf("ID usuario: %d\n", my_id);
    deleteFingerprint(my_id);
    ws.textAll("Huella eliminada");
    eliminarHuella = false;
  }

  if (registrarTarjeta) {
    ws.textAll("Registrando tarjeta RFID: " + correo);
    uint32_t my_id = getIDUsiario(correo);
    Serial.printf("ID usuario: %d\n", my_id);
    ws.textAll("Colocar su llavero o tarjeta en el dispostivo");
    registrar_tarjeta(my_id);
    registrarTarjeta = false;
  }

  if (eliminarTarjeta) {
    eliminar_tarjeta();
    eliminarTarjeta = false;
  }

  // Metodos de autenticación
  if (actual > eventoFinger)
  {
    authFinger();
    eventoFinger += intervaloFinger;
  }

  if (actual > eventoRFID)
  {
    readCard();
    eventoRFID += intervaloRFID;
  }
}

void authFinger()
{
  // Serial.println("Analizando la huella");
  int contador = 0;
  bool buscar = true;
  uint8_t p = -1;
  // while (contador < 100 && buscar)
  // {

  // analizando la huella
  // USE_SERIAL.printf("Intento %d \n", contador);
  // hasta que tome la huella este while seguira iterando
  while (p != FINGERPRINT_OK)
  {
    // Serial.printf("[CONTADOR]: %d\n", contador);
    contador++;
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Huella tomada correctamente");
      //showMenssage("Huella tomada");
      ws.textAll("Huella tomada");
      // delay(1000);
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Serial.println("Error al resivir los datos");
      break;
    case FINGERPRINT_NOFINGER:
      // USE_SERIAL.println("Huella no colocada");
      // Serial.print(".");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Error en la imagen");
      break;
    default:
      break;
    }

    if (contador == 40)
    {
      return;
    }
    // delay(100);
  }

  Serial.println("Salimos");

  p = finger.image2Tz();
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Imagen convertida");
    showMenssage("Imagen convertida");
    delay(1000);
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("FINGERPRINT_IMAGEMESS");
    break;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Error al resivir la info");
    break;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Error en algunas caracteristicas");
    break;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Huella no valida");
    break;
  default:
    showMenssage("Conversión de imagen error.");
    Serial.println("Error al convertir la huella");
    return;
  }

  // buscamos la huella en la base de datos del fingerprint
  p = finger.fingerSearch();
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Busqueda realizada");
    Serial.print("ID Huella: ");
    Serial.println(finger.fingerID);
    buscar = false;
    break;
  case FINGERPRINT_NOTFOUND:
    Serial.println("Huella no valida");
    //showMenssage("Huella no valida");
    ws.textAll("Huella no valida");
    delay(3000);
    return;
  default:
    return;
  }
  // }
  // USE_SERIAL.println("Salido del while");

  if (!buscar && finger.fingerID > 0)
  {
    showMenssage("Usuario ID: " + String(finger.fingerID));
    delay(2000);

    if (WiFi.status() == WL_CONNECTED)
    {

      String url = "http://blooming-tundra-94814.herokuapp.com/api/usuarios/authentication/" + (String)finger.fingerID;
      http.begin(wifiClient, url);
      int httpCode = http.GET();
      if (httpCode > 0)
      {
        if (httpCode == 200)
        {
          // USE_SERIAL.println(http.getString());
          DeserializationError error = deserializeJson(docUsuario, http.getString());
          if (error)
          {
            Serial.println("Error al convertir le json");
            Serial.println(error.c_str());
            return;
          }

          String nombreUsuario = docUsuario["nombre"];
          bool acceso = docUsuario["acceso_valido"];

          Serial.print("Nombre: ");
          Serial.println(nombreUsuario);
          Serial.print("Acceso: ");
          Serial.println(acceso);
          if (acceso)
          {
            showMenssage("Bienvenido \n" + nombreUsuario);

            // se abre la puerta
            // digitalWrite(PUERTA, LOW);
            // digitalWrite(PUERTA, HIGH);
            String payload = http.getString();
            Serial.println(payload);
            abrirPuerta(payload);

            // uint8_t segundos = 5;
            // for (int segundo = segundos; segundo != 0; segundo--)
            // {
            //   //setStyleDefault(1);
            //   display.setCursor(0, 2);
            //   display.println(nombreUsuario);
            //   display.setTextSize(2);
            //   display.setCursor((display.width() / 2) - 5, display.height() / 2);
            //   display.println(segundo);
            //   display.display();
            //   delay(1000);
            // }

            showMenssage("Puerta cerrada");
            Serial.println("Se abre la puerta");
          }
          else
          {
            showMenssage(nombreUsuario + " no tiene permitido acceder.");
            delay(5000);
          }

          // si el usuario es profesor, por ahora todo usuario que inicie se
          // realiza estos procesos.
          if (true)
          {
            // Estas fuciones van en el otro esp,
            // se enviarn peticiones para realizar las siguientes acciones
            // valorSensor = analogRead(FOTOPIN);
            // if (valorSensor < 1000) {
            //   // prender la luz
            //   encederLuces();
            // }
            // encenderAire();
            // encenderProyector();
          }
        }
        else
        {
          showMenssage("No responde la API Rest full");
          delay(2000);
        }

        http.end();
      }

      Serial.print("Code: ");
      Serial.println(httpCode);
    }
  }

  delay(100);
}

void peticionTest()
{

  String url = "http://jsonplaceholder.typicode.com/todos/1";

  http.begin(wifiClient, url);
  int httpCode = http.GET();
  Serial.println(httpCode);
  if (httpCode > 0 && httpCode == 200)
  {
    String payload = http.getString();

    Serial.println(payload);

    http.end();
  }
}

// void addHandlers() {

//   // handlers relacionados con las luces
//   server.on("/luces-prender", [] () {
//     server.sendHeader("Access-Control-Allow-Origin", "*", true);
//     encenderLuces();
//     server.send(200, "application/json", "{\"ok\": true}");
//   });

//   server.on("/luces-apagar", [] () {
//     server.sendHeader("Access-Control-Allow-Origin", "*", true);
//     apagarLuces();
//     server.send(200, "application/json", "{\"ok\": true}");
//   });

//   server.on("/luces-estado", [] () {
//     server.sendHeader("Access-Control-Allow-Origin", "*", true);
//     int estado = estadoLuces();
//     server.send(200, "application/json", "{\"estado\": " + String(estado) + "}");
//   });

//   // handlers relacionado con la puerta
//   server.on("/puerta-abrir", [] () {
//     server.sendHeader("Access-Control-Allow-Origin", "*", true);
//     abrirPuerta("");
//     server.send(200, "application/json", "{\"ok\": true}");
//   });
// }

void encenderLuces()
{
  digitalWrite(LUCES, LOW);
}

void apagarLuces()
{
  digitalWrite(LUCES, HIGH);
}

int estadoLuces()
{
  // si es 1 esta prendido
  // si es 0 esta apagado
  return !digitalRead(LUCES);
}

HTTPClient httpPuerta;
WiFiClient wifiClientPuerta;

// abrirPuerta enviara un pulso de
// electricidad a la chapa, este pulso
// durara 2 segundos
void abrirPuerta(String data)
{
  // digitalWrite(PUERTA, LOW);
  // delay(2000);
  // digitalWrite(PUERTA, HIGH);
  ws.textAll("Puerta abierta");
  Serial.println("Abir la puerta");

  httpPuerta.begin(wifiClientPuerta, "http://192.168.0.20/puerta");
  int contador = 0;
  int httpCode = 0;
  while (httpCode != 200 && contador < 2)
  {
    httpCode = httpPuerta.POST(data);
    Serial.printf("[esp]: %d\n", httpCode);
    contador++;
  }

  if (httpCode > 0 && httpCode == 200)
  {
    Serial.println("Se abrio la puerta");
  }

  httpPuerta.end();

  // debe hacer una petición al esp de adentro
  // por metodo post
}

// showMenssage muestra texto simple
// en la pantalla
void showMenssage(String msg)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void printHex(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
   Helper routine to dump a byte array as dec values to Serial.
*/
void printDec(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

void readCard()
{
  // Serial.println("read card");
  if (!rfid.PICC_IsNewCardPresent())
  {
    return;
  }

  if (!rfid.PICC_ReadCardSerial())
  {
    return;
  }

  Serial.println("PICC type: ");
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));
  String uid_string = "";
  Serial.println("UID");
  for (byte i = 0; i < rfid.uid.size; i++)
  {
    uid_string += rfid.uid.uidByte[i];
    if (rfid.uid.uidByte[i] < 0x10)
    {
      Serial.print(" 0");
    }
    else
    {
      Serial.print(" ");
    }
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
  Serial.print("UID: "); Serial.println(uid_string);
  abrirPuerta("");

  // if (rfid.uid.uidByte[0] != nuidPICC[0] ||
  //     rfid.uid.uidByte[1] != nuidPICC[1] ||
  //     rfid.uid.uidByte[2] != nuidPICC[2] ||
  //     rfid.uid.uidByte[3] != nuidPICC[3] ) {
  //   Serial.println(F("A new card has been detected."));

  //   // Store NUID into nuidPICC array
  //   for (byte i = 0; i < 4; i++) {
  //     nuidPICC[i] = rfid.uid.uidByte[i];
  //   }

  //   Serial.println(F("The NUID tag is:"));
  //   Serial.print(F("In hex: "));
  //   printHex(rfid.uid.uidByte, rfid.uid.size);
  //   Serial.println();
  //   Serial.print(F("In dec: "));
  //   printDec(rfid.uid.uidByte, rfid.uid.size);
  //   Serial.println();
  // }
  // else Serial.println(F("Card read previously."));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

// RegistrarUsuario toma el id donde se almacenara la huella
// en el fingerprint
uint8_t registrar_huella(uint32_t id)
{

  int p = -1;

  // la primera toma de la huella
  Serial.print("Esperando un dedo válido para guardar como #");
  ws.textAll("Esperando la huella");
  Serial.println(id);
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      //showMenssage("Imagen tomada");
      ws.textAll("Imagen tomada");
      delay(2000);
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
    delay(100);
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    //showMenssage("Imagen convertida");
    ws.textAll("Imagen convertida");
    delay(2000);
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  // Serial.println("Remove finger");
  Serial.println("Remover el dedo");
  // showMenssage("Quitar la huella");
  ws.textAll("Quitar la huella del fingerprint");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  Serial.println("Vuelva a colocar el mismo dedo");
  //showMenssage("Vuelva a colocar el mismo dedo");
  ws.textAll("Vuelva a colocar el mismo dedo");
  delay(2000);
  // Segunda toma de la huella
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Imagen tomada");
      //showMenssage("Imagen tomada");
      ws.textAll("Imagen tomada");
      delay(2000);
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print("\r.");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }

    delay(100);
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Imagen convertida");
    //showMenssage("Imagen convertida");
    ws.textAll("Imagen convertida");
    delay(2000);
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  // OK converted!
  Serial.print("Creando el modelo para #");
  Serial.println(id);
  // creando el modelo de la huella

  //showMenssage("Creando el modelo");
  //delay(2000);

  ws.textAll("Creando el modelo");

  p = finger.createModel();
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Prints matched!");
    //showMenssage("Modelo creado");
    ws.textAll("Modelo creado");
    delay(2000);
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_ENROLLMISMATCH)
  {
    Serial.println("Fingerprints did not match");
    showMenssage("Las huellas dactilares no coinciden");
    delay(2000);
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    showMenssage("Error desconocido");
    delay(2000);
    return p;
  }

  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK)
  {

    Serial.print("Almacenada \n");
    showMenssage("Huella almacenada");
    ws.textAll("Huella almacenada");
    delay(2000);
    return p; // esto agrege (analizarlo mas)
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    Serial.println("Could not store in that location");
    showMenssage("No se pudo almacenar en esa ubicación");
    delay(2000);
    return p;
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    Serial.println("Error writing to flash");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }
}

uint32_t getIDUsiario(String email) {
  StaticJsonDocument<500> docCorreo;
  uint32_t id_usuario = 0;
  String url = baseURL + "/api/usuarios/email/" + email;
  http.begin(wifiClient, url);
  int httpCode = http.GET();
  if (httpCode > 0 && httpCode == 200) {
    String payload = http.getString();
    Serial.print("Payload: "); Serial.println(payload);
    http.end();

    DeserializationError err = deserializeJson(docCorreo, payload);
    if (err) {
      Serial.println("Error al convertir");
      return 0;
    }

    id_usuario = docCorreo["id"];
  }

  return id_usuario;
}

// deleteFingerprint elimina una huella
uint8_t deleteFingerprint(uint8_t id)
{
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK)
  {
    Serial.println("Deleted!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    Serial.println("Could not delete in that location");
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    Serial.println("Error writing to flash");
  }
  else
  {
    Serial.print("Unknown error: 0x");
    Serial.println(p, HEX);
  }

  return p;
}

void registrar_tarjeta(uint32_t id) {
  // Serial.println("read card");
  bool ok = true;
  while(ok) {
    if (!rfid.PICC_IsNewCardPresent())
    {
      delay(500);
      continue;
    }

    if (!rfid.PICC_ReadCardSerial())
    {
      delay(500);
      continue;
    }
    Serial.println("PICC type: ");
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    Serial.println(rfid.PICC_GetTypeName(piccType));
    String uid_string = "";
    Serial.println("UID");
    for (byte i = 0; i < rfid.uid.size; i++)
    {
      uid_string += rfid.uid.uidByte[i];
    }
    Serial.print("UID: "); Serial.println(uid_string);
    
    ws.textAll("Tarjeta registrada " + uid_string + " ID: " + String(id));
    // Halt PICC
    rfid.PICC_HaltA();

    // Stop encryption on PCD
    rfid.PCD_StopCrypto1();
    ok = false;
  }

  
}

void eliminar_tarjeta() {
  ws.textAll("Tarjeta eliminada");
}