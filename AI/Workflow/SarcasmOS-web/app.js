let API_BASE = "http://localhost:8000";
let GOOGLE_CLIENT_ID = "";
const API_BASE_CANDIDATES = ["http://localhost:8000", "http://localhost:8001"];

const loginView = document.getElementById("loginView");
const loginBot = document.getElementById("loginBot");
const googleLoginButton = document.getElementById("googleLoginButton");
const loginError = document.getElementById("loginError");
const loginSignOutBtn = document.getElementById("loginSignOutBtn");
const userBadge = document.getElementById("userBadge");
const userAvatar = document.getElementById("userAvatar");
const userName = document.getElementById("userName");
const userEmail = document.getElementById("userEmail");
const signOutBtn = document.getElementById("signOutBtn");
const adminPanel = document.getElementById("adminPanel");
const adminRefresh = document.getElementById("adminRefresh");
const adminUsersList = document.getElementById("adminUsersList");
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
const apiHealthRefresh = document.getElementById("apiHealthRefresh");
const apiHealthList = document.getElementById("apiHealthList");
const mainView = document.getElementById("mainView");
const faceView = document.getElementById("faceView");
const voiceChatView = document.getElementById("voiceChatView");
const openFaceView = document.getElementById("openFaceView");
const openVoiceChatView = document.getElementById("openVoiceChatView");
const closeFaceView = document.getElementById("closeFaceView");
const closeVoiceChatView = document.getElementById("closeVoiceChatView");
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
const voiceChatList = document.getElementById("voiceChatList");
const voiceChatRecordBtn = document.getElementById("voiceChatRecordBtn");
const voiceChatStopBtn = document.getElementById("voiceChatStopBtn");
const voiceChatTextInput = document.getElementById("voiceChatTextInput");
const voiceChatTextSend = document.getElementById("voiceChatTextSend");
const voiceChatRecordState = document.getElementById("voiceChatRecordState");
const voiceChatNewChat = document.getElementById("voiceChatNewChat");
const voiceChatSessions = document.getElementById("voiceChatSessions");
const voiceChatFontSmall = document.getElementById("voiceChatFontSmall");
const voiceChatFontLarge = document.getElementById("voiceChatFontLarge");
const voiceChatFontSize = document.getElementById("voiceChatFontSize");
const historyList = document.getElementById("historyList");
const faceHistoryList = document.getElementById("faceHistoryList");
const clearHistory = document.getElementById("clearHistory");
const clearFaceHistory = document.getElementById("clearFaceHistory");
let svgEyes = [];
let svgPupils = [];
let svgMouthGroup = null;

const HISTORY_STORAGE_KEY = "sarcasmos.chatHistory";
const CHAT_FONT_STORAGE_KEY = "sarcasmos.voiceChatFontScale";
const AUTH_STORAGE_KEY = "sarcasmos.googleUser";
const DEFAULT_CHAT_ID = "default";
let mediaRecorder = null;
let audioChunks = [];
let activePlaybackTarget = "main";
const chatSessions = [];
let activeChatId = DEFAULT_CHAT_ID;
let chatHistory = [];
let pendingQuestion = "";
let blinkTimer = null;
let talkTimer = null;
let talkRaf = null;
let thinkTimer = null;
let thinkLongTimer = null;
let isBusy = false;
let voiceChatFontScale = 1;
const audioSyncMap = new Map();
let audioSyncEnabled = false;
let currentUser = null;

audioPlayer.crossOrigin = "anonymous";
faceAudioPlayer.crossOrigin = "anonymous";

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
  if (!svgMouthGroup) {
    return;
  }
  svgMouthGroup.style.transform = "scaleY(1)";
}

function stopTalkLoop() {
  if (svgMouthGroup) {
    svgMouthGroup.style.transform = "scaleY(1)";
  }
}

function getAudioSyncState(audioEl) {
  if (!audioSyncEnabled) {
    return null;
  }
  if (audioSyncMap.has(audioEl)) {
    return audioSyncMap.get(audioEl);
  }
  const AudioContext = window.AudioContext || window.webkitAudioContext;
  if (!AudioContext) {
    return null;
  }
  const context = new AudioContext();
  const analyser = context.createAnalyser();
  analyser.fftSize = 1024;
  analyser.smoothingTimeConstant = 0.7;
  const source = context.createMediaElementSource(audioEl);
  source.connect(analyser);
  analyser.connect(context.destination);

  const state = {
    context,
    analyser,
    data: new Uint8Array(analyser.fftSize),
    rafId: null,
    smoothed: 0,
    isAudible: false,
    lastVoiceAt: 0,
  };
  audioSyncMap.set(audioEl, state);
  return state;
}

function enableAudioSync() {
  if (audioSyncEnabled) {
    return;
  }
  audioSyncEnabled = true;
  getAudioSyncState(audioPlayer);
  getAudioSyncState(faceAudioPlayer);
  for (const state of audioSyncMap.values()) {
    if (state.context.state === "suspended") {
      state.context.resume().catch(() => {});
    }
  }
  if (!audioPlayer.paused) {
    startMouthSync(audioPlayer);
  }
  if (!faceAudioPlayer.paused) {
    startMouthSync(faceAudioPlayer);
  }
}

function startMouthSync(audioEl) {
  if (!svgMouthGroup) {
    return false;
  }
  const state = getAudioSyncState(audioEl);
  if (!state || state.rafId) {
    return Boolean(state);
  }
  if (state.context.state === "suspended") {
    state.context.resume().catch(() => {});
  }
  setSpeaking(false);
  const voiceStartThreshold = 0.028;
  const voiceStopThreshold = 0.014;
  const pauseHoldMs = 180;
  const tick = () => {
    if (audioEl.paused || audioEl.ended) {
      stopMouthSync(audioEl);
      setSpeaking(false);
      return;
    }
    state.analyser.getByteTimeDomainData(state.data);
    let sum = 0;
    for (let i = 0; i < state.data.length; i += 1) {
      const v = (state.data[i] - 128) / 128;
      sum += v * v;
    }
    const rms = Math.sqrt(sum / state.data.length);
    const smoothing = rms > state.smoothed ? 0.35 : 0.82;
    state.smoothed = state.smoothed * smoothing + rms * (1 - smoothing);

    const now = performance.now();
    const hasVoice = rms >= voiceStartThreshold || state.smoothed >= voiceStartThreshold;
    const isSilence = rms <= voiceStopThreshold && state.smoothed <= voiceStopThreshold;
    if (hasVoice) {
      state.lastVoiceAt = now;
    }
    const isAudible = hasVoice || (!isSilence && state.isAudible) || now - state.lastVoiceAt < pauseHoldMs;
    if (state.isAudible !== isAudible) {
      state.isAudible = isAudible;
      setSpeaking(isAudible);
    }
    if (!isAudible) {
      svgMouthGroup.style.transform = "scaleY(1)";
    }
    state.rafId = requestAnimationFrame(tick);
  };
  state.rafId = requestAnimationFrame(tick);
  return true;
}

function stopMouthSync(audioEl) {
  const state = audioSyncMap.get(audioEl);
  if (state && state.rafId) {
    cancelAnimationFrame(state.rafId);
    state.rafId = null;
    state.smoothed = 0;
    state.isAudible = false;
    state.lastVoiceAt = 0;
  }
  if (svgMouthGroup) {
    svgMouthGroup.style.transform = "scaleY(1)";
  }
}

function setSpeaking(isSpeaking) {
  if (isSpeaking) {
    faceView.classList.add("speaking");
    voiceChatView.classList.add("speaking");
    startTalkLoop();
  } else {
    faceView.classList.remove("speaking");
    voiceChatView.classList.remove("speaking");
    stopTalkLoop();
  }
}

function getAllAudioElements() {
  return [
    audioPlayer,
    faceAudioPlayer,
    ...document.querySelectorAll(".voice-chat-audio"),
  ];
}

function pauseAllAudio(except) {
  for (const player of getAllAudioElements()) {
    if (player && player !== except && !player.paused) {
      player.pause();
    }
  }
}

function setThinking(isThinking, mode = "") {
  document.body.classList.toggle("thinking", isThinking);
  document.body.classList.toggle("thinking-audio", isThinking && mode === "audio");
  document.body.classList.toggle("thinking-long", false);
  faceView.classList.toggle("thinking", isThinking);
  faceView.classList.toggle("thinking-audio", isThinking && mode === "audio");
  faceView.classList.toggle("thinking-long", false);
  voiceChatView.classList.toggle("thinking", isThinking);
  voiceChatView.classList.toggle("thinking-audio", isThinking && mode === "audio");
  voiceChatView.classList.toggle("thinking-long", false);
  if (isThinking) {
    startThinkingLoop();
    startThinkingLongTimer();
  } else {
    stopThinkingLoop();
    stopThinkingLongTimer();
  }
}

function startThinkingLongTimer() {
  if (thinkLongTimer) {
    return;
  }
  thinkLongTimer = setTimeout(() => {
    document.body.classList.add("thinking-long");
    faceView.classList.add("thinking-long");
    voiceChatView.classList.add("thinking-long");
  }, 3800);
}

function stopThinkingLongTimer() {
  if (thinkLongTimer) {
    clearTimeout(thinkLongTimer);
    thinkLongTimer = null;
  }
  document.body.classList.remove("thinking-long");
  faceView.classList.remove("thinking-long");
  voiceChatView.classList.remove("thinking-long");
}

function startThinkingLoop() {
  if (thinkTimer || svgPupils.length === 0) {
    return;
  }
  const sequence = [
    { x: -6, y: -2, r: 6 },
    { x: 4, y: -5, r: -4 },
    { x: 8, y: 1, r: -6 },
    { x: -4, y: 6, r: 4 },
    { x: 0, y: 0, r: 0 },
  ];
  let step = 0;
  thinkTimer = setInterval(() => {
    const target = sequence[step % sequence.length];
    for (const pupil of svgPupils) {
      pupil.setAttribute(
        "transform",
        `translate(${target.x} ${target.y}) rotate(${target.r})`
      );
    }
    step += 1;
  }, 520);
}

function stopThinkingLoop() {
  if (thinkTimer) {
    clearInterval(thinkTimer);
    thinkTimer = null;
  }
  setLookDirection("look-center");
}

function setRecordingState(label) {
  recordState.textContent = label;
  faceRecordState.textContent = label;
  voiceChatRecordState.textContent = label;
}

function setLoading(isLoading, label, mode = "") {
  isBusy = isLoading;
  recordBtn.disabled = isLoading;
  stopBtn.disabled = !mediaRecorder || !mediaRecorder.state || mediaRecorder.state === "inactive";
  uploadSend.disabled = isLoading;
  textSend.disabled = isLoading;
  faceRecordBtn.disabled = isLoading;
  faceStopBtn.disabled = !mediaRecorder || !mediaRecorder.state || mediaRecorder.state === "inactive";
  faceUploadSend.disabled = isLoading;
  faceTextSend.disabled = isLoading;
  voiceChatRecordBtn.disabled = isLoading;
  voiceChatStopBtn.disabled = !mediaRecorder || !mediaRecorder.state || mediaRecorder.state === "inactive";
  voiceChatTextSend.disabled = isLoading;
  setRecordingState(label || (isLoading ? "Thinking..." : "Idle"));
  setThinking(isLoading, mode);
}

function showError(message) {
  errorOutput.textContent = message || "";
}

function showLoginError(message) {
  if (loginError) {
    loginError.textContent = message || "";
  }
}

function authHeaders() {
  return currentUser?.token ? { Authorization: `Bearer ${currentUser.token}` } : {};
}

async function loadPublicConfig() {
  const errors = [];
  for (const baseUrl of API_BASE_CANDIDATES) {
    try {
      const response = await fetch(`${baseUrl}/api/config`, { cache: "no-store" });
      if (!response.ok) {
        throw new Error(`${response.status} ${response.statusText}`);
      }
      API_BASE = baseUrl;
      const config = await response.json();
      GOOGLE_CLIENT_ID = String(config.googleClientId || "").trim();
      return config;
    } catch (error) {
      errors.push(`${baseUrl}: ${error.message || "unreachable"}`);
    }
  }
  throw new Error(`Backend config unavailable. ${errors.join(" | ")}`);
}

function saveUserSession(user) {
  currentUser = user;
  try {
    localStorage.setItem(AUTH_STORAGE_KEY, JSON.stringify(user));
  } catch (error) {
    console.warn("Failed to save auth session.", error);
  }
  renderAuthState();
}

function loadUserSession() {
  try {
    const raw = localStorage.getItem(AUTH_STORAGE_KEY);
    if (!raw) {
      return null;
    }
    const user = JSON.parse(raw);
    if (!user || !user.email || !user.token) {
      return null;
    }
    return user;
  } catch (error) {
    console.warn("Failed to load auth session.", error);
    return null;
  }
}

function clearUserSession() {
  const token = currentUser?.token;
  currentUser = null;
  try {
    localStorage.removeItem(AUTH_STORAGE_KEY);
  } catch (error) {
    console.warn("Failed to clear auth session.", error);
  }
  renderAuthState();
  if (token) {
    fetch(`${API_BASE}/api/auth/logout`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
    }).catch(() => {});
  }
}

function renderAuthState() {
  const isSignedIn = Boolean(currentUser?.email && currentUser?.token);
  const isAuthorized = Boolean(currentUser?.authorized);
  const canUseApp = isSignedIn && isAuthorized;
  if (!canUseApp) {
    closeFacePanel();
    closeVoiceChatPanel();
  }
  loginView?.classList.toggle("hidden", canUseApp);
  mainView?.classList.toggle("hidden", !canUseApp);
  mainView?.setAttribute("aria-hidden", canUseApp ? "false" : "true");
  adminPanel?.classList.toggle("hidden", !currentUser?.isAdmin);
  loginSignOutBtn?.classList.toggle("hidden", !isSignedIn);
  googleLoginButton?.classList.toggle("hidden", isSignedIn);

  if (userBadge) {
    userBadge.classList.toggle("hidden", !canUseApp);
  }
  if (userName) {
    userName.textContent = currentUser?.name || "Signed in";
  }
  if (userEmail) {
    const role = currentUser?.isAdmin ? "admin" : currentUser?.authorized ? "authorized" : "pending";
    userEmail.textContent = currentUser?.email ? `${currentUser.email} - ${role}` : "";
  }
  if (userAvatar) {
    userAvatar.src = currentUser?.picture || "";
    userAvatar.classList.toggle("hidden", !currentUser?.picture);
  }
}

function handleGoogleCredential(response) {
  loginWithGoogle(response.credential || "");
}

async function loginWithGoogle(credential) {
  showLoginError("");
  try {
    const response = await fetch(`${API_BASE}/api/auth/google`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ credential }),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Google sign-in failed.");
    }
    saveUserSession({ ...data.user, token: data.token });
    if (!data.user.authorized) {
      showLoginError("Your Google account is signed in, but an admin must authorize access.");
    }
    if (data.user.isAdmin) {
      loadAdminUsers();
    }
  } catch (error) {
    showLoginError(error.message || "Google sign-in failed.");
  }
}

function renderGoogleButton() {
  if (!googleLoginButton) {
    return;
  }
  googleLoginButton.innerHTML = "";
  if (!GOOGLE_CLIENT_ID) {
    showLoginError("Set GOOGLE_CLIENT_ID in backend/.env to enable Google sign-in.");
    return;
  }
  if (!window.google?.accounts?.id) {
    setTimeout(renderGoogleButton, 250);
    return;
  }
  window.google.accounts.id.initialize({
    client_id: GOOGLE_CLIENT_ID,
    callback: handleGoogleCredential,
  });
  window.google.accounts.id.renderButton(googleLoginButton, {
    theme: "filled_black",
    size: "large",
    type: "standard",
    shape: "pill",
    text: "signin_with",
    logo_alignment: "left",
    width: 280,
  });
}

function initLoginBotLook() {
  if (!loginBot) {
    return;
  }
  let eyes = Array.from(loginBot.querySelectorAll(".login-bot-eye-group"));
  if (!eyes.length) {
    eyes = Array.from(loginBot.querySelectorAll(".login-bot-eye"));
  }
  if (!eyes.length) {
    return;
  }
  const setEyes = (x, y) => {
    for (const eye of eyes) {
      eye.style.transform = `translate(${x}px, ${y}px)`;
    }
  };
  document.addEventListener("pointermove", (event) => {
    if (loginView?.classList.contains("hidden")) {
      return;
    }
    const rect = loginBot.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const centerY = rect.top + rect.height / 2;
    const dx = Math.max(-1, Math.min(1, (event.clientX - centerX) / (window.innerWidth * 0.32)));
    const dy = Math.max(-1, Math.min(1, (event.clientY - centerY) / (window.innerHeight * 0.28)));
    setEyes(Math.round(dx * 16), Math.round(dy * 12));
  });
  document.addEventListener("pointerleave", () => setEyes(0, 0));
}

async function initAuth() {
  currentUser = loadUserSession();
  try {
    await loadPublicConfig();
  } catch (error) {
    showLoginError(error.message || "Could not load login configuration.");
    renderAuthState();
    return;
  }
  if (currentUser?.token) {
    try {
      const response = await fetch(`${API_BASE}/api/auth/me`, { headers: authHeaders() });
      const data = await response.json();
      if (!response.ok) {
        throw new Error(data.error || "Session expired.");
      }
      saveUserSession({ ...data.user, token: currentUser.token });
    } catch (error) {
      currentUser = null;
      localStorage.removeItem(AUTH_STORAGE_KEY);
      showLoginError("Session expired. Sign in again.");
    }
  }
  renderAuthState();
  renderGoogleButton();
  if (currentUser?.isAdmin) {
    loadAdminUsers();
  }
}

const API_HEALTH_CHECKS = [
  {
    name: "Backend",
    path: "/api/status",
    method: "GET",
    description: "Main backend reachability",
  },
  {
    name: "Robot status",
    path: "/api/status",
    method: "GET",
    description: "Bender runtime status",
  },
  {
    name: "Chat history",
    path: "/api/history",
    method: "GET",
    description: "Saved conversations",
  },
  {
    name: "API schema",
    path: "/openapi.json",
    method: "GET",
    description: "Backend route registry",
  },
  {
    name: "Public config",
    path: "/api/config",
    method: "GET",
    description: "Google Sign-In configuration",
  },
  {
    name: "AI services",
    path: "/api/services/status",
    method: "GET",
    description: "STT, LLM, and TTS configuration",
    expandServices: true,
  },
];

function renderApiHealth(items, isChecking = false) {
  if (!apiHealthList) {
    return;
  }
  apiHealthList.innerHTML = "";
  const checks = items.length ? items : API_HEALTH_CHECKS.map((item) => ({
    ...item,
    status: isChecking ? "checking" : "unknown",
    detail: isChecking ? "Checking..." : "Not checked yet",
  }));

  for (const item of checks) {
    const row = document.createElement("div");
    row.className = `api-health-item ${item.status}`;

    const text = document.createElement("div");
    const title = document.createElement("p");
    title.className = "api-health-title";
    title.textContent = item.name;
    const meta = document.createElement("p");
    meta.className = "api-health-meta";
    meta.textContent = `${item.method} ${item.path} - ${item.description}`;
    const detail = document.createElement("p");
    detail.className = "api-health-detail";
    detail.textContent = item.detail;
    text.append(title, meta, detail);

    const badge = document.createElement("span");
    badge.className = "api-health-badge";
    badge.textContent = item.status === "ok" ? "OK" : item.status === "checking" ? "..." : "FAIL";

    row.append(text, badge);
    apiHealthList.append(row);
  }
}

async function checkApiHealth() {
  if (!apiHealthRefresh) {
    return;
  }
  apiHealthRefresh.disabled = true;
  renderApiHealth([], true);
  const results = [];
  for (const check of API_HEALTH_CHECKS) {
    const started = performance.now();
    try {
      const response = await fetch(`${API_BASE}${check.path}`, {
        method: check.method,
        cache: "no-store",
      });
      const elapsed = Math.round(performance.now() - started);
      if (!response.ok) {
        const body = await response.text().catch(() => "");
        throw new Error(`${response.status} ${response.statusText}${body ? ` - ${body.slice(0, 120)}` : ""}`);
      }
      const data = await response.json().catch(() => null);
      if (check.expandServices && data && data.services) {
        for (const key of ["stt", "llm", "tts"]) {
          const service = data.services[key] || {};
          results.push({
            name: service.name || key.toUpperCase(),
            path: check.path,
            method: check.method,
            description: service.model ? `Model: ${service.model}` : `${key.toUpperCase()} service`,
            status: service.ok ? "ok" : "fail",
            detail: service.ok
              ? `${service.detail || "Configured"} - ${service.base_url || "No base URL shown"}`
              : service.detail || data.error || "Service is not configured correctly",
          });
        }
        continue;
      }
      results.push({
        ...check,
        status: "ok",
        detail: `Responding in ${elapsed} ms`,
      });
    } catch (error) {
      results.push({
        ...check,
        status: "fail",
        detail: error.message || "Failed to fetch",
      });
    }
  }
  renderApiHealth(results);
  apiHealthRefresh.disabled = false;
}

async function loadAdminUsers() {
  if (!adminUsersList || !currentUser?.isAdmin) {
    return;
  }
  adminUsersList.innerHTML = `<p class="helper-text">Loading users...</p>`;
  try {
    const response = await fetch(`${API_BASE}/api/admin/users`, { headers: authHeaders() });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to load users.");
    }
    renderAdminUsers(data.users || []);
  } catch (error) {
    adminUsersList.innerHTML = `<p class="error">${escapeHtml(error.message || "Failed to load users.")}</p>`;
  }
}

function renderAdminUsers(users) {
  if (!adminUsersList) {
    return;
  }
  if (!users.length) {
    adminUsersList.innerHTML = `<p class="helper-text">No users have signed in yet.</p>`;
    return;
  }
  adminUsersList.innerHTML = users
    .sort((a, b) => String(a.email).localeCompare(String(b.email)))
    .map((user) => `
      <div class="admin-user" data-email="${escapeHtml(user.email)}">
        <img src="${escapeHtml(user.picture || "")}" alt="" class="${user.picture ? "" : "hidden"}" />
        <div>
          <p>${escapeHtml(user.name || user.email)}</p>
          <span>${escapeHtml(user.email)}</span>
        </div>
        <label>
          <input class="admin-user-authorized" type="checkbox" ${user.authorized ? "checked" : ""} />
          Authorized
        </label>
        <label>
          <input class="admin-user-admin" type="checkbox" ${user.isAdmin ? "checked" : ""} />
          Admin
        </label>
      </div>
    `)
    .join("");
  bindAdminUserControls();
}

function bindAdminUserControls() {
  for (const row of adminUsersList.querySelectorAll(".admin-user")) {
    const email = row.dataset.email;
    const authorized = row.querySelector(".admin-user-authorized");
    const isAdmin = row.querySelector(".admin-user-admin");
    authorized?.addEventListener("change", () => updateAdminUser(email, { authorized: authorized.checked }));
    isAdmin?.addEventListener("change", () => updateAdminUser(email, { isAdmin: isAdmin.checked }));
  }
}

async function updateAdminUser(email, patch) {
  try {
    const response = await fetch(`${API_BASE}/api/admin/users/${encodeURIComponent(email)}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json", ...authHeaders() },
      body: JSON.stringify(patch),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to update user.");
    }
    if (data.user?.email === currentUser?.email) {
      saveUserSession({ ...data.user, token: currentUser.token });
    }
    await loadAdminUsers();
  } catch (error) {
    showError(error.message || "Failed to update user.");
    await loadAdminUsers();
  }
}

function applyVoiceChatFontScale() {
  const scale = Math.min(Math.max(voiceChatFontScale, 0.9), 1.45);
  voiceChatFontScale = scale;
  const size = 1.08 * scale;
  voiceChatList.style.setProperty("--voice-chat-font-size", `${size.toFixed(2)}rem`);
  if (voiceChatFontSize) {
    voiceChatFontSize.textContent = `${Math.round(scale * 100)}%`;
  }
  try {
    localStorage.setItem(CHAT_FONT_STORAGE_KEY, String(scale));
  } catch (error) {
    console.warn("Failed to save chat font size.", error);
  }
}

function loadVoiceChatFontScale() {
  try {
    const raw = localStorage.getItem(CHAT_FONT_STORAGE_KEY);
    if (raw) {
      const parsed = Number(raw);
      if (!Number.isNaN(parsed)) {
        voiceChatFontScale = parsed;
      }
    }
  } catch (error) {
    console.warn("Failed to load chat font size.", error);
  }
  applyVoiceChatFontScale();
}

function changeVoiceChatFontScale(delta) {
  voiceChatFontScale += delta;
  applyVoiceChatFontScale();
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll("\"", "&quot;")
    .replaceAll("'", "&#039;");
}

function createChat(title = "New chat", items = []) {
  const now = new Date().toISOString();
  return {
    id: `chat-${Date.now()}-${Math.random().toString(16).slice(2)}`,
    title,
    createdAt: now,
    updatedAt: now,
    items,
  };
}

function getActiveChat() {
  let chat = chatSessions.find((session) => session.id === activeChatId);
  if (!chat) {
    chat = chatSessions[0];
  }
  if (!chat) {
    chat = createChat("Chat principal");
    chat.id = DEFAULT_CHAT_ID;
    chatSessions.push(chat);
  }
  activeChatId = chat.id;
  chatHistory = chat.items;
  return chat;
}

function syncActiveChat() {
  const chat = getActiveChat();
  chat.items = chatHistory;
  chat.updatedAt = new Date().toISOString();
  if (!chat.title || chat.title === "New chat") {
    const firstQuestion = chat.items[0]?.question || chat.items[chat.items.length - 1]?.question || "";
    chat.title = firstQuestion ? firstQuestion.slice(0, 42) : "New chat";
  }
}

function getAllHistoryItems() {
  return chatSessions.flatMap((chat) => Array.isArray(chat.items) ? chat.items : []);
}

function getActiveChatContext() {
  return [...chatHistory].reverse().map((entry) => ({
    question: entry.question || "",
    answer: entry.answer || "",
    timestamp: entry.timestamp || "",
  }));
}

function renderAllHistoryViews() {
  getActiveChat();
  renderHistory();
  renderChatSessions();
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
      syncActiveChat();
      renderAllHistoryViews();
      persistHistory();
    }
    pendingQuestion = "";
  }
  if (data.audio_url) {
    const audioUrl = `${API_BASE}${data.audio_url}`;
    audioPlayer.src = audioUrl;
    faceAudioPlayer.src = audioUrl;
    if (activePlaybackTarget === "voice") {
      const voiceAudios = voiceChatList.querySelectorAll(".voice-chat-audio");
      const voiceAudio = voiceAudios[voiceAudios.length - 1];
      if (voiceAudio) {
        pauseAllAudio(voiceAudio);
        voiceAudio.play().catch(() => {
          setRecordingState("Audio ready (click play)." );
        });
        if (!startMouthSync(voiceAudio)) {
          setSpeaking(true);
        }
      }
    } else if (activePlaybackTarget === "face") {
      pauseAllAudio(faceAudioPlayer);
      faceAudioPlayer.play().catch(() => {
        setRecordingState("Audio ready (click play)." );
      });
      if (!startMouthSync(faceAudioPlayer)) {
        setSpeaking(true);
      }
    } else {
      pauseAllAudio(audioPlayer);
      audioPlayer.play().catch(() => {
        setRecordingState("Audio ready (click play)." );
      });
      if (!startMouthSync(audioPlayer)) {
        setSpeaking(true);
      }
    }
  }
}

function renderHistory() {
  const items = chatHistory.map((entry, index) => {
    return `
      <div class="history-item" data-history-index="${index}">
        <div class="history-meta">
          <div class="label">${escapeHtml(entry.timestamp)}</div>
          <button class="history-delete" type="button" aria-label="Delete this chat">Delete</button>
        </div>
        <button class="history-open" type="button">
          <p>${escapeHtml(entry.question)}</p>
        </button>
      </div>
    `;
  });
  const html = items.join("");
  historyList.innerHTML = html;
  faceHistoryList.innerHTML = html;
  renderVoiceChat();
  bindHistoryClicks();
}

function renderChatSessions() {
  if (!voiceChatSessions) {
    return;
  }
  const html = chatSessions
    .map((chat) => {
      const count = Array.isArray(chat.items) ? chat.items.length : 0;
      const activeClass = chat.id === activeChatId ? " active" : "";
      return `
        <div class="voice-chat-session" data-chat-id="${escapeHtml(chat.id)}">
          <button class="voice-chat-session-open${activeClass}" type="button">
            <span class="voice-chat-session-title">${escapeHtml(chat.title || "New chat")}</span>
            <span class="voice-chat-session-meta">${count} messages</span>
          </button>
          <button class="voice-chat-session-delete" type="button" aria-label="Delete chat">Delete</button>
        </div>
      `;
    })
    .join("");
  voiceChatSessions.innerHTML = html;
  bindChatSessionClicks();
}

function renderVoiceChat() {
  const items = [...chatHistory].reverse();
  const html = items
    .map((entry, reverseIndex) => {
      const index = chatHistory.length - 1 - reverseIndex;
      const question = entry.question || "";
      const answer = entry.answer || "";
      const audio = entry.audioUrl
        ? `<audio class="voice-chat-audio" controls src="${escapeHtml(entry.audioUrl)}"></audio>`
        : "";
      return `
        <div class="voice-chat-message user">
          <div class="label">You</div>
          <p>${escapeHtml(question)}</p>
        </div>
        <div class="voice-chat-message assistant" data-history-index="${index}">
          <div class="voice-chat-message-top">
            <div class="label">Bender</div>
            <button class="voice-chat-message-delete" type="button">Delete</button>
          </div>
          <p>${escapeHtml(answer)}</p>
          ${audio}
        </div>
      `;
    })
    .join("");
  voiceChatList.innerHTML = html;
  bindVoiceChatAudio();
  bindVoiceChatMessageDeletes();
  voiceChatList.scrollTop = voiceChatList.scrollHeight;
}

function bindVoiceChatAudio() {
  for (const player of voiceChatList.querySelectorAll(".voice-chat-audio")) {
    player.crossOrigin = "anonymous";
    player.addEventListener("play", () => {
      pauseAllAudio(player);
      if (!startMouthSync(player)) {
        setSpeaking(true);
      }
    });
    player.addEventListener("pause", () => {
      setSpeaking(false);
      stopMouthSync(player);
    });
    player.addEventListener("ended", () => {
      setSpeaking(false);
      stopMouthSync(player);
    });
  }
}

function bindVoiceChatMessageDeletes() {
  for (const button of voiceChatList.querySelectorAll(".voice-chat-message-delete")) {
    button.addEventListener("click", async () => {
      const item = button.closest(".voice-chat-message");
      const index = Number(item?.dataset.historyIndex);
      if (Number.isNaN(index)) {
        return;
      }
      await deleteHistoryEntry(index);
    });
  }
}

function bindChatSessionClicks() {
  for (const item of voiceChatSessions.querySelectorAll(".voice-chat-session")) {
    const chatId = item.dataset.chatId;
    const openButton = item.querySelector(".voice-chat-session-open");
    const deleteButton = item.querySelector(".voice-chat-session-delete");
    openButton?.addEventListener("click", () => {
      activeChatId = chatId;
      getActiveChat();
      renderAllHistoryViews();
      persistHistory();
    });
    deleteButton?.addEventListener("click", async (event) => {
      event.stopPropagation();
      await deleteChatSession(chatId);
    });
  }
}

function bindHistoryClicks() {
  for (const item of document.querySelectorAll(".history-item")) {
    const openButton = item.querySelector(".history-open");
    const deleteButton = item.querySelector(".history-delete");
    if (openButton) {
      openButton.addEventListener("click", () => {
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
    if (deleteButton) {
      deleteButton.addEventListener("click", async (event) => {
        event.stopPropagation();
        const index = Number(item.dataset.historyIndex);
        if (Number.isNaN(index)) {
          return;
        }
        await deleteHistoryEntry(index);
      });
    }
  }
}

function getAudioFilename(audioUrl) {
  if (!audioUrl) {
    return "";
  }
  try {
    const url = new URL(audioUrl, API_BASE);
    const filename = url.pathname.split("/").filter(Boolean).pop() || "";
    return decodeURIComponent(filename);
  } catch (error) {
    return "";
  }
}

function isAudioStillReferenced(filename) {
  return getAllHistoryItems().some((entry) => getAudioFilename(entry.audioUrl) === filename);
}

async function deleteBackendAudio(filename) {
  if (!filename) {
    return;
  }
  try {
    const response = await fetch(`${API_BASE}/api/audio/${encodeURIComponent(filename)}`, {
      method: "DELETE",
    });
    if (!response.ok && response.status !== 404) {
      const data = await response.json().catch(() => ({}));
      throw new Error(data.error || "Audio delete failed");
    }
  } catch (error) {
    console.warn("Failed to delete backend audio.", error);
  }
}

async function deleteHistoryEntry(index) {
  const entry = chatHistory[index];
  if (!entry) {
    return;
  }
  const filename = getAudioFilename(entry.audioUrl);
  chatHistory.splice(index, 1);
  syncActiveChat();
  renderAllHistoryViews();

  if (filename && !isAudioStillReferenced(filename)) {
    await deleteBackendAudio(filename);
  }
  await persistHistory();
}

async function deleteChatSession(chatId) {
  const index = chatSessions.findIndex((chat) => chat.id === chatId);
  if (index === -1) {
    return;
  }
  const [removed] = chatSessions.splice(index, 1);
  const filenames = Array.from(
    new Set((removed.items || []).map((entry) => getAudioFilename(entry.audioUrl)).filter(Boolean))
  );

  if (chatSessions.length === 0) {
    const chat = createChat("New chat");
    activeChatId = chat.id;
    chatSessions.push(chat);
  } else if (activeChatId === chatId) {
    activeChatId = chatSessions[Math.max(0, index - 1)]?.id || chatSessions[0].id;
  }

  getActiveChat();
  renderAllHistoryViews();
  for (const filename of filenames) {
    if (!isAudioStillReferenced(filename)) {
      await deleteBackendAudio(filename);
    }
  }
  await persistHistory();
}

async function createNewVoiceChat() {
  const chat = createChat("New chat");
  chatSessions.unshift(chat);
  activeChatId = chat.id;
  getActiveChat();
  renderAllHistoryViews();
  await persistHistory();
}

async function persistHistory() {
  syncActiveChat();
  const payload = {
    activeChatId,
    chats: chatSessions,
    items: getAllHistoryItems(),
  };
  try {
    localStorage.setItem(HISTORY_STORAGE_KEY, JSON.stringify(payload));
  } catch (error) {
    console.warn("Failed to save chat history locally.", error);
  }
  try {
    await fetch(`${API_BASE}/api/history`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
  } catch (error) {
    console.warn("Failed to save chat history remotely.", error);
  }
}

async function loadHistory() {
  let loaded = false;
  const applyHistoryData = (data) => {
    chatSessions.length = 0;
    if (data && Array.isArray(data.chats) && data.chats.length > 0) {
      for (const chat of data.chats) {
        chatSessions.push({
          id: chat.id || `chat-${Date.now()}-${Math.random().toString(16).slice(2)}`,
          title: chat.title || "New chat",
          createdAt: chat.createdAt || "",
          updatedAt: chat.updatedAt || "",
          items: Array.isArray(chat.items) ? chat.items : [],
        });
      }
      activeChatId = data.activeChatId || chatSessions[0].id;
      getActiveChat();
      return true;
    }
    if (data && Array.isArray(data.items)) {
      const chat = createChat("Chat principal", data.items);
      chat.id = DEFAULT_CHAT_ID;
      chatSessions.push(chat);
      activeChatId = chat.id;
      getActiveChat();
      return true;
    }
    return false;
  };

  try {
    const response = await fetch(`${API_BASE}/api/history`);
    if (response.ok) {
      const data = await response.json();
      loaded = applyHistoryData(data);
    }
  } catch (error) {
    console.warn("Failed to load chat history remotely.", error);
  }

  if (!loaded) {
    try {
      const raw = localStorage.getItem(HISTORY_STORAGE_KEY);
      if (!raw) {
        throw new Error("No local history.");
      }
      const parsed = JSON.parse(raw);
      loaded = applyHistoryData(Array.isArray(parsed) ? { items: parsed } : parsed);
    } catch (error) {
      console.warn("Failed to load chat history locally.", error);
    }
  }

  if (chatSessions.length === 0) {
    const chat = createChat("New chat");
    activeChatId = chat.id;
    chatSessions.push(chat);
    getActiveChat();
  }
}

async function sendAudioBlob(blob, filename) {
  const formData = new FormData();
  formData.append("audio", blob, filename);
  formData.append("context", JSON.stringify(getActiveChatContext()));
  formData.append("chatId", activeChatId);
  setLoading(true, "Sending audio...", "audio");
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
      body: JSON.stringify({ message, context: getActiveChatContext(), chatId: activeChatId }),
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
    voiceChatRecordBtn.disabled = true;
    stopBtn.disabled = false;
    faceStopBtn.disabled = false;
    voiceChatStopBtn.disabled = false;
  } catch (error) {
    showError("Microphone permission denied or unavailable.");
  }
}

function stopRecording() {
  if (mediaRecorder && mediaRecorder.state !== "inactive") {
    mediaRecorder.stop();
    stopBtn.disabled = true;
    faceStopBtn.disabled = true;
    voiceChatStopBtn.disabled = true;
    recordBtn.disabled = false;
    faceRecordBtn.disabled = false;
    voiceChatRecordBtn.disabled = false;
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

voiceChatRecordBtn.addEventListener("click", () => {
  activePlaybackTarget = "voice";
  startRecording();
});

stopBtn.addEventListener("click", stopRecording);
faceStopBtn.addEventListener("click", stopRecording);
voiceChatStopBtn.addEventListener("click", stopRecording);

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

voiceChatTextSend.addEventListener("click", () => {
  activePlaybackTarget = "voice";
  const message = voiceChatTextInput.value.trim();
  if (!message) {
    showError("Type a message first.");
    return;
  }
  pendingQuestion = message;
  sendTextMessage(message);
});

statusBtn.addEventListener("click", refreshStatus);
apiHealthRefresh.addEventListener("click", checkApiHealth);
signOutBtn.addEventListener("click", clearUserSession);
loginSignOutBtn.addEventListener("click", clearUserSession);
adminRefresh.addEventListener("click", loadAdminUsers);

clearHistory.addEventListener("click", async () => {
  const filenames = Array.from(new Set(chatHistory.map((entry) => getAudioFilename(entry.audioUrl)).filter(Boolean)));
  chatHistory.length = 0;
  syncActiveChat();
  renderAllHistoryViews();
  for (const filename of filenames) {
    if (!isAudioStillReferenced(filename)) {
      await deleteBackendAudio(filename);
    }
  }
  await persistHistory();
});

clearFaceHistory.addEventListener("click", async () => {
  const filenames = Array.from(new Set(chatHistory.map((entry) => getAudioFilename(entry.audioUrl)).filter(Boolean)));
  chatHistory.length = 0;
  syncActiveChat();
  renderAllHistoryViews();
  for (const filename of filenames) {
    if (!isAudioStillReferenced(filename)) {
      await deleteBackendAudio(filename);
    }
  }
  await persistHistory();
});

voiceChatNewChat.addEventListener("click", createNewVoiceChat);
voiceChatFontSmall.addEventListener("click", () => changeVoiceChatFontScale(-0.1));
voiceChatFontLarge.addEventListener("click", () => changeVoiceChatFontScale(0.1));

openFaceView.addEventListener("click", () => {
  mainView.classList.add("hidden");
  faceView.classList.remove("hidden");
  faceView.setAttribute("aria-hidden", "false");
  document.body.classList.add("face-open");
  faceView.scrollTop = 0;
  setLookDirection("look-center");
  startBlinkLoop();
});

openVoiceChatView.addEventListener("click", () => {
  mainView.classList.add("hidden");
  voiceChatView.classList.remove("hidden");
  voiceChatView.setAttribute("aria-hidden", "false");
  document.body.classList.add("face-open");
  setLookDirection("look-center");
  startBlinkLoop();
  renderAllHistoryViews();
});

closeFaceView.addEventListener("click", closeFacePanel);
closeVoiceChatView.addEventListener("click", closeVoiceChatPanel);

faceView.addEventListener("click", (event) => {
  if (event.target === faceView && !isBusy) {
    closeFacePanel();
  }
});

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !faceView.classList.contains("hidden") && !isBusy) {
    closeFacePanel();
  }
  if (event.key === "Escape" && !voiceChatView.classList.contains("hidden") && !isBusy) {
    closeVoiceChatPanel();
  }
});

for (const player of [audioPlayer, faceAudioPlayer]) {
  player.addEventListener("play", () => {
    if (!startMouthSync(player)) {
      setSpeaking(true);
    }
  });
  player.addEventListener("pause", () => {
    setSpeaking(false);
    stopMouthSync(player);
  });
  player.addEventListener("ended", () => {
    setSpeaking(false);
    stopMouthSync(player);
  });
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
  stopMouthSync(audioPlayer);
  stopMouthSync(faceAudioPlayer);
  faceView.classList.add("hidden");
  faceView.setAttribute("aria-hidden", "true");
  document.body.classList.remove("face-open");
  mainView.classList.remove("hidden");
}

function closeVoiceChatPanel() {
  stopBlinkLoop();
  voiceChatView.classList.remove("speaking");
  setLookDirection("look-center");
  stopTalkLoop();
  voiceChatView.classList.add("hidden");
  voiceChatView.setAttribute("aria-hidden", "true");
  document.body.classList.remove("face-open");
  mainView.classList.remove("hidden");
}

document.addEventListener("DOMContentLoaded", async () => {
  const faceSvg = faceView.querySelector(".face-svg");
  const voiceFaceHead = document.getElementById("voiceChatFace");
  if (faceSvg && voiceFaceHead) {
    const cloned = faceSvg.outerHTML
      .replaceAll("id=\"shadow\"", "id=\"shadow-voice\"")
      .replaceAll("url(#shadow)", "url(#shadow-voice)");
    voiceFaceHead.innerHTML = cloned;
  }
  initSvgRefs();
  setLookDirection("look-center");
  initLoginBotLook();
  await initAuth();
  loadVoiceChatFontScale();
  loadHistory().then(() => renderAllHistoryViews());
  checkApiHealth();
});

document.addEventListener("pointerdown", enableAudioSync, { once: true });
document.addEventListener("keydown", enableAudioSync, { once: true });
