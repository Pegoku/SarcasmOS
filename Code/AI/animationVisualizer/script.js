const canvas = document.getElementById("stage");
const ctx = canvas.getContext("2d");
const audioFileInput = document.getElementById("audioFile");
const resolutionXInput = document.getElementById("resolutionX");
const resolutionYInput = document.getElementById("resolutionY");
const pitchControl = document.getElementById("pitchControl");
const pitchValue = document.getElementById("pitchValue");
const player = document.getElementById("player");
const statusLabel = document.getElementById("status");

let audioContext;
let mediaSource;
let analyser;
let frequencyData;
let timeData;
let currentObjectUrl = null;
let smoothedOpen = 0.1;
let smoothedPitch = 0;

const state = {
  energy: 0,
  pitchHz: 0,
  ready: false,
  audioName: "",
};

function setStatus(message) {
  statusLabel.textContent = message;
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function updateOutputs() {
  pitchValue.textContent = `${Number(pitchControl.value).toFixed(2)}x`;
}

function resizeCanvas() {
  canvas.width = 960;
  canvas.height = 480;
  drawMouthGrid();
}

function drawCell(x, y, width, height, color) {
  ctx.fillStyle = color;
  ctx.fillRect(x, y, width, height);
}

function drawMouthGrid() {
  const width = canvas.width;
  const height = canvas.height;
  const cellsX = clamp(Number(resolutionXInput.value) || 24, 4, 128);
  const cellsY = clamp(Number(resolutionYInput.value) || 12, 2, 64);
  const gap = 3;
  const mouthOpen = clamp(smoothedOpen, 0, 1);
  const pitchFactor = clamp((smoothedPitch / 280) * Number(pitchControl.value), 0, 1.4);

  ctx.fillStyle = "#000000";
  ctx.fillRect(0, 0, width, height);

  const areaWidth = width * 0.76;
  const areaHeight = height * 0.34;
  const startX = (width - areaWidth) / 2;
  const startY = (height - areaHeight) / 2;
  const cellWidth = (areaWidth - gap * (cellsX - 1)) / cellsX;
  const cellHeight = (areaHeight - gap * (cellsY - 1)) / cellsY;
  const centerY = (cellsY - 1) / 2;
  const activeRows = Math.max(1, Math.round(mouthOpen * cellsY * 0.92));
  const halfBand = activeRows / 2;
  const phase = performance.now() * 0.01;

  for (let row = 0; row < cellsY; row += 1) {
    for (let col = 0; col < cellsX; col += 1) {
      const x = startX + col * (cellWidth + gap);
      const y = startY + row * (cellHeight + gap);
      const distanceFromCenter = Math.abs(row - centerY);
      const inBand = distanceFromCenter <= halfBand;
      let color = "#000000";

      if (inBand) {
        const wave = Math.sin(col * 0.65 + phase + row * 0.35 + pitchFactor * 2.8);
        const edge = halfBand === 0 ? 1 : distanceFromCenter / halfBand;
        const brightness = wave + (1 - edge) * 0.9 + pitchFactor * 0.35;
        color = brightness > 0.9 ? "#ffffff" : "#ffd400";
      }

      drawCell(x, y, cellWidth, cellHeight, color);
    }
  }

  ctx.strokeStyle = "rgba(255,255,255,0.08)";
  ctx.lineWidth = 1;
  ctx.strokeRect(startX - 12, startY - 12, areaWidth + 24, areaHeight + 24);
}

function detectPitch(buffer, sampleRate) {
  let rms = 0;

  for (let index = 0; index < buffer.length; index += 1) {
    const sample = buffer[index];
    rms += sample * sample;
  }

  rms = Math.sqrt(rms / buffer.length);

  if (rms < 0.015) {
    return 0;
  }

  let bestOffset = -1;
  let bestCorrelation = 0;
  const minSamples = Math.floor(sampleRate / 600);
  const maxSamples = Math.floor(sampleRate / 70);

  for (let offset = minSamples; offset <= maxSamples; offset += 1) {
    let correlation = 0;

    for (let index = 0; index < buffer.length - offset; index += 1) {
      correlation += 1 - Math.abs(buffer[index] - buffer[index + offset]);
    }

    correlation /= buffer.length - offset;

    if (correlation > bestCorrelation) {
      bestCorrelation = correlation;
      bestOffset = offset;
    }
  }

  if (bestCorrelation > 0.9 && bestOffset > 0) {
    return sampleRate / bestOffset;
  }

  return 0;
}

function analyseAudioFrame() {
  if (!analyser || !timeData || !frequencyData) {
    state.energy = 0;
    state.pitchHz = 0;
    return;
  }

  analyser.getByteTimeDomainData(timeData);
  analyser.getFloatTimeDomainData(frequencyData);

  let energy = 0;
  for (let index = 0; index < timeData.length; index += 1) {
    const sample = (timeData[index] - 128) / 128;
    energy += sample * sample;
  }

  state.energy = Math.sqrt(energy / timeData.length);
  state.pitchHz = detectPitch(frequencyData, audioContext.sampleRate);
}

function animate() {
  analyseAudioFrame();

  const pitchInfluence = Number(pitchControl.value);
  const normalizedPitch = clamp(state.pitchHz / 280, 0, 1) * 0.45 * pitchInfluence;
  const targetOpen = clamp(state.energy * 7.2 + normalizedPitch, 0, 1);

  smoothedOpen += (targetOpen - smoothedOpen) * 0.28;
  smoothedPitch += (state.pitchHz - smoothedPitch) * 0.2;

  drawMouthGrid();
  window.requestAnimationFrame(animate);
}

function ensureAudioGraph() {
  if (audioContext) {
    return;
  }

  audioContext = new window.AudioContext();
  analyser = audioContext.createAnalyser();
  analyser.fftSize = 2048;
  analyser.smoothingTimeConstant = 0.72;
  timeData = new Uint8Array(analyser.fftSize);
  frequencyData = new Float32Array(analyser.fftSize);
  mediaSource = audioContext.createMediaElementSource(player);
  mediaSource.connect(analyser);
  analyser.connect(audioContext.destination);
}

async function loadAudioFile(file) {
  if (!file) {
    return;
  }

  try {
    ensureAudioGraph();

    if (currentObjectUrl) {
      URL.revokeObjectURL(currentObjectUrl);
    }

    currentObjectUrl = URL.createObjectURL(file);
    player.src = currentObjectUrl;
    state.ready = true;
    state.audioName = file.name;
    setStatus(`Loaded ${file.name}. Press play to animate the mouth.`);

    if (audioContext.state === "suspended") {
      await audioContext.resume();
    }
  } catch (error) {
    setStatus("Audio setup failed. Try a different file.");
    console.error(error);
  }
}

audioFileInput.addEventListener("change", (event) => {
  const [file] = event.target.files;
  loadAudioFile(file);
});

resolutionXInput.addEventListener("input", resizeCanvas);
resolutionYInput.addEventListener("input", resizeCanvas);

pitchControl.addEventListener("input", updateOutputs);
player.addEventListener("play", async () => {
  if (!state.ready) {
    return;
  }

  ensureAudioGraph();

  if (audioContext.state === "suspended") {
    await audioContext.resume();
  }

  setStatus(`Animating ${state.audioName}...`);
});

player.addEventListener("pause", () => {
  setStatus(state.ready ? `Paused ${state.audioName}.` : "Upload a voice clip to wake the robot.");
});

player.addEventListener("ended", () => {
  setStatus(state.ready ? `Finished ${state.audioName}.` : "Upload a voice clip to wake the robot.");
});

updateOutputs();
resizeCanvas();
animate();
