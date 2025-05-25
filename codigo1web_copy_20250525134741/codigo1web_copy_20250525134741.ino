/*
 * MONITOR WEB LOCAL - SOLO DASHBOARD
 * Sin ThingSpeak - IP fácil de copiar
 * 
 * INSTRUCCIONES:
 * 1. Sube este código al ESP32
 * 2. Abre Monitor Serie - la IP aparecerá varias veces y se quedará quieta
 * 3. Copia la IP tranquilamente
 * 4. Abre esa IP en tu navegador
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============ CONFIGURACIÓN DE RED ============
const char* WIFI_SSID = "JuanPabloG";
const char* WIFI_PASSWORD = "12345678";

// ============ CONFIGURACIÓN DE HARDWARE ============
const int PIN_SENSOR_VOLTAJE = 33;
const int PIN_SENSOR_CORRIENTE = 32;
const float FACTOR_VOLTAJE = 8.0;
const float VOLTAJE_REF_ESP32 = 3.3;
const int RESOLUCION_ADC = 4095;

LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);

// ============ VARIABLES GLOBALES ============
float voltaje_actual = 0.0;
float corriente_actual = 0.0;
float potencia_actual = 0.0;
unsigned long tiempo_anterior = 0;
const unsigned long INTERVALO_LECTURA = 2000;  // Cada 2 segundos
const unsigned long INTERVALO_SERIE = 10000;   // Monitor Serie cada 10 segundos (más tranquilo)

// Historial para gráfico
float historial_potencia[20];
int indice_historial = 0;

// Estadísticas
float voltaje_max = 0, corriente_max = 0, potencia_max = 0;
int total_lecturas = 0;

// Control de tiempo para Monitor Serie
unsigned long tiempo_serie_anterior = 0;
bool mostrar_ip_constante = true;
int contador_ip = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Mostrar información inicial de forma clara
    Serial.println("");
    Serial.println("===============================================");
    Serial.println("    MONITOR WEB LOCAL - ENERGIA EOLICA        ");
    Serial.println("===============================================");
    Serial.println("");
    
    // Inicializar historial
    for(int i = 0; i < 20; i++) {
        historial_potencia[i] = 0;
    }
    
    pinMode(PIN_SENSOR_VOLTAJE, INPUT);
    pinMode(PIN_SENSOR_CORRIENTE, INPUT);
    
    inicializar_lcd();
    conectar_wifi();
    configurar_servidor_web();
    
    // Mostrar IP varias veces para que sea fácil de copiar
    mostrar_informacion_ip();
}

void loop() {
    server.handleClient();
    
    unsigned long tiempo_actual = millis();
    
    // Actualizar datos cada 2 segundos
    if (tiempo_actual - tiempo_anterior >= INTERVALO_LECTURA) {
        leer_sensores();
        calcular_potencia();
        actualizar_estadisticas();
        guardar_en_historial();
        actualizar_lcd();
        
        tiempo_anterior = tiempo_actual;
    }
    
    // Mostrar datos en Serie cada 10 segundos (más tranquilo)
    if (tiempo_actual - tiempo_serie_anterior >= INTERVALO_SERIE) {
        mostrar_datos_serie_tranquilo();
        tiempo_serie_anterior = tiempo_actual;
    }
    
    // Mostrar IP cada 30 segundos para recordatorio
    if (contador_ip < 5 && tiempo_actual > (contador_ip + 1) * 30000) {
        mostrar_recordatorio_ip();
        contador_ip++;
    }
    
    delay(100);
}

void inicializar_lcd() {
    Serial.println("📱 Inicializando LCD...");
    
    lcd.begin();
    lcd.backlight();
    
    lcd.setCursor(0, 0);
    lcd.print("Monitor Web Local");
    lcd.setCursor(0, 1);
    lcd.print("Energia Eolica");
    delay(2000);
    
    Serial.println("✅ LCD listo");
}

void conectar_wifi() {
    Serial.println("🌐 Conectando a WiFi: " + String(WIFI_SSID));
    Serial.println("   Contraseña: " + String(WIFI_PASSWORD));
    Serial.println("");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Conectando WiFi");
    lcd.setCursor(0, 1);
    lcd.print("Espera...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 20) {
        delay(1000);
        Serial.print(".");
        intentos++;
        
        // Actualizar LCD con progreso
        lcd.setCursor(15, 1);
        lcd.print(intentos % 2 == 0 ? "*" : " ");
    }
    
    Serial.println("");
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("🎉 ¡WiFi conectado exitosamente!");
        Serial.println("📍 Dirección IP asignada: " + WiFi.localIP().toString());
        Serial.println("📶 Intensidad de señal: " + String(WiFi.RSSI()) + " dBm");
        Serial.println("");
        
        // Mostrar en LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Conectado!");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
        delay(4000);
    } else {
        Serial.println("❌ ERROR: No se pudo conectar a WiFi");
        Serial.println("🔧 Verifica el nombre de red y contraseña");
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Error WiFi");
        lcd.setCursor(0, 1);
        lcd.print("Revisar config");
        delay(5000);
    }
}

void configurar_servidor_web() {
    Serial.println("🌐 Configurando servidor web...");
    
    // Página principal del dashboard
    server.on("/", HTTP_GET, []() {
        String html = generar_pagina_web();
        server.send(200, "text/html", html);
    });
    
    // API para obtener datos en tiempo real
    server.on("/datos", HTTP_GET, []() {
        String json = "{";
        json += "\"voltaje\":" + String(voltaje_actual, 1) + ",";
        json += "\"corriente\":" + String(corriente_actual, 2) + ",";
        json += "\"potencia\":" + String(potencia_actual, 1) + ",";
        json += "\"voltaje_max\":" + String(voltaje_max, 1) + ",";
        json += "\"corriente_max\":" + String(corriente_max, 2) + ",";
        json += "\"potencia_max\":" + String(potencia_max, 1) + ",";
        json += "\"total\":" + String(total_lecturas);
        json += "}";
        
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", json);
    });
    
    server.begin();
    Serial.println("✅ Servidor web iniciado correctamente");
    Serial.println("");
}

void mostrar_informacion_ip() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("🎯 ¡INFORMACIÓN IMPORTANTE - COPIA ESTA DIRECCIÓN!");
        Serial.println("===============================================");
        Serial.println("");
        Serial.println("📋 TU DASHBOARD WEB ESTÁ EN:");
        Serial.println("");
        Serial.println("    http://" + WiFi.localIP().toString());
        Serial.println("");
        Serial.println("📋 INSTRUCCIONES:");
        Serial.println("   1. Copia la dirección de arriba");
        Serial.println("   2. Pégala en tu navegador");
        Serial.println("   3. Presiona Enter");
        Serial.println("   4. ¡Disfruta tu dashboard!");
        Serial.println("");
        Serial.println("💡 IMPORTANTE:");
        Serial.println("   - Tu dispositivo debe estar en la misma WiFi");
        Serial.println("   - Funciona en móviles, tablets y computadoras");
        Serial.println("   - Se actualiza automáticamente cada 2 segundos");
        Serial.println("");
        Serial.println("===============================================");
        Serial.println("");
        Serial.println("⏳ El sistema comenzará a funcionar en 10 segundos...");
        Serial.println("   (Los datos aparecerán cada 10 segundos para no molestar)");
        Serial.println("");
        
        // Pausa de 10 segundos para que pueda copiar tranquilamente
        for(int i = 10; i > 0; i--) {
            Serial.println("⏰ Iniciando en " + String(i) + " segundos... (IP: http://" + WiFi.localIP().toString() + ")");
            delay(1000);
        }
        
        Serial.println("🚀 ¡Sistema iniciado! Dashboard disponible en: http://" + WiFi.localIP().toString());
        Serial.println("");
    }
}

void mostrar_recordatorio_ip() {
    Serial.println("💡 RECORDATORIO: Tu dashboard está en http://" + WiFi.localIP().toString());
    Serial.println("");
}

String generar_pagina_web() {
    String html = "<!DOCTYPE html>";
    html += "<html lang='es'>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Monitor Energia Eolica - Local</title>";
    
    // CSS mejorado y elegante
    html += "<style>";
    html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
    html += "body { font-family: 'Segoe UI', sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; color: #333; }";
    html += ".header { background: rgba(255,255,255,0.95); backdrop-filter: blur(10px); padding: 25px 0; box-shadow: 0 4px 20px rgba(0,0,0,0.1); }";
    html += ".header-content { max-width: 1200px; margin: 0 auto; padding: 0 20px; text-align: center; }";
    html += ".header h1 { font-size: 32px; color: #2d3748; margin-bottom: 8px; }";
    html += ".header p { color: #718096; font-size: 16px; margin-bottom: 15px; }";
    html += ".status-badge { display: inline-block; background: linear-gradient(45deg, #48bb78, #38a169); color: white; padding: 8px 20px; border-radius: 25px; font-size: 14px; font-weight: 600; }";
    html += ".container { max-width: 1200px; margin: 0 auto; padding: 40px 20px; }";
    html += ".metrics-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 30px; margin-bottom: 40px; }";
    html += ".metric-card { background: rgba(255,255,255,0.95); backdrop-filter: blur(10px); border-radius: 20px; padding: 30px; box-shadow: 0 10px 40px rgba(0,0,0,0.1); transition: all 0.3s ease; position: relative; overflow: hidden; }";
    html += ".metric-card:hover { transform: translateY(-8px); box-shadow: 0 15px 50px rgba(0,0,0,0.15); }";
    html += ".metric-card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 5px; background: var(--accent); }";
    html += ".voltage { --accent: linear-gradient(90deg, #ff6b6b, #ee5a52); }";
    html += ".current { --accent: linear-gradient(90deg, #4ecdc4, #44a08d); }";
    html += ".power { --accent: linear-gradient(90deg, #45b7d1, #96c93d); }";
    html += ".metric-icon { width: 60px; height: 60px; border-radius: 15px; display: flex; align-items: center; justify-content: center; font-size: 24px; color: white; margin-bottom: 15px; }";
    html += ".voltage .metric-icon { background: linear-gradient(45deg, #ff6b6b, #ee5a52); }";
    html += ".current .metric-icon { background: linear-gradient(45deg, #4ecdc4, #44a08d); }";
    html += ".power .metric-icon { background: linear-gradient(45deg, #45b7d1, #96c93d); }";
    html += ".metric-title { font-size: 16px; color: #718096; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 10px; font-weight: 600; }";
    html += ".metric-value { font-size: 42px; font-weight: 800; color: #2d3748; margin-bottom: 8px; }";
    html += ".metric-unit { font-size: 18px; color: #a0aec0; font-weight: 500; }";
    html += ".chart-section { background: rgba(255,255,255,0.95); backdrop-filter: blur(10px); border-radius: 20px; padding: 35px; margin-bottom: 30px; box-shadow: 0 10px 40px rgba(0,0,0,0.1); }";
    html += ".chart-title { font-size: 24px; font-weight: 700; color: #2d3748; margin-bottom: 25px; text-align: center; }";
    html += ".chart-container { height: 250px; display: flex; align-items: end; justify-content: space-around; background: linear-gradient(135deg, #f8fafc, #e2e8f0); border-radius: 15px; padding: 20px; box-shadow: inset 0 2px 10px rgba(0,0,0,0.05); }";
    html += ".chart-bar { background: linear-gradient(180deg, #4facfe 0%, #00f2fe 100%); border-radius: 6px 6px 0 0; width: 25px; min-height: 6px; margin: 0 2px; transition: all 0.4s ease; position: relative; }";
    html += ".chart-bar:hover { background: linear-gradient(180deg, #43e97b 0%, #38f9d7 100%); transform: scaleY(1.05) scaleX(1.1); }";
    html += ".stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 25px; }";
    html += ".stat-card { background: rgba(255,255,255,0.9); backdrop-filter: blur(10px); padding: 25px; border-radius: 15px; text-align: center; box-shadow: 0 5px 20px rgba(0,0,0,0.08); }";
    html += ".stat-label { font-size: 14px; color: #718096; text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 10px; font-weight: 600; }";
    html += ".stat-value { font-size: 28px; font-weight: 700; color: #2d3748; }";
    html += ".footer { background: rgba(255,255,255,0.95); backdrop-filter: blur(10px); padding: 20px 0; text-align: center; color: #718096; font-size: 14px; }";
    html += ".update-indicator { position: fixed; top: 20px; right: 20px; background: #48bb78; color: white; padding: 10px 15px; border-radius: 25px; font-size: 12px; z-index: 1000; }";
    html += "@media (max-width: 768px) { .metrics-grid { grid-template-columns: 1fr; } .metric-value { font-size: 32px; } .chart-container { height: 200px; } }";
    html += "</style>";
    
    html += "</head>";
    html += "<body>";
    
    // Indicador de actualización
    html += "<div class='update-indicator' id='update-indicator'>🔄 Conectando...</div>";
    
    // Header
    html += "<div class='header'>";
    html += "<div class='header-content'>";
    html += "<h1>🌪️ Monitor de Energía Eólica</h1>";
    html += "<p>Sistema de Monitoreo Local | Proyecto Universitario</p>";
    html += "<div class='status-badge'>🟢 Sistema Activo | Solo Web Local</div>";
    html += "</div>";
    html += "</div>";
    
    // Container principal
    html += "<div class='container'>";
    
    // Métricas principales
    html += "<div class='metrics-grid'>";
    
    html += "<div class='metric-card voltage'>";
    html += "<div class='metric-icon'>⚡</div>";
    html += "<div class='metric-title'>Voltaje</div>";
    html += "<div class='metric-value' id='voltaje'>" + String(voltaje_actual, 1) + "</div>";
    html += "<div class='metric-unit'>Voltios</div>";
    html += "</div>";
    
    html += "<div class='metric-card current'>";
    html += "<div class='metric-icon'>🔄</div>";
    html += "<div class='metric-title'>Corriente</div>";
    html += "<div class='metric-value' id='corriente'>" + String(corriente_actual, 2) + "</div>";
    html += "<div class='metric-unit'>Amperios</div>";
    html += "</div>";
    
    html += "<div class='metric-card power'>";
    html += "<div class='metric-icon'>💡</div>";
    html += "<div class='metric-title'>Potencia</div>";
    html += "<div class='metric-value' id='potencia'>" + String(potencia_actual, 1) + "</div>";
    html += "<div class='metric-unit'>Watts</div>";
    html += "</div>";
    
    html += "</div>"; // Fin metrics-grid
    
    // Gráfico mejorado
    html += "<div class='chart-section'>";
    html += "<div class='chart-title'>📊 Historial de Potencia | Últimas 20 Lecturas</div>";
    html += "<div class='chart-container' id='chart'>";
    
    // Generar barras del gráfico
    float max_pot = 1;
    for(int i = 0; i < 20; i++) {
        if(historial_potencia[i] > max_pot) max_pot = historial_potencia[i];
    }
    
    for(int i = 0; i < 20; i++) {
        int altura = (historial_potencia[i] / max_pot) * 210;
        html += "<div class='chart-bar' style='height:" + String(altura) + "px;' title='" + String(historial_potencia[i], 1) + "W'></div>";
    }
    
    html += "</div>";
    html += "</div>";
    
    // Estadísticas mejoradas
    html += "<div class='stats-grid'>";
    html += "<div class='stat-card'>";
    html += "<div class='stat-label'>⚡ Voltaje Máximo</div>";
    html += "<div class='stat-value' id='v-max'>" + String(voltaje_max, 1) + " V</div>";
    html += "</div>";
    html += "<div class='stat-card'>";
    html += "<div class='stat-label'>🔄 Corriente Máxima</div>";
    html += "<div class='stat-value' id='i-max'>" + String(corriente_max, 2) + " A</div>";
    html += "</div>";
    html += "<div class='stat-card'>";
    html += "<div class='stat-label'>💡 Potencia Máxima</div>";
    html += "<div class='stat-value' id='p-max'>" + String(potencia_max, 1) + " W</div>";
    html += "</div>";
    html += "<div class='stat-card'>";
    html += "<div class='stat-label'>📊 Total Lecturas</div>";
    html += "<div class='stat-value' id='total'>" + String(total_lecturas) + "</div>";
    html += "</div>";
    html += "</div>";
    
    html += "</div>"; // Fin container
    
    // Footer
    html += "<div class='footer'>";
    html += "<p><strong>Monitor Web Local</strong> | ESP32 + Sensores FZ04430 & ACS712</p>";
    html += "<p>Actualización automática cada 2 segundos | Sin conexión a internet requerida</p>";
    html += "</div>";
    
    // JavaScript mejorado
    html += "<script>";
    html += "let actualizaciones = 0;";
    html += "function actualizar() {";
    html += "  const indicator = document.getElementById('update-indicator');";
    html += "  indicator.textContent = '🔄 Actualizando...';";
    html += "  indicator.style.background = '#ed8936';";
    html += "  fetch('/datos')";
    html += "    .then(r => r.json())";
    html += "    .then(d => {";
    html += "      document.getElementById('voltaje').textContent = d.voltaje;";
    html += "      document.getElementById('corriente').textContent = d.corriente;";
    html += "      document.getElementById('potencia').textContent = d.potencia;";
    html += "      document.getElementById('v-max').textContent = d.voltaje_max + ' V';";
    html += "      document.getElementById('i-max').textContent = d.corriente_max + ' A';";
    html += "      document.getElementById('p-max').textContent = d.potencia_max + ' W';";
    html += "      document.getElementById('total').textContent = d.total;";
    html += "      actualizaciones++;";
    html += "      indicator.textContent = '✅ Actualizado (' + actualizaciones + ')';";
    html += "      indicator.style.background = '#48bb78';";
    html += "    })";
    html += "    .catch(e => {";
    html += "      indicator.textContent = '❌ Error de conexión';";
    html += "      indicator.style.background = '#f56565';";
    html += "    });";
    html += "}";
    html += "setInterval(actualizar, 2000);";
    html += "window.onload = actualizar;";
    html += "</script>";
    
    html += "</body>";
    html += "</html>";
    
    return html;
}

void actualizar_estadisticas() {
    total_lecturas++;
    
    if(voltaje_actual > voltaje_max) voltaje_max = voltaje_actual;
    if(corriente_actual > corriente_max) corriente_max = corriente_actual;
    if(potencia_actual > potencia_max) potencia_max = potencia_actual;
}

void guardar_en_historial() {
    historial_potencia[indice_historial] = potencia_actual;
    indice_historial = (indice_historial + 1) % 20;
}

void leer_sensores() {
    // Leer voltaje con promedio
    long suma_v = 0;
    for (int i = 0; i < 10; i++) {
        suma_v += analogRead(PIN_SENSOR_VOLTAJE);
        delay(2);
    }
    float lectura_v = suma_v / 10.0;
    voltaje_actual = (lectura_v * VOLTAJE_REF_ESP32 / RESOLUCION_ADC) * FACTOR_VOLTAJE;
    voltaje_actual = abs(voltaje_actual);
    
    // Leer corriente con promedio
    long suma_i = 0;
    for (int i = 0; i < 10; i++) {
        suma_i += analogRead(PIN_SENSOR_CORRIENTE);
        delay(2);
    }
    float lectura_i = suma_i / 10.0;
    float voltaje_sensor = (lectura_i * VOLTAJE_REF_ESP32) / RESOLUCION_ADC;
    corriente_actual = abs((voltaje_sensor - 1.65) / 0.185);
    
    // Filtros de ruido
    if (corriente_actual < 0.05) corriente_actual = 0.0;
    if (voltaje_actual < 0.1) voltaje_actual = 0.0;
}

void calcular_potencia() {
    potencia_actual = voltaje_actual * corriente_actual;
}

void actualizar_lcd() {
    lcd.clear();
    
    // Primera línea: Voltaje y Corriente
    lcd.setCursor(0, 0);
    lcd.print("V:");
    lcd.print(voltaje_actual, 1);
    lcd.print("V I:");
    lcd.print(corriente_actual, 2);
    lcd.print("A");
    
    // Segunda línea: Potencia y estado
    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(potencia_actual, 1);
    lcd.print("W");
    
    // Indicadores
    if (WiFi.status() == WL_CONNECTED) {
        lcd.setCursor(13, 1);
        lcd.print("Web");
    }
}

void mostrar_datos_serie_tranquilo() {
    Serial.println("📊 ===== DATOS ACTUALES =====");
    Serial.println("⚡ Voltaje:   " + String(voltaje_actual, 2) + " V");
    Serial.println("🔄 Corriente: " + String(corriente_actual, 3) + " A");
    Serial.println("💡 Potencia:  " + String(potencia_actual, 2) + " W");
    Serial.println("🌐 Dashboard: http://" + WiFi.localIP().toString());
    Serial.println("📊 Lecturas:  " + String(total_lecturas));
    Serial.println("===============================");
    Serial.println("");
}