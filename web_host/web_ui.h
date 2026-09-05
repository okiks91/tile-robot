#pragma once
#include <Arduino.h>

// ============================================================================
// Tile Robot â€” Central Web UI HTML & JavaScript
// ============================================================================

static const char PROGMEM INDEX_HTML[] = R"rawhtml(<!DOCTYPE html>
<html><head>
<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>
<title>Tile Robot Central Controller</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;-webkit-user-select:none;-moz-user-select:none;-ms-user-select:none;user-select:none;-webkit-touch-callout:none !important;-webkit-user-drag:none !important;touch-action:none !important;}
html,body{width:100%;height:100%;overflow:hidden;background:#050811;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;-webkit-touch-callout:none !important;-webkit-user-select:none !important;user-select:none !important;}

/* Video Stream from ESP32-CAM (800x600 px) */
#stream{position:absolute;top:0;left:0;width:100%;height:100%;object-fit:contain;z-index:1;pointer-events:none !important;-webkit-user-select:none !important;user-select:none !important;-webkit-touch-callout:none !important;-webkit-user-drag:none !important;background:#000;}

/* Stream Placeholder when camera is connecting */
#streamOverlay{position:absolute;top:0;left:0;width:100%;height:100%;display:flex;align-items:center;justify-content:center;color:rgba(255,255,255,0.4);font-size:14px;letter-spacing:1px;z-index:2;pointer-events:none;}

/* Obstacle warning edge glows */
.warn{position:absolute;z-index:10;pointer-events:none;opacity:0;transition:opacity 0.15s;}
#warnLeft{top:0;left:0;width:55px;height:100%;background:linear-gradient(to right,rgba(239,68,68,0.85),transparent);}
#warnRight{top:0;right:0;width:55px;height:100%;background:linear-gradient(to left,rgba(239,68,68,0.85),transparent);}
#warnBottom{bottom:0;left:0;width:100%;height:55px;background:linear-gradient(to top,rgba(239,68,68,0.85),transparent);}

/* Top UI Header Bar */
#topBar{position:absolute;top:12px;left:12px;right:12px;display:flex;justify-content:space-between;align-items:center;z-index:50;}
#statusBox{background:rgba(15,23,42,0.75);backdrop-filter:blur(12px);border:1px solid rgba(255,255,255,0.25);border-radius:14px;padding:6px 14px;color:#fff;font-size:12px;display:flex;flex-direction:column;gap:2px;cursor:pointer;box-shadow:0 4px 15px rgba(0,0,0,0.5);}
.subStatus{font-size:10px;color:#94a3b8;font-family:monospace;}

/* Mode Switcher (MANUAL / AUTO) */
#modeGroup{display:flex;gap:8px;background:rgba(15,23,42,0.75);backdrop-filter:blur(12px);border:1px solid rgba(255,255,255,0.25);border-radius:24px;padding:4px;box-shadow:0 4px 15px rgba(0,0,0,0.5);}
.modeBtn{padding:8px 16px;border-radius:20px;font-size:12px;font-weight:700;letter-spacing:0.5px;color:#94a3b8;border:none;background:transparent;cursor:pointer;transition:all 0.2s;pointer-events:auto;}
.modeBtn.active-manual{background:#f59e0b;color:#000;box-shadow:0 0 14px rgba(245,158,11,0.85);}
.modeBtn.active-auto{background:#10b981;color:#000;box-shadow:0 0 14px rgba(16,185,129,0.85);}

/* Middle-Left Steering Mode Switcher (wheel / slide) */
#steerSwitcher{position:absolute;top:50%;left:12px;transform:translateY(-50%);display:flex;flex-direction:column;gap:6px;background:rgba(15,23,42,0.65);backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,0.2);border-radius:12px;padding:6px;z-index:50;}
.switchBtn{background:transparent;border:1px solid transparent;color:rgba(255,255,255,0.65);font-size:11px;font-weight:700;padding:6px 12px;border-radius:8px;cursor:pointer;text-transform:lowercase;transition:all 0.15s;}
.switchBtn.active-switch{background:rgba(255,255,255,0.25);color:#fff;border-color:rgba(255,255,255,0.4);box-shadow:0 0 10px rgba(255,255,255,0.3);}

/* Middle-Right Action Buttons (CLEAN / SPRAY) */
#actionPanel{position:absolute;top:42%;right:12px;transform:translateY(-50%);display:flex;flex-direction:column;gap:12px;background:rgba(15,23,42,0.65);backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,0.2);border-radius:16px;padding:8px;z-index:50;}
.actionBtn{width:92px;height:46px;background:rgba(15,23,42,0.35);backdrop-filter:blur(8px);-webkit-backdrop-filter:blur(8px);border:1.5px solid rgba(255,255,255,0.25);color:rgba(255,255,255,0.9);border-radius:12px;display:flex;align-items:center;justify-content:center;gap:6px;font-size:12px;font-weight:800;letter-spacing:1.5px;cursor:pointer;box-shadow:0 4px 15px rgba(0,0,0,0.4);transition:all 0.2s cubic-bezier(0.4,0,0.2,1);user-select:none;-webkit-user-select:none;touch-action:manipulation;pointer-events:auto;}
.actionBtn:hover{background:rgba(255,255,255,0.15);border-color:rgba(255,255,255,0.45);}
.actionDot{width:7px;height:7px;border-radius:50%;background:rgba(255,255,255,0.4);transition:all 0.2s ease;}

.cleanBtn.active{background:rgba(16,185,129,0.4);border-color:#34d399;color:#6ee7b7;box-shadow:0 0 20px rgba(16,185,129,0.7),inset 0 0 10px rgba(16,185,129,0.3);text-shadow:0 0 8px rgba(52,211,153,0.8);}
.cleanBtn.active .cleanDot{background:#34d399;box-shadow:0 0 8px #34d399;}

.sprayBtn.active{background:rgba(14,165,233,0.45);border-color:#38bdf8;color:#bae6fd;box-shadow:0 0 20px rgba(14,165,233,0.8),inset 0 0 10px rgba(14,165,233,0.3);text-shadow:0 0 8px rgba(56,189,248,0.8);}
.sprayBtn.active .sprayDot{background:#38bdf8;box-shadow:0 0 8px #38bdf8;}

/* Bottom Controls Container */
#controls{position:absolute;bottom:0;left:0;width:100%;height:50%;z-index:40;pointer-events:none;display:flex;justify-content:space-between;align-items:flex-end;padding:0 20px 20px;}

/* 2.5 Turns Steering Wheel (900 deg total lock-to-lock) */
#wheelContainer{width:180px;height:180px;pointer-events:auto;position:relative;display:flex;align-items:center;justify-content:center;}
#wheel{width:170px;height:170px;border-radius:50%;border:6px solid rgba(255,255,255,0.45);background:rgba(255,255,255,0.08);backdrop-filter:blur(8px);position:relative;display:flex;align-items:center;justify-content:center;box-shadow:0 6px 25px rgba(0,0,0,0.5);}
#wheelHub{width:60px;height:60px;border-radius:50%;background:rgba(255,255,255,0.2);border:2px solid rgba(255,255,255,0.45);}
#wheelSpokeH{position:absolute;width:100%;height:12px;background:rgba(255,255,255,0.3);}
#wheelSpokeV{position:absolute;width:12px;height:50%;top:50%;background:rgba(255,255,255,0.3);}
#wheelTopMarker{position:absolute;top:5px;width:8px;height:18px;background:#38bdf8;border-radius:4px;box-shadow:0 0 8px #38bdf8;}
#wheelText{position:absolute;bottom:-26px;color:#fff;font-size:11px;font-weight:700;background:rgba(0,0,0,0.6);padding:2px 10px;border-radius:10px;border:1px solid rgba(255,255,255,0.2);}

/* Sliding Steering Bar */
#sliderContainer{width:190px;height:120px;pointer-events:auto;position:relative;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px;}
#sliderTrack{width:180px;height:8px;background:rgba(255,255,255,0.25);backdrop-filter:blur(6px);border-radius:4px;position:relative;border:1px solid rgba(255,255,255,0.35);display:flex;align-items:center;}
#sliderCenterTick{position:absolute;left:50%;top:-4px;width:2px;height:16px;background:#38bdf8;transform:translateX(-50%);border-radius:1px;}
#sliderThumb{width:46px;height:46px;border-radius:50%;background:rgba(255,255,255,0.85);backdrop-filter:blur(8px);border:2px solid #fff;box-shadow:0 0 15px rgba(255,255,255,0.6),0 4px 10px rgba(0,0,0,0.5);position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);cursor:pointer;}
#sliderText{color:#fff;font-size:11px;font-weight:700;background:rgba(0,0,0,0.6);padding:2px 10px;border-radius:10px;border:1px solid rgba(255,255,255,0.2);}

/* Pedals: GAS, BRAKE, REVERSE */
#pedals{display:flex;flex-direction:column;gap:10px;pointer-events:auto;align-items:flex-end;}
.pedal{width:95px;border-radius:16px;display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:800;letter-spacing:1px;color:#fff;border:2px solid rgba(255,255,255,0.3);backdrop-filter:blur(8px);cursor:pointer;box-shadow:0 4px 15px rgba(0,0,0,0.4);transition:transform 0.08s;}
#gas{background:rgba(16,185,129,0.35);height:95px;}
#gas.pressed{background:rgba(16,185,129,0.85);box-shadow:0 0 20px rgba(16,185,129,0.9);transform:scale(0.95);}
#brake{background:rgba(239,68,68,0.35);height:55px;}
#brake.pressed{background:rgba(239,68,68,0.85);box-shadow:0 0 20px rgba(239,68,68,0.9);transform:scale(0.95);}
#rev{background:rgba(59,130,246,0.35);height:55px;}
#rev.pressed{background:rgba(59,130,246,0.85);box-shadow:0 0 20px rgba(59,130,246,0.9);transform:scale(0.95);}
</style>
</head><body oncontextmenu="return false;" onselectstart="return false;" ondragstart="return false;">

<!-- Live Video Feed from ESP32-CAM (High-FPS MJPEG) -->
<img id="stream" src="http://192.168.4.3/stream" onerror="setTimeout(function(){ var s=document.getElementById('stream'); if(s) s.src='http://' + (window.camIP || '192.168.4.3') + '/stream?t=' + Date.now(); }, 1200);">

<div id="warnLeft" class="warn"></div>
<div id="warnRight" class="warn"></div>
<div id="warnBottom" class="warn"></div>

<div id="topBar">
  <div id="statusBox" title="Tap to change Target IPs">
    <div id="statusTelemetry">Connecting to Robot Telemetry...</div>
    <div class="subStatus" id="statusLink">Motor: Connecting... | Cam: 192.168.4.3</div>
  </div>
  <div id="modeGroup">
    <button class="modeBtn active-manual" id="btnManual" onclick="setMode('manual')">MANUAL</button>
    <button class="modeBtn" id="btnAuto" onclick="setMode('auto')">AUTO</button>
  </div>
</div>

<!-- Middle-Left Steering Mode Switcher -->
<div id="steerSwitcher">
  <button class="switchBtn active-switch" id="btnWheelMode" onclick="setSteerMode('wheel')">wheel</button>
  <button class="switchBtn" id="btnSlideMode" onclick="setSteerMode('slide')">slide</button>
</div>

<!-- Middle-Right Action Buttons (CLEAN / SPRAY) -->
<div id="actionPanel">
  <button class="actionBtn cleanBtn" id="btnClean" onclick="toggleClean()">
    <span class="actionDot cleanDot"></span>
    <span>CLEAN</span>
  </button>
  <button class="actionBtn sprayBtn" id="btnSpray" onclick="triggerSpray()">
    <span class="actionDot sprayDot"></span>
    <span>SPRAY</span>
  </button>
</div>

<div id="controls">
  <!-- Rotating Steering Wheel (2.5 turns) -->
  <div id="wheelContainer">
    <div id="wheel">
      <div id="wheelSpokeH"></div>
      <div id="wheelSpokeV"></div>
      <div id="wheelHub"></div>
      <div id="wheelTopMarker"></div>
    </div>
    <div id="wheelText">0.0 Turns</div>
  </div>

  <!-- Sliding Steering Bar -->
  <div id="sliderContainer" style="display:none;">
    <div id="sliderTrack">
      <div id="sliderCenterTick"></div>
      <div id="sliderThumb"></div>
    </div>
    <div id="sliderText">0%</div>
  </div>

  <div id="pedals">
    <div id="gas" class="pedal">GAS</div>
    <div id="brake" class="pedal">BRAKE</div>
    <div id="rev" class="pedal">REVERSE</div>
  </div>
</div>

<script>
var savedMotor = localStorage.getItem('motor_ip');
if (!savedMotor || savedMotor.indexOf('192.168.1.') === 0) {
  savedMotor = '192.168.4.2';
  localStorage.setItem('motor_ip', savedMotor);
}
var motorIP = savedMotor;

var savedCam = localStorage.getItem('cam_ip');
if (!savedCam || savedCam.indexOf('192.168.1.') === 0) {
  savedCam = '192.168.4.3';
  localStorage.setItem('cam_ip', savedCam);
}
var camIP = savedCam;

var streamImg = document.getElementById('stream');
streamImg.src = 'http://' + camIP + '/stream';

var ws = null;
var wsConnected = false;

var currentMode = 'manual';
var currentSteerMode = 'wheel';
var steerAngle = 0;
var gasPressed = false;
var revPressed = false;
var brakePressed = false;

var MAX_TOTAL_DEG = 450;
var currentWheelDeg = 0;
var isDraggingWheel = false;
var lastTouchAngle = 0;

var isDraggingSlider = false;
var sliderStartX = 0;
var sliderTrackWidth = 180;
var sliderHalfTravel = 67;

var wheelEl = document.getElementById('wheel');
var wheelContainer = document.getElementById('wheelContainer');
var wheelText = document.getElementById('wheelText');
var sliderContainer = document.getElementById('sliderContainer');
var sliderTrack = document.getElementById('sliderTrack');
var sliderThumb = document.getElementById('sliderThumb');
var sliderText = document.getElementById('sliderText');
var btnWheelMode = document.getElementById('btnWheelMode');
var btnSlideMode = document.getElementById('btnSlideMode');

var btnManual = document.getElementById('btnManual');
var btnAuto = document.getElementById('btnAuto');
var gasBtn = document.getElementById('gas');
var brakeBtn = document.getElementById('brake');
var revBtn = document.getElementById('rev');
var statusTelem = document.getElementById('statusTelemetry');
var statusLink = document.getElementById('statusLink');
var statusBox = document.getElementById('statusBox');
var warnL = document.getElementById('warnLeft');
var warnR = document.getElementById('warnRight');
var warnB = document.getElementById('warnBottom');

statusBox.addEventListener('click', function() {
  var newMotor = prompt('Motor Controller (main.ino) IP address:', motorIP);
  if (newMotor && newMotor.trim() !== '') {
    motorIP = newMotor.trim();
    localStorage.setItem('motor_ip', motorIP);
  }
  var newCam = prompt('ESP32-CAM Stream IP address:', camIP);
  if (newCam && newCam.trim() !== '') {
    camIP = newCam.trim();
    localStorage.setItem('cam_ip', camIP);
    streamImg.src = 'http://' + camIP + '/stream';
  }
  if (ws) {
    ws.onclose = null;
    ws.close();
  }
  connectWS();
});

function connectWS() {
  var host = window.location.hostname || '192.168.4.1';
  var wsUrl = 'ws://' + host + ':81/';
  try {
    ws = new WebSocket(wsUrl);
  } catch(e) {
    statusLink.textContent = 'Motor WS Connecting (' + host + ':81)...';
    setTimeout(connectWS, 1500);
    return;
  }

  ws.onopen = function() {
    wsConnected = true;
    statusLink.textContent = 'Robot: Online (' + host + ') | Cam: ' + camIP;
    statusLink.style.color = '#4ade80';
  };

  ws.onmessage = function(e) {
    try {
      var d = JSON.parse(e.data);
      statusTelem.textContent = 'F:' + d.f.toFixed(0) + ' L:' + d.l.toFixed(0) + ' R:' + d.r.toFixed(0) + ' B:' + d.b.toFixed(0) + 'cm';

      if (d.m && d.m !== currentMode) {
        currentMode = d.m;
        updateModeButtons();
      }

      if (currentMode === 'manual') {
        var THRESH = 15.0;
        warnL.style.opacity = (d.l > 0 && d.l <= THRESH) ? Math.min(1, (THRESH - d.l) / THRESH + 0.3) : 0;
        warnR.style.opacity = (d.r > 0 && d.r <= THRESH) ? Math.min(1, (THRESH - d.r) / THRESH + 0.3) : 0;
        warnB.style.opacity = (d.b > 0 && d.b <= THRESH) ? Math.min(1, (THRESH - d.b) / THRESH + 0.3) : 0;
      } else {
        warnL.style.opacity = 0;
        warnR.style.opacity = 0;
        warnB.style.opacity = 0;
      }
    } catch(err) {}
  };

  ws.onclose = function() {
    wsConnected = false;
    statusLink.textContent = 'Robot: Connecting... | Cam: ' + camIP;
    statusLink.style.color = '#f87171';
    setTimeout(connectWS, 1500);
  };

  ws.onerror = function() {
    try { ws.close(); } catch(e) {}
  };
}
connectWS();

function setMode(mode) {
  currentMode = mode;
  updateModeButtons();
  if (wsConnected && ws.readyState === WebSocket.OPEN) {
    ws.send('M:' + mode);
  }
  fetch('/mode?set=' + mode).catch(function() {});
}

function updateModeButtons() {
  if (currentMode === 'auto') {
    btnAuto.className = 'modeBtn active-auto';
    btnManual.className = 'modeBtn';
  } else {
    btnManual.className = 'modeBtn active-manual';
    btnAuto.className = 'modeBtn';
  }
}

function setSteerMode(mode) {
  currentSteerMode = mode;
  steerAngle = 0;
  if (mode === 'wheel') {
    btnWheelMode.className = 'switchBtn active-switch';
    btnSlideMode.className = 'switchBtn';
    wheelContainer.style.display = 'flex';
    sliderContainer.style.display = 'none';
  } else {
    btnSlideMode.className = 'switchBtn active-switch';
    btnWheelMode.className = 'switchBtn';
    wheelContainer.style.display = 'none';
    sliderContainer.style.display = 'flex';
  }
  updateDrive();
}

function sendStop() {
  if (currentMode !== 'manual') return;
  if (wsConnected && ws.readyState === WebSocket.OPEN) {
    ws.send('X');
  } else {
    fetch('/cmd?action=stop').catch(function() {});
  }
}

function sendDrivePacket(angle, throttle) {
  if (currentMode !== 'manual') return;
  if (wsConnected && ws.readyState === WebSocket.OPEN) {
    ws.send('D:' + angle + ',' + throttle);
  } else {
    fetch('/cmd?action=steer&angle=' + angle + '&throttle=' + throttle).catch(function() {});
  }
}

function updateDrive() {
  if (currentMode !== 'manual') return;

  if (brakePressed) {
    sendStop();
    return;
  }

  if (gasPressed) {
    sendDrivePacket(steerAngle, 200);
  } else if (revPressed) {
    sendDrivePacket(steerAngle, -200);
  } else if (Math.abs(steerAngle) > 5) {
    sendDrivePacket(steerAngle, 0);
  } else {
    sendStop();
  }
}

setInterval(function() {
  updateDrive();
}, 40);

function getAngle(x, y, rect) {
  var cx = rect.left + rect.width / 2;
  var cy = rect.top + rect.height / 2;
  return Math.atan2(x - cx, -(y - cy)) * (180 / Math.PI);
}

var activeWheelPointerId = null;

wheelContainer.addEventListener('pointerdown', function(e) {
  e.preventDefault();
  activeWheelPointerId = e.pointerId;
  wheelContainer.setPointerCapture(e.pointerId);
  isDraggingWheel = true;
  var rect = wheelContainer.getBoundingClientRect();
  lastTouchAngle = getAngle(e.clientX, e.clientY, rect);
  wheelEl.style.transition = 'none';
});

wheelContainer.addEventListener('pointermove', function(e) {
  if (!isDraggingWheel || e.pointerId !== activeWheelPointerId) return;
  e.preventDefault();
  var rect = wheelContainer.getBoundingClientRect();
  var newAngle = getAngle(e.clientX, e.clientY, rect);
  var delta = newAngle - lastTouchAngle;
  if (delta > 180) delta -= 360;
  if (delta < -180) delta += 360;

  currentWheelDeg += delta;
  currentWheelDeg = Math.max(-MAX_TOTAL_DEG, Math.min(MAX_TOTAL_DEG, currentWheelDeg));
  lastTouchAngle = newAngle;

  wheelEl.style.transform = 'rotate(' + currentWheelDeg + 'deg)';
  var turns = (currentWheelDeg / 360).toFixed(1);
  wheelText.textContent = turns + ' Turns';
  steerAngle = Math.round((currentWheelDeg / MAX_TOTAL_DEG) * 100);

  updateDrive();
});

var endWheel = function(e) {
  if (!isDraggingWheel) return;
  if (e && e.pointerId !== undefined && e.pointerId !== activeWheelPointerId) return;
  isDraggingWheel = false;
  activeWheelPointerId = null;
  currentWheelDeg = 0;
  steerAngle = 0;
  wheelEl.style.transition = 'transform 0.35s cubic-bezier(0.2,0.8,0.2,1)';
  wheelEl.style.transform = 'rotate(0deg)';
  wheelText.textContent = '0.0 Turns';
  updateDrive();
  setTimeout(function() { wheelEl.style.transition = 'none'; }, 350);
};

wheelContainer.addEventListener('pointerup', endWheel);
wheelContainer.addEventListener('pointercancel', endWheel);

var activeSliderPointerId = null;

function moveSliderAt(clientX) {
  var rect = sliderTrack.getBoundingClientRect();
  var centerX = rect.left + rect.width / 2;
  var offset = clientX - centerX;
  offset = Math.max(-sliderHalfTravel, Math.min(sliderHalfTravel, offset));

  var pct = (offset / sliderHalfTravel) * 100;
  steerAngle = Math.round(pct);

  sliderThumb.style.left = 'calc(50% + ' + offset + 'px)';
  sliderText.textContent = steerAngle < 0 ? ('L ' + Math.abs(steerAngle) + '%') : (steerAngle > 0 ? ('R ' + steerAngle + '%') : '0%');
  updateDrive();
}

sliderContainer.addEventListener('pointerdown', function(e) {
  e.preventDefault();
  activeSliderPointerId = e.pointerId;
  sliderContainer.setPointerCapture(e.pointerId);
  isDraggingSlider = true;
  sliderThumb.style.transition = 'none';
  moveSliderAt(e.clientX);
});

sliderContainer.addEventListener('pointermove', function(e) {
  if (!isDraggingSlider || e.pointerId !== activeSliderPointerId) return;
  e.preventDefault();
  moveSliderAt(e.clientX);
});

var endSlider = function(e) {
  if (!isDraggingSlider) return;
  if (e && e.pointerId !== undefined && e.pointerId !== activeSliderPointerId) return;
  isDraggingSlider = false;
  activeSliderPointerId = null;
  steerAngle = 0;
  sliderThumb.style.transition = 'left 0.3s cubic-bezier(0.2,0.8,0.2,1)';
  sliderThumb.style.left = '50%';
  sliderText.textContent = '0%';
  updateDrive();
};

sliderContainer.addEventListener('pointerup', endSlider);
sliderContainer.addEventListener('pointercancel', endSlider);

function bindPointerPedal(el, onDown, onUp) {
  var activePedalPointerId = null;
  el.addEventListener('pointerdown', function(e) {
    e.preventDefault();
    activePedalPointerId = e.pointerId;
    try { el.setPointerCapture(e.pointerId); } catch(err) {}
    onDown();
  });
  var releaseHandler = function(e) {
    if (activePedalPointerId !== null && (e.pointerId === undefined || e.pointerId === activePedalPointerId)) {
      activePedalPointerId = null;
      onUp();
    }
  };
  el.addEventListener('pointerup', releaseHandler);
  el.addEventListener('pointercancel', releaseHandler);
}

bindPointerPedal(gasBtn,
  function() { gasPressed = true; gasBtn.classList.add('pressed'); updateDrive(); },
  function() { gasPressed = false; gasBtn.classList.remove('pressed'); updateDrive(); }
);

bindPointerPedal(revBtn,
  function() { revPressed = true; revBtn.classList.add('pressed'); updateDrive(); },
  function() { revPressed = false; revBtn.classList.remove('pressed'); updateDrive(); }
);

bindPointerPedal(brakeBtn,
  function() { brakePressed = true; brakeBtn.classList.add('pressed'); updateDrive(); },
  function() { brakePressed = false; brakeBtn.classList.remove('pressed'); updateDrive(); }
);

var sprayTimer = null;
function triggerSpray() {
  var btn = document.getElementById('btnSpray');
  if (btn) {
    btn.classList.add('active');
    if (sprayTimer) clearTimeout(sprayTimer);
    sprayTimer = setTimeout(function() { btn.classList.remove('active'); }, 5000);
  }
  fetch('/spray').catch(function() {});
}

var cleanState = false;
var cleanSprayTimer = null;
function toggleClean() {
  cleanState = !cleanState;
  var btn = document.getElementById('btnClean');
  if (btn) {
    if (cleanState) {
      btn.classList.add('active');
      var sprayBtn = document.getElementById('btnSpray');
      if (sprayBtn) {
        sprayBtn.classList.add('active');
        if (cleanSprayTimer) clearTimeout(cleanSprayTimer);
        cleanSprayTimer = setTimeout(function() { sprayBtn.classList.remove('active'); }, 3000);
      }
    } else {
      btn.classList.remove('active');
    }
  }
  fetch('/clean?state=' + (cleanState ? 'on' : 'off')).catch(function() {});
}

function pollSensorsBackup() {
  if (wsConnected) return;
  fetch('http://' + motorIP + '/sensors', {mode:'cors'})
    .then(function(r) { return r.json(); })
    .then(function(d) {
      statusLink.textContent = 'Motor HTTP: Online (' + motorIP + ') | Cam: ' + camIP;
      statusLink.style.color = '#4ade80';
      statusTelem.textContent = 'F:' + d.front.toFixed(0) + ' L:' + d.left.toFixed(0) + ' R:' + d.right.toFixed(0) + ' B:' + d.rear.toFixed(0) + 'cm';
      if (d.clean !== undefined && d.clean !== cleanState) {
        cleanState = d.clean;
        var b = document.getElementById('btnClean');
        if (b) {
          if (cleanState) b.classList.add('active');
          else b.classList.remove('active');
        }
      }
      if (d.mode && d.mode !== currentMode) {
        currentMode = d.mode;
        updateModeButtons();
      }
    })
    .catch(function() {});
}
setInterval(pollSensorsBackup, 500);

window.addEventListener('contextmenu', function(e) { e.preventDefault(); return false; }, { capture: true });
document.addEventListener('contextmenu', function(e) { e.preventDefault(); return false; }, { capture: true });
</script>
</body></html>
)rawhtml";
