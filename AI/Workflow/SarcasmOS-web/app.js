let API_BASE = "";
let GOOGLE_CLIENT_ID = "";

const loginView = document.getElementById("loginView");
const loginBot = document.getElementById("loginBot");
const homeBenderSvg = document.querySelector(".home-bender-svg");
const googleLoginButton = document.getElementById("googleLoginButton");
const loginError = document.getElementById("loginError");
const loginSignOutBtn = document.getElementById("loginSignOutBtn");
const userBadge = document.getElementById("userBadge");
const userAvatar = document.getElementById("userAvatar");
const userName = document.getElementById("userName");
const userEmail = document.getElementById("userEmail");
const signOutBtn = document.getElementById("signOutBtn");
const openAdminConsole = document.getElementById("openAdminConsole");
const adminView = document.getElementById("adminView");
const adminUserAvatar = document.getElementById("adminUserAvatar");
const adminUserName = document.getElementById("adminUserName");
const adminUserEmail = document.getElementById("adminUserEmail");
const adminSignOutBtn = document.getElementById("adminSignOutBtn");
const openSarcasmConsole = document.getElementById("openSarcasmConsole");
const adminPanel = document.getElementById("adminPanel");
const adminRefresh = document.getElementById("adminRefresh");
const adminUsersList = document.getElementById("adminUsersList");
const googleToolsPanel = document.getElementById("googleToolsPanel");
const googleToolsRefresh = document.getElementById("googleToolsRefresh");
const googleCalendarStatus = document.getElementById("googleCalendarStatus");
const googleCalendarHelp = document.getElementById("googleCalendarHelp");
const connectGoogleCalendar = document.getElementById("connectGoogleCalendar");
const disconnectGoogleCalendar = document.getElementById("disconnectGoogleCalendar");
const googleToolsError = document.getElementById("googleToolsError");
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
const audioReplyToggle = document.getElementById("audioReplyToggle");
const mainView = document.getElementById("mainView");
const faceView = document.getElementById("faceView");
const voiceChatView = document.getElementById("voiceChatView");
const openFaceView = document.getElementById("openFaceView");
const openVoiceChatView = document.getElementById("openVoiceChatView");
const heroFaceViewBtn = document.getElementById("heroFaceViewBtn");
const heroVoiceChatBtn = document.getElementById("heroVoiceChatBtn");
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
const voiceChatAudioReplyToggle = document.getElementById("voiceChatAudioReplyToggle");
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
const HISTORY_STORAGE_VERSION = "v2";
const AUDIO_REPLY_STORAGE_KEY = "sarcasmos.audioReplyEnabled";
const GOOGLE_CALENDAR_SCOPE = "https://www.googleapis.com/auth/calendar.readonly";
const GOOGLE_TOOLS_CHECK_INTERVAL_MS = 30000;
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
let audioReplyEnabled = true;
let googleToolsState = null;
let googleToolsMonitor = null;
let currentQuota = null;
let adminConsoleOverride = false;
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

function friendlyRequestError(response, data, fallback) {
  if (response?.status === 429) {
    return "Se te han acabado los mensajes hasta la proxima semana. Pide a un admin que te reactive 5 mensajes desde el panel si necesitas seguir ahora.";
  }
  return data?.error || fallback;
}

function showLoginError(message) {
  if (loginError) {
    loginError.textContent = message || "";
  }
}

function authHeaders() {
  return currentUser?.token ? { Authorization: `Bearer ${currentUser.token}` } : {};
}

function userHistoryStorageKey() {
  const email = String(currentUser?.email || "anonymous").trim().toLowerCase();
  return `${HISTORY_STORAGE_KEY}.${HISTORY_STORAGE_VERSION}.${email}`;
}

async function loadPublicConfig() {
  const errors = [];
  for (const baseUrl of apiBaseCandidates()) {
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

function apiBaseCandidates() {
  const sameOrigin = "";
  const localBackends = ["http://localhost:8000", "http://localhost:8001"];
  const isLocalhost = ["localhost", "127.0.0.1", ""].includes(window.location.hostname);
  if (!isLocalhost || window.location.port === "9000") {
    return [sameOrigin, ...localBackends];
  }
  return [...localBackends, sameOrigin];
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
  currentQuota = null;
  adminConsoleOverride = false;
  stopGoogleToolsMonitor();
  renderGoogleToolsStatus(null);
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
  const isAdmin = Boolean(currentUser?.isAdmin);
  const canUseApp = isSignedIn && isAuthorized && (!isAdmin || adminConsoleOverride);
  const canUseAdmin = isSignedIn && isAuthorized && isAdmin && !adminConsoleOverride;
  if (!canUseApp) {
    closeFacePanel();
    closeVoiceChatPanel();
  }
  loginView?.classList.toggle("hidden", canUseApp || canUseAdmin);
  mainView?.classList.toggle("hidden", !canUseApp);
  mainView?.setAttribute("aria-hidden", canUseApp ? "false" : "true");
  adminView?.classList.toggle("hidden", !canUseAdmin);
  adminView?.setAttribute("aria-hidden", canUseAdmin ? "false" : "true");
  adminPanel?.classList.toggle("hidden", !canUseAdmin);
  googleToolsPanel?.classList.toggle("hidden", !canUseApp);
  loginSignOutBtn?.classList.toggle("hidden", !isSignedIn);
  googleLoginButton?.classList.toggle("hidden", isSignedIn);
  openAdminConsole?.classList.toggle("hidden", !isAdmin);
  if (canUseApp) {
    startGoogleToolsMonitor();
  } else {
    stopGoogleToolsMonitor();
  }

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
  if (adminUserName) {
    adminUserName.textContent = currentUser?.name || "Admin";
  }
  if (adminUserEmail) {
    adminUserEmail.textContent = currentUser?.email || "";
  }
  if (adminUserAvatar) {
    adminUserAvatar.src = currentUser?.picture || "";
    adminUserAvatar.classList.toggle("hidden", !currentUser?.picture);
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
    saveUserSession({ ...data.user, token: data.token, sessionExpiresAt: data.expiresAt || "" });
    currentQuota = data.quota || null;
    await loadHistory();
    renderAllHistoryViews();
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

function setGoogleToolsError(message) {
  if (googleToolsError) {
    googleToolsError.textContent = message || "";
  }
}

function formatGoogleToolCheckTime(value) {
  if (!value) {
    return "";
  }
  const checkedAt = new Date(value);
  if (Number.isNaN(checkedAt.getTime())) {
    return "";
  }
  return checkedAt.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

function renderGoogleToolsStatus(status) {
  googleToolsState = status || null;
  const calendar = status?.calendar || {};
  const connected = Boolean(calendar.connected);
  const configured = Boolean(calendar.configured || calendar.expiresAt || calendar.error);
  const needsReconnect = Boolean(calendar.needsReconnect || (configured && !connected));
  const checkedLabel = formatGoogleToolCheckTime(calendar.lastCheckedAt);
  if (googleCalendarStatus) {
    if (connected) {
      const suffix = checkedLabel ? ` - checked ${checkedLabel}` : "";
      googleCalendarStatus.textContent = `Connected until ${new Date(calendar.expiresAt).toLocaleString()}${suffix}`;
    } else if (calendar.error) {
      googleCalendarStatus.textContent = calendar.error;
    } else if (calendar.expiresAt) {
      googleCalendarStatus.textContent = "Permission expired. Reconnect Calendar.";
    } else {
      googleCalendarStatus.textContent = "Not connected.";
    }
  }
  if (googleCalendarHelp) {
    googleCalendarHelp.href = calendar.helpUrl || "#";
    googleCalendarHelp.classList.toggle("hidden", !calendar.helpUrl);
  }
  if (connectGoogleCalendar) {
    connectGoogleCalendar.textContent = connected || needsReconnect ? "Reconnect Calendar" : "Connect Calendar";
  }
  if (disconnectGoogleCalendar) {
    disconnectGoogleCalendar.disabled = !configured;
  }
}

async function loadGoogleToolsStatus(options = {}) {
  if (!currentUser?.authorized) {
    return;
  }
  const check = Boolean(options.check);
  const quiet = Boolean(options.quiet);
  if (!quiet) {
    setGoogleToolsError("");
  }
  try {
    const suffix = check ? "?check=1" : "";
    const response = await fetch(`${API_BASE}/api/google-tools${suffix}`, {
      headers: authHeaders(),
      cache: "no-store",
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to load Google tools.");
    }
    renderGoogleToolsStatus(data);
    if (!quiet && data?.calendar?.error) {
      setGoogleToolsError(data.calendar.error);
    }
    return data;
  } catch (error) {
    if (!quiet) {
      setGoogleToolsError(error.message || "Failed to load Google tools.");
    }
    return null;
  }
}

function startGoogleToolsMonitor() {
  if (googleToolsMonitor || !currentUser?.authorized) {
    return;
  }
  checkGoogleToolsConnection({ quiet: true });
  googleToolsMonitor = window.setInterval(() => {
    checkGoogleToolsConnection({ quiet: true });
  }, GOOGLE_TOOLS_CHECK_INTERVAL_MS);
}

function stopGoogleToolsMonitor() {
  if (!googleToolsMonitor) {
    return;
  }
  window.clearInterval(googleToolsMonitor);
  googleToolsMonitor = null;
}

function requestGoogleCalendarToken(prompt = "consent") {
  return new Promise((resolve, reject) => {
    if (!GOOGLE_CLIENT_ID) {
      reject(new Error("GOOGLE_CLIENT_ID is not configured."));
      return;
    }
    if (!window.google?.accounts?.oauth2) {
      reject(new Error("Google OAuth client is not loaded yet."));
      return;
    }
    const tokenClient = window.google.accounts.oauth2.initTokenClient({
      client_id: GOOGLE_CLIENT_ID,
      scope: GOOGLE_CALENDAR_SCOPE,
      prompt,
      callback: (response) => {
        if (response.error) {
          reject(new Error(response.error_description || response.error));
          return;
        }
        resolve(response);
      },
    });
    tokenClient.requestAccessToken();
  });
}

async function connectCalendarTool() {
  setGoogleToolsError("");
  if (connectGoogleCalendar) {
    connectGoogleCalendar.disabled = true;
  }
  try {
    const token = await requestGoogleCalendarToken("consent");
    const response = await fetch(`${API_BASE}/api/google-tools/calendar`, {
      method: "POST",
      headers: { "Content-Type": "application/json", ...authHeaders() },
      body: JSON.stringify({
        accessToken: token.access_token,
        expiresIn: Number(token.expires_in || 3600),
        scope: token.scope || GOOGLE_CALENDAR_SCOPE,
      }),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to connect Google Calendar.");
    }
    renderGoogleToolsStatus(data);
  } catch (error) {
    setGoogleToolsError(error.message || "Failed to connect Google Calendar.");
  } finally {
    if (connectGoogleCalendar) {
      connectGoogleCalendar.disabled = false;
    }
  }
}

async function refreshCalendarToolIfNeeded() {
  const calendar = googleToolsState?.calendar;
  if (!calendar?.expiresAt) {
    return;
  }
  const expiresAt = new Date(calendar.expiresAt).getTime();
  if (Number.isNaN(expiresAt) || expiresAt - Date.now() > 120000) {
    return;
  }
  try {
    const token = await requestGoogleCalendarToken("");
    const response = await fetch(`${API_BASE}/api/google-tools/calendar`, {
      method: "POST",
      headers: { "Content-Type": "application/json", ...authHeaders() },
      body: JSON.stringify({
        accessToken: token.access_token,
        expiresIn: Number(token.expires_in || 3600),
        scope: token.scope || GOOGLE_CALENDAR_SCOPE,
      }),
    });
    const data = await response.json();
    if (response.ok) {
      renderGoogleToolsStatus(data);
    }
  } catch (error) {
    console.warn("Google Calendar silent refresh failed.", error);
    renderGoogleToolsStatus({
      calendar: {
        connected: false,
        expiresAt: calendar.expiresAt,
        scope: calendar.scope || GOOGLE_CALENDAR_SCOPE,
      },
    });
  }
}

async function checkGoogleToolsConnection(options = {}) {
  await refreshCalendarToolIfNeeded();
  if (googleToolsState?.calendar?.needsReconnect) {
    return googleToolsState;
  }
  return loadGoogleToolsStatus({ check: true, quiet: Boolean(options.quiet) });
}

async function disconnectCalendarTool() {
  setGoogleToolsError("");
  try {
    const response = await fetch(`${API_BASE}/api/google-tools/calendar`, {
      method: "DELETE",
      headers: authHeaders(),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to disconnect Google Calendar.");
    }
    renderGoogleToolsStatus(data);
  } catch (error) {
    setGoogleToolsError(error.message || "Failed to disconnect Google Calendar.");
  }
}

function initLoginBotLook() {
  if (!loginBot) {
    return;
  }
  const pupils = Array.from(loginBot.querySelectorAll(".login-bot-pupil"));
  if (!pupils.length) {
    return;
  }
  const pupilStates = pupils.map((pupil) => {
    const baseTransform = pupil.getAttribute("transform") || "";
    const centerX = Number(pupil.getAttribute("x") || 0) + Number(pupil.getAttribute("width") || 0) / 2;
    const centerY = Number(pupil.getAttribute("y") || 0) + Number(pupil.getAttribute("height") || 0) / 2;
    return { pupil, baseTransform, centerX, centerY };
  });
  const setPupils = (x, y) => {
    for (const state of pupilStates) {
      state.pupil.setAttribute(
        "transform",
        `translate(${x} ${y}) ${state.baseTransform}`
      );
    }
  };
  document.addEventListener("pointermove", (event) => {
    if (loginView?.classList.contains("hidden")) {
      return;
    }
    const svg = loginBot.querySelector("svg");
    const point = svg?.createSVGPoint();
    if (!svg || !point) {
      return;
    }
    point.x = event.clientX;
    point.y = event.clientY;
    const cursor = point.matrixTransform(svg.getScreenCTM().inverse());
    let totalX = 0;
    let totalY = 0;
    for (const state of pupilStates) {
      totalX += cursor.x - state.centerX;
      totalY += cursor.y - state.centerY;
    }
    const avgX = totalX / pupilStates.length;
    const avgY = totalY / pupilStates.length;
    const angle = Math.atan2(avgY, avgX);
    const distance = Math.min(1, Math.hypot(avgX, avgY) / 170);
    const x = Math.round(Math.cos(angle) * distance * 15);
    const y = Math.round(Math.sin(angle) * distance * 10);
    setPupils(x, y);
  });
  document.addEventListener("pointerleave", () => setPupils(0, 0));
}

function initHomeBenderLook() {
  if (!homeBenderSvg) {
    return;
  }
  const pupils = Array.from(homeBenderSvg.querySelectorAll(".home-bender-pupil"));
  if (!pupils.length) {
    return;
  }
  const pupilStates = pupils.map((pupil) => {
    const baseTransform = pupil.getAttribute("transform") || "";
    const centerX = Number(pupil.getAttribute("x") || 0) + Number(pupil.getAttribute("width") || 0) / 2;
    const centerY = Number(pupil.getAttribute("y") || 0) + Number(pupil.getAttribute("height") || 0) / 2;
    return { pupil, baseTransform, centerX, centerY };
  });
  const setPupils = (x, y) => {
    for (const state of pupilStates) {
      state.pupil.setAttribute("transform", `translate(${x} ${y}) ${state.baseTransform}`);
    }
  };
  document.addEventListener("pointermove", (event) => {
    if (mainView?.classList.contains("hidden")) {
      return;
    }
    const point = homeBenderSvg.createSVGPoint();
    point.x = event.clientX;
    point.y = event.clientY;
    const matrix = homeBenderSvg.getScreenCTM();
    if (!matrix) {
      return;
    }
    const cursor = point.matrixTransform(matrix.inverse());
    let totalX = 0;
    let totalY = 0;
    for (const state of pupilStates) {
      totalX += cursor.x - state.centerX;
      totalY += cursor.y - state.centerY;
    }
    const avgX = totalX / pupilStates.length;
    const avgY = totalY / pupilStates.length;
    const angle = Math.atan2(avgY, avgX);
    const distance = Math.min(1, Math.hypot(avgX, avgY) / 170);
    const x = Math.round(Math.cos(angle) * distance * 14);
    const y = Math.round(Math.sin(angle) * distance * 9);
    setPupils(x, y);
  });
  document.addEventListener("pointerleave", () => setPupils(0, 0));
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
      saveUserSession({ ...data.user, token: currentUser.token, sessionExpiresAt: currentUser.sessionExpiresAt || "" });
      currentQuota = data.quota || null;
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
  if (currentUser?.authorized) {
    loadGoogleToolsStatus({ check: true, quiet: true });
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
        headers: authHeaders(),
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
        <button class="admin-user-chats ghost" type="button">Chats</button>
        <button class="admin-user-reset-quota ghost" type="button">Reset 5 chats</button>
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
    const chatsButton = row.querySelector(".admin-user-chats");
    const resetQuotaButton = row.querySelector(".admin-user-reset-quota");
    authorized?.addEventListener("change", () => updateAdminUser(email, { authorized: authorized.checked }));
    isAdmin?.addEventListener("change", () => updateAdminUser(email, { isAdmin: isAdmin.checked }));
    chatsButton?.addEventListener("click", () => loadAdminUserChats(email, row));
    resetQuotaButton?.addEventListener("click", () => resetAdminUserQuota(email, resetQuotaButton));
  }
}

async function loadAdminUserChats(email, row) {
  const existing = row.querySelector(".admin-chat-summary");
  if (existing) {
    existing.remove();
    return;
  }
  const panel = document.createElement("div");
  panel.className = "admin-chat-summary";
  panel.innerHTML = `<p class="helper-text">Loading chat summary...</p>`;
  row.appendChild(panel);
  try {
    const response = await fetch(`${API_BASE}/api/admin/users/${encodeURIComponent(email)}/chats`, {
      headers: authHeaders(),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to load chats.");
    }
    const chats = Array.isArray(data.chats) ? data.chats : [];
    panel.innerHTML = `
      <p><strong>${data.chatCount || 0}</strong> chats, <strong>${data.messageCount || 0}</strong> messages</p>
      ${chats.length ? chats.map((chat) => `
        <div class="admin-chat-item" data-chat-id="${escapeHtml(chat.id || "")}">
          <div class="admin-chat-head">
            <p>${escapeHtml(chat.title || "New chat")} <span>${escapeHtml(chat.updatedAt || "")}</span></p>
            <button class="admin-chat-delete ghost" type="button">Delete</button>
          </div>
          <small>${Number(chat.messageCount || 0)} messages</small>
          ${Array.isArray(chat.items) && chat.items.length ? chat.items.map((item) => `
            <div class="admin-chat-turn" data-item-index="${Number(item.index || 0)}">
              <div class="admin-chat-turn-head">
                <small>${escapeHtml(item.timestamp || "")}</small>
                <button class="admin-chat-turn-delete ghost" type="button">Delete</button>
              </div>
              <p><strong>User:</strong> ${escapeHtml(item.question || "None")}</p>
              <p><strong>Bender:</strong> ${escapeHtml(item.answer || "None")}</p>
            </div>
          `).join("") : `
            <small>Last question: ${escapeHtml(chat.lastQuestion || "None")}</small>
            <small>Last answer: ${escapeHtml(chat.lastAnswer || "None")}</small>
          `}
        </div>
      `).join("") : `<p class="helper-text">No chats yet.</p>`}
    `;
    for (const button of panel.querySelectorAll(".admin-chat-delete")) {
      button.addEventListener("click", () => deleteAdminUserChat(email, button.closest(".admin-chat-item")?.dataset.chatId, row));
    }
    for (const button of panel.querySelectorAll(".admin-chat-turn-delete")) {
      button.addEventListener("click", () => {
        const chatItem = button.closest(".admin-chat-item");
        const turn = button.closest(".admin-chat-turn");
        deleteAdminUserChatTurn(email, chatItem?.dataset.chatId, turn?.dataset.itemIndex, row);
      });
    }
  } catch (error) {
    panel.innerHTML = `<p class="error">${escapeHtml(error.message || "Failed to load chats.")}</p>`;
  }
}

async function deleteAdminUserChatTurn(email, chatId, itemIndex, row) {
  if (!chatId || itemIndex === undefined) {
    return;
  }
  try {
    const response = await fetch(
      `${API_BASE}/api/admin/users/${encodeURIComponent(email)}/chats/${encodeURIComponent(chatId)}/items/${encodeURIComponent(itemIndex)}`,
      {
        method: "DELETE",
        headers: authHeaders(),
      }
    );
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to delete message.");
    }
    row.querySelector(".admin-chat-summary")?.remove();
    await loadAdminUserChats(email, row);
  } catch (error) {
    showError(error.message || "Failed to delete message.");
  }
}

async function deleteAdminUserChat(email, chatId, row) {
  if (!chatId) {
    return;
  }
  try {
    const response = await fetch(`${API_BASE}/api/admin/users/${encodeURIComponent(email)}/chats/${encodeURIComponent(chatId)}`, {
      method: "DELETE",
      headers: authHeaders(),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to delete chat.");
    }
    row.querySelector(".admin-chat-summary")?.remove();
    await loadAdminUserChats(email, row);
  } catch (error) {
    showError(error.message || "Failed to delete chat.");
  }
}

async function resetAdminUserQuota(email, button) {
  if (button) {
    button.disabled = true;
  }
  try {
    const response = await fetch(`${API_BASE}/api/admin/users/${encodeURIComponent(email)}/quota/reset`, {
      method: "POST",
      headers: authHeaders(),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to reset quota.");
    }
    await loadAdminUsers();
  } catch (error) {
    showError(error.message || "Failed to reset quota.");
  } finally {
    if (button) {
      button.disabled = false;
    }
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
      saveUserSession({
        ...data.user,
        token: currentUser.token,
        sessionExpiresAt: currentUser.sessionExpiresAt || "",
      });
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

function applyAudioReplyPreference() {
  if (audioReplyToggle) {
    audioReplyToggle.checked = audioReplyEnabled;
  }
  if (voiceChatAudioReplyToggle) {
    voiceChatAudioReplyToggle.checked = audioReplyEnabled;
  }
  try {
    localStorage.setItem(AUDIO_REPLY_STORAGE_KEY, audioReplyEnabled ? "1" : "0");
  } catch (error) {
    console.warn("Failed to save audio reply preference.", error);
  }
}

function loadAudioReplyPreference() {
  try {
    const raw = localStorage.getItem(AUDIO_REPLY_STORAGE_KEY);
    if (raw === "0") {
      audioReplyEnabled = false;
    }
  } catch (error) {
    console.warn("Failed to load audio reply preference.", error);
  }
  applyAudioReplyPreference();
}

function setAudioReplyPreference(enabled) {
  audioReplyEnabled = Boolean(enabled);
  applyAudioReplyPreference();
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
  if (data?.quota) {
    currentQuota = data.quota;
  }
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
  } else {
    audioPlayer.removeAttribute("src");
    faceAudioPlayer.removeAttribute("src");
    audioPlayer.load();
    faceAudioPlayer.load();
    setRecordingState("Text answer ready.");
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
    localStorage.setItem(userHistoryStorageKey(), JSON.stringify(payload));
  } catch (error) {
    console.warn("Failed to save chat history locally.", error);
  }
  try {
    await fetch(`${API_BASE}/api/history`, {
      method: "POST",
      headers: { "Content-Type": "application/json", ...authHeaders() },
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
    const response = await fetch(`${API_BASE}/api/history`, { headers: authHeaders() });
    if (response.ok) {
      const data = await response.json();
      loaded = applyHistoryData(data);
    }
  } catch (error) {
    console.warn("Failed to load chat history remotely.", error);
  }

  if (!loaded) {
    try {
      const raw = localStorage.getItem(userHistoryStorageKey());
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
  await refreshCalendarToolIfNeeded();
  const formData = new FormData();
  formData.append("audio", blob, filename);
  formData.append("context", JSON.stringify(getActiveChatContext()));
  formData.append("chatId", activeChatId);
  formData.append("synthesizeAudio", audioReplyEnabled ? "true" : "false");
  setLoading(true, "Sending audio...", "audio");
  showError("");

  try {
    const response = await fetch(`${API_BASE}/api/chat/audio`, {
      method: "POST",
      headers: authHeaders(),
      body: formData,
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(friendlyRequestError(response, data, "Audio request failed"));
    }
    updateResult(data);
  } catch (error) {
    showError(error.message || "Audio request failed");
  } finally {
    setLoading(false, "Idle");
  }
}

async function sendTextMessage(message) {
  await refreshCalendarToolIfNeeded();
  setLoading(true, "Sending text...");
  showError("");

  try {
    const response = await fetch(`${API_BASE}/api/chat/text`, {
      method: "POST",
      headers: { "Content-Type": "application/json", ...authHeaders() },
      body: JSON.stringify({
        message,
        context: getActiveChatContext(),
        chatId: activeChatId,
        synthesizeAudio: audioReplyEnabled,
      }),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(friendlyRequestError(response, data, "Text request failed"));
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
adminSignOutBtn.addEventListener("click", clearUserSession);
openSarcasmConsole.addEventListener("click", () => {
  adminConsoleOverride = true;
  renderAuthState();
});
openAdminConsole.addEventListener("click", () => {
  adminConsoleOverride = false;
  renderAuthState();
});
adminRefresh.addEventListener("click", loadAdminUsers);
googleToolsRefresh.addEventListener("click", () => loadGoogleToolsStatus({ check: true }));
connectGoogleCalendar.addEventListener("click", connectCalendarTool);
disconnectGoogleCalendar.addEventListener("click", disconnectCalendarTool);

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
audioReplyToggle.addEventListener("change", () => setAudioReplyPreference(audioReplyToggle.checked));
voiceChatAudioReplyToggle.addEventListener("change", () => setAudioReplyPreference(voiceChatAudioReplyToggle.checked));

openFaceView.addEventListener("click", () => {
  mainView.classList.add("hidden");
  faceView.classList.remove("hidden");
  faceView.setAttribute("aria-hidden", "false");
  document.body.classList.add("face-open");
  faceView.scrollTop = 0;
  setLookDirection("look-center");
  startBlinkLoop();
});

heroFaceViewBtn.addEventListener("click", () => openFaceView.click());

openVoiceChatView.addEventListener("click", () => {
  mainView.classList.add("hidden");
  voiceChatView.classList.remove("hidden");
  voiceChatView.setAttribute("aria-hidden", "false");
  document.body.classList.add("face-open");
  setLookDirection("look-center");
  startBlinkLoop();
  renderAllHistoryViews();
});

heroVoiceChatBtn.addEventListener("click", () => openVoiceChatView.click());

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
  initHomeBenderLook();
  await initAuth();
  loadVoiceChatFontScale();
  loadAudioReplyPreference();
  loadHistory().then(() => renderAllHistoryViews());
  checkApiHealth();
});

document.addEventListener("visibilitychange", () => {
  if (!document.hidden && currentUser?.authorized) {
    checkGoogleToolsConnection({ quiet: true });
  }
});

document.addEventListener("pointerdown", enableAudioSync, { once: true });
document.addEventListener("keydown", enableAudioSync, { once: true });
