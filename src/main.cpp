#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h" // Configuration locale non versionnee (voir include/secrets.example.h)
#include <esp_camera.h>
#include "mbedtls/base64.h"

// --- Inclusion des bibliothèques de l'écran ---
#include <Adafruit_GFX.h>    // Bibliothèque graphique générale
#include <Adafruit_ST7735.h> // Pilote spécifique ST7735
#include <SPI.h>

// --- Edge Impulse : bibliothèque "camera_intelligente_inferencing" placée dans le dossier lib/ ---
#include <camera_intelligente_inferencing.h>

#define USE_NETWORK 1   // 0 = test sans Wi-Fi ni MQTT, 1 = réseau activé

// --- Wi-Fi & ThingsBoard ---
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = 1883;
const char* access_token = THINGSBOARD_TOKEN;

// --- Broche pour l'état de la batterie
const int BATT_PIN = 3;   // GPIO3 (ADC1)

// --- Broches de la caméra OV7670 ---
#define PWDN_GPIO_NUM     -1 //GND
#define RESET_GPIO_NUM    -1 //GND
#define XCLK_GPIO_NUM      18
#define SIOD_GPIO_NUM      1 //SDA avec résistance 4.7k à 3V3
#define SIOC_GPIO_NUM      2 //SCL avec résistance 4.7k à 3V3
#define Y9_GPIO_NUM       14 //D7
#define Y8_GPIO_NUM       13 //D6
#define Y7_GPIO_NUM       12 //D5
#define Y6_GPIO_NUM       11 //D4
#define Y5_GPIO_NUM       10 //D3
#define Y4_GPIO_NUM        9 //D2
#define Y3_GPIO_NUM        8 //D1
#define Y2_GPIO_NUM        7 //D0
#define VSYNC_GPIO_NUM     5
#define HREF_GPIO_NUM      6
#define PCLK_GPIO_NUM      4

// --- Broches de l'écran TFT SPI ---
#define TFT_CS      38
#define TFT_DC      39
#define TFT_RST     40
#define TFT_MOSI    41
#define TFT_SCLK    42

// ===================== Réglages de la reconnaissance de chiffres =====================
#define CAM_W                 320   // largeur de l'image caméra (QVGA)
#define CAM_H                 240   // hauteur de l'image caméra (QVGA)
#define ROI_SIZE              196   // côté (en pixels caméra) de la zone analysée, centrée dans l'image.
                                    // Doit être un multiple de 28 : 168, 196 ou 224 par exemple.
#define CONF_THRESHOLD        0.70f // confiance minimale pour accepter un chiffre
#define STABLE_COUNT          3     // nombre de résultats identiques consécutifs avant d'afficher/envoyer
#define INFERENCE_INTERVAL_MS 300   // délai minimal entre deux reconnaissances
#define MIN_CONTRAST          40    // écart min. (0-255) entre pixel le plus clair et le plus sombre, sinon "pas de chiffre"
#define AUTO_CONTRAST         1     // 1 = étire le contraste de la zone avant de l'envoyer au modèle
#define INVERT_INPUT          0     // 1 = inverse noir/blanc (à essayer si tu écris en noir sur papier blanc
                                    //     et que le modèle a été entraîné sur des chiffres blancs sur fond noir, style MNIST)
#define DEBUG_DUMP_INPUT      0     // 1 = affiche dans le moniteur série ce que voit le modèle (28x28 en ASCII)
// =====================================================================================

#define NN_W EI_CLASSIFIER_INPUT_WIDTH
#define NN_H EI_CLASSIFIER_INPUT_HEIGHT

#define ROI_X0    ((CAM_W - ROI_SIZE) / 2)
#define ROI_Y0    ((CAM_H - ROI_SIZE) / 2)
#define ROI_BLOCK (ROI_SIZE / NN_W)   // nombre de pixels caméra moyennés pour obtenir 1 pixel du modèle

// Cadre jaune dessiné sur l'écran (l'image affichée fait la moitié de l'image caméra : 160x120)
#define ROI_DX0 (ROI_X0 / 2)
#define ROI_DX1 ((ROI_X0 + ROI_SIZE) / 2 - 1)
#define ROI_DY0 (ROI_Y0 / 2)
#define ROI_DY1 ((ROI_Y0 + ROI_SIZE) / 2 - 1)

static_assert(NN_W == NN_H, "Le modele doit avoir une entree carree");
static_assert(NN_W * NN_H == EI_CLASSIFIER_RAW_SAMPLE_COUNT, "Le modele doit etre en niveaux de gris (1 canal)");
static_assert(ROI_SIZE % NN_W == 0, "ROI_SIZE doit etre un multiple de la taille d'entree du modele (28)");
static_assert(ROI_SIZE <= CAM_H, "ROI_SIZE trop grand pour l'image camera");

// On crée un pointeur pour l'écran pour pouvoir l'initialiser manuellement après le bus SPI
Adafruit_ST7735 *tft = nullptr;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastSend = 0;
unsigned long lastInference = 0;
unsigned long lastMqttAttempt = 0;

// Image 28x28 en niveaux de gris envoyée au modèle
static uint8_t gray_input[NN_W * NN_H];

// Dernier chiffre reconnu de façon stable (-1 = aucun)
static int stableDigit = -1;
static int stablePct = 0;

// ---------------------------------------------------------------------------
// Caméra
// ---------------------------------------------------------------------------
bool init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 10000000;
    config.frame_size = FRAMESIZE_QVGA;     // 320x240
    config.pixel_format = PIXFORMAT_RGB565; // Format idéal natif compatible écran
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        // Serial.printf("Échec initialisation caméra: 0x%x\n", err);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Wi-Fi / MQTT
// ---------------------------------------------------------------------------
void setup_wifi() {
    delay(10);
    // Serial.println("\nConnexion au réseau : " + String(ssid));
    WiFi.disconnect(true);
    delay(500);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);

    WiFi.setTxPower(WIFI_POWER_11dBm);  // au lieu de 19,5 dBm par défaut
    WiFi.setSleep(true);                // modem sleep entre les paquets
    //Vérifie ensuite WiFi.RSSI() : si le signal reste meilleur que -75 dBm, tu peux descendre jusqu’à WIFI_POWER_8_5dBm.
    
    // On abandonne après 20 s pour que l'écran et la reconnaissance fonctionnent même sans Wi-Fi
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(500);
        // Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        // Serial.println("\nWi-Fi connecté !");
        if (tft) { tft->println("WiFi OK !"); }
    } else {
        // Serial.println("\nWi-Fi indisponible : fonctionnement hors ligne.");
        if (tft) { tft->println("WiFi KO"); }
    }
}

// Version non bloquante : si MQTT tombe, l'écran et la reconnaissance continuent de tourner
void mqtt_maintain() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (client.connected()) {
        client.loop();
        return;
    }

    unsigned long now = millis();
    if (lastMqttAttempt != 0 && now - lastMqttAttempt < 5000) return;
    lastMqttAttempt = now;

    // Serial.print("Tentative de connexion MQTT à ThingsBoard...");
    String clientId = "ESP32S3Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), access_token, nullptr)) {
        // Serial.println(" connecté !");
    } else {
        // Serial.print(" Échec, rc=");
        // Serial.print(client.state());
        // Serial.println(" Nouvelle tentative dans 5 secondes.");
    }
}

// Envoie seulement le chiffre reconnu (petit message, envoyé uniquement quand il change)
static void publish_digit() {
    if (!client.connected()) return;
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"digit\":%d,\"confidence\":%d}", stableDigit, stablePct);
    client.publish("v1/devices/me/telemetry", msg);
}


// Envoie l'image (JPEG en base64) + le dernier chiffre reconnu
static void send_image(camera_fb_t *fb) {
   // uint8_t * jpeg_buf = NULL;
   // size_t jpeg_len = 0;
   //  bool converted = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, fb->format, 70, &jpeg_buf, &jpeg_len);
    
   //JPEG environ 4 fois plus léger, donc moins de 200 ms pour l'envoyer à ThingsBoard (même avec le Wi-Fi à 11 dBm)
   static uint16_t *small_buf = nullptr;   // 160*120*2 = 38 400 octets en PSRAM
    if (!small_buf) small_buf = (uint16_t *)ps_malloc(160 * 120 * 2);
    if (!small_buf) return;

    const uint16_t *src = (const uint16_t *)fb->buf;
    for (int y = 0; y < 120; y++)
        for (int x = 0; x < 160; x++)
            small_buf[y * 160 + x] = src[(y * 2) * CAM_W + x * 2];

    uint8_t *jpeg_buf = NULL;
    size_t jpeg_len = 0;
    bool converted = fmt2jpg((uint8_t *)small_buf, 160 * 120 * 2, 160, 120,
                            PIXFORMAT_RGB565, 45, &jpeg_buf, &jpeg_len);
    if (!converted) {
        // Serial.println("Conversion JPEG échouée.");
        return;
    }

    size_t output_len = 0;
    mbedtls_base64_encode(NULL, 0, &output_len, jpeg_buf, jpeg_len);

    unsigned char * base64_buf = (unsigned char *)ps_malloc(output_len + 1);
    if (base64_buf != NULL) {
        size_t actual_output_len = 0;
        mbedtls_base64_encode(base64_buf, output_len, &actual_output_len, jpeg_buf, jpeg_len);
        base64_buf[actual_output_len] = '\0';

        //Récupération de l'état de la batterie (en pourcentage)
        float v = analogReadMilliVolts(BATT_PIN) * 2 / 1000.0;   // volts
        int pct = constrain(map(v * 100, 300, 450, 0, 100), 0, 100);
        
        // Le texte base64 n'est pas copié dans le document JSON (simple pointeur) : un petit document suffit
        StaticJsonDocument<256> doc;
        doc["camera_feed"] = (const char*)base64_buf;
        doc["digit"] = stableDigit;
        doc["confidence"] = stablePct;
        doc["battery"] = pct; // pourcentage de batterie (0-100)

        String payload;
        serializeJson(doc, payload);

        if (client.publish("v1/devices/me/telemetry", payload.c_str())) {
            // Serial.println("Image transmise à ThingsBoard.");
        } else {
            // Serial.println("Échec de l'envoi de l'image.");
        }
        free(base64_buf);
    }
    free(jpeg_buf);
}

// ---------------------------------------------------------------------------
// Affichage
// ---------------------------------------------------------------------------

// Affiche l'image caméra (160x120) + cadre jaune de la zone analysée par le modèle
static void draw_frame(const camera_fb_t *fb) {
    static uint16_t lineBuf[160]; // buffer d'une ligne
    const uint16_t *src = (const uint16_t *)fb->buf;

    tft->startWrite();
    tft->setAddrWindow(0, 0, 160, 120); // les 8 lignes du bas restent libres pour le texte

    for (int y = 0; y < 120; y++) {
        const uint16_t *row = src + (y * 2) * CAM_W; // échantillonnage 320x240 -> 160x120
        for (int x = 0; x < 160; x++) {
            uint16_t pixel = row[x * 2];
            // Inversion d'octets obligatoire pour les couleurs de cet écran
            lineBuf[x] = (pixel >> 8) | (pixel << 8);
        }

        // Cadre de la zone analysée
        if (y == ROI_DY0 || y == ROI_DY1) {
            for (int x = ROI_DX0; x <= ROI_DX1; x++) lineBuf[x] = ST7735_YELLOW;
        } else if (y > ROI_DY0 && y < ROI_DY1) {
            lineBuf[ROI_DX0] = ST7735_YELLOW;
            lineBuf[ROI_DX1] = ST7735_YELLOW;
        }

        tft->writePixels(lineBuf, 160); // envoi de toute la ligne en une fois
    }
    tft->endWrite();
}

// Affiche le résultat dans la bande noire du bas de l'écran
static void show_prediction() {
    char line[32];
    if (stableDigit >= 0) {
        snprintf(line, sizeof(line), "Chiffre : %d (%d%%)", stableDigit, stablePct);
    } else {
        snprintf(line, sizeof(line), "Chiffre : ?");
    }
    tft->fillRect(0, 120, 160, 8, ST7735_BLACK);
    tft->setTextColor(stableDigit >= 0 ? ST7735_GREEN : ST7735_WHITE);
    tft->setCursor(2, 120);
    tft->print(line);
}

// ---------------------------------------------------------------------------
// Reconnaissance de chiffres (Edge Impulse)
// ---------------------------------------------------------------------------

// Découpe la zone centrale de l'image, la réduit à 28x28 (moyenne par blocs) en niveaux de gris.
// Retourne false si l'image est trop uniforme (rien à reconnaître).
static bool prepare_input(const camera_fb_t *fb) {
    const uint16_t *src = (const uint16_t *)fb->buf;
    uint8_t vmin = 255, vmax = 0;

    for (int oy = 0; oy < NN_H; oy++) {
        for (int ox = 0; ox < NN_W; ox++) {
            uint32_t sum = 0;
            for (int by = 0; by < ROI_BLOCK; by++) {
                const uint16_t *row = src + (ROI_Y0 + oy * ROI_BLOCK + by) * CAM_W + ROI_X0 + ox * ROI_BLOCK;
                for (int bx = 0; bx < ROI_BLOCK; bx++) {
                    uint16_t raw = row[bx];
                    uint16_t px = (raw >> 8) | (raw << 8); // même correction d'octets que pour l'écran
                    uint32_t r5 = (px >> 11) & 0x1F;
                    uint32_t g6 = (px >> 5) & 0x3F;
                    uint32_t b5 = px & 0x1F;
                    uint32_t r = (r5 << 3) | (r5 >> 2);
                    uint32_t g = (g6 << 2) | (g6 >> 4);
                    uint32_t b = (b5 << 3) | (b5 >> 2);
                    sum += (77 * r + 150 * g + 29 * b) >> 8; // gris = 0.30 R + 0.59 G + 0.11 B
                }
            }
            uint8_t v = (uint8_t)(sum / (ROI_BLOCK * ROI_BLOCK));
            gray_input[oy * NN_W + ox] = v;
            if (v < vmin) vmin = v;
            if (v > vmax) vmax = v;
        }
    }

    if ((int)vmax - (int)vmin < MIN_CONTRAST) return false; // image uniforme

#if AUTO_CONTRAST
    for (int i = 0; i < NN_W * NN_H; i++) {
        gray_input[i] = (uint8_t)(((uint32_t)(gray_input[i] - vmin) * 255) / (vmax - vmin));
    }
#endif
#if INVERT_INPUT
    for (int i = 0; i < NN_W * NN_H; i++) gray_input[i] = 255 - gray_input[i];
#endif

#if DEBUG_DUMP_INPUT
    const char *ramp = " .:-=+*#%@";
    for (int y = 0; y < NN_H; y++) {
        for (int x = 0; x < NN_W; x++) // Serial.write(ramp[gray_input[y * NN_W + x] * 9 / 255]);
        // Serial.println();
    }
    // Serial.println();
#endif
    return true;
}

// Fournit l'image au classifieur : Edge Impulse attend un pixel = 0xRRGGBB stocké dans un float
// (pour un modèle "Grayscale", il refait lui-même la conversion en gris)
static int ei_get_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        uint32_t g = gray_input[offset + i];
        out_ptr[i] = (float)((g << 16) | (g << 8) | g);
    }
    return 0;
}

static void update_prediction(const camera_fb_t *fb) {
    int digit = -1;
    int pct = 0;

    if (prepare_input(fb)) {
        ei::signal_t signal;
        signal.total_length = NN_W * NN_H;
        signal.get_data = &ei_get_data;

        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
        if (err != EI_IMPULSE_OK) {
            // Serial.printf("Erreur du classifieur Edge Impulse : %d\n", (int)err);
            return;
        }

        int best = 0;
        float bestVal = 0.0f;
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            if (result.classification[i].value > bestVal) {
                bestVal = result.classification[i].value;
                best = (int)i;
            }
        }
        if (bestVal >= CONF_THRESHOLD) {
            digit = atoi(ei_classifier_inferencing_categories[best]); // les étiquettes sont "0" à "9"
            pct = (int)(bestVal * 100.0f + 0.5f);
        }
    }

    // Stabilisation : on n'accepte un changement que s'il se répète STABLE_COUNT fois de suite
    static int candidate = -1;
    static int candCount = 0;
    if (digit == candidate) {
        if (candCount < 255) candCount++;
    } else {
        candidate = digit;
        candCount = 1;
    }

    if (candCount >= STABLE_COUNT) {
        if (candidate != stableDigit) {
            stableDigit = candidate;
            stablePct = pct;
            show_prediction();
            if (stableDigit >= 0) {
                // Serial.printf("Chiffre reconnu : %d (%d %%)\n", stableDigit, stablePct);
                publish_digit();
            }
        } //else if (digit >= 0) {
           // stablePct = pct; // met à jour la confiance sans redessiner
        //}
    }
}

// ---------------------------------------------------------------------------

void setup() {

    setCpuFrequencyMhz(160); //Tu gagnes environ 30 à 40 mA. L’inférence sera un peu plus lente, mais tu es limité par la caméra et tu as 200 ms entre deux inférences.

    // --- Broche pour l'état de la batterie ---
    analogSetPinAttenuation(BATT_PIN, ADC_11db);
    
    // 1. Démarrage du port série (on n'attend plus indéfiniment : utile sans PC branché)
    // Serial.begin(115200);
    unsigned long t0 = millis();
    // while (!Serial && millis() - t0 < 3000) { delay(10); }
    // Serial.println("\n--- DÉMARRAGE DE L'ESP32-S3 ---");

    // 2. ÉTAPE CRITIQUE : Initialiser la PSRAM en premier avant d'activer le SPI de l'écran
    // Serial.print("Initialisation de la PSRAM... ");
    if (!psramInit()) {
        // Serial.println("[ÉCHEC] PSRAM introuvable ! Arrêt.");
        while (1);
    }
    // Serial.printf("[SUCCÈS] Mémoire libre : %d octets\n", ESP.getFreePsram());

    // 3. Configuration et isolation du bus SPI pour l'écran
    // Serial.println("Initialisation du bus SPI pour l'écran...");
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

    // 4. Initialisation de l'écran TFT ST7735 avec le bus SPI personnalisé
    tft = new Adafruit_ST7735(&SPI, TFT_CS, TFT_DC, TFT_RST);
    tft->initR(INITR_BLACKTAB);
    tft->setRotation(1); // Mode paysage (160x128)
    tft->fillScreen(ST7735_BLACK);
    tft->setTextColor(ST7735_WHITE);
    tft->setTextSize(1);
    tft->setCursor(10, 10);
    tft->println("Demarrage...");

    // 5. Initialisation des composants réseau et caméra
    #if USE_NETWORK
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    if (!client.setBufferSize(45000)) {
        // Serial.println("Attention : buffer MQTT de 45000 octets non alloué.");
    }
    #else
        // Serial.println("Mode test : Wi-Fi et MQTT désactivés.");
        tft->println("Mode test (hors ligne)");
    #endif

    tft->print("Init Camera... ");
    if (!init_camera()) {
        // Serial.println("Erreur critique : Caméra impossible à initialiser.");
        tft->setTextColor(ST7735_RED);
        tft->println("CAM ERR");
        while (1);
    }
    // Serial.println("Caméra OV7670 prête !");
    tft->setTextColor(ST7735_GREEN);
    tft->println("OK");

    // Serial.printf("Modèle Edge Impulse : entrée %dx%d, %d classes\n", NN_W, NN_H, EI_CLASSIFIER_LABEL_COUNT);
    delay(500);
    show_prediction();
}

void loop() {
    #if USE_NETWORK
        mqtt_maintain();
    #endif

   
    // Récupération de l'image (effectuée en continu pour l'écran)
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        // Serial.println("Échec de la capture d'image.");
        delay(50);
        return;
    }
    if (fb->width != CAM_W || fb->height != CAM_H) {
        // Serial.printf("Taille d'image inattendue : %dx%d\n", fb->width, fb->height);
        esp_camera_fb_return(fb);
        delay(50);
        return;
    }

    // --- AFFICHAGE SUR L'ÉCRAN TFT ---
    draw_frame(fb);

    unsigned long now = millis();

    // --- RECONNAISSANCE DU CHIFFRE ---
    if (now - lastInference >= INFERENCE_INTERVAL_MS) {
        lastInference = now;
        update_prediction(fb);
    }

    #if USE_NETWORK
    // --- ENVOI MQTT VERS THINGSBOARD (Toutes les 10 secondes) ---
    if (client.connected() && now - lastSend > 10000) {
        lastSend = now;
        send_image(fb);
   } else if (now - lastInference >= INFERENCE_INTERVAL_MS) {
    lastInference = now;
    update_prediction(fb);
   }
    #endif

    esp_camera_fb_return(fb);
   delay(50); // 20 images/s max pour ne pas saturer le bus SPI de l'écran
}
