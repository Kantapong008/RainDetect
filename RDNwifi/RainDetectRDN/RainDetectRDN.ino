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
LiquidCrystal_I2C lcd(0x27, 16, 2); // Verify I2C address (0x27 or 0x3F typically)

WiFiServer server(80); // Establish Webserver instance hosting over port 80

void setup() {
  Serial.begin(115200);
  while(!Serial);

  // Initialize LCD Module
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Booting up...");

  // Initialize BME680 Sensor Module Over Hardware I2C 
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    lcd.clear();
    lcd.print("BME680 Error!");
    while (1);
  }

  // Set up oversampling and filter configurations for the BME680
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  // Connect to the Local Wi-Fi network
  lcd.clear();
  lcd.print("Connecting WiFi...");
  Serial.print("Connecting to Network SSID: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  // Network successfully authenticated
  Serial.println("\nWi-Fi Connected!");
  Serial.print("Local IP Address to visit in Browser: ");
  Serial.println(WiFi.localIP());

  // Start the Local Web Server
  server.begin();

  lcd.clear();
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(3000);
}

void loop() {
  // 1. Tell BME680 to perform asynchronous reading evaluation
  if (!bme.performReading()) {
    Serial.println("Failed to perform sensor reading :(");
    return;
  }

  float temp     = bme.temperature;
  float humidity = bme.humidity;
  float pressure = bme.pressure / 100.0; // Convert Pascals to hPa (hectopascals)

  // 2. Barometric Rain Prediction Algorithm Logic
  // Falling barometric pressure combined with higher humidity correlates directly to rain
  String rainPrediction = "No Rain Expected";
  
  if (pressure < 1009.0 && humidity > 70.0) {
    rainPrediction = "High Rain Risk!!";
  } else if (pressure < 1013.0 && humidity > 55.0) {
    rainPrediction = "Unstable/Cloudy";
  } else if (pressure >= 1013.0) {
    rainPrediction = "Clear Skies";
  }

  // 3. Update the physical 16x2 LCD display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:" + String(temp, 1) + "C P:" + String(pressure, 0) + "hPa");
  lcd.setCursor(0, 1);
  lcd.print(rainPrediction);

  // 4. Listen for and serve arriving Webserver Client Requests
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected to Webserver.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Send standard HTTP responsive headers
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println("Refresh: 5"); // Automatically updates browser view every 5 seconds
            client.println();
            
            // Build out responsive HTML interface webpage structure code
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>body{font-family:sans-serif; text-align:center; background:#f4f4f9; color:#333;}");
            client.println(".card{background:white; max-width:400px; margin:20px auto; padding:20px; border-radius:10px; box-shadow:0 4px 8px rgba(0,0,0,0.1);}");
            client.println(".alert{font-size:24px; font-weight:bold; color:#e74c3c;}</style></head>");
            client.println("<body>");
            client.println("<h1>UNO R4 Weather Station</h1>");
            
            client.println("<div class=\"card\">");
            client.println("<h2>Forecast:</h2>");
            client.println("<p class=\"alert\">" + rainPrediction + "</p>");
            client.println("</div>");

            client.println("<div class=\"card\">");
            client.println("<h3>Live Metrics:</h3>");
            client.println("<p>Temperature: <b>" + String(temp, 2) + " &deg;C</b></p>");
            client.println("<p>Humidity: <b>" + String(humidity, 1) + " %</b></p>");
            client.println("<p>Pressure: <b>" + String(pressure, 2) + " hPa</b></p>");
            client.println("</div>");
            
            client.println("</body></html>");
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Client disconnected.");
  }

  delay(2000); // 2-second iteration delay loop pacing
}
