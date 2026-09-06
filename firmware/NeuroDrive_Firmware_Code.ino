#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

#define SDA_PIN 19
#define SCL_PIN 18

#define MPU6050_ADDR 0x68

const char* ssid = "MPU6050_Controller";
const char* password = "12345678";

WebServer server(80);

int16_t gyroX, gyroY, gyroZ;
int16_t accelX, accelY, accelZ;

// Lightly smoothed values (reduces jitter -> smoother steering/speed in-game)
float gyroX_f = 0, accelY_f = 0;
const float FILTER_ALPHA = 0.35;  // higher = more responsive, lower = smoother


// ==============================
// READ MPU6050
// ==============================

void readSensor() {

  // Accelerometer + Gyro burst read
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU6050_ADDR, 14);

  if (Wire.available() == 14) {

    accelX = (Wire.read() << 8) | Wire.read();
    accelY = (Wire.read() << 8) | Wire.read();
    accelZ = (Wire.read() << 8) | Wire.read();

    // Skip temperature
    Wire.read();
    Wire.read();

    gyroX = (Wire.read() << 8) | Wire.read();
    gyroY = (Wire.read() << 8) | Wire.read();
    gyroZ = (Wire.read() << 8) | Wire.read();

    // Exponential moving average smoothing
    gyroX_f  = (FILTER_ALPHA * gyroX)  + (1 - FILTER_ALPHA) * gyroX_f;
    accelY_f = (FILTER_ALPHA * accelY) + (1 - FILTER_ALPHA) * accelY_f;
  }
}


// ==============================
// SEND SENSOR DATA
// ==============================

void handleData() {

  String data = "{";

  data += "\"gx\":" + String(gyroX_f, 1);
  data += ",\"gy\":" + String(gyroY);
  data += ",\"gz\":" + String(gyroZ);

  data += ",\"ax\":" + String(accelX);
  data += ",\"ay\":" + String(accelY_f, 1);
  data += ",\"az\":" + String(accelZ);

  data += "}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", data);
}


// ==============================
// GAME WEBPAGE
// ==============================

void handleRoot() {

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>MPU6050 Racing</title>
<style>
  :root{
    --neon: #00e5ff;
    --neon2: #ff2d75;
    --accent: #ffd23f;
    --panel: rgba(20,24,38,0.72);
    --panel-border: rgba(255,255,255,0.08);
  }

  * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; user-select: none; }

  html, body {
    margin: 0;
    height: 100%;
    background: radial-gradient(circle at 50% 0%, #1b2440 0%, #0a0d18 60%, #050609 100%);
    color: #fff;
    font-family: 'Trebuchet MS', 'Segoe UI', Arial, sans-serif;
    overflow: hidden;
  }

  #app {
    max-width: 460px;
    margin: 0 auto;
    padding: 10px 10px 16px;
    display: flex;
    flex-direction: column;
    align-items: center;
    min-height: 100%;
  }

  h1 {
    margin: 4px 0 8px;
    font-size: 22px;
    letter-spacing: 1px;
    background: linear-gradient(90deg, var(--neon), var(--neon2));
    -webkit-background-clip: text;
    background-clip: text;
    color: transparent;
    text-shadow: 0 0 18px rgba(0,229,255,0.25);
  }

  /* ---------- DASHBOARD ---------- */
  #dashboard {
    display: flex;
    justify-content: space-between;
    gap: 8px;
    width: 100%;
    max-width: 420px;
    margin-bottom: 8px;
  }

  .stat {
    flex: 1;
    background: var(--panel);
    border: 1px solid var(--panel-border);
    border-radius: 12px;
    padding: 6px 4px;
    text-align: center;
    backdrop-filter: blur(6px);
  }

  .stat .label { font-size: 10px; opacity: 0.65; letter-spacing: 0.5px; }
  .stat .value { font-size: 18px; font-weight: bold; margin-top: 2px; }
  #livesVal { color: #ff5d7a; }
  #speedVal { color: var(--accent); }
  #comboVal { color: var(--neon); }

  /* ---------- GAME AREA ---------- */
  #game {
    position: relative;
    width: 100%;
    max-width: 420px;
    height: 62vh;
    max-height: 640px;
    margin: 4px auto 10px;
    overflow: hidden;
    border-radius: 14px;
    background: linear-gradient(#2b2f38, #3a3f4a);
    border: 3px solid #0d0f14;
    box-shadow: 0 0 0 2px #000 inset, 0 12px 30px rgba(0,0,0,0.55);
  }

  #sky {
    position: absolute;
    top: 0; left: 0; right: 0; height: 40%;
    background: linear-gradient(#2a3556, #4a5a86 60%, #7d93b8);
    z-index: 0;
  }

  .cloud {
    position: absolute;
    width: 60px; height: 18px;
    background: rgba(255,255,255,0.55);
    border-radius: 20px;
    filter: blur(1px);
  }

  #road {
    position: absolute;
    top: 0; bottom: 0;
    left: 14%;
    right: 14%;
    background: repeating-linear-gradient(
      #4a4e58, #4a4e58 2px, #45484f 2px, #45484f 4px
    ), #46494f;
    border-left: 8px solid #dcdcdc;
    border-right: 8px solid #dcdcdc;
  }

  .grass {
    position: absolute;
    top: 0; bottom: 0;
    width: 14%;
    background: repeating-linear-gradient(45deg, #1c6b2c, #1c6b2c 10px, #185f27 10px, #185f27 20px);
  }
  .grass.left { left: 0; }
  .grass.right { right: 0; }

  .rail {
    position: absolute;
    top: 0; bottom: 0;
    width: 6px;
    background: repeating-linear-gradient(#ffb000, #ffb000 18px, #8a5c00 18px, #8a5c00 22px);
  }
  .rail.left { left: calc(14% - 6px); }
  .rail.right { right: calc(14% - 6px); }

  .line {
    position: absolute;
    width: 10px;
    height: 70px;
    background: #f4f4f4;
    left: 50%;
    transform: translateX(-50%);
    border-radius: 3px;
    opacity: 0.9;
  }

  /* ---------- CAR ---------- */
  #car {
    position: absolute;
    width: 52px;
    height: 88px;
    bottom: 46px;
    left: 182px;
    transition: filter 0.1s linear;
    z-index: 5;
  }

  #carBody {
    position: absolute;
    inset: 0;
    background: linear-gradient(180deg, #ff5252, #c40808);
    border-radius: 14px 14px 10px 10px;
    box-shadow: 0 0 16px rgba(255,60,60,0.55), 0 6px 10px rgba(0,0,0,0.4);
  }

  #car.invincible #carBody {
    animation: flicker 0.15s linear infinite;
  }
  @keyframes flicker {
    0%, 100% { opacity: 1; filter: hue-rotate(0deg); }
    50% { opacity: 0.4; filter: hue-rotate(60deg); }
  }

  .windshield {
    position: absolute;
    width: 32px; height: 22px;
    background: linear-gradient(#0a1c2e, #1c3654);
    top: 12px; left: 10px;
    border-radius: 6px;
  }

  .headlight {
    position: absolute;
    width: 10px; height: 6px;
    background: #fff6c8;
    border-radius: 3px;
    top: 2px;
    box-shadow: 0 0 8px #fff6c8;
  }
  .headlight.l { left: 4px; }
  .headlight.r { right: 4px; }

  .beam {
    position: absolute;
    width: 30px;
    height: 60px;
    top: -58px;
    background: linear-gradient(to top, rgba(255,246,200,0.35), transparent);
    clip-path: polygon(30% 100%, 70% 100%, 100% 0, 0 0);
  }
  .beam.l { left: -3px; }
  .beam.r { right: -3px; }

  .wheel {
    position: absolute;
    width: 9px; height: 24px;
    background: #111;
    border-radius: 3px;
  }
  .w1 { left: -6px; top: 12px; }
  .w2 { right: -6px; top: 12px; }
  .w3 { left: -6px; bottom: 12px; }
  .w4 { right: -6px; bottom: 12px; }

  .exhaust {
    position: absolute;
    width: 10px; height: 10px;
    border-radius: 50%;
    background: rgba(200,200,200,0.5);
    bottom: -6px;
    left: 21px;
    pointer-events: none;
  }

  /* ---------- OBSTACLES / PICKUPS ---------- */
  .entity { position: absolute; z-index: 3; }

  .rock {
    width: 48px; height: 44px;
    background: linear-gradient(#8d8d8d, #545454);
    border-radius: 40% 60% 55% 45% / 50% 45% 55% 50%;
    box-shadow: 0 4px 6px rgba(0,0,0,0.4);
  }

  .cone {
    width: 36px; height: 46px;
  }
  .cone::before {
    content: "";
    position: absolute;
    left: 0; right: 0; bottom: 0; margin: auto;
    width: 0; height: 0;
    border-left: 18px solid transparent;
    border-right: 18px solid transparent;
    border-bottom: 46px solid #ff7a1a;
    filter: drop-shadow(0 3px 4px rgba(0,0,0,0.4));
  }
  .cone::after {
    content: "";
    position: absolute;
    left: 6px; bottom: 14px;
    width: 24px; height: 6px;
    background: #fff;
  }

  .rival {
    width: 50px; height: 84px;
    border-radius: 12px 12px 8px 8px;
    box-shadow: 0 0 12px rgba(0,0,0,0.5);
  }

  .coin {
    width: 26px; height: 26px;
    border-radius: 50%;
    background: radial-gradient(circle at 35% 30%, #fff6b0, #ffd23f 60%, #c99400);
    box-shadow: 0 0 10px rgba(255,210,63,0.7);
  }

  .shield {
    width: 30px; height: 30px;
    background: linear-gradient(135deg, #7df9ff, #0090c8);
    transform: rotate(45deg);
    border-radius: 6px;
    box-shadow: 0 0 12px rgba(0,229,255,0.8);
  }

  .popup {
    position: absolute;
    font-weight: bold;
    font-size: 14px;
    color: var(--accent);
    text-shadow: 0 0 6px rgba(0,0,0,0.8);
    pointer-events: none;
    z-index: 6;
    animation: floatUp 0.7s ease-out forwards;
  }
  @keyframes floatUp {
    from { transform: translateY(0); opacity: 1; }
    to   { transform: translateY(-40px); opacity: 0; }
  }

  /* ---------- OVERLAYS ---------- */
  .overlay {
    position: absolute;
    inset: 0;
    background: rgba(6,8,14,0.86);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    text-align: center;
    padding: 20px;
    z-index: 30;
    backdrop-filter: blur(2px);
  }
  .hidden { display: none !important; }

  .overlay h2 {
    font-size: 26px;
    margin: 0 0 6px;
    letter-spacing: 1px;
  }
  .overlay p { margin: 4px 0; opacity: 0.85; font-size: 14px; line-height: 1.5; }

  .btn {
    margin-top: 16px;
    padding: 12px 28px;
    font-size: 16px;
    font-weight: bold;
    border: none;
    border-radius: 30px;
    background: linear-gradient(90deg, var(--neon), #00b8d4);
    color: #001018;
    box-shadow: 0 6px 16px rgba(0,229,255,0.35);
    cursor: pointer;
  }
  .btn:active { transform: scale(0.96); }
  .btn.secondary {
    background: linear-gradient(90deg, #7d8494, #5a616f);
    color: #fff;
    margin-top: 8px;
  }

  .calBar {
    width: 200px;
    height: 8px;
    background: rgba(255,255,255,0.15);
    border-radius: 6px;
    overflow: hidden;
    margin-top: 14px;
  }
  .calFill {
    height: 100%;
    width: 0%;
    background: linear-gradient(90deg, var(--neon2), var(--accent));
  }

  #pauseBtn {
    position: absolute;
    top: 8px; right: 8px;
    width: 34px; height: 34px;
    border-radius: 50%;
    background: rgba(0,0,0,0.5);
    border: 1px solid rgba(255,255,255,0.2);
    color: #fff;
    font-size: 14px;
    z-index: 20;
  }

  .shake { animation: shakeAnim 0.3s; }
  @keyframes shakeAnim {
    0%, 100% { transform: translateX(0); }
    20% { transform: translateX(-8px); }
    40% { transform: translateX(8px); }
    60% { transform: translateX(-5px); }
    80% { transform: translateX(5px); }
  }

  #signalDot {
    display:inline-block; width:8px; height:8px; border-radius:50%;
    background:#33e07a; margin-left:6px; box-shadow:0 0 6px #33e07a;
  }
  #signalDot.bad { background:#ff5050; box-shadow:0 0 6px #ff5050; }

  .finalStats { display:flex; gap:18px; margin-top:10px; }
  .finalStats div { text-align:center; }
  .finalStats .fv { font-size:20px; font-weight:bold; }
  .finalStats .fl { font-size:11px; opacity:0.7; }
</style>
</head>
<body>

<div id="app">
  <h1>🏎️ MPU6050 RACING <span id="signalDot"></span></h1>

  <div id="dashboard">
    <div class="stat"><div class="label">SCORE</div><div class="value" id="scoreVal">0</div></div>
    <div class="stat"><div class="label">LIVES</div><div class="value" id="livesVal">3 / 3</div></div>
    <div class="stat"><div class="label">SPEED</div><div class="value" id="speedVal">5</div></div>
    <div class="stat"><div class="label">COMBO</div><div class="value" id="comboVal">x1</div></div>
  </div>

  <div id="game">
    <div id="sky">
      <div class="cloud" style="top:10px;left:20px;"></div>
      <div class="cloud" style="top:26px;left:120px;"></div>
      <div class="cloud" style="top:14px;right:30px;"></div>
    </div>

    <div class="grass left"></div>
    <div class="grass right"></div>
    <div class="rail left"></div>
    <div class="rail right"></div>
    <div id="road">
      <div class="line" id="line1"></div>
      <div class="line" id="line2"></div>
      <div class="line" id="line3"></div>
      <div class="line" id="line4"></div>
    </div>

    <button id="pauseBtn">⏸</button>

    <div id="car" class="hidden">
      <div id="carBody"></div>
      <div class="windshield"></div>
      <div class="headlight l"></div>
      <div class="headlight r"></div>
      <div class="beam l"></div>
      <div class="beam r"></div>
      <div class="wheel w1"></div>
      <div class="wheel w2"></div>
      <div class="wheel w3"></div>
      <div class="wheel w4"></div>
    </div>

    <!-- START SCREEN -->
    <div id="startScreen" class="overlay">
      <h2>Ready to race?</h2>
      <p>Tilt the sensor <b>left / right</b> to steer.<br>
         Tilt <b>forward</b> to speed up, <b>back</b> to slow down.<br>
         Grab 🟡 coins, chain dodges for combo bonus, and grab the blue shield for a shield.</p>
      <button class="btn" id="calibrateBtn">HOLD LEVEL &amp; START</button>
    </div>

    <!-- CALIBRATING -->
    <div id="calScreen" class="overlay hidden">
      <h2>Calibrating…</h2>
      <p>Keep the sensor still and level</p>
      <div class="calBar"><div class="calFill" id="calFill"></div></div>
    </div>

    <!-- PAUSED -->
    <div id="pauseScreen" class="overlay hidden">
      <h2>Paused</h2>
      <button class="btn" id="resumeBtn">RESUME</button>
    </div>

    <!-- GAME OVER -->
    <div id="gameOverScreen" class="overlay hidden">
      <h2 style="color:#ff4d5e;">💥 GAME OVER</h2>
      <div class="finalStats">
        <div><div class="fv" id="finalScore">0</div><div class="fl">SCORE</div></div>
        <div><div class="fv" id="finalBest">0</div><div class="fl">BEST</div></div>
        <div><div class="fv" id="finalCoins">0</div><div class="fl">COINS</div></div>
      </div>
      <button class="btn" id="retryBtn">RACE AGAIN</button>
      <button class="btn secondary" id="recalBtn">RECALIBRATE</button>
    </div>
  </div>
</div>

<script>
(function(){

// ==================================
// DOM REFS
// ==================================
const car = document.getElementById("car");
const game = document.getElementById("game");
const scoreDisplay = document.getElementById("scoreVal");
const livesDisplay = document.getElementById("livesVal");
const speedDisplay = document.getElementById("speedVal");
const comboDisplay = document.getElementById("comboVal");
const signalDot = document.getElementById("signalDot");

const startScreen = document.getElementById("startScreen");
const calScreen = document.getElementById("calScreen");
const calFill = document.getElementById("calFill");
const pauseScreen = document.getElementById("pauseScreen");
const gameOverScreen = document.getElementById("gameOverScreen");
const pauseBtn = document.getElementById("pauseBtn");

const GAME_W = 420;
const ROAD_MARGIN = 0.14; // matches CSS left/right %
const CAR_W = 52;

// ==================================
// STATE
// ==================================
let state = "start"; // start | calibrating | playing | paused | gameover
let carX = 182;
let carXTarget = 182;
let score = 0;
let coins = 0;
let lives = 3;
let speed = 5;
let combo = 1;
let comboCounter = 0;
let invincibleUntil = 0;
let shieldActive = false;
let lastTime = 0;
let spawnTimer = 0;
let spawnInterval = 1500;
let best = parseInt(localStorage.getItem("mpuRaceBest") || "0", 10);

let gyroX = 0, accelY = 0;
let gyroBase = 0, accelBase = 0;
let sensorOk = false;
let lastSensorTime = Date.now();

let lines = [
  document.getElementById("line1"),
  document.getElementById("line2"),
  document.getElementById("line3"),
  document.getElementById("line4")
];
lines.forEach((l,i)=> l.style.top = (i*180 - 180) + "px");

let entities = []; // {el, type, top, lane, hit}

// ==================================
// AUDIO (simple beeps, no files needed)
// ==================================
let audioCtx = null;
function beep(freq, dur, type, vol){
  try{
    if(!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = type || "square";
    osc.frequency.value = freq;
    gain.gain.value = vol || 0.06;
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.start();
    gain.gain.exponentialRampToValueAtTime(0.0001, audioCtx.currentTime + dur);
    osc.stop(audioCtx.currentTime + dur);
  }catch(e){}
}
const sfx = {
  coin: () => beep(1200, 0.12, "sine", 0.05),
  hit: () => beep(120, 0.25, "sawtooth", 0.08),
  shield: () => beep(700, 0.18, "triangle", 0.06),
  pass: () => beep(500, 0.05, "square", 0.02),
  over: () => beep(90, 0.5, "sawtooth", 0.08)
};

// ==================================
// SENSOR POLLING
// ==================================
async function getSensorData() {
  try {
    let response = await fetch("/data", {cache:"no-store"});
    let data = await response.json();
    gyroX = data.gx;
    accelY = data.ay;
    sensorOk = true;
    lastSensorTime = Date.now();
  } catch(e) {
    sensorOk = false;
  }
}
setInterval(getSensorData, 50);
setInterval(()=>{
  const stale = Date.now() - lastSensorTime > 700;
  signalDot.classList.toggle("bad", !sensorOk || stale);
}, 500);

// ==================================
// CALIBRATION
// ==================================
document.getElementById("calibrateBtn").addEventListener("click", startCalibration);
document.getElementById("recalBtn").addEventListener("click", ()=>{
  gameOverScreen.classList.add("hidden");
  startCalibration();
});

function startCalibration(){
  state = "calibrating";
  startScreen.classList.add("hidden");
  gameOverScreen.classList.add("hidden");
  calScreen.classList.remove("hidden");
  calFill.style.width = "0%";

  let samples = [];
  let elapsed = 0;
  const duration = 1200;
  const step = 50;

  const iv = setInterval(()=>{
    samples.push({g:gyroX, a:accelY});
    elapsed += step;
    calFill.style.width = Math.min(100, (elapsed/duration*100)) + "%";
    if(elapsed >= duration){
      clearInterval(iv);
      gyroBase = samples.reduce((s,v)=>s+v.g,0)/samples.length;
      accelBase = samples.reduce((s,v)=>s+v.a,0)/samples.length;
      calScreen.classList.add("hidden");
      resetGame();
      startGame();
    }
  }, step);
}

// ==================================
// GAME RESET / START
// ==================================
function resetGame(){
  carX = 182; carXTarget = 182;
  score = 0; coins = 0; lives = 3; speed = 5;
  combo = 1; comboCounter = 0;
  invincibleUntil = 0; shieldActive = false;
  spawnTimer = 0; spawnInterval = 1500;
  entities.forEach(e=> e.el.remove());
  entities = [];
  updateHUD();
  car.classList.remove("invincible");
}

function startGame(){
  state = "playing";
  car.classList.remove("hidden");
  lastTime = performance.now();
  requestAnimationFrame(gameLoop);
}

// ==================================
// PAUSE
// ==================================
pauseBtn.addEventListener("click", togglePause);
document.getElementById("resumeBtn").addEventListener("click", togglePause);
function togglePause(){
  if(state === "playing"){
    state = "paused";
    pauseScreen.classList.remove("hidden");
  } else if(state === "paused"){
    state = "playing";
    pauseScreen.classList.add("hidden");
    lastTime = performance.now();
    requestAnimationFrame(gameLoop);
  }
}

document.getElementById("retryBtn").addEventListener("click", ()=>{
  gameOverScreen.classList.add("hidden");
  resetGame();
  startGame();
});

// ==================================
// SPAWNING
// ==================================
const LANE_X = [70, 176, 282]; // left edge positions for 3 lanes (road ~ 356px wide minus margins)

function spawnEntity(){
  const roll = Math.random();
  let type;
  if(roll < 0.10) type = "coin";
  else if(roll < 0.15) type = "shield";
  else if(roll < 0.55) type = "rock";
  else if(roll < 0.80) type = "cone";
  else type = "rival";

  const lane = Math.floor(Math.random()*3);
  const el = document.createElement("div");
  el.className = "entity " + (type === "rival" ? "rival" : type);

  if(type === "rival"){
    const colors = ["#3b82f6","#22c55e","#a855f7","#f59e0b"];
    el.style.background = colors[Math.floor(Math.random()*colors.length)];
  }

  el.style.top = "-100px";
  const widths = {coin:26, shield:30, rock:48, cone:36, rival:50};
  el.style.left = (LANE_X[lane] + (46 - widths[type])/2) + "px";

  game.appendChild(el);
  entities.push({el, type, top:-100, lane, hit:false});
}

// ==================================
// SCORE POPUP
// ==================================
function popup(text, x, y){
  const p = document.createElement("div");
  p.className = "popup";
  p.style.left = x + "px";
  p.style.top = y + "px";
  p.textContent = text;
  game.appendChild(p);
  setTimeout(()=> p.remove(), 700);
}

// ==================================
// HUD
// ==================================
function updateHUD(){
  scoreDisplay.textContent = Math.floor(score);
  livesDisplay.textContent = Math.max(0, lives) + " / 3";
  speedDisplay.textContent = speed.toFixed(1);
  comboDisplay.textContent = "x" + combo;
}

// ==================================
// STEERING & SPEED (proportional, smoothed)
// ==================================
function steer(dt){
  const rel = gyroX - gyroBase;
  const deadzone = 400;
  let input = 0;
  if(Math.abs(rel) > deadzone){
    input = (rel > 0 ? 1 : -1) * Math.min(1, (Math.abs(rel)-deadzone)/6000);
  }
  carXTarget += input * 420 * dt; // px/sec at full tilt

  const minX = 8;
  const maxX = 420 - CAR_W - 8 - 2*(GAME_W*ROAD_MARGIN - 8) + (GAME_W*ROAD_MARGIN - 8);
  // simplified bounds based on visual road inset
  const leftBound = 6;
  const rightBound = 420 - 6 - CAR_W - 2*40; // approx accounting for grass+rail width baked visually
  carXTarget = Math.max(20, Math.min(340, carXTarget));

  // smooth (lerp) toward target for fluid motion
  carX += (carXTarget - carX) * Math.min(1, dt*10);
  car.style.left = carX + "px";
}

function controlSpeed(dt){
  const rel = accelY - accelBase;
  if(rel < -1500){
    speed += 3.2 * dt;
  } else if(rel > 1500){
    speed -= 3.2 * dt;
  } else {
    // gentle natural settle toward current baseline pace
    speed += (7 - speed) * 0.15 * dt;
  }

  const minSpeed = 3;
  const maxSpeed = 12 + Math.min(10, score/300); // caps out around 22 as score grows
  speed = Math.max(minSpeed, Math.min(maxSpeed, speed));
}

// ==================================
// ROAD MOVEMENT
// ==================================
function moveRoad(dt){
  const px = speed * 60 * dt;
  for(let l of lines){
    let top = parseFloat(l.style.top) + px;
    if(top > 700) top -= 720;
    l.style.top = top + "px";
  }
}

// ==================================
// COLLISION
// ==================================
function collides(aEl, bEl){
  const a = aEl.getBoundingClientRect();
  const b = bEl.getBoundingClientRect();
  const pad = 6; // slight forgiveness for fairer hitboxes
  return !(
    a.right - pad < b.left ||
    a.left + pad > b.right ||
    a.bottom - pad < b.top ||
    a.top + pad > b.bottom
  );
}

function handleHit(){
  if(shieldActive){
    shieldActive = false;
    invincibleUntil = performance.now() + 300;
    sfx.shield();
    return;
  }
  if(performance.now() < invincibleUntil) return;

  lives--;
  comboCounter = 0; combo = 1;
  speed = Math.max(3, speed - 3);
  invincibleUntil = performance.now() + 1400;
  car.classList.add("invincible");
  setTimeout(()=> car.classList.remove("invincible"), 1400);
  game.classList.add("shake");
  setTimeout(()=> game.classList.remove("shake"), 300);
  sfx.hit();
  updateHUD();

  if(lives <= 0){
    endGame();
  }
}

function moveEntities(dt){
  const px = speed * 60 * dt;
  for(let i = entities.length - 1; i >= 0; i--){
    const ent = entities[i];
    ent.top += px;
    ent.el.style.top = ent.top + "px";

    if(!ent.hit && collides(car, ent.el)){
      ent.hit = true;
      const cx = parseFloat(ent.el.style.left);
      if(ent.type === "coin"){
        coins++;
        score += 25 * combo;
        popup("+" + (25*combo), cx, ent.top);
        sfx.coin();
        ent.el.remove(); entities.splice(i,1);
        updateHUD();
        continue;
      } else if(ent.type === "shield"){
        shieldActive = true;
        popup("SHIELD!", cx, ent.top);
        sfx.shield();
        ent.el.remove(); entities.splice(i,1);
        continue;
      } else {
        handleHit();
        ent.el.remove(); entities.splice(i,1);
        continue;
      }
    }

    if(ent.top > 700){
      if(!ent.hit && ent.type !== "coin" && ent.type !== "shield"){
        // successfully dodged an obstacle
        comboCounter++;
        if(comboCounter % 5 === 0) combo = Math.min(8, combo + 1);
        score += 10 * combo;
        sfx.pass();
      }
      ent.el.remove();
      entities.splice(i,1);
      updateHUD();
    }
  }
}

// ==================================
// GAME LOOP
// ==================================
function gameLoop(now){
  if(state !== "playing") return;
  const dt = Math.min(0.05, (now - lastTime)/1000);
  lastTime = now;

  steer(dt);
  controlSpeed(dt);
  moveRoad(dt);

  // passive distance score
  score += speed * dt * 0.6;

  spawnTimer += dt*1000;
  spawnInterval = Math.max(650, 1500 - score*0.6);
  if(spawnTimer > spawnInterval){
    spawnTimer = 0;
    spawnEntity();
  }

  moveEntities(dt);
  updateHUD();

  requestAnimationFrame(gameLoop);
}

// ==================================
// GAME OVER
// ==================================
function endGame(){
  state = "gameover";
  sfx.over();
  if(score > best){
    best = Math.floor(score);
    localStorage.setItem("mpuRaceBest", best);
  }
  document.getElementById("finalScore").textContent = Math.floor(score);
  document.getElementById("finalBest").textContent = best;
  document.getElementById("finalCoins").textContent = coins;
  gameOverScreen.classList.remove("hidden");
}

})();
</script>

</body>
</html>
)rawliteral";


  server.send(
    200,
    "text/html",
    html
  );
}


// ==================================
// SETUP
// ==================================
void setup() {

  Serial.begin(115200);

  delay(1000);


  // I2C

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );


  // Wake MPU6050

  Wire.beginTransmission(
    MPU6050_ADDR
  );

  Wire.write(0x6B);

  Wire.write(0x00);

  Wire.endTransmission();


  // WiFi hotspot

  WiFi.softAP(
    ssid,
    password
  );


  Serial.println();

  Serial.println(
    "WiFi started!"
  );

  Serial.print(
    "IP Address: "
  );

  Serial.println(
    WiFi.softAPIP()
  );


  server.on(
    "/",
    handleRoot
  );

  server.on(
    "/data",
    handleData
  );


  server.begin();


  Serial.println(
    "Racing game server started!"
  );

}


// ==================================
// LOOP
// ==================================
void loop() {

  readSensor();

  server.handleClient();

}
