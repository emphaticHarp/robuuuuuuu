#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Airtel_Soumyajyoti wifi";
const char* password = "12345678";

WebServer server(80);

// Receiver pins
// Channel 1 - steering (D34=GPIO34)
// Channel 2 - throttle (D35=GPIO35)
// Channel 5 - 3-position toggle for LEDs (D15=GPIO15)
// Channel 6 - on/off toggle (D4=GPIO4)
#define CH1 34
#define CH2 35
#define CH5 15
#define CH6 4

// LED pins 
#define LEFT_LED 23      // D23 (GPIO 23 - safe, not used)
#define RIGHT_LED 21     // D21 (GPIO 21 - safe GPIO)
#define CH6_LED 22       // D22 (GPIO 22 - safe GPIO)
#define BRIGHTNESS_LED 5 // D5 (PWM channel 2)

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
float smoothThrottle = 0, smoothSteering = 0;

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
  if (val == 0) {
    return 1500; // failsafe - return center value
  }
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
float smooth(float current, float target) {
  return current + SMOOTH_ALPHA * (target - current);
}

// Motor control with proper throttle priority and minimum threshold
void controlMotors(int throttle, int steering) {
  int left = throttle - steering;
  int right = throttle + steering;

  // Constrain each motor independently
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);

  // LEFT MOTOR (IN1, IN2, ENA)
  // IN1=LOW, IN2=HIGH → Forward
  // IN1=HIGH, IN2=LOW → Backward
  if (left > 0) {
    // Forward
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    ledcWrite(MOTOR_PWM_CHANNEL_A, left);
  } else if (left < 0) {
    // Backward
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    ledcWrite(MOTOR_PWM_CHANNEL_A, -left);
  } else {
    // Stop
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    ledcWrite(MOTOR_PWM_CHANNEL_A, 0);
  }

  // RIGHT MOTOR (IN3, IN4, ENB)
  // IN3=LOW, IN4=HIGH → Forward
  // IN3=HIGH, IN4=LOW → Backward
  if (right > 0) {
    // Forward
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    ledcWrite(MOTOR_PWM_CHANNEL_B, right);
  } else if (right < 0) {
    // Backward
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    ledcWrite(MOTOR_PWM_CHANNEL_B, -right);
  } else {
    // Stop
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    ledcWrite(MOTOR_PWM_CHANNEL_B, 0);
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
// Webpage with ATOM RC Controller Monitor - Add this to your main.cpp
void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ATOM - RC Controller Monitor</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
  * { box-sizing: border-box; }

  :root {
    --background: #ffffff;
    --foreground: #09090b;
    --card: #f4f4f5;
    --card-foreground: #09090b;
    --primary: #18181b;
    --primary-foreground: #fafafa;
    --secondary: #e4e4e7;
    --secondary-foreground: #09090b;
    --muted: #d4d4d8;
    --muted-foreground: #71717a;
    --accent: #3b82f6;
    --border: #e4e4e7;
    --input: #f4f4f5;
    --ring: #3b82f6;
  }

  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', 'Oxygen', 'Ubuntu', 'Cantarell', 'Fira Sans', 'Droid Sans', 'Helvetica Neue', sans-serif;
    background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
    color: var(--foreground);
    margin: 0;
    padding: 12px;
    min-height: 100vh;
    -webkit-font-smoothing: antialiased;
    -moz-osx-font-smoothing: grayscale;
  }

  .container {
    width: 100%;
    max-width: 1600px;
    margin: 0 auto;
    background: var(--background);
    border-radius: 14px;
    border: 1px solid rgba(0, 0, 0, 0.05);
    padding: 14px;
    display: flex;
    flex-direction: column;
    gap: 10px;
    height: 100vh;
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.06), 0 4px 16px rgba(0, 0, 0, 0.04);
  }

  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    border-bottom: 1px solid var(--border);
    padding-bottom: 10px;
    gap: 10px;
  }

  .header-left {
    display: flex;
    flex-direction: column;
    gap: 1px;
  }

  .header-left h1 {
    margin: 0;
    font-size: 18px;
    font-weight: 800;
    background: linear-gradient(135deg, #3b82f6 0%, #06b6d4 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
  }

  .header-left p {
    margin: 0;
    font-size: 11px;
    color: var(--muted-foreground);
    font-weight: 500;
  }

  .header-right {
    display: flex;
    gap: 6px;
  }

  .led-badge {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    padding: 6px 10px;
    background: linear-gradient(135deg, #f8fafc 0%, #f1f5f9 100%);
    border: 1px solid var(--border);
    border-radius: 6px;
    min-width: 70px;
    transition: all 0.2s ease;
  }

  .led-badge:hover {
    border-color: var(--accent);
    box-shadow: 0 2px 6px rgba(59, 130, 246, 0.1);
  }

  .led-badge-label {
    font-size: 9px;
    font-weight: 700;
    color: var(--muted-foreground);
    text-transform: uppercase;
    letter-spacing: 0.4px;
  }

  .led-badge-status {
    font-size: 9px;
    font-weight: 700;
    color: var(--foreground);
  }

  .main-content {
    display: grid;
    grid-template-columns: 1fr 240px;
    gap: 12px;
    flex: 1;
    min-height: 0;
  }

  .chart-section {
    display: flex;
    flex-direction: column;
    gap: 6px;
    background: linear-gradient(135deg, #f8fafc 0%, #f1f5f9 100%);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 8px;
    min-height: 0;
    flex: 0.15;
    transition: all 0.2s ease;
  }

  .chart-section:hover {
    border-color: var(--accent);
    box-shadow: 0 2px 8px rgba(59, 130, 246, 0.08);
  }

  .chart-section h2 {
    margin: 0;
    font-size: 10px;
    font-weight: 700;
    color: var(--foreground);
    text-transform: uppercase;
    letter-spacing: 0.4px;
  }

  canvas {
    flex: 1;
    min-height: 0;
  }

  .sidebar {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .sidebar-section {
    background: linear-gradient(135deg, #f8fafc 0%, #f1f5f9 100%);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 10px;
    display: flex;
    flex-direction: column;
    gap: 8px;
    transition: all 0.2s ease;
  }

  .sidebar-section:hover {
    border-color: var(--accent);
    box-shadow: 0 2px 8px rgba(59, 130, 246, 0.08);
  }

  .sidebar-section h3 {
    margin: 0;
    font-size: 10px;
    font-weight: 700;
    color: var(--foreground);
    text-transform: uppercase;
    letter-spacing: 0.4px;
  }

  .channel-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 6px;
  }

  .channel-card {
    background: var(--background);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 8px;
    display: flex;
    flex-direction: column;
    gap: 4px;
    transition: all 0.2s ease;
    cursor: pointer;
  }

  .channel-card:hover {
    border-color: var(--accent);
    background: linear-gradient(135deg, #f9fafb 0%, #f5f7fa 100%);
    box-shadow: 0 2px 6px rgba(59, 130, 246, 0.1);
    transform: translateY(-1px);
  }

  .channel-card.ch1 { border-left: 2px solid #3b82f6; }
  .channel-card.ch2 { border-left: 2px solid #8b5cf6; }
  .channel-card.ch5 { border-left: 2px solid #10b981; }
  .channel-card.ch6 { border-left: 2px solid #f59e0b; }

  .channel-label {
    font-size: 10px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.3px;
  }

  .channel-label.ch1 { color: #3b82f6; }
  .channel-label.ch2 { color: #8b5cf6; }
  .channel-label.ch5 { color: #10b981; }
  .channel-label.ch6 { color: #f59e0b; }

  .channel-value {
    font-size: 14px;
    font-weight: 700;
    color: var(--foreground);
  }

  .channel-status {
    font-size: 10px;
    color: var(--muted-foreground);
    font-weight: 500;
  }

  .led-item {
    background: var(--background);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 8px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    text-align: center;
  }

  .led-item-label {
    font-size: 10px;
    font-weight: 600;
    color: var(--muted-foreground);
    text-transform: uppercase;
    letter-spacing: 0.3px;
  }

  .led-item-status {
    font-size: 10px;
    font-weight: 600;
    color: var(--foreground);
  }

  .stats-section {
    background: var(--background);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 8px;
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 6px;
  }

  .stat-item {
    display: flex;
    flex-direction: column;
    gap: 3px;
    text-align: center;
  }

  .stat-label {
    font-size: 9px;
    font-weight: 600;
    color: var(--muted-foreground);
    text-transform: uppercase;
    letter-spacing: 0.3px;
  }

  .stat-value {
    font-size: 12px;
    font-weight: 700;
    color: var(--foreground);
  }

  .led-indicator {
    display: inline-block;
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background: #d4d4d8;
    transition: all 0.2s ease;
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.1);
  }

  .led-indicator.left-on {
    background: #ef4444;
    box-shadow: 0 0 10px rgba(239, 68, 68, 0.5), inset 0 1px 2px rgba(0, 0, 0, 0.2);
    animation: blink 1s infinite;
  }

  .led-indicator.right-on {
    background: #06b6d4;
    box-shadow: 0 0 10px rgba(6, 182, 212, 0.5), inset 0 1px 2px rgba(0, 0, 0, 0.2);
    animation: blink 1s infinite;
  }

  .led-indicator.ch6-on {
    background: #f59e0b;
    box-shadow: 0 0 10px rgba(245, 158, 11, 0.5), inset 0 1px 2px rgba(0, 0, 0, 0.2);
  }

  .led-indicator.brightness-on {
    background: #fbbf24;
    box-shadow: 0 0 10px rgba(251, 191, 36, 0.5), inset 0 1px 2px rgba(0, 0, 0, 0.2);
  }

  @keyframes blink {
    0%, 49% { opacity: 1; }
    50%, 100% { opacity: 0.3; }
  }

  .footer {
    display: none;
  }

  @media (max-width: 1024px) {
    .main-content {
      grid-template-columns: 1fr;
    }
    .sidebar {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 12px;
    }
  }

  @media (max-width: 640px) {
    .container {
      padding: 16px;
      gap: 16px;
    }
    .header {
      flex-direction: column;
      align-items: flex-start;
      gap: 12px;
    }
    .header-right {
      width: 100%;
      flex-wrap: wrap;
    }
    .sidebar {
      grid-template-columns: 1fr;
    }
  }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="header-left">
      <h1>🎮 ATOM</h1>
      <p>RC Controller Monitor</p>
    </div>
    <div class="header-right">
      <div class="led-badge">
        <div class="led-badge-label">Left LED</div>
        <div class="led-indicator" id="left-led-indicator"></div>
        <div class="led-badge-status" id="left-led-status">—</div>
      </div>
      <div class="led-badge">
        <div class="led-badge-label">Brightness</div>
        <div class="led-indicator" id="brightness-indicator"></div>
        <div class="led-badge-status" id="brightness-status">—</div>
      </div>
      <div class="led-badge">
        <div class="led-badge-label">Right LED</div>
        <div class="led-indicator" id="right-led-indicator"></div>
        <div class="led-badge-status" id="right-led-status">—</div>
      </div>
    </div>
  </div>

  <div class="main-content">
    <div class="chart-section">
      <h2>Channel Activity</h2>
      <canvas id="chart" width="400" height="200"></canvas>
    </div>

    <div class="sidebar">
      <div class="sidebar-section">
        <h3>Channels</h3>
        <div class="channel-grid">
          <div class="channel-card ch1">
            <div class="channel-label ch1">CH1</div>
            <div class="channel-value" id="ch1-value">1500</div>
            <div class="channel-status" id="ch1-status">Center</div>
          </div>
          <div class="channel-card ch2">
            <div class="channel-label ch2">CH2</div>
            <div class="channel-value" id="ch2-value">1500</div>
            <div class="channel-status" id="ch2-status">Neutral</div>
          </div>
          <div class="channel-card ch5">
            <div class="channel-label ch5">CH5</div>
            <div class="channel-value" id="ch5-value">1500</div>
            <div class="channel-status" id="ch5-status">Middle</div>
          </div>
          <div class="channel-card ch6">
            <div class="channel-label ch6">CH6</div>
            <div class="channel-value" id="ch6-value">1500</div>
            <div class="channel-status" id="ch6-status">OFF</div>
          </div>
        </div>
      </div>

      <div class="sidebar-section">
        <h3>CH6 LED</h3>
        <div class="led-item">
          <div class="led-indicator" id="ch6-led-indicator"></div>
          <div class="led-item-status" id="ch6-led-status">—</div>
        </div>
      </div>

      <div class="sidebar-section">
        <h3>Graph Stats</h3>
        <div class="stats-section">
          <div class="stat-item">
            <div class="stat-label">CH1 Min</div>
            <div class="stat-value" id="ch1-graph-min">1000</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">CH1 Max</div>
            <div class="stat-value" id="ch1-graph-max">2000</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">CH2 Min</div>
            <div class="stat-value" id="ch2-graph-min">1000</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">CH2 Max</div>
            <div class="stat-value" id="ch2-graph-max">2000</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">CH5 Min</div>
            <div class="stat-value" id="ch5-graph-min">1000</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">CH5 Max</div>
            <div class="stat-value" id="ch5-graph-max">2000</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">CH6 Min</div>
            <div class="stat-value" id="ch6-graph-min">1000</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">CH6 Max</div>
            <div class="stat-value" id="ch6-graph-max">2000</div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="footer">
    <span class="footer-icon">⚡</span>
    <span>Real-time RC Controller Monitoring System</span>
  </div>
</div>

<script>
const ctx = document.getElementById('chart').getContext('2d');
const chart = new Chart(ctx, {
  type: 'bar',
  data: {
    labels: ['CH1', 'CH2', 'CH5', 'CH6'],
    datasets: [
      { 
        label: 'Channel Values', 
        data: [0, 0, 0, 0], 
        backgroundColor: ['#3b82f6', '#8b5cf6', '#10b981', '#f59e0b'],
        borderColor: ['#3b82f6', '#8b5cf6', '#10b981', '#f59e0b'],
        borderWidth: 0,
        yAxisID: 'y',
        barThickness: 35,
        borderRadius: 6,
        borderSkipped: false
      }
    ]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    interaction: {
      mode: 'index',
      intersect: false
    },
    plugins: {
      legend: {
        display: false
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
        grid: {
          color: '#f4f4f5',
          drawBorder: false
        },
        ticks: {
          color: '#a1a1aa',
          font: { size: 11 }
        }
      },
      x: {
        categoryPercentage: 0.6,
        barPercentage: 0.9,
        grid: {
          display: false,
          drawBorder: false
        },
        ticks: {
          color: '#a1a1aa',
          font: { size: 12, weight: '600' }
        }
      }
    }
  },
  plugins: [{
    id: 'customDataLabels',
    afterDatasetsDraw(chart) {
      const { ctx, data, chartArea: { left, top, width, height } } = chart;
      ctx.font = 'bold 11px sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'bottom';
      ctx.fillStyle = '#09090b';

      data.datasets.forEach((dataset, datasetIndex) => {
        const meta = chart.getDatasetMeta(datasetIndex);
        meta.data.forEach((bar, index) => {
          const value = dataset.data[index];
          const x = bar.x;
          const y = bar.y - 5;
          ctx.fillText(Math.round(value), x, y);
        });
      });
    }
  }]
});

let ch1Min = null, ch1Max = null;
let ch2Min = null, ch2Max = null;
let ch5Min = null, ch5Max = null;
let ch6Min = null, ch6Max = null;

function updateGraphMinMax(ch1, ch2, ch5, ch6) {
  // Initialize on first data
  if (ch1Min === null) ch1Min = ch1;
  if (ch1Max === null) ch1Max = ch1;
  if (ch2Min === null) ch2Min = ch2;
  if (ch2Max === null) ch2Max = ch2;
  if (ch5Min === null) ch5Min = ch5;
  if (ch5Max === null) ch5Max = ch5;
  if (ch6Min === null) ch6Min = ch6;
  if (ch6Max === null) ch6Max = ch6;
  
  // Update min/max
  if (ch1 < ch1Min) ch1Min = ch1;
  if (ch1 > ch1Max) ch1Max = ch1;
  document.getElementById('ch1-graph-min').textContent = Math.round(ch1Min);
  document.getElementById('ch1-graph-max').textContent = Math.round(ch1Max);
  
  if (ch2 < ch2Min) ch2Min = ch2;
  if (ch2 > ch2Max) ch2Max = ch2;
  document.getElementById('ch2-graph-min').textContent = Math.round(ch2Min);
  document.getElementById('ch2-graph-max').textContent = Math.round(ch2Max);
  
  if (ch5 < ch5Min) ch5Min = ch5;
  if (ch5 > ch5Max) ch5Max = ch5;
  document.getElementById('ch5-graph-min').textContent = Math.round(ch5Min);
  document.getElementById('ch5-graph-max').textContent = Math.round(ch5Max);
  
  if (ch6 < ch6Min) ch6Min = ch6;
  if (ch6 > ch6Max) ch6Max = ch6;
  document.getElementById('ch6-graph-min').textContent = Math.round(ch6Min);
  document.getElementById('ch6-graph-max').textContent = Math.round(ch6Max);
}

setInterval(() => {
  fetch('/data')
    .then(res => res.json())
    .then(data => {
      chart.data.datasets[0].data[0] = data.ch1;
      chart.data.datasets[0].data[1] = data.ch2;
      chart.data.datasets[0].data[2] = data.ch5;
      chart.data.datasets[0].data[3] = data.ch6;
      chart.update();

      updateGraphMinMax(data.ch1, data.ch2, data.ch5, data.ch6);
      
      document.getElementById('ch1-value').textContent = data.ch1 + ' µs';
      let ch1Status = 'Center';
      if (data.ch1 < 1400) ch1Status = '← Left';
      else if (data.ch1 > 1600) ch1Status = 'Right →';
      document.getElementById('ch1-status').textContent = ch1Status;

      document.getElementById('ch2-value').textContent = data.ch2 + ' µs';
      let ch2Status = 'Neutral';
      if (data.ch2 > 1700) ch2Status = '↑ Forward';
      else if (data.ch2 < 1300) ch2Status = '↓ Reverse';
      document.getElementById('ch2-status').textContent = ch2Status;

      document.getElementById('ch5-value').textContent = data.ch5 + ' µs';
      let ch5Status = 'Middle';
      if (data.ch5 > 1750) ch5Status = 'UP - Left Blink';
      else if (data.ch5 < 1250) ch5Status = 'DOWN - Right Blink';
      document.getElementById('ch5-status').textContent = ch5Status;

      document.getElementById('ch6-value').textContent = data.ch6 + ' µs';
      let ch6Status = 'OFF';
      if (data.ch6 > 1750) ch6Status = 'ON';
      document.getElementById('ch6-status').textContent = ch6Status;

      const leftIndicator = document.getElementById('left-led-indicator');
      if (data.ledStatus === 'LEFT BLINKING') {
        document.getElementById('left-led-status').textContent = '✓ On';
        leftIndicator.className = 'led-indicator left-on';
      } else {
        document.getElementById('left-led-status').textContent = 'Off';
        leftIndicator.className = 'led-indicator';
      }

      const rightIndicator = document.getElementById('right-led-indicator');
      if (data.ledStatus === 'RIGHT BLINKING') {
        document.getElementById('right-led-status').textContent = '✓ On';
        rightIndicator.className = 'led-indicator right-on';
      } else {
        document.getElementById('right-led-status').textContent = 'Off';
        rightIndicator.className = 'led-indicator';
      }

      const ch6Indicator = document.getElementById('ch6-led-indicator');
      if (data.ch6Led === 'ON') {
        document.getElementById('ch6-led-status').textContent = '✓ On';
        ch6Indicator.className = 'led-indicator ch6-on';
      } else {
        document.getElementById('ch6-led-status').textContent = 'Off';
        ch6Indicator.className = 'led-indicator';
      }

      const brightnessIndicator = document.getElementById('brightness-indicator');
      document.getElementById('brightness-status').textContent = data.brightness;
      if (data.brightness.includes('100%')) {
        brightnessIndicator.className = 'led-indicator brightness-on';
      } else {
        brightnessIndicator.className = 'led-indicator';
      }
    })
    .catch(err => console.error('Error:', err));
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
  delay(1000);

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

  Serial.println("\n\n=== ATOM RC Controller ===");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed!");
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.begin();
  Serial.println("Web server started.\n");
}

void loop() {
  server.handleClient();

  // Read all channels
  ch1Value = readChannel(CH1);
  ch2Value = readChannel(CH2);
  ch5Value = readChannel(CH5);
  ch6Value = readChannel(CH6);
  
  // Map channels to motor values
  // CH1 (Steering): 1000-2000µs → -255 to +255 (left to right)
  // CH2 (Throttle): 1000-2000µs → -255 to +255 (reverse to forward)
  int steering_before_smooth = map(ch1Value, 1000, 2000, -255, 255);
  int throttle_before_smooth = map(ch2Value, 1000, 2000, -255, 255);

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

  // Assign to global variables
  steering = steering_before_smooth;
  throttle = throttle_before_smooth;

  // Dead zone only for steering (steering is more sensitive to noise)
  if (abs(steering) < 20) steering = 0;

  // Smooth only steering, not throttle (throttle needs immediate response)
  smoothSteering = smooth(smoothSteering, steering);
  smoothThrottle = throttle;  // No smoothing for throttle - direct response

  // Debug output every 500ms
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 500) {
    Serial.print("CH1:");
    Serial.print(ch1Value);
    Serial.print(" CH2:");
    Serial.print(ch2Value);
    Serial.print(" | Raw Throttle:");
    Serial.print(throttle_before_smooth);
    Serial.print(" After Deadzone:");
    Serial.print(throttle);
    Serial.print(" After Smooth:");
    Serial.print((int)smoothThrottle);
    Serial.print(" | Left Motor:");
    Serial.print((int)smoothThrottle - (int)smoothSteering);
    Serial.print(" Right Motor:");
    Serial.println((int)smoothThrottle + (int)smoothSteering);
    lastDebug = millis();
  }

  // Control all LEDs
  controlLEDs(toggleState);
  controlCH6LED(ch6Value);
  controlBrightnessLED(ch2Value);

  // Control motors
  controlMotors((int)smoothThrottle, (int)smoothSteering);

  delay(50); // Update rate ~20Hz
}