#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Airtel_Soumyajyoti wifi";
const char* password = "12345678";

WebServer server(80);

// Receiver pins
// Channel 1 - steering (D34=GPIO34)
// Channel 2 - throttle (D35=GPIO35)
// Channel 5 - 3-position toggle for LEDs (D15=GPIO15)
// Channel 6 - on/off toggle (D4=GPIO16)
#define CH1 34
#define CH2 35
#define CH5 15
#define CH6 16

// LED pins 
#define LEFT_LED 13      // D13
#define RIGHT_LED 2      // D2
#define CH6_LED 19       // D19
#define BRIGHTNESS_LED 5 // D18

// Motor pins (as per README)
#define IN1 25   // D25
#define IN2 26   // D26
#define IN3 27   // D27
#define IN4 14   // D14
#define ENA 33   // D33 - LEDC channel 0
#define ENB 32   // D32 - LEDC channel 1

int ch5Value = 1500;

// LED control variables
int toggleState = 0; // 0=middle, 1=up, -1=down
int leftLedState = LOW;
int rightLedState = LOW;
unsigned long lastBlinkTime = 0;
const int BLINK_INTERVAL = 500; // milliseconds (500ms on/off)

// CH6 LED control
int ch6Value = 1500;
int ch6LedState = LOW;

// Brightness LED control
int ch2Value = 1500;
int brightnessValue = 0;

// Motor control variables
int ch1Value = 1500;
int throttle = 0, steering = 0;
int smoothThrottle = 0, smoothSteering = 0;

// PWM Configuration
#define PWM_FREQ 1000
#define PWM_RES 8
#define BRIGHTNESS_PWM_CHANNEL 2
#define MOTOR_PWM_CHANNEL_A 0
#define MOTOR_PWM_CHANNEL_B 1
#define SMOOTH_ALPHA 0.1

// Read receiver
int readChannel(int pin) {
  int val = pulseIn(pin, HIGH, 25000);
  if (val == 0) return 1500; // failsafe
  return val;
}

// LED blink control
void controlLEDs(int togglePos) {
  unsigned long currentTime = millis();
  
  // Turn off both LEDs by default
  digitalWrite(LEFT_LED, LOW);
  digitalWrite(RIGHT_LED, LOW);
  
  if (togglePos == 1) {
    // Toggle UP - blink left LED
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
      leftLedState = (leftLedState == LOW) ? HIGH : LOW;
      lastBlinkTime = currentTime;
    }
    digitalWrite(LEFT_LED, leftLedState);
  } 
  else if (togglePos == -1) {
    // Toggle DOWN - blink right LED
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
      rightLedState = (rightLedState == LOW) ? HIGH : LOW;
      lastBlinkTime = currentTime;
    }
    digitalWrite(RIGHT_LED, rightLedState);
  }
  // If togglePos == 0 (middle), both LEDs stay off (already set above)
}

// CH6 LED toggle control
void controlCH6LED(int ch6Val) {
  // Toggle mode: UP (>1750µs) = ON, DOWN (<1250µs) = OFF, MIDDLE = OFF
  if (ch6Val > 1750) {
    digitalWrite(CH6_LED, HIGH);
    ch6LedState = HIGH;
  } else {
    digitalWrite(CH6_LED, LOW);
    ch6LedState = LOW;
  }
}

// Exponential smooth movement
int smooth(int current, int target) {
  return current + SMOOTH_ALPHA * (target - current);
}

// Motor control with normalization for BO motors
void controlMotors(int throttle, int steering) {
  int left = throttle - steering;   // Fixed: subtract steering for correct left turn
  int right = throttle + steering;  // Fixed: add steering for correct right turn

  // Normalize to prevent overload on BO motors
  int maxVal = max(abs(left), abs(right));
  if (maxVal > 255) {
    left = left * 255 / maxVal;
    right = right * 255 / maxVal;
  }

  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);

  // LEFT MOTOR
  if (left >= 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    ledcWrite(MOTOR_PWM_CHANNEL_A, left);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    ledcWrite(MOTOR_PWM_CHANNEL_A, -left);
  }

  // RIGHT MOTOR
  if (right >= 0) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    ledcWrite(MOTOR_PWM_CHANNEL_B, right);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    ledcWrite(MOTOR_PWM_CHANNEL_B, -right);
  }
}

// Brightness LED control based on motion
void controlBrightnessLED(int throttleVal) {
  // Map throttle to brightness
  int throttle = map(throttleVal, 1000, 2000, -255, 255);
  
  if (abs(throttle) < 20) {
    // Car stopped - full brightness (100%)
    brightnessValue = 255;
  } else {
    // Car moving - 60% brightness
    brightnessValue = 153; // 255 * 0.6 ≈ 153
  }
  
  ledcWrite(BRIGHTNESS_PWM_CHANNEL, brightnessValue);
}

// Webpage with graph
void handleRoot() {
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Multi-LED Controller</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea, #764ba2);
      color: #2d3436;
      margin: 0;
      padding: 20px;
      min-height: 100vh;
    }
    .container {
      max-width: 1000px;
      margin: 0 auto;
      background: rgba(255, 255, 255, 0.95);
      border-radius: 15px;
      padding: 30px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.3);
    }
    h1 {
      text-align: center;
      color: #667eea;
      margin-bottom: 10px;
      font-size: 2.5em;
    }
    h2 {
      text-align: center;
      color: #636e72;
      margin-bottom: 30px;
    }
    .chart-container {
      background: white;
      border-radius: 10px;
      padding: 20px;
      margin: 20px 0;
      box-shadow: 0 5px 15px rgba(0,0,0,0.1);
    }
    canvas {
      max-width: 100%;
      height: auto;
    }
    .status-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
      margin: 20px 0;
    }
    .status-item {
      margin: 10px;
      padding: 15px 20px;
      border-radius: 8px;
      font-weight: bold;
      text-align: center;
    }
    .status-item p {
      margin: 5px 0;
      font-size: 1em;
    }
    .toggle-status {
      background: linear-gradient(135deg, #667eea, #764ba2);
      color: white;
    }
    .ch6-status {
      background: linear-gradient(135deg, #f093fb, #f5576c);
      color: white;
    }
    .brightness-status {
      background: linear-gradient(135deg, #4facfe, #00f2fe);
      color: white;
    }
    .led-indicator {
      display: inline-block;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      margin-right: 10px;
      background: #ccc;
    }
    .led-indicator.left-on {
      background: #ff6b6b;
      box-shadow: 0 0 10px #ff6b6b;
      animation: blink 1s infinite;
    }
    .led-indicator.right-on {
      background: #4ecdc4;
      box-shadow: 0 0 10px #4ecdc4;
      animation: blink 1s infinite;
    }
    .led-indicator.ch6-on {
      background: #ff87b2;
      box-shadow: 0 0 10px #ff87b2;
    }
    .led-indicator.brightness-on {
      background: #ffd93d;
      box-shadow: 0 0 15px #ffd93d;
    }
    @keyframes blink {
      0%, 49% { opacity: 1; }
      50%, 100% { opacity: 0.3; }
    }
  </style>
  </head>
  <body>
  <div class="container">
    <h1>🎮 RC Controller - All Channels Monitor</h1>
    <h2>Real-time Receiver Channel Display</h2>

    <div class="chart-container">
      <h3 style="text-align: center; color: #2d3436;">All Channels (CH1, CH2, CH5, CH6)</h3>
      <canvas id="chart" width="400" height="200"></canvas>
    </div>

    <div class="status-grid">
      <div style="background: linear-gradient(135deg, #667eea, #764ba2); color: white; border-radius: 8px; margin: 10px; padding: 15px; text-align: center;">
        <p style="margin: 5px 0;"><strong>🎮 CH1 (Steering)</strong></p>
        <p style="font-size: 1.3em;" id="ch1-value">1500</p>
        <p style="font-size: 0.9em;" id="ch1-status">Center</p>
      </div>
      
      <div style="background: linear-gradient(135deg, #f093fb, #f5576c); color: white; border-radius: 8px; margin: 10px; padding: 15px; text-align: center;">
        <p style="margin: 5px 0;"><strong>⚡ CH2 (Throttle)</strong></p>
        <p style="font-size: 1.3em;" id="ch2-value">1500</p>
        <p style="font-size: 0.9em;" id="ch2-status">Neutral</p>
      </div>
      
      <div style="background: linear-gradient(135deg, #11998e, #38ef7d); color: white; border-radius: 8px; margin: 10px; padding: 15px; text-align: center;">
        <p style="margin: 5px 0;"><strong>🔄 CH5 (LED Toggle)</strong></p>
        <p style="font-size: 1.3em;" id="ch5-value">1500</p>
        <p style="font-size: 0.9em;" id="ch5-status">Off</p>
      </div>
      
      <div style="background: linear-gradient(135deg, #ffc837, #ff8e15); color: white; border-radius: 8px; margin: 10px; padding: 15px; text-align: center;">
        <p style="margin: 5px 0;"><strong>🎚️ CH6 (On/Off)</strong></p>
        <p style="font-size: 1.3em;" id="ch6-value">1500</p>
        <p style="font-size: 0.9em;" id="ch6-status">Off</p>
      </div>
    </div>

    <div style="background: #ecf0f1; border-radius: 8px; margin: 20px 10px; padding: 20px; text-align: center;">
      <h3 style="color: #2d3436;">LED Status Summary</h3>
      <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 15px;">
        <div>
          <p style="margin: 5px 0; color: #7f8c8d;"><strong>Left LED (GPIO13)</strong></p>
          <p style="font-size: 1em;" id="left-led-status">OFF</p>
        </div>
        <div>
          <p style="margin: 5px 0; color: #7f8c8d;"><strong>Right LED (GPIO2)</strong></p>
          <p style="font-size: 1em;" id="right-led-status">OFF</p>
        </div>
        <div>
          <p style="margin: 5px 0; color: #7f8c8d;"><strong>CH6 LED (GPIO19)</strong></p>
          <p style="font-size: 1em;" id="ch6-led-status">OFF</p>
        </div>
        <div>
          <p style="margin: 5px 0; color: #7f8c8d;"><strong>Brightness (GPIO5)</strong></p>
          <p style="font-size: 1em;" id="brightness-status">100%</p>
        </div>
      </div>
    </div>
  </div>

  <script>
  const ctx = document.getElementById('chart').getContext('2d');
  const chart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { 
          label: 'CH1 (Steering)', 
          data: [], 
          borderColor: '#667eea', 
          backgroundColor: 'rgba(102, 126, 234, 0.1)',
          borderWidth: 2,
          tension: 0.4,
          yAxisID: 'y'
        },
        { 
          label: 'CH2 (Throttle)', 
          data: [], 
          borderColor: '#f5576c', 
          backgroundColor: 'rgba(245, 87, 108, 0.1)',
          borderWidth: 2,
          tension: 0.4,
          yAxisID: 'y'
        },
        { 
          label: 'CH5 (Toggle)', 
          data: [], 
          borderColor: '#38ef7d', 
          backgroundColor: 'rgba(56, 239, 125, 0.1)',
          borderWidth: 2,
          tension: 0.4,
          yAxisID: 'y'
        },
        { 
          label: 'CH6 (On/Off)', 
          data: [], 
          borderColor: '#ffc837', 
          backgroundColor: 'rgba(255, 200, 55, 0.1)',
          borderWidth: 2,
          tension: 0.4,
          yAxisID: 'y'
        }
      ]
    },
    options: {
      responsive: true,
      plugins: {
        legend: {
          position: 'top',
        },
        title: {
          display: false
        }
      },
      scales: {
        y: {
          beginAtZero: false,
          min: 1000,
          max: 2000,
          title: {
            display: true,
            text: 'Pulse Width (µs)'
          }
        },
        x: {
          title: {
            display: true,
            text: 'Time'
          }
        }
      }
    }
  });

  setInterval(() => {
    fetch('/data')
      .then(res => res.json())
      .then(data => {
        // Update chart - keep only last 40 points
        if (chart.data.labels.length > 40) {
          chart.data.labels.shift();
          chart.data.datasets[0].data.shift();
          chart.data.datasets[1].data.shift();
          chart.data.datasets[2].data.shift();
          chart.data.datasets[3].data.shift();
        }

        chart.data.labels.push(new Date().toLocaleTimeString());
        chart.data.datasets[0].data.push(data.ch1);
        chart.data.datasets[1].data.push(data.ch2);
        chart.data.datasets[2].data.push(data.ch5);
        chart.data.datasets[3].data.push(data.ch6);
        chart.update();

        // Update CH1 (Steering) values and status
        document.getElementById('ch1-value').textContent = data.ch1 + ' µs';
        let ch1Status = 'Center';
        if (data.ch1 < 1400) ch1Status = '← Left';
        else if (data.ch1 > 1600) ch1Status = 'Right →';
        document.getElementById('ch1-status').textContent = ch1Status;

        // Update CH2 (Throttle) values and status
        document.getElementById('ch2-value').textContent = data.ch2 + ' µs';
        let ch2Status = 'Neutral';
        if (data.ch2 > 1700) ch2Status = '↑ Forward';
        else if (data.ch2 < 1300) ch2Status = '↓ Reverse';
        document.getElementById('ch2-status').textContent = ch2Status;

        // Update CH5 (Toggle) values and status
        document.getElementById('ch5-value').textContent = data.ch5 + ' µs';
        let ch5Status = 'Middle';
        if (data.ch5 > 1750) ch5Status = 'UP - Left Blink';
        else if (data.ch5 < 1250) ch5Status = 'DOWN - Right Blink';
        document.getElementById('ch5-status').textContent = ch5Status;

        // Update CH6 (On/Off) values and status
        document.getElementById('ch6-value').textContent = data.ch6 + ' µs';
        let ch6Status = 'OFF';
        if (data.ch6 > 1750) ch6Status = 'ON';
        document.getElementById('ch6-status').textContent = ch6Status;

        // Update LED Status
        if (data.ledStatus === 'LEFT BLINKING') {
          document.getElementById('left-led-status').textContent = '✓ BLINKING';
          document.getElementById('left-led-status').style.color = '#ff6b6b';
        } else {
          document.getElementById('left-led-status').textContent = 'OFF';
          document.getElementById('left-led-status').style.color = '#95a5a6';
        }

        if (data.ledStatus === 'RIGHT BLINKING') {
          document.getElementById('right-led-status').textContent = '✓ BLINKING';
          document.getElementById('right-led-status').style.color = '#4ecdc4';
        } else {
          document.getElementById('right-led-status').textContent = 'OFF';
          document.getElementById('right-led-status').style.color = '#95a5a6';
        }

        document.getElementById('ch6-led-status').textContent = data.ch6Led === 'ON' ? '✓ ON' : 'OFF';
        document.getElementById('ch6-led-status').style.color = data.ch6Led === 'ON' ? '#ff87b2' : '#95a5a6';

        document.getElementById('brightness-status').textContent = data.brightness;
        document.getElementById('brightness-status').style.color = data.brightness.includes('100%') ? '#ffd93d' : '#f39c12';
      })
      .catch(err => console.error('Error fetching data:', err));
  }, 500);
  </script>

  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", page);
}

// Send data
void handleData() {
  String ledStatus = "OFF";
  if (toggleState == 1) {
    ledStatus = "LEFT BLINKING";
  } else if (toggleState == -1) {
    ledStatus = "RIGHT BLINKING";
  } else {
    ledStatus = "OFF";
  }

  String ch6Status = ch6LedState == HIGH ? "ON" : "OFF";
  String brightnessStatus = brightnessValue == 255 ? "100% (Stopped)" : "60% (Moving)";

  String motorStatus = "STOPPED";
  if (smoothThrottle > 50) {
    motorStatus = "FORWARD";
  } else if (smoothThrottle < -50) {
    motorStatus = "BACKWARD";
  }

  String json = "{";
  json += "\"ch1\":" + String(ch1Value) + ",";
  json += "\"ch2\":" + String(ch2Value) + ",";
  json += "\"ch5\":" + String(ch5Value) + ",";
  json += "\"ch6\":" + String(ch6Value) + ",";
  json += "\"ledStatus\":\"" + ledStatus + "\",";
  json += "\"ch6Led\":\"" + ch6Status + "\",";
  json += "\"brightness\":\"" + brightnessStatus + "\",";
  json += "\"motorStatus\":\"" + motorStatus + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Multi-LED + Motor Controller...");

  // Receiver pins setup
  pinMode(CH1, INPUT);
  pinMode(CH2, INPUT);
  pinMode(CH5, INPUT);
  pinMode(CH6, INPUT);

  // LED pins setup
  pinMode(LEFT_LED, OUTPUT);
  pinMode(RIGHT_LED, OUTPUT);
  pinMode(CH6_LED, OUTPUT);
  pinMode(BRIGHTNESS_LED, OUTPUT);
  
  Serial.println("\nTesting LEDs...");
  // Test all LEDs
  Serial.println("LED Test: Left (GPIO13)...");
  digitalWrite(LEFT_LED, HIGH);
  delay(300);
  digitalWrite(LEFT_LED, LOW);
  
  Serial.println("LED Test: Right (GPIO2)...");
  digitalWrite(RIGHT_LED, HIGH);
  delay(300);
  digitalWrite(RIGHT_LED, LOW);
  
  Serial.println("LED Test: CH6 Toggle (GPIO19)...");
  digitalWrite(CH6_LED, HIGH);
  delay(300);
  digitalWrite(CH6_LED, LOW);
  
  Serial.println("LED tests complete.");

  // Motor pins setup
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Setup PWM for brightness LED
  ledcSetup(BRIGHTNESS_PWM_CHANNEL, PWM_FREQ, PWM_RES);
  ledcAttachPin(BRIGHTNESS_LED, BRIGHTNESS_PWM_CHANNEL);
  ledcWrite(BRIGHTNESS_PWM_CHANNEL, 0);

  // Setup PWM for motors
  ledcSetup(MOTOR_PWM_CHANNEL_A, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA, MOTOR_PWM_CHANNEL_A);

  ledcSetup(MOTOR_PWM_CHANNEL_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENB, MOTOR_PWM_CHANNEL_B);

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();

  // Read all channels
  ch1Value = readChannel(CH1);
  ch2Value = readChannel(CH2);
  ch5Value = readChannel(CH5);
  ch6Value = readChannel(CH6);
  
  // Debug output
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 500) {
    Serial.println("\n===== Debug Info =====");
    Serial.print("CH1(Steering): "); Serial.print(ch1Value); Serial.print("µs => "); Serial.println(steering);
    Serial.print("CH2(Throttle): "); Serial.print(ch2Value); Serial.print("µs => "); Serial.println(throttle);
    Serial.print("CH5(Toggle): "); Serial.print(ch5Value); Serial.print("µs => State: ");
    if (toggleState == 1) Serial.println("UP (Left LED blink)");
    else if (toggleState == -1) Serial.println("DOWN (Right LED blink)");
    else Serial.println("MIDDLE (No LED)");
    
    Serial.print("CH6(On/Off): "); Serial.print(ch6Value); Serial.print("µs => ");
    if (ch6Value > 1750) Serial.println("ON");
    else Serial.println("OFF");
    
    Serial.print("LED Status - Left(GPIO13): "); Serial.print(digitalRead(LEFT_LED));
    Serial.print(" | Right(GPIO2): "); Serial.print(digitalRead(RIGHT_LED));
    Serial.print(" | CH6(GPIO19): "); Serial.print(digitalRead(CH6_LED));
    Serial.print(" | Brightness(GPIO5): "); Serial.println(brightnessValue);
    Serial.println("====================\n");
    lastDebug = millis();
  }

  // Determine toggle state based on CH5 value
  // UP: > 1750µs, MIDDLE: 1250-1750µs, DOWN: < 1250µs
  if (ch5Value > 1750) {
    toggleState = 1;  // Toggle UP - Left LED blinks
  } 
  else if (ch5Value < 1250) {
    toggleState = -1; // Toggle DOWN - Right LED blinks
  } 
  else {
    toggleState = 0;  // Toggle MIDDLE - No LED
  }

  // Map channels to motor values - CH1 for steering, CH2 for throttle
  steering = map(ch1Value, 1000, 2000, -255, 255);
  throttle = map(ch2Value, 1000, 2000, 255, -255);  // Inverted for correct direction

  // Dead zone
  if (abs(throttle) < 20) throttle = 0;
  if (abs(steering) < 20) steering = 0;

  // Smooth
  smoothThrottle = smooth(smoothThrottle, throttle);
  smoothSteering = smooth(smoothSteering, steering);

  // Control all LEDs
  controlLEDs(toggleState);
  controlCH6LED(ch6Value);
  controlBrightnessLED(ch2Value);

  // Control motors
  controlMotors(smoothThrottle, smoothSteering);

  delay(50); // Update rate ~20Hz
}