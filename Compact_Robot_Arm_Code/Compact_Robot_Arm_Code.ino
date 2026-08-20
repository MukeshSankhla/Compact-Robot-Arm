#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <WiFi.h>
#include <WebServer.h>

Adafruit_PWMServoDriver pwm(0x40);

// =====================================================
// WIFI ACCESS POINT
// =====================================================

const char* AP_SSID     = "RobotArm-ESP32";
const char* AP_PASSWORD = "12345678";

WebServer server(80);

// =====================================================
// PCA9685 CHANNELS
// =====================================================
// M1 = BASE
// M2 = SHOULDER
// M3 = ELBOW
// M4 = WRIST
// M5 = GRIPPER
// =====================================================

#define M1_CH 0
#define M2_CH 1
#define M3_CH 2
#define M4_CH 3
#define M5_CH 4

// =====================================================
// JOYSTICK PINS
// =====================================================
// J1_X -> SHOULDER (M2)
// J1_Y -> BASE     (M1)
// J2_X -> ELBOW    (M3)
// J2_Y -> WRIST    (M4)
// =====================================================

#define J1_X  34
#define J1_Y  35
#define J1_SW 25

#define J2_X  32
#define J2_Y  33
#define J2_SW 27

// =====================================================
// SERVO PWM
// =====================================================

#define SERVO_MIN 150
#define SERVO_MAX 600

// =====================================================
// HOME POSITION
// =====================================================

float M1 = 100;
float M2 = 0;
float M3 = 180;
float M4 = 90;
float M5 = 0;

// =====================================================
// JOYSTICK CENTER
// =====================================================

int J1X_CENTER;
int J1Y_CENTER;
int J2X_CENTER;
int J2Y_CENTER;

// =====================================================
// JOYSTICK CONTROL
// =====================================================

#define DEADZONE 260
#define SPEED    1.5
#define SAMPLES  8

// =====================================================
// SERVO FUNCTION
// =====================================================

void setServo(uint8_t channel, float angle)
{
  angle = constrain(angle, 0, 180);

  int pulse = map(
    (int)angle,
    0,
    180,
    SERVO_MIN,
    SERVO_MAX
  );

  pwm.setPWM(channel, 0, pulse);
}


// =====================================================
// SMOOTH SERVO MOVE  (no jumps on power-on / home)
// =====================================================

void slowMove(uint8_t channel, float fromAngle, float toAngle,
              float stepSize = 0.8f, int msPerStep = 8)
{
  fromAngle = constrain(fromAngle, 0, 180);
  toAngle   = constrain(toAngle,   0, 180);

  if (fromAngle < toAngle)
  {
    for (float a = fromAngle; a <= toAngle; a += stepSize)
    {
      setServo(channel, a);
      delay(msPerStep);
    }
  }
  else
  {
    for (float a = fromAngle; a >= toAngle; a -= stepSize)
    {
      setServo(channel, a);
      delay(msPerStep);
    }
  }

  setServo(channel, toAngle); // ensure exact final position
}


// =====================================================
// AVERAGED ADC READ
// =====================================================

int readAveraged(int pin)
{
  long sum = 0;

  for (int i = 0; i < SAMPLES; i++)
  {
    sum += analogRead(pin);
  }

  return sum / SAMPLES;
}


// =====================================================
// CALIBRATE JOYSTICKS
// =====================================================

void calibrateJoysticks()
{
  Serial.println();
  Serial.println("=================================");
  Serial.println(" JOYSTICK CALIBRATION");
  Serial.println(" KEEP JOYSTICKS CENTERED");
  Serial.println("=================================");

  delay(2000);

  long sumJ1X = 0;
  long sumJ1Y = 0;
  long sumJ2X = 0;
  long sumJ2Y = 0;

  const int samples = 100;

  for (int i = 0; i < samples; i++)
  {
    sumJ1X += analogRead(J1_X);
    sumJ1Y += analogRead(J1_Y);

    sumJ2X += analogRead(J2_X);
    sumJ2Y += analogRead(J2_Y);

    delay(5);
  }

  J1X_CENTER = sumJ1X / samples;
  J1Y_CENTER = sumJ1Y / samples;

  J2X_CENTER = sumJ2X / samples;
  J2Y_CENTER = sumJ2Y / samples;

  Serial.println();

  Serial.print("J1 X Center (SHOULDER): ");
  Serial.println(J1X_CENTER);

  Serial.print("J1 Y Center (BASE): ");
  Serial.println(J1Y_CENTER);

  Serial.print("J2 X Center (ELBOW): ");
  Serial.println(J2X_CENTER);

  Serial.print("J2 Y Center (WRIST): ");
  Serial.println(J2Y_CENTER);

  Serial.println();
}


// =====================================================
// JOYSTICK MOVEMENT
// =====================================================

float joystick(int pin, int center)
{
  int value = readAveraged(pin);

  int difference = value - center;

  if (abs(difference) < DEADZONE)
  {
    return 0;
  }

  float movement;

  if (difference > 0)
  {
    movement = (float)(difference - DEADZONE)
               / (4095.0 - center - DEADZONE);
  }
  else
  {
    movement = (float)(difference + DEADZONE)
               / (center - DEADZONE);
  }

  movement = constrain(movement, -1.0, 1.0);

  return movement;
}


// =====================================================
// HOME  (smooth – no snapping)
// =====================================================

void home()
{
  // Smooth-move each axis to its home position
  slowMove(M1_CH, M1, 100);  M1 = 100;
  slowMove(M2_CH, M2,   0);  M2 =   0;
  slowMove(M3_CH, M3, 180);  M3 = 180;
  slowMove(M4_CH, M4,  90);  M4 =  90;
  slowMove(M5_CH, M5,   0);  M5 =   0;

  Serial.println();
  Serial.println("HOME POSITION");
}


// =====================================================
// WEB PAGE  – Premium UI
// =====================================================

const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>5-Axis Robot Arm Controller</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;600&display=swap" rel="stylesheet">

<style>
  :root {
    --bg:        #07090f;
    --surface:   #0e1117;
    --glass:     rgba(255,255,255,0.04);
    --border:    rgba(255,255,255,0.08);
    --accent:    #00e5ff;
    --accent2:   #7c4dff;
    --green:     #00e676;
    --orange:    #ff9100;
    --red:       #ff1744;
    --text:      #e8eaf6;
    --muted:     #546e7a;
    --radius:    16px;
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  html, body {
    height: 100%;
    background: var(--bg);
    color: var(--text);
    font-family: 'Inter', sans-serif;
    overflow-x: hidden;
  }

  /* ── Animated background grid ── */
  body::before {
    content: '';
    position: fixed;
    inset: 0;
    background-image:
      linear-gradient(rgba(0,229,255,0.03) 1px, transparent 1px),
      linear-gradient(90deg, rgba(0,229,255,0.03) 1px, transparent 1px);
    background-size: 40px 40px;
    pointer-events: none;
    z-index: 0;
  }

  .page {
    position: relative;
    z-index: 1;
    max-width: 700px;
    margin: 0 auto;
    padding: 16px;
    padding-bottom: 40px;
  }

  /* ── Header ── */
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 20px;
    padding: 14px 20px;
    background: var(--glass);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    backdrop-filter: blur(12px);
  }

  .header-title {
    font-size: 1.15rem;
    font-weight: 700;
    letter-spacing: 0.04em;
    background: linear-gradient(90deg, var(--accent), var(--accent2));
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
  }

  .header-sub {
    font-size: 0.7rem;
    color: var(--muted);
    margin-top: 2px;
    font-family: 'JetBrains Mono', monospace;
  }

  .status-badge {
    display: flex;
    align-items: center;
    gap: 7px;
    font-size: 0.72rem;
    color: var(--green);
    font-weight: 600;
    letter-spacing: 0.06em;
  }

  .dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: var(--green);
    box-shadow: 0 0 8px var(--green);
    animation: pulse 2s infinite;
  }

  @keyframes pulse {
    0%, 100% { opacity: 1; transform: scale(1); }
    50%       { opacity: 0.5; transform: scale(0.8); }
  }

  /* ── Section label ── */
  .section-label {
    font-size: 0.65rem;
    font-weight: 600;
    letter-spacing: 0.12em;
    color: var(--muted);
    text-transform: uppercase;
    margin-bottom: 8px;
    padding-left: 4px;
  }

  /* ── Glass card ── */
  .card {
    background: var(--glass);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 16px 18px;
    margin-bottom: 14px;
    backdrop-filter: blur(8px);
    transition: border-color 0.3s;
  }

  .card:hover { border-color: rgba(0,229,255,0.2); }

  /* ── SVG Robot Arm ── */
  #robot-svg-wrap {
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 8px 0 4px;
  }

  #robot-svg {
    width: 100%;
    max-width: 500px;
    height: 200px;
    overflow: visible;
  }



  /* ── Joystick panel ── */
  .joy-panel {
    display: flex;
    justify-content: space-around;
    align-items: flex-start;
    gap: 10px;
    flex-wrap: wrap;
  }

  .joy-block {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
  }

  .joy-title {
    font-size: 0.68rem;
    font-weight: 600;
    color: var(--muted);
    letter-spacing: 0.08em;
    text-transform: uppercase;
  }

  .joy-canvas-wrap {
    position: relative;
    width: 140px;
    height: 140px;
  }

  canvas.joystick {
    border-radius: 50%;
    touch-action: none;
    cursor: grab;
    display: block;
  }

  canvas.joystick:active { cursor: grabbing; }

  .joy-axes {
    display: flex;
    gap: 10px;
    font-family: 'JetBrains Mono', monospace;
    font-size: 0.65rem;
    color: var(--muted);
  }

  .joy-axes span { color: var(--accent); }

  /* ── Gripper ── */
  .gripper-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    flex-wrap: wrap;
  }

  .gripper-vis {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 80px;
    height: 60px;
  }

  .gripper-svg { overflow: visible; }

  .grip-btn-wrap {
    display: flex;
    gap: 10px;
    flex: 1;
    justify-content: flex-end;
  }

  /* ── Buttons ── */
  .btn {
    border: none;
    border-radius: 12px;
    padding: 13px 22px;
    font-size: 0.82rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    cursor: pointer;
    transition: transform 0.12s, box-shadow 0.2s, opacity 0.2s;
    text-transform: uppercase;
    position: relative;
    overflow: hidden;
  }

  .btn::after {
    content: '';
    position: absolute;
    inset: 0;
    background: white;
    opacity: 0;
    transition: opacity 0.15s;
  }

  .btn:active::after { opacity: 0.12; }
  .btn:active { transform: scale(0.95); }

  .btn-home {
    background: linear-gradient(135deg, #1565c0, #0288d1);
    color: white;
    box-shadow: 0 4px 16px rgba(2,136,209,0.35);
  }

  .btn-home:hover { box-shadow: 0 4px 24px rgba(2,136,209,0.55); }

  .btn-open {
    background: linear-gradient(135deg, #00897b, #00e676);
    color: #07090f;
    box-shadow: 0 4px 16px rgba(0,230,118,0.3);
  }

  .btn-open:hover { box-shadow: 0 4px 24px rgba(0,230,118,0.5); }

  .btn-close {
    background: linear-gradient(135deg, #e65100, #ff9100);
    color: white;
    box-shadow: 0 4px 16px rgba(255,145,0,0.3);
  }

  .btn-close:hover { box-shadow: 0 4px 24px rgba(255,145,0,0.5); }

  /* ── Action bar ── */
  .action-bar {
    display: flex;
    gap: 10px;
    justify-content: center;
    flex-wrap: wrap;
  }



  /* ── Footer ── */
  .footer {
    text-align: center;
    font-size: 0.62rem;
    color: var(--muted);
    margin-top: 20px;
    letter-spacing: 0.06em;
  }

  /* ── Responsive ── */
  @media (max-width: 420px) {
    .joy-canvas-wrap { width: 120px; height: 120px; }
    canvas.joystick { width: 120px !important; height: 120px !important; }
  }
</style>
</head>

<body>
<div class="page">

  <!-- ── HEADER ── -->
  <div class="header">
    <div>
      <div class="header-title">5-AXIS ROBOT ARM</div>
      <div class="header-sub">ESP32 · PCA9685 · WiFi AP</div>
    </div>
    <div class="status-badge">
      <div class="dot"></div>
      LIVE
    </div>
  </div>





  <!-- ── SVG STICK ROBOT ── -->
  <div class="section-label">Robot Visualisation</div>
  <div class="card">
    <div id="robot-svg-wrap">
      <svg id="robot-svg" viewBox="0 0 500 210" xmlns="http://www.w3.org/2000/svg">
        <!-- Grid lines for depth feel -->
        <line x1="0" y1="180" x2="500" y2="180" stroke="rgba(255,255,255,0.06)" stroke-width="1"/>
        <line x1="0" y1="160" x2="500" y2="160" stroke="rgba(255,255,255,0.03)" stroke-width="1"/>

        <!-- Base platform -->
        <rect id="svg-base-plate" x="185" y="178" width="130" height="8" rx="4"
              fill="rgba(0,229,255,0.15)" stroke="rgba(0,229,255,0.4)" stroke-width="1.5"/>

        <!-- Arm segments (drawn via JS) -->
        <line id="svg-seg1" x1="250" y1="178" x2="250" y2="128"
              stroke="#00e5ff" stroke-width="5" stroke-linecap="round"/>
        <line id="svg-seg2" x1="250" y1="128" x2="280" y2="88"
              stroke="#7c4dff" stroke-width="4.5" stroke-linecap="round"/>

        <!-- Gripper lines -->
        <line id="svg-grip1" x1="330" y1="58" x2="340" y2="52"
              stroke="#ff1744" stroke-width="3" stroke-linecap="round"/>
        <line id="svg-grip2" x1="330" y1="58" x2="340" y2="64"
              stroke="#ff1744" stroke-width="3" stroke-linecap="round"/>

        <!-- Joints (circles drawn on top) -->
        <circle id="svg-j0" cx="250" cy="178" r="6" fill="#00e5ff" opacity="0.9"/>
        <circle id="svg-j1" cx="250" cy="128" r="5" fill="#7c4dff" opacity="0.9"/>
        <circle id="svg-j2" cx="280" cy="88"  r="4.5" fill="#ff9100" opacity="0.9"/>

        <!-- In-SVG angle labels (updated by JS) -->
        <g id="svg-labels" font-family="JetBrains Mono,monospace" font-size="8" text-anchor="middle">
          <!-- BASE label -->
          <text id="lbl-base-name" x="250" y="200" fill="#00e5ff" opacity="0.7" font-size="7" letter-spacing="0.04em">BASE</text>
          <text id="tv1"           x="250" y="208" fill="#00e5ff" opacity="0.95" font-weight="bold">100°</text>

          <!-- SHOULDER label -->
          <text id="lbl-shl-name" x="262" y="126" fill="#00e5ff" opacity="0.7" font-size="7">SHOULDER</text>
          <text id="tv2"           x="262" y="134" fill="#00e5ff" opacity="0.95" font-weight="bold">0°</text>

          <!-- ELBOW label -->
          <text id="lbl-elb-name" x="292" y="86"  fill="#7c4dff" opacity="0.7" font-size="7">ELBOW</text>
          <text id="tv3"           x="292" y="94"  fill="#7c4dff" opacity="0.95" font-weight="bold">180°</text>

          <!-- WRIST label -->
          <text id="lbl-wrs-name" x="322" y="65"  fill="#ff9100" opacity="0.7" font-size="7">WRIST</text>
          <text id="tv4"           x="322" y="73"  fill="#ff9100" opacity="0.95" font-weight="bold">90°</text>

          <!-- GRIPPER label -->
          <text id="lbl-grp-name" x="344" y="55"  fill="#ff1744" opacity="0.7" font-size="7">GRIP</text>
          <text id="tv5"           x="344" y="63"  fill="#ff1744" opacity="0.95" font-weight="bold">0°</text>
        </g>

        <!-- Legend -->
        <g font-family="Inter,sans-serif" font-size="8" fill="rgba(255,255,255,0.38)">
          <rect x="6" y="6" width="7" height="7" rx="1" fill="#00e5ff"/>
          <text x="17" y="13">SHOULDER</text>
          <rect x="6" y="18" width="7" height="7" rx="1" fill="#7c4dff"/>
          <text x="17" y="25">ELBOW</text>
          <rect x="6" y="30" width="7" height="7" rx="1" fill="#ff9100"/>
          <text x="17" y="37">WRIST</text>
          <rect x="6" y="42" width="7" height="7" rx="1" fill="#ff1744"/>
          <text x="17" y="49">FINGERS</text>
        </g>
      </svg>
    </div>
  </div>





  <!-- ── VIRTUAL JOYSTICKS ── -->
  <div class="section-label">Virtual Joysticks</div>
  <div class="card">
    <div class="joy-panel">

      <div class="joy-block">
        <div class="joy-title">J1 — Base · Shoulder</div>
        <div class="joy-canvas-wrap">
          <canvas class="joystick" id="joy1" width="140" height="140"></canvas>
        </div>
        <div class="joy-axes">
          X(BASE):<span id="j1xv">0.00</span>&nbsp;
          Y(SHLD):<span id="j1yv">0.00</span>
        </div>
      </div>

      <div class="joy-block">
        <div class="joy-title">J2 — Wrist · Elbow</div>
        <div class="joy-canvas-wrap">
          <canvas class="joystick" id="joy2" width="140" height="140"></canvas>
        </div>
        <div class="joy-axes">
          X(WRST):<span id="j2xv">0.00</span>&nbsp;
          Y(ELBW):<span id="j2yv">0.00</span>
        </div>
      </div>

    </div>
  </div>


  <!-- ── GRIPPER ── -->
  <div class="section-label">Gripper Control</div>
  <div class="card">
    <div class="gripper-row">

      <!-- Mini gripper animation -->
      <div class="gripper-vis">
        <svg class="gripper-svg" id="grip-svg" width="80" height="60" viewBox="0 0 80 60">
          <line x1="40" y1="10" x2="40" y2="38" stroke="#ff1744" stroke-width="4" stroke-linecap="round"/>
          <line id="grip-top"    x1="40" y1="38" x2="60" y2="20" stroke="#ff9100" stroke-width="3.5" stroke-linecap="round"/>
          <line id="grip-bottom" x1="40" y1="38" x2="60" y2="56" stroke="#ff9100" stroke-width="3.5" stroke-linecap="round"/>
          <circle cx="40" cy="38" r="4" fill="#ff1744"/>
        </svg>
      </div>

      <div class="grip-btn-wrap">
        <button class="btn btn-open"  id="btnOpen"  onclick="gripper('open')">⬡ OPEN</button>
        <button class="btn btn-close" id="btnClose" onclick="gripper('close')">⬡ CLOSE</button>
      </div>

    </div>
  </div>


  <!-- ── SYSTEM ── -->
  <div class="section-label">System</div>
  <div class="card">
    <div class="action-bar">
      <button class="btn btn-home" onclick="homeRobot()">⌂ HOME</button>
    </div>
  </div>


  <div class="footer">
    RobotArm-ESP32 · 192.168.4.1 · 5-Axis PCA9685 Controller
  </div>

</div><!-- /page -->


<script>
// ============================================================
// HELPERS
// ============================================================

// ============================================================
// GRIPPER
// ============================================================

function gripper(action) {
  fetch('/gripper?action=' + action)
    .then(() => updatePositions());
  animateGripper(action === 'open');
}

function animateGripper(open) {
  const top    = document.getElementById('grip-top');
  const bottom = document.getElementById('grip-bottom');
  const gap    = open ? 22 : 8;
  top.setAttribute('x2',    '60');
  top.setAttribute('y2',    String(38 - gap));
  bottom.setAttribute('x2', '60');
  bottom.setAttribute('y2', String(38 + gap));
}

// ============================================================
// HOME
// ============================================================

function homeRobot() {
  const btn = document.querySelector('.btn-home');
  btn.style.opacity = '0.5';
  fetch('/home').then(() => {
    btn.style.opacity = '1';
    setTimeout(updatePositions, 4000); // wait for arm to ease home
  });
}

// ============================================================
// STATUS POLLING
// ============================================================

let prevVals = { m1: -1, m2: -1, m3: -1, m4: -1, m5: -1 };

function updatePositions() {
  fetch('/status')
    .then(r => r.json())
    .then(d => {
      updateArmSVG(
        parseFloat(d.m1),
        parseFloat(d.m2),
        parseFloat(d.m3),
        parseFloat(d.m4),
        parseFloat(d.m5)
      );

      animateGripper(parseFloat(d.m5) > 45);
    })
    .catch(() => {}); // silent on network hiccup
}

setInterval(updatePositions, 500);

window.addEventListener('load', () => {
  animateGripper(false);
  updatePositions();
});

// ============================================================
// SVG ROBOT ARM  (side-view 2D forward kinematics)
// ============================================================
// Segment lengths (px): shoulder, elbow, fingers
const SEG = [80, 70, 30];
const BASE_X = 250;
const BASE_Y = 170;

function deg2rad(d) { return d * Math.PI / 180; }

function updateArmSVG(m1, m2, m3, m4, m5) {
  // ── Shoulder / Elbow in the vertical plane (side view) ──
  // Shoulder: servo 0° → pointing down-right, 90° → level, 180° → pointing up
  const a_shoulder = deg2rad(180 - m2);          // screen angle for shoulder
  const a_elbow    = deg2rad(180 - m3);          // elbow bend relative to previous

  // BASE rotation: map 0-180° → ±60 px lateral shift for a parallax hint
  const baseShift = ((m1 - 90) / 90) * 60;

  const bx = BASE_X + baseShift;
  const by = BASE_Y;

  // Shoulder joint position (Elbow pivot)
  const j1x = bx  + SEG[0] * Math.cos(a_shoulder);
  const j1y = by  - SEG[0] * Math.sin(a_shoulder);

  // Elbow joint position (Wrist pivot)
  const a_elbow_arm = a_shoulder + (a_elbow - Math.PI); // running arm direction
  const j2x = j1x + SEG[1] * Math.cos(a_elbow_arm);
  const j2y = j1y - SEG[1] * Math.sin(a_elbow_arm);

  // Fingers angle relative to elbow arm, controlled by wrist angle m4
  const a_fingers_arm = a_elbow_arm + deg2rad(m4 - 90);

  // Gripper open/close spread (m5: 0°=closed, 90°=open)
  const gripSpread = deg2rad(25 * (m5 / 90));

  // Gripper fingers rotate around j2 according to wrist roll + grip spread
  const g1x = j2x + SEG[2] * Math.cos(a_fingers_arm + gripSpread);
  const g1y = j2y - SEG[2] * Math.sin(a_fingers_arm + gripSpread);
  const g2x = j2x + SEG[2] * Math.cos(a_fingers_arm - gripSpread);
  const g2y = j2y - SEG[2] * Math.sin(a_fingers_arm - gripSpread);

  function setLine(id, x1, y1, x2, y2) {
    const el = document.getElementById(id);
    if (el) {
      el.setAttribute('x1', x1.toFixed(1));
      el.setAttribute('y1', y1.toFixed(1));
      el.setAttribute('x2', x2.toFixed(1));
      el.setAttribute('y2', y2.toFixed(1));
    }
  }

  function setCircle(id, cx, cy) {
    const el = document.getElementById(id);
    if (el) {
      el.setAttribute('cx', cx.toFixed(1));
      el.setAttribute('cy', cy.toFixed(1));
    }
  }

  // Base plate
  const bp = document.getElementById('svg-base-plate');
  if (bp) {
    bp.setAttribute('x', (bx - 65).toFixed(1));
    bp.setAttribute('y', (by).toFixed(1));
  }

  setLine('svg-seg1', bx,  by,  j1x, j1y);
  setLine('svg-seg2', j1x, j1y, j2x, j2y);

  setLine('svg-grip1', j2x, j2y, g1x, g1y);
  setLine('svg-grip2', j2x, j2y, g2x, g2y);

  setCircle('svg-j0', bx,  by);
  setCircle('svg-j1', j1x, j1y);
  setCircle('svg-j2', j2x, j2y);

  // ── Move in-SVG angle labels to follow their joints ──
  function setLabel(nameId, valId, x, y, angle, suffix) {
    const nx = document.getElementById(nameId);
    const vx = document.getElementById(valId);
    if (!nx || !vx) return;
    nx.setAttribute('x', x.toFixed(1));
    nx.setAttribute('y', y.toFixed(1));
    vx.setAttribute('x', x.toFixed(1));
    vx.setAttribute('y', (y + 9).toFixed(1));
    vx.textContent = Math.round(angle) + suffix;
  }

  // BASE: centred below the base plate
  setLabel('lbl-base-name', 'tv1', bx, by + 12, m1, '°');

  // SHOULDER: offset right of midpoint of shoulder segment
  setLabel('lbl-shl-name',  'tv2', (bx + j1x)/2 + 15, (by + j1y)/2 - 5, m2, '°');

  // ELBOW: offset right of elbow joint (j1)
  setLabel('lbl-elb-name',  'tv3', j1x + 15, j1y - 5, m3, '°');

  // WRIST: offset right of wrist joint (j2)
  setLabel('lbl-wrs-name',  'tv4', j2x + 15, j2y - 5, m4, '°');

  // GRIPPER: offset right of fingers
  setLabel('lbl-grp-name',  'tv5', (j2x + g1x)/2 + 18, (j2y + g1y)/2 - 5, m5, '°');
}

// ============================================================
// VIRTUAL JOYSTICK CLASS
// ============================================================

class VirtualJoystick {
  constructor(canvasId, jIndex, axisLabels) {
    this.canvas = document.getElementById(canvasId);
    this.ctx    = this.canvas.getContext('2d');
    this.jIndex = jIndex;
    this.labels = axisLabels; // [xLabel, yLabel]

    this.W = this.canvas.width;
    this.H = this.canvas.height;
    this.cx = this.W / 2;
    this.cy = this.H / 2;
    this.maxR = this.W / 2 - 18;  // max knob travel
    this.knobR = 22;

    this.kx = 0; // knob offset from center
    this.ky = 0;
    this.active = false;

    this._bindEvents();
    this._render();
    this._loop();
  }

  _bindEvents() {
    const c = this.canvas;
    c.addEventListener('mousedown',  e => this._start(e.offsetX, e.offsetY));
    c.addEventListener('mousemove',  e => { if (this.active) this._move(e.offsetX, e.offsetY); });
    c.addEventListener('mouseup',    () => this._release());
    c.addEventListener('mouseleave', () => this._release());

    c.addEventListener('touchstart',  e => { e.preventDefault(); const t = this._touch(e); this._start(t.x, t.y); }, { passive: false });
    c.addEventListener('touchmove',   e => { e.preventDefault(); const t = this._touch(e); if (this.active) this._move(t.x, t.y); }, { passive: false });
    c.addEventListener('touchend',    () => this._release());
    c.addEventListener('touchcancel', () => this._release());
  }

  _touch(e) {
    const r = this.canvas.getBoundingClientRect();
    const t = e.touches[0];
    return { x: t.clientX - r.left, y: t.clientY - r.top };
  }

  _start(x, y) {
    this.active = true;
    this._move(x, y);
  }

  _move(x, y) {
    let dx = x - this.cx;
    let dy = y - this.cy;
    const dist = Math.sqrt(dx*dx + dy*dy);
    if (dist > this.maxR) {
      dx = dx / dist * this.maxR;
      dy = dy / dist * this.maxR;
    }
    this.kx = dx;
    this.ky = dy;
  }

  _release() {
    this.active = false;
    // Snap back animation handled in _loop
  }

  get normX() { return this.kx / this.maxR; }
  get normY() { return this.ky / this.maxR; }

  _render() {
    const ctx = this.ctx;
    ctx.clearRect(0, 0, this.W, this.H);

    // Outer ring
    const grad = ctx.createRadialGradient(this.cx, this.cy, 10, this.cx, this.cy, this.cx);
    grad.addColorStop(0,   'rgba(0,229,255,0.06)');
    grad.addColorStop(0.7, 'rgba(0,229,255,0.04)');
    grad.addColorStop(1,   'rgba(0,229,255,0.0)');
    ctx.beginPath();
    ctx.arc(this.cx, this.cy, this.cx - 4, 0, Math.PI * 2);
    ctx.fillStyle = grad;
    ctx.fill();
    ctx.strokeStyle = 'rgba(0,229,255,0.25)';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Crosshairs
    ctx.strokeStyle = 'rgba(255,255,255,0.08)';
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 4]);
    ctx.beginPath();
    ctx.moveTo(this.cx, 8); ctx.lineTo(this.cx, this.H - 8);
    ctx.moveTo(8, this.cy); ctx.lineTo(this.W - 8, this.cy);
    ctx.stroke();
    ctx.setLineDash([]);

    // Deadzone ring
    ctx.beginPath();
    ctx.arc(this.cx, this.cy, 14, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Line from center to knob
    const kAbsX = this.cx + this.kx;
    const kAbsY = this.cy + this.ky;
    ctx.beginPath();
    ctx.moveTo(this.cx, this.cy);
    ctx.lineTo(kAbsX, kAbsY);
    ctx.strokeStyle = 'rgba(0,229,255,0.4)';
    ctx.lineWidth = 2;
    ctx.stroke();

    // Knob shadow
    ctx.beginPath();
    ctx.arc(kAbsX, kAbsY, this.knobR + 4, 0, Math.PI * 2);
    const sg = ctx.createRadialGradient(kAbsX, kAbsY, 0, kAbsX, kAbsY, this.knobR + 4);
    sg.addColorStop(0, 'rgba(0,229,255,0.2)');
    sg.addColorStop(1, 'rgba(0,229,255,0)');
    ctx.fillStyle = sg;
    ctx.fill();

    // Knob
    const kg = ctx.createRadialGradient(kAbsX - 4, kAbsY - 4, 2, kAbsX, kAbsY, this.knobR);
    kg.addColorStop(0, '#80f7ff');
    kg.addColorStop(1, '#0097a7');
    ctx.beginPath();
    ctx.arc(kAbsX, kAbsY, this.knobR, 0, Math.PI * 2);
    ctx.fillStyle = kg;
    ctx.fill();
    ctx.strokeStyle = 'rgba(0,229,255,0.6)';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Center dot
    ctx.beginPath();
    ctx.arc(this.cx, this.cy, 4, 0, Math.PI * 2);
    ctx.fillStyle = 'rgba(255,255,255,0.3)';
    ctx.fill();
  }

  _loop() {
    if (!this.active && (Math.abs(this.kx) > 0.5 || Math.abs(this.ky) > 0.5)) {
      this.kx *= 0.82;
      this.ky *= 0.82;
    }
    this._render();
    requestAnimationFrame(() => this._loop());
  }
}

// ============================================================
// JOYSTICK COMMAND LOOP
// ============================================================

let j1, j2;

window.addEventListener('load', () => {
  j1 = new VirtualJoystick('joy1', 1, ['BASE','SHOULDER']);
  j2 = new VirtualJoystick('joy2', 2, ['ELBOW','WRIST']);
});

const SPEED_UI  = 1.2;  // degrees per tick
const JOY_TICK  = 30;   // ms between commands
const DEAD_UI   = 0.08; // normalized deadzone

// Locally tracked angles for UI joystick (synced from /status)
let uiM1 = 100, uiM2 = 0, uiM3 = 180, uiM4 = 90;
let joyBusy = false;

async function sendJoyMove(motor, angle) {
  await fetch('/motor?m=' + motor + '&a=' + angle.toFixed(1));
}

setInterval(async () => {
  if (joyBusy) return;
  if (!j1 || !j2) return;

  const nx1 = j1.normX; // J1 X → BASE (M1)     reversed
  const ny1 = j1.normY; // J1 Y → SHOULDER (M2)
  const nx2 = j2.normY; // J2 Y → ELBOW (M3)    ← swapped
  const ny2 = j2.normX; // J2 X → WRIST (M4)    ← swapped

  document.getElementById('j1xv').innerText = nx1.toFixed(2);
  document.getElementById('j1yv').innerText = ny1.toFixed(2);
  document.getElementById('j2xv').innerText = j2.normX.toFixed(2);
  document.getElementById('j2yv').innerText = j2.normY.toFixed(2);

  const promises = [];

  if (Math.abs(nx1) > DEAD_UI) {
    uiM1 = Math.max(0, Math.min(180, uiM1 - nx1 * SPEED_UI));
    promises.push(sendJoyMove(1, uiM1));
  }
  if (Math.abs(ny1) > DEAD_UI) {
    uiM2 = Math.max(0, Math.min(180, uiM2 - ny1 * SPEED_UI));
    promises.push(sendJoyMove(2, uiM2));
  }
  if (Math.abs(nx2) > DEAD_UI) {
    uiM3 = Math.max(0, Math.min(180, uiM3 + nx2 * SPEED_UI));
    promises.push(sendJoyMove(3, uiM3));
  }
  if (Math.abs(ny2) > DEAD_UI) {
    uiM4 = Math.max(0, Math.min(180, uiM4 + ny2 * SPEED_UI));
    promises.push(sendJoyMove(4, uiM4));
  }

  if (promises.length) {
    joyBusy = true;
    await Promise.all(promises);
    joyBusy = false;
  }

}, JOY_TICK);

// ============================================================
// Sync uiM* from status (so manual slider and physical
// joystick don't fight the UI joystick values)
// ============================================================

setInterval(() => {
  fetch('/status')
    .then(r => r.json())
    .then(d => {
      if (!j1 || !j2) return;
      if (!j1.active && !j2.active) {
        uiM1 = parseFloat(d.m1);
        uiM2 = parseFloat(d.m2);
        uiM3 = parseFloat(d.m3);
        uiM4 = parseFloat(d.m4);
      }
    })
    .catch(() => {});
}, 1000);

</script>
</body>
</html>
)rawliteral";


// =====================================================
// WEB: MAIN PAGE
// =====================================================

void handleRoot()
{
  server.send(
    200,
    "text/html",
    MAIN_PAGE
  );
}


// =====================================================
// WEB: MOTOR CONTROL
// =====================================================

void handleMotor()
{
  if (!server.hasArg("m") || !server.hasArg("a"))
  {
    server.send(
      400,
      "text/plain",
      "Missing motor or angle"
    );

    return;
  }

  int   motor = server.arg("m").toInt();
  float angle = server.arg("a").toFloat();

  angle = constrain(angle, 0, 180);

  switch (motor)
  {
    case 1:
      M1 = angle;
      setServo(M1_CH, M1);
      break;

    case 2:
      M2 = angle;
      setServo(M2_CH, M2);
      break;

    case 3:
      M3 = angle;
      setServo(M3_CH, M3);
      break;

    case 4:
      M4 = angle;
      setServo(M4_CH, M4);
      break;

    default:
      server.send(400, "text/plain", "Invalid motor");
      return;
  }

  server.send(200, "text/plain", "OK");
}


// =====================================================
// WEB: GRIPPER
// FIX: open = 90° (gripper physically opens)
//      close = 0°  (gripper physically closes)
// =====================================================

void handleGripper()
{
  if (!server.hasArg("action"))
  {
    server.send(400, "text/plain", "Missing action");
    return;
  }

  String action = server.arg("action");

  if (action == "open")
  {
    M5 = 90;                    // ← FIX: was 0 (was actually closing)
    setServo(M5_CH, M5);
    Serial.println("WEB: M5 OPEN (90 deg)");
  }
  else if (action == "close")
  {
    M5 = 0;                     // ← FIX: was 90 (was actually opening)
    setServo(M5_CH, M5);
    Serial.println("WEB: M5 CLOSE (0 deg)");
  }

  server.send(200, "text/plain", "OK");
}


// =====================================================
// WEB: HOME
// =====================================================

void handleHome()
{
  home();  // smooth move – no snapping

  server.send(200, "text/plain", "HOME");
}


// =====================================================
// WEB: STATUS
// =====================================================

void handleStatus()
{
  String json = "{";

  json += "\"m1\":" + String(M1, 1) + ",";
  json += "\"m2\":" + String(M2, 1) + ",";
  json += "\"m3\":" + String(M3, 1) + ",";
  json += "\"m4\":" + String(M4, 1) + ",";
  json += "\"m5\":" + String(M5, 1);

  json += "}";

  server.send(200, "application/json", json);
}


// =====================================================
// START WEB SERVER
// =====================================================

void startWebServer()
{
  server.on("/",       HTTP_GET, handleRoot);
  server.on("/motor",  HTTP_GET, handleMotor);
  server.on("/gripper",HTTP_GET, handleGripper);
  server.on("/home",   HTTP_GET, handleHome);
  server.on("/status", HTTP_GET, handleStatus);

  server.begin();

  Serial.println("Web server started");
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  // ---------------------------------------------------
  // ESP32 I2C
  // ---------------------------------------------------

  Wire.begin(21, 22);

  // ---------------------------------------------------
  // PCA9685
  // ---------------------------------------------------

  pwm.begin();
  pwm.setPWMFreq(50);

  // ---------------------------------------------------
  // Buttons
  // ---------------------------------------------------

  pinMode(J1_SW, INPUT_PULLUP);
  pinMode(J2_SW, INPUT_PULLUP);

  delay(500);

  // ---------------------------------------------------
  // Joystick calibration
  // ---------------------------------------------------

  calibrateJoysticks();

  // ---------------------------------------------------
  // HOME – smooth move to safe start position
  // ---------------------------------------------------

  home();

  delay(500);

  // ===================================================
  // START ESP32 ACCESS POINT
  // ===================================================

  WiFi.mode(WIFI_AP);

  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println();
  Serial.println("=================================");
  Serial.println(" ESP32 ROBOT ACCESS POINT");
  Serial.println("=================================");

  Serial.print("SSID: ");
  Serial.println(AP_SSID);

  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);

  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  Serial.println();

  // ---------------------------------------------------
  // Start Web Server
  // ---------------------------------------------------

  startWebServer();

  Serial.println("=================================");
  Serial.println(" ROBOT READY");
  Serial.println("=================================");
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  // ===================================================
  // PROCESS WEB REQUESTS
  // ===================================================

  server.handleClient();


  // ===================================================
  // PHYSICAL JOYSTICK CONTROL
  // ===================================================

  // -------------------------------------------------
  // READ JOYSTICKS
  // -------------------------------------------------

  float j1x = joystick(J1_X, J1X_CENTER);
  float j1y = joystick(J1_Y, J1Y_CENTER);
  float j2x = joystick(J2_X, J2X_CENTER);
  float j2y = joystick(J2_Y, J2Y_CENTER);


  // =================================================
  // J1 X -> M2 SHOULDER (reversed)
  // =================================================

  if (j1x != 0)
  {
    M2 -= j1x * SPEED;
    M2  = constrain(M2, 0, 180);
    setServo(M2_CH, M2);
  }


  // =================================================
  // J1 Y -> M1 BASE (reversed)
  // =================================================

  if (j1y != 0)
  {
    M1 -= j1y * SPEED;
    M1  = constrain(M1, 0, 180);
    setServo(M1_CH, M1);
  }


  // =================================================
  // J2 X -> M3 ELBOW (reversed)
  // =================================================

  if (j2x != 0)
  {
    M3 += j2x * SPEED;
    M3  = constrain(M3, 0, 180);
    setServo(M3_CH, M3);
  }


  // =================================================
  // J2 Y -> M4 WRIST
  // =================================================

  if (j2y != 0)
  {
    M4 += j2y * SPEED;
    M4  = constrain(M4, 0, 180);
    setServo(M4_CH, M4);
  }


  // ===================================================
  // J1 BUTTON -> M5 OPEN  (90 deg)
  // ===================================================

  if (digitalRead(J1_SW) == LOW)
  {
    M5 = 90;
    setServo(M5_CH, M5);
    Serial.println("M5 OPEN");
    delay(300);
  }


  // ===================================================
  // J2 BUTTON -> M5 CLOSE  (0 deg)
  // ===================================================

  if (digitalRead(J2_SW) == LOW)
  {
    M5 = 0;
    setServo(M5_CH, M5);
    Serial.println("M5 CLOSE");
    delay(300);
  }


  delay(20);
}