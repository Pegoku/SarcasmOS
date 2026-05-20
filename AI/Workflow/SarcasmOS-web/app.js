const API_BASE = "http://localhost:8000";

const uploadInput = document.getElementById("uploadInput");
const uploadSend = document.getElementById("uploadSend");
const recordBtn = document.getElementById("recordBtn");
const stopBtn = document.getElementById("stopBtn");
const recordState = document.getElementById("recordState");
const textInput = document.getElementById("textInput");
const textSend = document.getElementById("textSend");
const transcriptOutput = document.getElementById("transcriptOutput");
const answerOutput = document.getElementById("answerOutput");
const audioPlayer = document.getElementById("audioPlayer");
const errorOutput = document.getElementById("errorOutput");
const statusBtn = document.getElementById("statusBtn");
const statusOutput = document.getElementById("statusOutput");
const mainView = document.getElementById("mainView");
const faceView = document.getElementById("faceView");
const openFaceView = document.getElementById("openFaceView");
const closeFaceView = document.getElementById("closeFaceView");
const faceUploadInput = document.getElementById("faceUploadInput");
const faceUploadSend = document.getElementById("faceUploadSend");
const faceRecordBtn = document.getElementById("faceRecordBtn");
const faceStopBtn = document.getElementById("faceStopBtn");
const faceRecordState = document.getElementById("faceRecordState");
const faceTextInput = document.getElementById("faceTextInput");
const faceTextSend = document.getElementById("faceTextSend");
const faceTranscriptOutput = document.getElementById("faceTranscriptOutput");
const faceAnswerOutput = document.getElementById("faceAnswerOutput");
const faceAudioPlayer = document.getElementById("faceAudioPlayer");
const historyList = document.getElementById("historyList");
const faceHistoryList = document.getElementById("faceHistoryList");
const clearHistory = document.getElementById("clearHistory");
const clearFaceHistory = document.getElementById("clearFaceHistory");
let svgEyes = [];
let svgPupils = [];
let svgMouthGroup = null;

const HISTORY_STORAGE_KEY = "sarcasmos.chatHistory";
let mediaRecorder = null;
let audioChunks = [];
let activePlaybackTarget = "main";
const chatHistory = [];
let pendingQuestion = "";
let blinkTimer = null;
let talkTimer = null;
let talkRaf = null;

function initSvgRefs() {
  svgEyes = Array.from(document.querySelectorAll(".svg-eye"));
  svgPupils = Array.from(document.querySelectorAll(".svg-pupil"));
  svgMouthGroup = document.querySelector(".svg-mouth-group");

  for (const eye of svgEyes) {
    eye.style.transformBox = "fill-box";
    eye.style.transformOrigin = "center";
  }
  for (const pupil of svgPupils) {
    pupil.style.transformBox = "fill-box";
    pupil.style.transformOrigin = "center";
  }
  if (svgMouthGroup) {
    svgMouthGroup.style.transformBox = "fill-box";
    svgMouthGroup.style.transformOrigin = "center";
  }
}

function setLookDirection(direction) {
  const directions = ["look-left", "look-right", "look-up", "look-down", "look-center"];
  for (const item of directions) {
    faceView.classList.remove(item);
  }
  if (direction) {
    faceView.classList.add(direction);
  }
  const offsets = {
    "look-left": { x: -10, y: 0, r: 8 },
    "look-right": { x: 10, y: 0, r: -8 },
    "look-up": { x: 0, y: -8, r: 0 },
    "look-down": { x: 0, y: 8, r: 0 },
    "look-center": { x: 0, y: 0, r: 0 },
  };
  const target = offsets[direction] || offsets["look-center"];
  for (const pupil of svgPupils) {
    pupil.setAttribute(
      "transform",
      `translate(${target.x} ${target.y}) rotate(${target.r})`
    );
  }
}

function triggerBlink() {
  faceView.classList.remove("blinking");
  void faceView.offsetWidth;
  faceView.classList.add("blinking");
  for (const eye of svgEyes) {
    eye.style.transform = "scaleY(0.08)";
  }
  setTimeout(() => {
    for (const eye of svgEyes) {
      eye.style.transform = "scaleY(1)";
    }
  }, 120);
}

function startBlinkLoop() {
  if (blinkTimer) {
    return;
  }
  blinkTimer = setInterval(() => {
    triggerBlink();
  }, 4200);
}

function stopBlinkLoop() {
  if (blinkTimer) {
    clearInterval(blinkTimer);
    blinkTimer = null;
  }
  for (const eye of svgEyes) {
    eye.style.transform = "scaleY(1)";
  }
}

function startTalkLoop() {
  if (talkRaf || !svgMouthGroup) {
    return;
  }
  const start = performance.now();
  const tick = (now) => {
    const t = (now - start) / 1000;
    const scale = 0.82 + Math.abs(Math.sin(t * 10)) * 0.35;
    svgMouthGroup.style.transform = `scaleY(${scale.toFixed(3)})`;
    talkRaf = requestAnimationFrame(tick);
  };
  talkRaf = requestAnimationFrame(tick);
}

function stopTalkLoop() {
  if (talkRaf) {
    cancelAnimationFrame(talkRaf);
    talkRaf = null;
  }
  if (svgMouthGroup) {
    svgMouthGroup.style.transform = "scaleY(1)";
  }
}

function setSpeaking(isSpeaking) {
  if (isSpeaking) {
    faceView.classList.add("speaking");
    startTalkLoop();
  } else {
    faceView.classList.remove("speaking");
    stopTalkLoop();
  }
}

function setRecordingState(label) {
  recordState.textContent = label;
  faceRecordState.textContent = label;
}

function setLoading(isLoading, label) {
  recordBtn.disabled = isLoading;
  stopBtn.disabled = !mediaRecorder || !mediaRecorder.state || mediaRecorder.state === "inactive";
  uploadSend.disabled = isLoading;
  textSend.disabled = isLoading;
  faceRecordBtn.disabled = isLoading;
  faceStopBtn.disabled = !mediaRecorder || !mediaRecorder.state || mediaRecorder.state === "inactive";
  faceUploadSend.disabled = isLoading;
  faceTextSend.disabled = isLoading;
  setRecordingState(label || (isLoading ? "Thinking..." : "Idle"));
}

function showError(message) {
  errorOutput.textContent = message || "";
}

function updateResult(data) {
  if (data.transcript !== undefined) {
    transcriptOutput.textContent = data.transcript || "(empty transcript)";
    faceTranscriptOutput.textContent = transcriptOutput.textContent;
  }
  if (data.answer !== undefined) {
    answerOutput.textContent = data.answer || "(empty answer)";
    faceAnswerOutput.textContent = answerOutput.textContent;
  }
  if (data.answer || data.transcript) {
    const question = (data.transcript || pendingQuestion || "").trim();
    if (question) {
      const entry = {
        question,
        answer: data.answer || "",
        audioUrl: data.audio_url ? `${API_BASE}${data.audio_url}` : "",
        timestamp: new Date().toLocaleTimeString(),
      };
      chatHistory.unshift(entry);
      renderHistory();
      persistHistory();
    }
    pendingQuestion = "";
  }
  if (data.audio_url) {
    const audioUrl = `${API_BASE}${data.audio_url}`;
    audioPlayer.src = audioUrl;
    faceAudioPlayer.src = audioUrl;
    if (activePlaybackTarget === "face") {
      faceAudioPlayer.play().catch(() => {
        setRecordingState("Audio ready (click play)." );
      });
      setSpeaking(true);
    } else {
      audioPlayer.play().catch(() => {
        setRecordingState("Audio ready (click play)." );
      });
      setSpeaking(true);
    }
  }
}

function renderHistory() {
  const items = chatHistory.map((entry, index) => {
    return `
      <button class="history-item" data-history-index="${index}">
        <div class="label">${entry.timestamp}</div>
        <p>${entry.question}</p>
      </button>
    `;
  });
  const html = items.join("");
  historyList.innerHTML = html;
  faceHistoryList.innerHTML = html;
  bindHistoryClicks();
}

function bindHistoryClicks() {
  for (const item of document.querySelectorAll(".history-item")) {
    item.addEventListener("click", () => {
      const index = Number(item.dataset.historyIndex);
      const entry = chatHistory[index];
      if (!entry) {
        return;
      }
      transcriptOutput.textContent = entry.question || "(empty transcript)";
      faceTranscriptOutput.textContent = transcriptOutput.textContent;
      answerOutput.textContent = entry.answer || "(empty answer)";
      faceAnswerOutput.textContent = answerOutput.textContent;
      if (entry.audioUrl) {
        audioPlayer.src = entry.audioUrl;
        faceAudioPlayer.src = entry.audioUrl;
      }
    });
  }
}

async function persistHistory() {
  try {
    localStorage.setItem(HISTORY_STORAGE_KEY, JSON.stringify(chatHistory));
  } catch (error) {
    console.warn("Failed to save chat history locally.", error);
  }
  try {
    await fetch(`${API_BASE}/api/history`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ items: chatHistory }),
    });
  } catch (error) {
    console.warn("Failed to save chat history remotely.", error);
  }
}

async function loadHistory() {
  let loaded = false;
  try {
    const response = await fetch(`${API_BASE}/api/history`);
    if (response.ok) {
      const data = await response.json();
      if (Array.isArray(data.items)) {
        chatHistory.splice(0, chatHistory.length, ...data.items);
        loaded = true;
      }
    }
  } catch (error) {
    console.warn("Failed to load chat history remotely.", error);
  }

  if (!loaded) {
    try {
      const raw = localStorage.getItem(HISTORY_STORAGE_KEY);
      if (!raw) {
        return;
      }
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed)) {
        chatHistory.splice(0, chatHistory.length, ...parsed);
      }
    } catch (error) {
      console.warn("Failed to load chat history locally.", error);
    }
  }
}

async function sendAudioBlob(blob, filename) {
  const formData = new FormData();
  formData.append("audio", blob, filename);
  setLoading(true, "Sending audio...");
  showError("");

  try {
    const response = await fetch(`${API_BASE}/api/chat/audio`, {
      method: "POST",
      body: formData,
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Audio request failed");
    }
    updateResult(data);
  } catch (error) {
    showError(error.message || "Audio request failed");
  } finally {
    setLoading(false, "Idle");
  }
}

async function sendTextMessage(message) {
  setLoading(true, "Sending text...");
  showError("");

  try {
    const response = await fetch(`${API_BASE}/api/chat/text`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ message }),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Text request failed");
    }
    updateResult(data);
  } catch (error) {
    showError(error.message || "Text request failed");
  } finally {
    setLoading(false, "Idle");
  }
}

async function refreshStatus() {
  showError("");
  try {
    const response = await fetch(`${API_BASE}/api/status`);
    const data = await response.json();
    statusOutput.textContent = JSON.stringify(data, null, 2);
  } catch (error) {
    showError("Failed to load status");
  }
}

async function startRecording() {
  showError("");
  try {
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    audioChunks = [];
    mediaRecorder = new MediaRecorder(stream);
    mediaRecorder.addEventListener("dataavailable", (event) => {
      if (event.data.size > 0) {
        audioChunks.push(event.data);
      }
    });
    mediaRecorder.addEventListener("stop", () => {
      const blob = new Blob(audioChunks, { type: mediaRecorder.mimeType || "audio/webm" });
      sendAudioBlob(blob, "recording.webm");
      stream.getTracks().forEach((track) => track.stop());
    });
    mediaRecorder.start();
    setRecordingState("Recording...");
    recordBtn.disabled = true;
    faceRecordBtn.disabled = true;
    stopBtn.disabled = false;
    faceStopBtn.disabled = false;
  } catch (error) {
    showError("Microphone permission denied or unavailable.");
  }
}

function stopRecording() {
  if (mediaRecorder && mediaRecorder.state !== "inactive") {
    mediaRecorder.stop();
    stopBtn.disabled = true;
    faceStopBtn.disabled = true;
    recordBtn.disabled = false;
    faceRecordBtn.disabled = false;
    setRecordingState("Processing recording...");
  }
}

uploadSend.addEventListener("click", () => {
  activePlaybackTarget = "main";
  const file = uploadInput.files[0];
  if (!file) {
    showError("Choose an audio file first.");
    return;
  }
  pendingQuestion = "";
  sendAudioBlob(file, file.name || "upload.wav");
});

faceUploadSend.addEventListener("click", () => {
  activePlaybackTarget = "face";
  const file = faceUploadInput.files[0];
  if (!file) {
    showError("Choose an audio file first.");
    return;
  }
  pendingQuestion = "";
  sendAudioBlob(file, file.name || "upload.wav");
});

recordBtn.addEventListener("click", () => {
  activePlaybackTarget = "main";
  startRecording();
});

faceRecordBtn.addEventListener("click", () => {
  activePlaybackTarget = "face";
  startRecording();
});

stopBtn.addEventListener("click", stopRecording);
faceStopBtn.addEventListener("click", stopRecording);

textSend.addEventListener("click", () => {
  activePlaybackTarget = "main";
  const message = textInput.value.trim();
  if (!message) {
    showError("Type a message first.");
    return;
  }
  pendingQuestion = message;
  sendTextMessage(message);
});

faceTextSend.addEventListener("click", () => {
  activePlaybackTarget = "face";
  const message = faceTextInput.value.trim();
  if (!message) {
    showError("Type a message first.");
    return;
  }
  pendingQuestion = message;
  sendTextMessage(message);
});

statusBtn.addEventListener("click", refreshStatus);

clearHistory.addEventListener("click", async () => {
  chatHistory.length = 0;
  renderHistory();
  await persistHistory();
});

clearFaceHistory.addEventListener("click", async () => {
  chatHistory.length = 0;
  renderHistory();
  await persistHistory();
});

openFaceView.addEventListener("click", () => {
  mainView.classList.add("hidden");
  faceView.classList.remove("hidden");
  faceView.setAttribute("aria-hidden", "false");
  setLookDirection("look-center");
  startBlinkLoop();
});

closeFaceView.addEventListener("click", closeFacePanel);

faceView.addEventListener("click", (event) => {
  if (event.target === faceView) {
    closeFacePanel();
  }
});

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !faceView.classList.contains("hidden")) {
    closeFacePanel();
  }
});

for (const player of [audioPlayer, faceAudioPlayer]) {
  player.addEventListener("play", () => setSpeaking(true));
  player.addEventListener("pause", () => setSpeaking(false));
  player.addEventListener("ended", () => setSpeaking(false));
}

for (const button of document.querySelectorAll("button[data-command]")) {
  button.addEventListener("click", async () => {
    const command = button.dataset.command;
    showError("");
    try {
      const response = await fetch(`${API_BASE}/api/command`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ command }),
      });
      const data = await response.json();
      if (!response.ok) {
        throw new Error(data.error || "Command failed");
      }
      answerOutput.textContent = JSON.stringify(data, null, 2);
      if (command.startsWith("eye.look.")) {
        const direction = command.split(".").pop();
        const map = {
          left: "look-left",
          right: "look-right",
          up: "look-up",
          down: "look-down",
          center: "look-center",
        };
        setLookDirection(map[direction] || "look-center");
      }
    } catch (error) {
      showError(error.message || "Command failed");
    }
  });
}

function closeFacePanel() {
  stopBlinkLoop();
  faceView.classList.remove("speaking");
  faceView.classList.remove("blinking");
  setLookDirection("look-center");
  stopTalkLoop();
  faceView.classList.add("hidden");
  faceView.setAttribute("aria-hidden", "true");
  mainView.classList.remove("hidden");
}

document.addEventListener("DOMContentLoaded", () => {
  initSvgRefs();
  setLookDirection("look-center");
  loadHistory().then(() => renderHistory());
});
