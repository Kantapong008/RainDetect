#include <Wire.h>
#include <WiFiS3.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <LiquidCrystal_I2C.h> 

// 1. Wi-Fi Configuration Network Credentials
const char ssid[] = "POCO F6";     
const char pass[] = "last1825"; 

// 2. Hardware Object Declarations 
Adafruit_BME680 bme; 
LiquidCrystal_I2C lcd(0x27, 16, 2); 

WiFiServer server(80); 

// Global variables for sensor caching
float temp = 0.0, humidity = 0.0, pressure = 0.0;
String rainPrediction = "Initialising...";
bool isRaining = false;

// Non-blocking timer tracking variables
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000; // Update metrics internally every 2 seconds

void setup() {
  Serial.begin(115200);
  while(!Serial);

  Wire.begin();
  Wire.setClock(100000); 

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Booting up...");

  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    lcd.clear();
    lcd.print("BME680 Error!");
    while (1);
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(0, 0); 

  lcd.clear();
  lcd.print("Connecting WiFi...");
  
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(1000);
    Serial.print(".");
  }
  delay(1000);

  Serial.println("\nWi-Fi Fully Synchronized!");
  Serial.print("Local IP Address: ");
  Serial.println(WiFi.localIP());

  server.begin();

  lcd.clear();
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString()); 
  delay(2000);
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Timed background sensor sampling
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;

    if (bme.performReading()) {
      temp     = bme.temperature;
      humidity = bme.humidity;
      pressure = bme.pressure / 100.0; 

      if (pressure < 1009.0 && humidity > 70.0) {
        rainPrediction = "High Rain Risk!!";
        isRaining = true;
      } else if (pressure < 1013.0 && humidity > 55.0) {
        rainPrediction = "Unstable/Cloudy";
        isRaining = false;
      } else {
        rainPrediction = "Clear Skies";
        isRaining = false;
      }

      // Sync physical LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("T:" + String(temp, 1) + "C P:" + String(pressure, 0) + "hPa");
      lcd.setCursor(0, 1);
      lcd.print(rainPrediction);
    }
  }

  // 2. High-speed network handler
  WiFiClient client = server.available();
  if (client) {
    String HTTP_req = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        HTTP_req += c;
        
        // Stop reading once header reaches end of first line breakdown
        if (c == '\n') {
          
          // MODE A: Browser background background script asks only for raw metrics data string
          if (HTTP_req.indexOf("GET /data") >= 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println("Connection: close");
            client.println();
            // Format metrics string sent to browser: "Temp,Humidity,Pressure,Prediction,isRaining"
            client.print(String(temp, 2) + "|" + String(humidity, 1) + "|" + String(pressure, 2) + "|" + rainPrediction + "|" + (isRaining ? "1" : "0"));
            break;
          }
          
          // MODE B: Initial browser connection loads static framework design just once
          else if (HTTP_req.indexOf("GET / ") >= 0 || HTTP_req.indexOf("GET /index") >= 0) {
            String htmlPage = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>";
            htmlPage += "body{font-family:sans-serif;text-align:center;background:#2c3e50;color:#fff;margin:0;overflow:hidden;}";
            htmlPage += ".card{background:rgba(255,255,255,0.15);backdrop-filter:blur(10px);max-width:400px;margin:40px auto;padding:20px;border-radius:15px;box-shadow:0 8px 16px rgba(0,0,0,0.3);}";
            htmlPage += ".alert{font-size:28px;font-weight:bold;color:#ff7675;}";
            htmlPage += ".rain{position:absolute;left:0;top:0;width:100%;height:100%;z-index:1;pointer-events:none;display:none;}";
            htmlPage += ".drop{position:absolute;bottom:100%;width:2px;height:20px;background:linear-gradient(transparent,#74b9ff);animation:fall .8s linear infinite;}";
            htmlPage += "@keyframes fall{to{transform:translateY(105vh);}}</style></head><body>";
            
            // Build fixed 20 drop rain canvas layer ready to toggle visibility
            htmlPage += "<div class=\"rain\" id=\"rain-canvas\">";
            for (int i = 0; i < 20; i++) {
              htmlPage += "<div class=\"drop\" style=\"left:" + String(random(0, 100)) + "%;animation-delay:-" + String(random(0, 800)) + "ms;\"></div>";
            }
            htmlPage += "</div>";
            
            htmlPage += "<h1>UNO R4 Live Weather Station</h1>";
            htmlPage += "<div class=\"card\"><h2>Forecast:</h2><p class=\"alert\" id=\"forecast-text\">Loading...</p></div>";
            htmlPage += "<div class=\"card\"><h3>Live Metrics:</h3>";
            htmlPage += "<p>Temperature: <b id=\"temp-text\">0.00</b> &deg;C</p>";
            htmlPage += "<p>Humidity: <b id=\"hum-text\">0.0</b> %</p>";
            htmlPage += "<p>Pressure: <b id=\"pres-text\">0.00</b> hPa</p></div>";
            
            // AJAX script fetches raw /data pipe every 2 seconds asynchronously
            htmlPage += "<script>";
            htmlPage += "let rTime=sessionStorage.getItem('rainTime')?parseInt(sessionStorage.getItem('rainTime')):0;";
            htmlPage += "function updateMetrics(){";
            htmlPage += "  fetch('/data').then(res=>res.text()).then(data=>{";
            htmlPage += "    let parts=data.split('|');";
            htmlPage += "    document.getElementById('temp-text').innerText=parts[0];";
            htmlPage += "    document.getElementById('hum-text').innerText=parts[1];";
            htmlPage += "    document.getElementById('pres-text').innerText=parts[2];";
            htmlPage += "    document.getElementById('forecast-text').innerText=parts[3];";
            htmlPage += "    if(parts[4]==='1'){";
            htmlPage += "      document.getElementById('rain-canvas').style.display='block';";
            htmlPage += "      rTime+=2; sessionStorage.setItem('rainTime',rTime);";
            htmlPage += "      if(rTime>=20){alert('⚠️ WARNING: Continuous heavy rain detected!');}";
            htmlPage += "    }else{";
            htmlPage += "      document.getElementById('rain-canvas').style.display='none';";
            htmlPage += "      rTime=0; sessionStorage.removeItem('rainTime');";
            htmlPage += "    }";
            htmlPage += "  });";
            htmlPage += "}";
            htmlPage += "setInterval(updateMetrics,2000); updateMetrics();"; // Triggers loop routine every 2000ms
            htmlPage += "</script></body></html>";

            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.print("Content-Length: ");
            client.println(htmlPage.length());
            client.println("Connection: close");
            client.println();
            client.print(htmlPage);
            break;
          } else {
            break;
          }
        }
      }
    }
    delay(5);
    client.stop();
  }
}

