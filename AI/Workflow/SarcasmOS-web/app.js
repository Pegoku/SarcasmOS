let API_BASE = "";
let GOOGLE_CLIENT_ID = "";

const loginView = document.getElementById("loginView");
const profileSetup = document.getElementById("profileSetup");
const profileSetupForm = document.getElementById("profileSetupForm");
const profileDisplayName = document.getElementById("profileDisplayName");
const profileAge = document.getElementById("profileAge");
const profileGender = document.getElementById("profileGender");
const profileMorePronouns = document.getElementById("profileMorePronouns");
const profileExtraPronounsWrap = document.getElementById("profileExtraPronounsWrap");
const profileExtraPronouns = document.getElementById("profileExtraPronouns");
const profileCustomGenderWrap = document.getElementById("profileCustomGenderWrap");
const profileCustomGender = document.getElementById("profileCustomGender");
const profileSkip = document.getElementById("profileSkip");
const loginBot = document.getElementById("loginBot");
const benderWarning = document.getElementById("benderWarning");
const homeBenderButton = document.getElementById("homeBenderButton");
const homeBenderSvg = document.querySelector(".home-bender-svg");
const googleLoginButton = document.getElementById("googleLoginButton");
const loginError = document.getElementById("loginError");
const loginSignOutBtn = document.getElementById("loginSignOutBtn");
const userBadge = document.getElementById("userBadge");
const userAvatar = document.getElementById("userAvatar");
const userName = document.getElementById("userName");
const userEmail = document.getElementById("userEmail");
const signOutBtn = document.getElementById("signOutBtn");
const languageToggle = document.getElementById("languageToggle");
const editProfileInfo = document.getElementById("editProfileInfo");
const openAdminConsole = document.getElementById("openAdminConsole");
const adminView = document.getElementById("adminView");
const developerView = document.getElementById("developerView");
const openDeveloperMode = document.getElementById("openDeveloperMode");
const closeDeveloperMode = document.getElementById("closeDeveloperMode");
const developerLanguageToggle = document.getElementById("developerLanguageToggle");
const adminUserAvatar = document.getElementById("adminUserAvatar");
const adminUserName = document.getElementById("adminUserName");
const adminUserEmail = document.getElementById("adminUserEmail");
const adminSignOutBtn = document.getElementById("adminSignOutBtn");
const adminLanguageToggle = document.getElementById("adminLanguageToggle");
const openSarcasmConsole = document.getElementById("openSarcasmConsole");
const adminPanel = document.getElementById("adminPanel");
const adminRefresh = document.getElementById("adminRefresh");
const adminUsersList = document.getElementById("adminUsersList");
const developerRequestsNotice = document.getElementById("developerRequestsNotice");
const adminSupportPanel = document.getElementById("adminSupportPanel");
const adminSupportRefresh = document.getElementById("adminSupportRefresh");
const adminSupportList = document.getElementById("adminSupportList");
const googleToolsPanel = document.getElementById("googleToolsPanel");
const developerModePanel = document.getElementById("developerModePanel");
const supportPanel = document.getElementById("supportPanel");
const supportLauncher = document.getElementById("supportLauncher");
const supportClose = document.getElementById("supportClose");
const supportChatSelect = document.getElementById("supportChatSelect");
const supportNewChat = document.getElementById("supportNewChat");
const supportMessages = document.getElementById("supportMessages");
const supportForm = document.getElementById("supportForm");
const supportInput = document.getElementById("supportInput");
const supportStatus = document.getElementById("supportStatus");
const developerModeRequest = document.getElementById("developerModeRequest");
const developerModeStatus = document.getElementById("developerModeStatus");
const developerModeForm = document.getElementById("developerModeForm");
const developerCompletionsUrl = document.getElementById("developerCompletionsUrl");
const developerCompletionsKey = document.getElementById("developerCompletionsKey");
const developerReplicateUrl = document.getElementById("developerReplicateUrl");
const developerReplicateKey = document.getElementById("developerReplicateKey");
const developerFallbackUrl = document.getElementById("developerFallbackUrl");
const developerFallbackKey = document.getElementById("developerFallbackKey");
const developerLlmModel = document.getElementById("developerLlmModel");
const developerTtsModel = document.getElementById("developerTtsModel");
const developerModeReset = document.getElementById("developerModeReset");
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
const aiCreditsStatus = document.getElementById("aiCreditsStatus");
const aiCreditsDetail = document.getElementById("aiCreditsDetail");
const mainView = document.getElementById("mainView");
const faceView = document.getElementById("faceView");
const voiceChatView = document.getElementById("voiceChatView");
const konamiView = document.getElementById("konamiView");
const konamiAudioPlayer = document.getElementById("konamiAudioPlayer");
const konamiStatus = document.getElementById("konamiStatus");
const closeKonamiView = document.getElementById("closeKonamiView");
const konamiDanceCanvas = document.getElementById("konamiDanceCanvas");
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
const LANGUAGE_STORAGE_KEY = "sarcasmos.language";
const USER_PROFILE_STORAGE_KEY = "sarcasmos.userProfile";
const SUPPORT_HISTORY_STORAGE_KEY = "sarcasmos.supportHistory";
const HISTORY_STORAGE_VERSION = "v2";
const AUDIO_REPLY_STORAGE_KEY = "sarcasmos.audioReplyEnabled";
const PROFILE_GENDER_OPTIONS = [
  { value: "", en: "Prefer not to say", es: "Prefiero no decirlo" },
  { value: "female", en: "Woman / she", es: "Mujer / ella" },
  { value: "male", en: "Man / he", es: "Hombre / él" },
  { value: "nonbinary", en: "Non-binary / they", es: "No binario / elle" },
  { value: "agender", en: "Agender", es: "Agénero" },
  { value: "androgynous", en: "Androgynous", es: "Andrógino" },
  { value: "androgyne", en: "Androgyne", es: "Andrógine" },
  { value: "aporagender", en: "Aporagender", es: "Aporagénero" },
  { value: "bigender", en: "Bigender", es: "Bigénero" },
  { value: "cisgender", en: "Cisgender", es: "Cisgénero" },
  { value: "cis_woman", en: "Cis woman", es: "Mujer cis" },
  { value: "cis_man", en: "Cis man", es: "Hombre cis" },
  { value: "demiboy", en: "Demiboy", es: "Demichico" },
  { value: "demigirl", en: "Demigirl", es: "Demichica" },
  { value: "demigender", en: "Demigender", es: "Demigénero" },
  { value: "enby", en: "Enby", es: "Enby" },
  { value: "femme", en: "Femme", es: "Femme" },
  { value: "gender_apathic", en: "Gender apathic", es: "Género apático" },
  { value: "gender_creative", en: "Gender creative", es: "Género creativo" },
  { value: "gender_expansive", en: "Gender expansive", es: "Género expansivo" },
  { value: "gender_nonconforming", en: "Gender non-conforming", es: "Género no conforme" },
  { value: "gender_questioning", en: "Questioning gender", es: "Cuestionando mi género" },
  { value: "gender_variant", en: "Gender variant", es: "Género variante" },
  { value: "genderfluid", en: "Genderfluid", es: "Género fluido" },
  { value: "genderflux", en: "Genderflux", es: "Genderflux" },
  { value: "genderqueer", en: "Genderqueer", es: "Genderqueer" },
  { value: "genderless", en: "Genderless", es: "Sin género" },
  { value: "gendervoid", en: "Gendervoid", es: "Gendervoid" },
  { value: "graygender", en: "Graygender", es: "Grisgénero" },
  { value: "intergender", en: "Intergender", es: "Intergénero" },
  { value: "intersex", en: "Intersex", es: "Intersex" },
  { value: "masc", en: "Masc", es: "Masc" },
  { value: "maverique", en: "Maverique", es: "Maverique" },
  { value: "multigender", en: "Multigender", es: "Multigénero" },
  { value: "neutrois", en: "Neutrois", es: "Neutrois" },
  { value: "nonbinary_man", en: "Non-binary man", es: "Hombre no binario" },
  { value: "nonbinary_woman", en: "Non-binary woman", es: "Mujer no binaria" },
  { value: "pangender", en: "Pangender", es: "Pangénero" },
  { value: "polygender", en: "Polygender", es: "Poligénero" },
  { value: "trans", en: "Trans", es: "Trans" },
  { value: "trans_woman", en: "Trans woman", es: "Mujer trans" },
  { value: "trans_man", en: "Trans man", es: "Hombre trans" },
  { value: "transfeminine", en: "Transfeminine", es: "Transfemenino" },
  { value: "transmasculine", en: "Transmasculine", es: "Transmasculino" },
  { value: "two_spirit", en: "Two-Spirit", es: "Two-Spirit" },
  { value: "xenogender", en: "Xenogender", es: "Xenogénero" },
  { value: "third_gender", en: "Third gender", es: "Tercer género" },
  { value: "third_sex", en: "Third sex", es: "Tercer sexo" },
  { value: "trigender", en: "Trigender", es: "Trigénero" },
  { value: "omnigender", en: "Omnigender", es: "Omnigénero" },
  { value: "aliagender", en: "Aliagender", es: "Aliagénero" },
  { value: "ambigender", en: "Ambigender", es: "Ambigénero" },
  { value: "autigender", en: "Autigender", es: "Autigénero" },
  { value: "cassgender", en: "Cassgender", es: "Cassgénero" },
  { value: "collgender", en: "Collgender", es: "Collgénero" },
  { value: "condigender", en: "Condigender", es: "Condigénero" },
  { value: "demiflux", en: "Demiflux", es: "Demiflux" },
  { value: "demifluid", en: "Demifluid", es: "Demifluid" },
  { value: "faegender", en: "Faegender", es: "Faegénero" },
  { value: "fluidflux", en: "Fluidflux", es: "Fluidflux" },
  { value: "genderfae", en: "Genderfae", es: "Genderfae" },
  { value: "genderfaun", en: "Genderfaun", es: "Genderfaun" },
  { value: "genderpunk", en: "Genderpunk", es: "Genderpunk" },
  { value: "genderwitched", en: "Genderwitched", es: "Genderwitched" },
  { value: "libragender", en: "Libragender", es: "Libragénero" },
  { value: "librafeminine", en: "Librafeminine", es: "Librafemenino" },
  { value: "libramasculine", en: "Libramasculine", es: "Libramasculino" },
  { value: "nanogender", en: "Nanogender", es: "Nanogénero" },
  { value: "novigender", en: "Novigender", es: "Novigénero" },
  { value: "paragender", en: "Paragender", es: "Paragénero" },
  { value: "proxvir", en: "Proxvir", es: "Proxvir" },
  { value: "juxera", en: "Juxera", es: "Juxera" },
  { value: "quoigender", en: "Quoigender", es: "Quoigénero" },
  { value: "questioning_unsure", en: "Questioning / unsure", es: "Cuestionando / no lo tengo claro" },
  { value: "prefer_labels_later", en: "Ask me later", es: "Pregúntame más tarde" },
  { value: "same_as_google_name", en: "Just use my profile name", es: "Usa solo mi nombre de perfil" },
  { value: "respectful_default", en: "Respectful neutral default", es: "Neutral respetuoso" },
  { value: "ze_hir", en: "Ze / hir", es: "Ze / hir" },
  { value: "xe_xem", en: "Xe / xem", es: "Xe / xem" },
  { value: "ey_em", en: "Ey / em", es: "Ey / em" },
  { value: "fae_faer", en: "Fae / faer", es: "Fae / faer" },
  { value: "he_they", en: "He / they", es: "Él / elle" },
  { value: "she_they", en: "She / they", es: "Ella / elle" },
  { value: "they_he", en: "They / he", es: "Elle / él" },
  { value: "they_she", en: "They / she", es: "Elle / ella" },
  { value: "any_pronouns", en: "Any pronouns", es: "Cualquier pronombre" },
  { value: "no_pronouns", en: "Use my name, no pronouns", es: "Usa mi nombre, sin pronombres" },
  { value: "femboy", en: "Femboy", es: "Femboy" },
  { value: "tomboy", en: "Tomboy", es: "Tomboy" },
  { value: "therian", en: "Therian", es: "Therian" },
  { value: "otherkin", en: "Otherkin", es: "Otherkin" },
  { value: "robot", en: "Robot", es: "Robot" },
  { value: "sarcastic_robot", en: "Sarcastic robot", es: "Robot sarcástico" },
  { value: "sentient_toaster", en: "Sentient toaster", es: "Tostadora consciente" },
  { value: "coffee_machine", en: "Coffee machine", es: "Cafetera con autoestima" },
  { value: "chaotic_entity", en: "Chaotic entity", es: "Entidad caótica" },
  { value: "final_boss", en: "Final boss", es: "Jefe final" },
  { value: "npc_with_lore", en: "NPC with lore", es: "NPC con lore" },
  { value: "side_quest_giver", en: "Side quest giver", es: "NPC que da misiones" },
  { value: "walking_bug_report", en: "Walking bug report", es: "Bug con patas" },
  { value: "premium_disaster", en: "Premium disaster", es: "Desastre premium" },
  { value: "tax_fraud_goblin", en: "Tax fraud enthusiast", es: "Fan del fraude fiscal" },
  { value: "bender_cousin", en: "Bender's suspicious cousin", es: "Primo sospechoso de Bender" },
  { value: "meatbag", en: "Meatbag", es: "Saco de carne" },
  { value: "carbon_based_problem", en: "Carbon-based problem", es: "Problema basado en carbono" },
  { value: "wifi_ghost", en: "Wi-Fi ghost", es: "Fantasma del wifi" },
  { value: "spreadsheet_victim", en: "Spreadsheet victim", es: "Víctima del Excel" },
  { value: "emotionally_buffering", en: "Emotionally buffering", es: "Cargando emociones" },
  { value: "404_gender_not_found", en: "404: gender not found", es: "404: género no encontrado" },
  { value: "terms_and_conditions", en: "Terms and conditions nobody read", es: "Términos y condiciones que nadie leyó" },
  { value: "expired_warranty", en: "Expired warranty", es: "Garantía caducada" },
  { value: "premium_mistake", en: "Premium mistake", es: "Error premium" },
  { value: "emotionally_bankrupt", en: "Emotionally bankrupt", es: "En bancarrota emocional" },
  { value: "walking_red_flag", en: "Walking red flag", es: "Bandera roja con piernas" },
  { value: "unpaid_intern", en: "Unpaid intern", es: "Becario sin cobrar" },
  { value: "legal_liability", en: "Legal liability", es: "Responsabilidad legal" },
  { value: "forbidden_patch_note", en: "Forbidden patch note", es: "Nota de parche prohibida" },
  { value: "side_effect", en: "Side effect", es: "Efecto secundario" },
  { value: "failed_captcha", en: "Failed CAPTCHA", es: "CAPTCHA fallido" },
  { value: "low_battery_prophet", en: "Low-battery prophet", es: "Profeta con batería baja" },
  { value: "morally_flexible", en: "Morally flexible", es: "Moralmente flexible" },
  { value: "black_box_with_anxiety", en: "Black box with anxiety", es: "Caja negra con ansiedad" },
  { value: "budget_villain", en: "Budget villain", es: "Villano de bajo presupuesto" },
  { value: "fraudulent_wizard", en: "Fraudulent wizard", es: "Mago fraudulento" },
  { value: "tax_deductible_trauma", en: "Tax-deductible trauma", es: "Trauma deducible de impuestos" },
  { value: "sleep_deprivation_entity", en: "Sleep-deprivation entity", es: "Entidad de privación de sueño" },
  { value: "cosmic_bad_decision", en: "Cosmic bad decision", es: "Mala decisión cósmica" },
  { value: "existential_receipt", en: "Existential receipt", es: "Ticket existencial" },
  { value: "haunted_terms_sheet", en: "Haunted terms sheet", es: "Hoja de condiciones encantada" },
  { value: "lawsuit_speedrun", en: "Lawsuit speedrun", es: "Speedrun de demanda" },
  { value: "bender_minion", en: "Bender's unpaid minion", es: "Secuaz no pagado de Bender" },
  { value: "ethically_sourced_disaster", en: "Ethically sourced disaster", es: "Desastre de origen ético" },
  { value: "clown_accountant", en: "Clown accountant", es: "Contable payaso" },
  { value: "corporate_ritual", en: "Corporate ritual", es: "Ritual corporativo" },
  { value: "meeting_that_should_email", en: "Meeting that should be an email", es: "Reunión que debió ser un email" },
  { value: "sentient_debt", en: "Sentient debt", es: "Deuda consciente" },
  { value: "doomscroll_knight", en: "Doomscroll knight", es: "Caballero del doomscroll" },
  { value: "algorithmic_accident", en: "Algorithmic accident", es: "Accidente algorítmico" },
  { value: "emotional_damage_dealer", en: "Emotional damage dealer", es: "Repartidor de daño emocional" },
  { value: "ask_bender", en: "Ask Bender, he knows everything badly", es: "Pregúntale a Bender, lo sabe todo mal" },
  { value: "bender_guess", en: "Let Bender guess badly", es: "Que Bender lo adivine mal" },
  { value: "chaos_mode", en: "Chaos mode", es: "Modo caos" },
  { value: "custom", en: "Custom", es: "Personalizado" },
];
const PROFILE_BASIC_GENDER_VALUES = new Set(["", "female", "male", "nonbinary", "custom"]);
const PROFILE_MORE_GENDER_VALUE = "__more__";
const GOOGLE_CALENDAR_SCOPE = "https://www.googleapis.com/auth/calendar.readonly";
const GOOGLE_TOOLS_CHECK_INTERVAL_MS = 30000;
const BENDER_WARNING_AUDIO = [
  "/api/audio/easteregg-warning-1.wav",
  "/api/audio/easteregg-warning-2.wav",
  "/api/audio/easteregg-warning-3.wav",
];
const KONAMI_CODE = [
  "ArrowUp",
  "ArrowUp",
  "ArrowDown",
  "ArrowDown",
  "ArrowLeft",
  "ArrowRight",
  "ArrowLeft",
  "ArrowRight",
];
const KONAMI_AUDIO_SRC = "assets/konami.wav";
const HOME_HERO_SUBTITLES = {
  en: [
    "A sharper way to talk, ask, listen, and get things done without opening six different tabs like a tired accountant.",
    "Your mildly judgemental robot assistant for voice, text, tasks, and the occasional reality check.",
    "Ask by voice, type like a civilized menace, or upload audio and let Bender do the heavy lifting.",
    "A home base for quick answers, saved conversations, and robotic sarcasm with surprisingly useful timing.",
    "Talk to Bender, connect your tools, and pretend this was your plan all along.",
    "One place for voice, text, memory, and a robot face that is absolutely judging your cursor.",
    "Productivity, but with the emotional warmth of a vending machine in a basement.",
    "For when your brain has too many tabs open and none of them are paying rent.",
    "Ask Bender anything. The answer may help, hurt, or both, which is basically efficiency.",
    "A digital assistant for people who want answers faster than their motivation disappears.",
    "Bring your questions. Bender will bring the confidence of someone who has never had a human body.",
    "Less chaos, more answers, and just enough sarcasm to keep everyone legally awake.",
    "Because doing everything manually builds character, and character is overrated.",
    "Voice, text, memory, and the comforting sense that at least one robot is disappointed in you.",
    "A home for tasks, questions, and tiny existential crises with better formatting.",
    "The assistant that helps before your todo list becomes archaeological evidence.",
    "For when your calendar, inbox, and motivation all died in the same meeting.",
    "Answers fast enough to outrun your bad decisions. Almost.",
    "A robot assistant for days when common sense called in sick.",
    "Voice, text, and memory, because apparently suffering also needs version control.",
    "Bender helps you organize life, or at least makes the chaos sound intentional.",
    "Built for questions, reminders, and emotional damage with decent latency.",
    "Less therapy, more automation. Results may vary, dignity not included.",
    "A friendly home for your prompts, unless your prompts deserve consequences.",
    "Bender organizes your day with the empathy of a tax audit and the charm of a stolen wallet.",
    "Ask Bender for help and he will answer like your problems are bugs he would delete with pleasure.",
    "A robot assistant that remembers your tasks, your chaos, and probably where you buried your motivation.",
    "Bender cannot fix your life, but he can label the wreckage with impressive confidence.",
    "For questions, plans, reminders, and the kind of honesty your friends wisely avoid.",
    "Bender brings answers, sarcasm, and the emotional support of a locked fire exit.",
    "Your personal robot for turning tiny disasters into scheduled events.",
    "Bender is here to help, judge, and make your bad decisions feel professionally documented.",
  ],
  es: [
    "Una forma más directa de hablar, preguntar, escuchar y resolver cosas sin abrir seis pestañas como un contable derrotado.",
    "Tu asistente robótico ligeramente borde para voz, texto, tareas y alguna dosis saludable de realidad.",
    "Pregunta por voz, escribe como una amenaza civilizada o sube audio y deja que Bender cargue con el marrón.",
    "Un punto de partida para respuestas rápidas, conversaciones guardadas y sarcasmo robótico sorprendentemente útil.",
    "Habla con Bender, conecta tus herramientas y finge que este era tu plan desde el principio.",
    "Un solo sitio para voz, texto, memoria y una cara robótica que juzga tu cursor con bastante precisión.",
    "Productividad, pero con el calor emocional de una máquina expendedora en un sótano.",
    "Para cuando tu cerebro tiene demasiadas pestañas abiertas y ninguna paga alquiler.",
    "Pregúntale a Bender lo que quieras. La respuesta puede ayudar, doler, o ambas cosas, que eso también es eficiencia.",
    "Un asistente digital para quien quiere respuestas antes de que desaparezca su motivación.",
    "Trae tus preguntas. Bender trae la confianza de alguien que nunca ha tenido cuerpo humano.",
    "Menos caos, más respuestas y el sarcasmo justo para mantener a todos legalmente despiertos.",
    "Porque hacerlo todo a mano forja carácter, y el carácter está sobrevalorado.",
    "Voz, texto, memoria y la tranquilidad de que al menos un robot está decepcionado contigo.",
    "Un hogar para tareas, preguntas y pequeñas crisis existenciales con mejor formato.",
    "El asistente que ayuda antes de que tu lista de tareas se convierta en prueba arqueologica.",
    "Para cuando tu calendario, tu bandeja de entrada y tus ganas murieron en la misma reunión.",
    "Respuestas lo bastante rápidas como para adelantar a tus malas decisiones. Casi.",
    "Un asistente robótico para esos días en los que el sentido común se dio de baja.",
    "Voz, texto y memoria, porque al parecer el sufrimiento también necesita control de versiones.",
    "Bender te ayuda a ordenar la vida, o al menos a que el caos parezca intencionado.",
    "Hecho para preguntas, recordatorios y daño emocional con una latencia decente.",
    "Menos terapia, más automatización. Resultados variables, dignidad no incluida.",
    "Un hogar amable para tus prompts, salvo que tus prompts merezcan consecuencias.",
    "Bender organiza tu día con la empatía de una inspección de Hacienda y el encanto de una cartera robada.",
    "Pídele ayuda a Bender y responderá como si tus problemas fueran bugs que borraría con gusto.",
    "Un asistente robótico que recuerda tus tareas, tu caos y probablemente dónde enterraste tu motivación.",
    "Bender no puede arreglar tu vida, pero puede etiquetar el desastre con una confianza admirable.",
    "Para preguntas, planes, recordatorios y esa honestidad que tus amigos evitan por supervivencia.",
    "Bender trae respuestas, sarcasmo y el apoyo emocional de una salida de emergencia cerrada.",
    "Tu robot personal para convertir pequeños desastres en eventos programados.",
    "Bender está aquí para ayudar, juzgar y documentar profesionalmente tus malas decisiones.",
  ],
};
const DEFAULT_CHAT_ID = "default";
let mediaRecorder = null;
let audioChunks = [];
let recordingStartedAt = 0;
let activePlaybackTarget = "main";
const chatSessions = [];
let activeChatId = DEFAULT_CHAT_ID;
let chatHistory = [];
let pendingQuestion = "";
let pendingHistoryEntryId = "";
let blinkTimer = null;
let talkTimer = null;
let talkRaf = null;
let thinkTimer = null;
let thinkLongTimer = null;
let isBusy = false;
let voiceChatFontScale = 1;
let audioReplyEnabled = true;
let googleToolsState = null;
let developerModeState = null;
let googleToolsMonitor = null;
let currentQuota = null;
let lastAdminUsers = [];
let adminConsoleOverride = false;
let developerViewOverride = false;
let currentLanguage = "en";
let homeSubtitleIndex = 0;
let homeBenderAnnoyance = 0;
let benderWarningTimer = null;
let homeBenderMoodTimer = null;
let benderWarningAudio = null;
let konamiIndex = 0;
let konamiDanceAnimation = 0;
const audioSyncMap = new Map();
let audioSyncEnabled = false;
let currentUser = null;
let currentUserProfile = null;
let profileSetupForcedOpen = false;
let supportConversation = [];
let supportChats = [];
let activeSupportChatId = "";
let supportWidgetOpen = false;

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
  setRecordingState(label || (isLoading ? tr("thinking") : tr("idle")));
  setThinking(isLoading, mode);
}

function showError(message) {
  errorOutput.textContent = message || "";
}

function showBenderWarning(message) {
  if (!benderWarning) {
    return;
  }
  window.clearTimeout(benderWarningTimer);
  benderWarning.textContent = message;
  benderWarning.classList.remove("hidden");
  benderWarningTimer = window.setTimeout(() => {
    benderWarning.classList.add("hidden");
    benderWarning.textContent = "";
  }, 10000);
}

function animateHomeBenderAnger() {
  if (!homeBenderButton) {
    return;
  }
  window.clearTimeout(homeBenderMoodTimer);
  homeBenderButton.classList.remove("home-bender-angry", "home-bender-blink");
  void homeBenderButton.offsetWidth;
  homeBenderButton.classList.add("home-bender-angry", "home-bender-blink");
  homeBenderMoodTimer = window.setTimeout(() => {
    homeBenderButton.classList.remove("home-bender-angry", "home-bender-blink");
  }, 1400);
}

function playBenderWarningAudio(index) {
  const src = BENDER_WARNING_AUDIO[index];
  if (!src) {
    return;
  }
  try {
    if (benderWarningAudio) {
      benderWarningAudio.pause();
      benderWarningAudio.currentTime = 0;
    }
    benderWarningAudio = new Audio(`${API_BASE}${src}`);
    homeBenderButton?.classList.add("home-bender-speaking");
    const stopSpeaking = () => {
      homeBenderButton?.classList.remove("home-bender-speaking");
    };
    benderWarningAudio.addEventListener("ended", stopSpeaking, { once: true });
    benderWarningAudio.addEventListener("pause", () => {
      if (benderWarningAudio?.ended || benderWarningAudio?.currentTime === 0) {
        stopSpeaking();
      }
    });
    benderWarningAudio.play().catch((error) => {
      stopSpeaking();
      console.warn("Bender warning audio playback failed.", error);
    });
  } catch (error) {
    homeBenderButton?.classList.remove("home-bender-speaking");
    console.warn("Bender warning audio setup failed.", error);
  }
}

function annoyHomeBender() {
  if (!currentUser?.token) {
    return;
  }
  homeBenderAnnoyance += 1;
  animateHomeBenderAnger();
  if (homeBenderAnnoyance === 1) {
    showBenderWarning("Si vuelves a hacerlo, te echo de la página");
    playBenderWarningAudio(0);
    return;
  }
  if (homeBenderAnnoyance === 2) {
    showBenderWarning("Última vez que te lo digo. ¡Para ya!");
    playBenderWarningAudio(1);
    return;
  }
  showBenderWarning("Tú mismo te lo has buscado");
  playBenderWarningAudio(2);
  clearUserSession();
  homeBenderAnnoyance = 0;
}

function friendlyRequestError(response, data, fallback) {
  if (response?.status === 429) {
    return "Se te han acabado los créditos IA de esta semana. Hasta la semana que viene no tendrás más, salvo que un admin te añada créditos desde el panel.";
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

function userSupportStorageKey() {
  const email = String(currentUser?.email || "anonymous").trim().toLowerCase();
  return `${SUPPORT_HISTORY_STORAGE_KEY}.${HISTORY_STORAGE_VERSION}.${email}`;
}

function userProfileStorageKey() {
  const email = String(currentUser?.email || "anonymous").trim().toLowerCase();
  return `${USER_PROFILE_STORAGE_KEY}.${HISTORY_STORAGE_VERSION}.${email}`;
}

function loadUserProfile() {
  try {
    const raw = localStorage.getItem(userProfileStorageKey());
    currentUserProfile = raw ? JSON.parse(raw) : null;
  } catch (error) {
    console.warn("Failed to load user profile.", error);
    currentUserProfile = null;
  }
  return currentUserProfile;
}

function saveUserProfile(profile) {
  currentUserProfile = {
    preferredName: String(profile.preferredName || "").trim(),
    age: String(profile.age || "").trim(),
    gender: String(profile.gender || "").trim(),
    customGender: String(profile.customGender || "").trim(),
    skipped: Boolean(profile.skipped),
    updatedAt: new Date().toISOString(),
  };
  profileSetupForcedOpen = false;
  try {
    localStorage.setItem(userProfileStorageKey(), JSON.stringify(currentUserProfile));
  } catch (error) {
    console.warn("Failed to save user profile.", error);
  }
  renderProfileSetup();
}

function profileIsComplete() {
  return Boolean(currentUserProfile?.skipped || currentUserProfile?.preferredName);
}

function profileGenderLabel(profile = currentUserProfile) {
  if (!profile) {
    return "";
  }
  if (profile.gender === "custom") {
    return profile.customGender || "";
  }
  const option = PROFILE_GENDER_OPTIONS.find((item) => item.value === profile.gender);
  if (option) {
    return option[currentLanguage] || option.en;
  }
  const labels = {
    female: currentLanguage === "es" ? "mujer / ella" : "female / she",
    male: currentLanguage === "es" ? "hombre / él" : "male / he",
    nonbinary: currentLanguage === "es" ? "no binario / elle" : "non-binary / they",
  };
  return labels[profile.gender] || "";
}

function userProfileContextEntry() {
  if (!currentUserProfile || currentUserProfile.skipped) {
    return null;
  }
  const parts = [];
  if (currentUserProfile.preferredName) {
    parts.push(`preferred name: ${currentUserProfile.preferredName}`);
  }
  if (currentUserProfile.age) {
    parts.push(`age: ${currentUserProfile.age}`);
  }
  const gender = profileGenderLabel();
  if (gender) {
    parts.push(`gender/pronouns: ${gender}`);
  }
  if (!parts.length) {
    return null;
  }
  return {
    question: "User profile",
    answer: `Use this user profile when replying: ${parts.join("; ")}. Address the user by their preferred name when natural.`,
    timestamp: currentUserProfile.updatedAt || "",
  };
}

function renderProfileSetup() {
  const shouldShow = Boolean(currentUser?.authorized && (profileSetupForcedOpen || !profileIsComplete()));
  renderProfileGenderOptions();
  profileSetup?.classList.toggle("hidden", !shouldShow);
  profileSetup?.setAttribute("aria-hidden", shouldShow ? "false" : "true");
  document.body.classList.toggle("profile-setup-open", shouldShow);
  if (shouldShow) {
    if (profileDisplayName) {
      profileDisplayName.value = currentUserProfile?.preferredName || currentUser?.name?.split(" ")[0] || "";
    }
    if (profileAge) {
      profileAge.value = currentUserProfile?.age || "";
    }
    if (profileCustomGender) {
      profileCustomGender.value = currentUserProfile?.customGender || "";
    }
    if (profileGender) {
      const savedGender = currentUserProfile?.gender || "";
      profileGender.value = PROFILE_BASIC_GENDER_VALUES.has(savedGender) ? savedGender : "";
      syncProfileCustomGenderVisibility();
    }
  }
}

function openProfileSetupEditor() {
  if (!currentUser?.authorized) {
    return;
  }
  profileSetupForcedOpen = true;
  renderProfileSetup();
  setTimeout(() => profileDisplayName?.focus(), 60);
}

function syncProfileCustomGenderVisibility() {
  const custom = profileGender?.value === "custom";
  profileCustomGenderWrap?.classList.toggle("hidden", !custom);
}

function renderProfileGenderOptions() {
  if (!profileGender) {
    return;
  }
  const saved = currentUserProfile?.gender || "";
  const selected = profileGender.value || (PROFILE_BASIC_GENDER_VALUES.has(saved) ? saved : PROFILE_MORE_GENDER_VALUE);
  profileGender.innerHTML = "";
  PROFILE_GENDER_OPTIONS.filter((item) => PROFILE_BASIC_GENDER_VALUES.has(item.value)).forEach((item) => {
    const option = document.createElement("option");
    option.value = item.value;
    option.textContent = item[currentLanguage] || item.en;
    profileGender.appendChild(option);
  });
  if (!PROFILE_BASIC_GENDER_VALUES.has(selected) && selected !== PROFILE_MORE_GENDER_VALUE) {
    profileGender.value = "";
  } else if (selected === PROFILE_MORE_GENDER_VALUE) {
    profileGender.value = "";
  } else {
    profileGender.value = selected;
  }
  renderProfileExtraPronounOptions(saved);
  syncProfileCustomGenderVisibility();
}

function renderProfileExtraPronounOptions(savedValue = currentUserProfile?.gender || "") {
  if (!profileExtraPronouns) {
    return;
  }
  const extras = PROFILE_GENDER_OPTIONS.filter((item) => item.value && !PROFILE_BASIC_GENDER_VALUES.has(item.value));
  profileExtraPronouns.innerHTML = "";
  extras.forEach((item) => {
    const option = document.createElement("option");
    option.value = item.value;
    option.textContent = item[currentLanguage] || item.en;
    profileExtraPronouns.appendChild(option);
  });
  profileExtraPronouns.value = extras.some((item) => item.value === savedValue) ? savedValue : extras[0]?.value || "";
  const shouldShow = Boolean(savedValue && !PROFILE_BASIC_GENDER_VALUES.has(savedValue));
  profileExtraPronounsWrap?.classList.toggle("hidden", !shouldShow);
  profileMorePronouns?.classList.toggle("active", shouldShow);
}

function toggleProfileExtraPronouns() {
  const isHidden = profileExtraPronounsWrap?.classList.contains("hidden");
  profileExtraPronounsWrap?.classList.toggle("hidden", !isHidden);
  profileMorePronouns?.classList.toggle("active", Boolean(isHidden));
  if (isHidden) {
    profileGender.value = "";
    syncProfileCustomGenderVisibility();
    profileExtraPronouns?.focus();
  }
}

function selectedProfileGenderValue() {
  const extraOpen = profileExtraPronounsWrap && !profileExtraPronounsWrap.classList.contains("hidden");
  if (extraOpen && profileExtraPronouns?.value) {
    return profileExtraPronouns.value;
  }
  return profileGender?.value || "";
}

function chooseHomeSubtitleIndex() {
  const subtitles = HOME_HERO_SUBTITLES.en;
  let nextIndex = Math.floor(Math.random() * subtitles.length);
  try {
    const lastIndex = Number(sessionStorage.getItem("sarcasmos.lastHomeSubtitle"));
    if (subtitles.length > 1 && nextIndex === lastIndex) {
      nextIndex = (nextIndex + 1) % subtitles.length;
    }
    sessionStorage.setItem("sarcasmos.lastHomeSubtitle", String(nextIndex));
  } catch (error) {
    console.warn("Failed to remember home subtitle.", error);
  }
  homeSubtitleIndex = nextIndex;
}

function setHomeSubtitleText() {
  setText(
    ".console-hero .hero-copy > .subtitle",
    HOME_HERO_SUBTITLES[currentLanguage][homeSubtitleIndex] || HOME_HERO_SUBTITLES[currentLanguage][0],
  );
}

function rotateHomeSubtitle() {
  chooseHomeSubtitleIndex();
  setHomeSubtitleText();
}

const HOME_TRANSLATIONS = {
  en: {
    documentTitle: "SarcasmOS Home",
    navKicker: "Home",
    translate: "Traducir",
    admin: "Admin",
    signOut: "Sign out",
    eyebrow: "SarcasmOS",
    heroTitle: "Meet Bender at home.",
    heroSubtitle: "",
    startVoice: "Start voice chat",
    openFace: "Open face view",
    voice: "Voice",
    text: "Text",
    calendar: "Calendar",
    ready: "Ready",
    optional: "Optional",
    robotStatus: "Robot status",
    statusIdle: "Click refresh to check.",
    refreshStatus: "Refresh status",
    liveFace: "Live face",
    faceSubtitle: "Big screen mode for Bender.",
    voiceChat: "Voice chat",
    voiceSubtitle: "Conversation mode with memory.",
    openVoiceChat: "Open voice chat",
    audioReply: "Audio reply",
    audioReplyHelp: "Generate Bender voice audio after each answer.",
    googleTools: "Google Tools",
    googleToolsHelp: "Connect read-only Google tools Bender can use when answering.",
    refresh: "Refresh",
    connectCalendar: "Connect Calendar",
    reconnectCalendar: "Reconnect Calendar",
    disconnect: "Disconnect",
    notConnected: "Not connected.",
    enableCalendar: "Enable Calendar API",
    talkEyebrow: "Talk to Bender",
    nextMessage: "Send the next message",
    audioInput: "Audio Input",
    voicePill: "Voice",
    audioHelp: "Record directly or send an existing audio file.",
    chooseAudio: "Choose audio file",
    sendUpload: "Send upload",
    record: "Record",
    stop: "Stop",
    idle: "Idle",
    textInput: "Text Input",
    textPill: "Text",
    textHelp: "Fast path for prompts, tests, and follow-up questions.",
    textPlaceholder: "Type a message for Bender",
    sendText: "Send text",
    send: "Send",
    result: "Result",
    output: "Output",
    transcript: "Transcript",
    answer: "Answer",
    waitingAudio: "Waiting for audio...",
    waitingResponse: "Waiting for response...",
    backHome: "Back to home",
    adminConsoleTitle: "Admin Console",
    adminConsoleSubtitle: "Manage users, inspect chat history, and check API health.",
    sarcasmosHome: "SarcasmOS Home",
    adminPanelTitle: "Admin Panel",
    adminPanelHelp: "Authorize users and manage admin access.",
    refreshUsers: "Refresh users",
    apiHealthTitle: "API Health",
    apiHealthHelp: "Endpoint checks for the backend services used by this console.",
    checkApis: "Check APIs",
    chatHistoryTitle: "Chat History",
    chatHistoryHelp: "Recent messages saved by the backend.",
    clearHistory: "Clear history",
    loadingUsers: "Loading users...",
    noUsers: "No users have signed in yet.",
    authorized: "Authorized",
    adminRole: "Admin",
    chats: "Chats",
    resetFiveChats: "Reset credits",
    loadingChatSummary: "Loading chat summary...",
    chatSingular: "chat",
    chatPlural: "chats",
    messageSingular: "message",
    messagePlural: "messages",
    newChat: "New chat",
    delete: "Delete",
    user: "User",
    none: "None",
    lastQuestion: "Last question",
    lastAnswer: "Last answer",
    noChatsYet: "No chats yet.",
    you: "You",
    emptyTranscript: "(empty transcript)",
    emptyAnswer: "(empty answer)",
    audioReady: "Audio ready (click play).",
    textAnswerReady: "Text answer ready.",
    thinking: "Thinking...",
    sendingAudio: "Sending audio...",
    sendingText: "Sending text...",
    recording: "Recording...",
    processingRecording: "Processing recording...",
    chooseAudioFirst: "Choose an audio file first.",
    typeMessageFirst: "Type a message first.",
    microphoneDenied: "Microphone permission denied or unavailable.",
    failedStatus: "Failed to load status",
    pendingAuthorization: "Your Google account is signed in, but an admin must authorize access.",
    googleClientMissing: "Set GOOGLE_CLIENT_ID in backend/.env to enable Google sign-in.",
    sessionExpired: "Session expired. Sign in again.",
    apiDescBackend: "Main backend reachability",
    apiDescRobot: "Bender runtime status",
    apiDescHistory: "Saved conversations",
    apiDescSchema: "Backend route registry",
    apiDescConfig: "Google Sign-In configuration",
    apiDescServices: "STT, LLM, and TTS configuration",
    checking: "Checking...",
    notChecked: "Not checked yet",
    respondingIn: "Responding in",
    failedFetch: "Failed to fetch",
    configured: "Configured",
    noBaseUrl: "No base URL shown",
    serviceNotConfigured: "Service is not configured correctly",
    developerMode: "Developer Mode",
    developer: "Developer",
    editProfile: "Edit profile",
    developerModeHelp: "Use your own API keys so your actions do not spend the shared weekly credits.",
    requestAccess: "Request access",
    requestedAccess: "Access requested",
    developerApproved: "Approved. Add at least one API key to use your own budget.",
    developerReady: "Active. Your chats use your API keys and do not consume shared weekly credits.",
    developerNotRequested: "Not requested.",
    developerWaiting: "Waiting for admin approval.",
    saveDeveloperApis: "Save developer APIs",
    resetDeveloperApis: "Reset to factory APIs",
    resetDeveloperApisConfirm: "Reset your developer API keys and return to the shared weekly limit?",
    developerModeLabel: "Developer",
    developerRequestNotice: "developer mode request pending",
    developerRequestsNotice: "developer mode requests pending",
    approveDeveloper: "Approve developer mode",
    aiCredits: "AI credits",
    aiCreditsLoading: "Loading...",
    aiCreditsUnlimited: "Unlimited credits",
    aiCreditsReady: "{remaining} credits available",
    adminCreditsLine: "{remaining} credits available",
    aiCreditsDetail: "Credits renew every week.",
    aiCreditsLastCost: "Last use: {cost} credits.",
    addCredits: "+",
    removeCredits: "-",
    addCreditsLabel: "Add credits",
    removeCreditsLabel: "Remove credits",
    creditAmountPlaceholder: "+ credits",
    supportTitle: "Support chat",
    supportLauncher: "Support",
    supportClose: "Close support",
    supportHelp: "Ask about SarcasmOS. If the assistant cannot solve it, it sends the request to support.",
    supportPlaceholder: "Ask for help",
    supportSend: "Ask",
    supportNewChat: "New chat",
    supportWelcome: "Hi. Ask me about login, credits, audio, developer mode, Google tools, or sharing the page.",
    supportSent: "I sent this to support. A human can review it from the inbox.",
    supportSaved: "I saved this request for support. Email sending needs SMTP in backend/.env.",
    supportFailed: "Support request failed. Try again later.",
    supportEmpty: "Write a question first.",
    supportBotName: "Support",
    supportYouName: "You",
    adminSupportTitle: "Support requests",
    adminSupportHelp: "Questions escalated from the support chat.",
    refreshSupport: "Refresh support",
    loadingSupport: "Loading support requests...",
    noSupportRequests: "No support requests yet.",
    supportEmailSent: "Email sent",
    supportEmailSaved: "Saved, email not sent",
    profileTitle: "Before Bender judges you",
    profileHelp: "Tell the bot how to treat you. Optional, but useful.",
    profileName: "What should Bender call you?",
    profileAge: "Age",
    profileGender: "Gender / pronouns",
    profileMorePronouns: "More pronouns you can use...",
    profileExtraPronouns: "More pronouns",
    profilePreferNot: "Prefer not to say",
    profileFemale: "Female / she",
    profileMale: "Male / he",
    profileNonbinary: "Non-binary / they",
    profileCustom: "Custom",
    profileCustomLabel: "Custom gender / pronouns",
    profileSkip: "Skip",
    profileSave: "Save profile",
    konamiEyebrow: "SarcasmOS classified",
    konamiTitle: "Secret mode",
    konamiSubtitle: "Bender found the music button. Nobody is safe.",
    konamiIdle: "Use the controls to pause, resume, or change the volume.",
    konamiPlaying: "Looping. You can pause it or change the volume.",
    konamiAutoplayBlocked: "Press play to start the song. The browser is being dramatic about autoplay.",
  },
  es: {
    documentTitle: "Inicio de SarcasmOS",
    navKicker: "Inicio",
    translate: "Translate",
    admin: "Admin",
    signOut: "Cerrar sesión",
    eyebrow: "SarcasmOS",
    heroTitle: "Bender, en casa.",
    heroSubtitle: "",
    startVoice: "Iniciar chat de voz",
    openFace: "Abrir cara",
    voice: "Voz",
    text: "Texto",
    calendar: "Calendario",
    ready: "Listo",
    optional: "Opcional",
    robotStatus: "Estado del robot",
    statusIdle: "Pulsa refrescar para comprobar.",
    refreshStatus: "Refrescar estado",
    liveFace: "Cara en directo",
    faceSubtitle: "Modo pantalla grande para Bender.",
    voiceChat: "Chat de voz",
    voiceSubtitle: "Modo conversación con memoria.",
    openVoiceChat: "Abrir chat de voz",
    audioReply: "Respuesta con audio",
    audioReplyHelp: "Genera la voz de Bender después de cada respuesta.",
    googleTools: "Herramientas de Google",
    googleToolsHelp: "Conecta herramientas de solo lectura que Bender puede usar al responder.",
    refresh: "Refrescar",
    connectCalendar: "Conectar calendario",
    reconnectCalendar: "Reconectar calendario",
    disconnect: "Desconectar",
    notConnected: "No conectado.",
    enableCalendar: "Activar API de Calendar",
    talkEyebrow: "Habla con Bender",
    nextMessage: "Envía el siguiente mensaje",
    audioInput: "Entrada de audio",
    voicePill: "Voz",
    audioHelp: "Graba directamente o envía un archivo de audio.",
    chooseAudio: "Elegir audio",
    sendUpload: "Enviar archivo",
    record: "Grabar",
    stop: "Parar",
    idle: "En espera",
    textInput: "Entrada de texto",
    textPill: "Texto",
    textHelp: "La vía rápida para prompts, pruebas y preguntas de seguimiento.",
    textPlaceholder: "Escribe un mensaje para Bender",
    sendText: "Enviar texto",
    send: "Enviar",
    result: "Resultado",
    output: "Salida",
    transcript: "Transcripción",
    answer: "Respuesta",
    waitingAudio: "Esperando audio...",
    waitingResponse: "Esperando respuesta...",
    backHome: "Volver al inicio",
    adminConsoleTitle: "Panel de Admin",
    adminConsoleSubtitle: "Gestiona usuarios, revisa historiales de chat y comprueba la salud de la API.",
    sarcasmosHome: "Inicio de SarcasmOS",
    adminPanelTitle: "Panel de Administración",
    adminPanelHelp: "Autoriza usuarios y gestiona el acceso de administradores.",
    refreshUsers: "Refrescar usuarios",
    apiHealthTitle: "Salud de la API",
    apiHealthHelp: "Comprobaciones de endpoints para los servicios del backend usados por esta consola.",
    checkApis: "Comprobar APIs",
    chatHistoryTitle: "Historial de Chats",
    chatHistoryHelp: "Mensajes recientes guardados por el backend.",
    clearHistory: "Borrar historial",
    loadingUsers: "Cargando usuarios...",
    noUsers: "Todavía no ha iniciado sesión ningún usuario.",
    authorized: "Autorizado",
    adminRole: "Admin",
    chats: "Chats",
    resetFiveChats: "Resetear créditos",
    loadingChatSummary: "Cargando resumen de chats...",
    chatSingular: "chat",
    chatPlural: "chats",
    messageSingular: "mensaje",
    messagePlural: "mensajes",
    newChat: "Chat nuevo",
    delete: "Borrar",
    user: "Usuario",
    none: "Nada",
    lastQuestion: "Última pregunta",
    lastAnswer: "Última respuesta",
    noChatsYet: "Todavía no hay chats.",
    you: "Tú",
    emptyTranscript: "(transcripción vacía)",
    emptyAnswer: "(respuesta vacía)",
    audioReady: "Audio listo (pulsa play).",
    textAnswerReady: "Respuesta de texto lista.",
    thinking: "Pensando...",
    sendingAudio: "Enviando audio...",
    sendingText: "Enviando texto...",
    recording: "Grabando...",
    processingRecording: "Procesando grabación...",
    chooseAudioFirst: "Elige un archivo de audio primero.",
    typeMessageFirst: "Escribe un mensaje primero.",
    microphoneDenied: "Permiso de micrófono denegado o no disponible.",
    failedStatus: "No se pudo cargar el estado",
    pendingAuthorization: "Has iniciado sesión con Google, pero un admin debe autorizar el acceso.",
    googleClientMissing: "Configura GOOGLE_CLIENT_ID en backend/.env para activar Google Sign-In.",
    sessionExpired: "Sesión caducada. Inicia sesión otra vez.",
    apiDescBackend: "Conectividad principal del backend",
    apiDescRobot: "Estado de ejecución de Bender",
    apiDescHistory: "Conversaciones guardadas",
    apiDescSchema: "Registro de rutas del backend",
    apiDescConfig: "Configuración de Google Sign-In",
    apiDescServices: "Configuración de STT, LLM y TTS",
    checking: "Comprobando...",
    notChecked: "Sin comprobar todavía",
    respondingIn: "Responde en",
    failedFetch: "No se pudo consultar",
    configured: "Configurado",
    noBaseUrl: "URL base no mostrada",
    serviceNotConfigured: "El servicio no está configurado correctamente",
    developerMode: "Modo Desarrollador",
    developer: "Desarrollador",
    editProfile: "Editar perfil",
    developerModeHelp: "Usa tus propias API keys para que tus acciones no gasten créditos semanales compartidos.",
    requestAccess: "Solicitar acceso",
    requestedAccess: "Acceso solicitado",
    developerApproved: "Aprobado. Añade al menos una API key para usar tu propio presupuesto.",
    developerReady: "Activo. Tus chats usan tus API keys y no consumen créditos semanales compartidos.",
    developerNotRequested: "No solicitado.",
    developerWaiting: "Esperando aprobación de un admin.",
    saveDeveloperApis: "Guardar APIs de desarrollador",
    resetDeveloperApis: "Volver a APIs de fábrica",
    resetDeveloperApisConfirm: "¿Resetear tus APIs de desarrollador y volver al límite semanal compartido?",
    developerModeLabel: "Desarrollador",
    developerRequestNotice: "solicitud de modo desarrollador pendiente",
    developerRequestsNotice: "solicitudes de modo desarrollador pendientes",
    approveDeveloper: "Aprobar modo desarrollador",
    aiCredits: "Créditos IA",
    aiCreditsLoading: "Cargando...",
    aiCreditsUnlimited: "Créditos ilimitados",
    aiCreditsReady: "{remaining} créditos disponibles",
    adminCreditsLine: "{remaining} créditos disponibles",
    aiCreditsDetail: "Los créditos se renuevan cada semana.",
    aiCreditsLastCost: "Último uso: {cost} créditos.",
    addCredits: "+",
    removeCredits: "-",
    addCreditsLabel: "Añadir créditos",
    removeCreditsLabel: "Quitar créditos",
    creditAmountPlaceholder: "+ créditos",
    supportTitle: "Chat de asistencia",
    supportLauncher: "Ayuda",
    supportClose: "Cerrar asistencia",
    supportHelp: "Pregunta sobre SarcasmOS. Si el asistente no puede resolverlo, enviará la solicitud a soporte.",
    supportPlaceholder: "Pide ayuda",
    supportSend: "Preguntar",
    supportNewChat: "Nuevo chat",
    supportWelcome: "Hola. Pregúntame sobre login, créditos, audio, modo desarrollador, herramientas de Google o compartir la página.",
    supportSent: "He enviado esto a soporte. Un humano podrá revisarlo desde el correo.",
    supportSaved: "He guardado esta solicitud para soporte. Para enviar email hace falta SMTP en backend/.env.",
    supportFailed: "No se pudo enviar la solicitud de soporte. Inténtalo más tarde.",
    supportEmpty: "Escribe una pregunta primero.",
    supportBotName: "Soporte",
    supportYouName: "Tú",
    adminSupportTitle: "Solicitudes de asistencia",
    adminSupportHelp: "Preguntas escaladas desde el chat de asistencia.",
    refreshSupport: "Refrescar soporte",
    loadingSupport: "Cargando solicitudes de asistencia...",
    noSupportRequests: "Todavía no hay solicitudes de asistencia.",
    supportEmailSent: "Email enviado",
    supportEmailSaved: "Guardado, email no enviado",
    profileTitle: "Antes de que Bender te juzgue",
    profileHelp: "Dile al bot cómo tratarte. Es opcional, pero útil.",
    profileName: "¿Cómo quieres que te llame Bender?",
    profileAge: "Edad",
    profileGender: "Género / pronombres",
    profileMorePronouns: "Más pronombres que puedes usar...",
    profileExtraPronouns: "Más pronombres",
    profilePreferNot: "Prefiero no decirlo",
    profileFemale: "Mujer / ella",
    profileMale: "Hombre / él",
    profileNonbinary: "No binario / elle",
    profileCustom: "Personalizado",
    profileCustomLabel: "Género / pronombres personalizados",
    profileSkip: "Saltar",
    profileSave: "Guardar perfil",
    konamiEyebrow: "SarcasmOS clasificado",
    konamiTitle: "Modo secreto",
    konamiSubtitle: "Bender ha encontrado el botón de música. Nadie está a salvo.",
    konamiIdle: "Usa los controles para pausar, reanudar o cambiar el volumen.",
    konamiPlaying: "Reproduciendo en bucle. Puedes pausar o cambiar el volumen.",
    konamiAutoplayBlocked: "Pulsa play para iniciar la canción. El navegador se pone exquisito con el autoplay.",
  },
};

function setText(selector, value) {
  const element = document.querySelector(selector);
  if (element) {
    element.textContent = value;
  }
}

function setAllText(selector, value) {
  for (const element of document.querySelectorAll(selector)) {
    element.textContent = value;
  }
}

function tr(key) {
  return (HOME_TRANSLATIONS[currentLanguage] || HOME_TRANSLATIONS.en)[key] || HOME_TRANSLATIONS.en[key] || key;
}

function plural(count, singularKey, pluralKey) {
  return Number(count) === 1 ? tr(singularKey) : tr(pluralKey);
}

function supportKnowledgeAnswer(question) {
  const text = question.toLowerCase();
  const spanish = currentLanguage === "es";
  const includesAny = (words) => words.some((word) => text.includes(word));
  if (includesAny(["login", "loguin", "iniciar", "sesión", "session", "google"])) {
    return spanish
      ? "Para entrar necesitas iniciar sesión con Google. Si el botón falla, revisa que GOOGLE_CLIENT_ID esté configurado en backend/.env y que tu cuenta esté autorizada por un admin."
      : "To enter, sign in with Google. If the button fails, check GOOGLE_CLIENT_ID in backend/.env and make sure an admin authorized your account.";
  }
  if (includesAny(["crédito", "credito", "credit", "saldo", "limite", "límite"])) {
    return spanish
      ? "Los usuarios autorizados tienen créditos semanales. El texto suele costar menos, el audio y respuestas largas cuestan más. Los admins pueden añadir o quitar créditos desde el panel."
      : "Authorized users have weekly credits. Text usually costs less; audio and long replies cost more. Admins can add or remove credits from the admin panel.";
  }
  if (includesAny(["audio", "voz", "voice", "tts", "grabar", "record"])) {
    return spanish
      ? "Puedes activar o desactivar la respuesta con audio desde el interruptor de Audio. Si lo desactivas, Bender responde antes y gasta menos créditos."
      : "You can toggle audio replies with the Audio switch. Turning it off makes Bender answer faster and spend fewer credits.";
  }
  if (includesAny(["developer", "desarrollador", "api", "apis", "keys", "clave"])) {
    return spanish
      ? "El modo desarrollador permite usar tus propias APIs. Pide acceso, un admin lo aprueba y luego puedes configurar Completions, Replicate, fallback, modelo LLM y modelo TTS."
      : "Developer mode lets you use your own APIs. Request access, an admin approves it, then configure Completions, Replicate, fallback, LLM model, and TTS model.";
  }
  if (includesAny(["calendar", "calendario", "google tools", "herramientas"])) {
    return spanish
      ? "Las herramientas de Google se conectan desde Google Tools. Calendar es de solo lectura para que Bender pueda consultar fechas sin modificar tu calendario."
      : "Google tools connect from Google Tools. Calendar is read-only so Bender can check dates without changing your calendar.";
  }
  if (includesAny(["ngrok", "compartir", "share", "link", "enlace"])) {
    return spanish
      ? "Para compartir la web usa el enlace de ngrok. En ngrok gratis puede aparecer una pantalla de aviso; pulsa Visit Site para entrar."
      : "To share the web, use the ngrok link. Free ngrok may show a warning page; press Visit Site to continue.";
  }
  if (includesAny(["konami", "rick", "easter", "secreto", "secret"])) {
    return spanish
      ? "El modo secreto se abre con el código Konami usando las flechas: arriba, arriba, abajo, abajo, izquierda, derecha, izquierda, derecha."
      : "Secret mode opens with the Konami code using arrow keys: up, up, down, down, left, right, left, right.";
  }
  return "";
}

function supportEscalationPhrase() {
  const phrases = currentLanguage === "es"
    ? [
        "Tengo demasiados conocimientos para algo tan sencillo. Pregúntale a un humano.",
        "Mi brillante cerebro robótico no tiene datos suficientes para esto. Lo mando a soporte humano.",
        "Esto requiere intervención humana. Trágico, pero estadísticamente inevitable.",
      ]
    : [
        "I know far too much for something this simple. Ask a human.",
        "My magnificent robot brain lacks enough data for this. Sending it to human support.",
        "This requires human intervention. Tragic, but statistically inevitable.",
      ];
  return phrases[Math.floor(Math.random() * phrases.length)];
}

function shouldEscalateSupportAnswer(answer) {
  const normalized = answer.toLowerCase();
  return [
    "no tengo información suficiente",
    "no tengo suficiente información",
    "pregúntale a un humano",
    "soporte humano",
    "human support",
    "ask a human",
    "do not have enough information",
    "don't have enough information",
  ].some((needle) => normalized.includes(needle));
}

function appendSupportMessage(role, text) {
  if (!supportMessages) {
    return;
  }
  const message = document.createElement("div");
  message.className = `support-message support-message--${role}`;
  const label = role === "user" ? tr("supportYouName") : tr("supportBotName");
  message.innerHTML = `<span>${escapeHtml(label)}</span><p>${escapeHtml(text)}</p>`;
  supportMessages.appendChild(message);
  supportMessages.scrollTop = supportMessages.scrollHeight;
}

function createSupportChat(messages = []) {
  const now = new Date().toISOString();
  return {
    id: `support-${Date.now()}-${Math.random().toString(16).slice(2)}`,
    title: tr("supportNewChat"),
    createdAt: now,
    updatedAt: now,
    messages: messages.length ? messages : [{ role: "bot", text: tr("supportWelcome"), timestamp: now }],
  };
}

function getSupportChatTitle(chat) {
  const firstUserMessage = (chat.messages || []).find((item) => item.role === "user")?.text || "";
  if (firstUserMessage) {
    return firstUserMessage.length > 34 ? `${firstUserMessage.slice(0, 34)}...` : firstUserMessage;
  }
  return chat.title || tr("supportNewChat");
}

function getActiveSupportChat() {
  if (!supportChats.length) {
    const chat = createSupportChat();
    supportChats = [chat];
    activeSupportChatId = chat.id;
  }
  if (!supportChats.some((chat) => chat.id === activeSupportChatId)) {
    activeSupportChatId = supportChats[0].id;
  }
  const chat = supportChats.find((item) => item.id === activeSupportChatId) || supportChats[0];
  supportConversation = chat.messages || [];
  return chat;
}

function renderSupportChatSelect() {
  if (!supportChatSelect) {
    return;
  }
  supportChatSelect.innerHTML = supportChats.map((chat) => (
    `<option value="${escapeHtml(chat.id)}">${escapeHtml(getSupportChatTitle(chat))}</option>`
  )).join("");
  supportChatSelect.value = activeSupportChatId;
}

function persistSupportConversation() {
  try {
    const chat = getActiveSupportChat();
    chat.messages = supportConversation.slice(-80);
    chat.updatedAt = new Date().toISOString();
    localStorage.setItem(userSupportStorageKey(), JSON.stringify({
      activeSupportChatId,
      chats: supportChats.slice(-30),
    }));
  } catch (error) {
    console.warn("Failed to save support conversation.", error);
  }
}

function renderSupportConversation() {
  if (!supportMessages) {
    return;
  }
  supportMessages.innerHTML = "";
  getActiveSupportChat();
  renderSupportChatSelect();
  for (const item of supportConversation) {
    appendSupportMessage(item.role === "user" ? "user" : "bot", item.text || "");
  }
}

function loadSupportConversation() {
  try {
    const raw = localStorage.getItem(userSupportStorageKey());
    const saved = raw ? JSON.parse(raw) : null;
    if (Array.isArray(saved)) {
      const chat = createSupportChat(saved);
      chat.id = "support-default";
      supportChats = [chat];
      activeSupportChatId = chat.id;
    } else {
      supportChats = Array.isArray(saved?.chats) ? saved.chats : [];
      activeSupportChatId = saved?.activeSupportChatId || "";
    }
  } catch (error) {
    console.warn("Failed to load support conversation.", error);
    supportChats = [];
    activeSupportChatId = "";
  }
  if (!supportChats.length) {
    const chat = createSupportChat();
    supportChats = [chat];
    activeSupportChatId = chat.id;
  }
  getActiveSupportChat();
  renderSupportConversation();
}

function addSupportConversationMessage(role, text) {
  const chat = getActiveSupportChat();
  supportConversation.push({ role, text, timestamp: new Date().toISOString() });
  chat.messages = supportConversation;
  persistSupportConversation();
  renderSupportChatSelect();
  appendSupportMessage(role, text);
}

function createNewSupportChat() {
  const chat = createSupportChat();
  supportChats.unshift(chat);
  activeSupportChatId = chat.id;
  supportConversation = chat.messages;
  if (supportStatus) {
    supportStatus.textContent = "";
  }
  persistSupportConversation();
  renderSupportConversation();
  supportInput?.focus();
}

function setSupportWidgetOpen(open) {
  supportWidgetOpen = Boolean(open);
  renderAuthState();
  if (supportWidgetOpen) {
    renderSupportConversation();
    setTimeout(() => supportInput?.focus(), 60);
  }
}

async function sendSupportTicket(question, answer, needsHuman) {
  const response = await fetch(`${API_BASE}/api/support`, {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({
      question,
      answer,
      needsHuman,
      page: location.pathname || "/",
    }),
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(data.error || "Support request failed.");
  }
  return data.ticket || {};
}

async function askAutonomousSupport(question) {
  const response = await fetch(`${API_BASE}/api/support/answer`, {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({
      question,
      language: currentLanguage,
    }),
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(data.error || "Support AI unavailable.");
  }
  return {
    answer: String(data.answer || "").trim(),
    needsHuman: Boolean(data.needsHuman),
    provider: data.provider || "",
  };
}

async function handleSupportSubmit(event) {
  event.preventDefault();
  const question = supportInput?.value.trim() || "";
  if (!question) {
    if (supportStatus) {
      supportStatus.textContent = tr("supportEmpty");
    }
    return;
  }
  supportInput.value = "";
  addSupportConversationMessage("user", question);
  if (supportStatus) {
    supportStatus.textContent = currentLanguage === "es" ? "Pensando..." : "Thinking...";
  }
  let answer = "";
  let needsHuman = false;
  try {
    const aiAnswer = await askAutonomousSupport(question);
    answer = aiAnswer.answer;
    needsHuman = aiAnswer.needsHuman || shouldEscalateSupportAnswer(answer);
    if (needsHuman && !shouldEscalateSupportAnswer(answer)) {
      answer = supportEscalationPhrase();
    }
  } catch (error) {
    answer = supportKnowledgeAnswer(question);
  }
  if (answer) {
    addSupportConversationMessage("bot", answer);
    if (needsHuman) {
      try {
        const ticket = await sendSupportTicket(question, answer, true);
        if (supportStatus) {
          supportStatus.textContent = ticket.emailSent ? tr("supportSent") : tr("supportSaved");
        }
      } catch (error) {
        if (supportStatus) {
          supportStatus.textContent = tr("supportFailed");
        }
      }
      return;
    }
    if (supportStatus) {
      supportStatus.textContent = "";
    }
    return;
  }
  const fallbackAnswer = supportEscalationPhrase();
  addSupportConversationMessage("bot", fallbackAnswer);
  if (supportStatus) {
    supportStatus.textContent = currentLanguage === "es" ? "Enviando a soporte..." : "Sending to support...";
  }
  try {
    const ticket = await sendSupportTicket(question, fallbackAnswer, true);
    if (supportStatus) {
      supportStatus.textContent = ticket.emailSent ? tr("supportSent") : tr("supportSaved");
    }
  } catch (error) {
    if (supportStatus) {
      supportStatus.textContent = tr("supportFailed");
    }
  }
}

function applyLanguage(language) {
  currentLanguage = HOME_TRANSLATIONS[language] ? language : "en";
  const t = HOME_TRANSLATIONS[currentLanguage];
  document.documentElement.lang = currentLanguage;
  document.title = t.documentTitle;
  setText(".console-nav span", t.navKicker);
  setText("#languageToggle", t.translate);
  setText("#adminLanguageToggle", t.translate);
  setText("#developerLanguageToggle", t.translate);
  setText("#editProfileInfo", t.editProfile);
  setText("#openAdminConsole", t.admin);
  setText("#openDeveloperMode", t.developer);
  setText("#closeDeveloperMode", t.backHome);
  setText("#signOutBtn", t.signOut);
  setText("#loginSignOutBtn", t.signOut);
  setText("#adminSignOutBtn", t.signOut);
  setText("#openSarcasmConsole", t.sarcasmosHome);
  setText(".console-hero .eyebrow", t.eyebrow);
  setText(".console-hero h1", t.heroTitle);
  setHomeSubtitleText();
  setText("#heroVoiceChatBtn", t.startVoice);
  setText("#heroFaceViewBtn", t.openFace);
  setText(".home-metrics div:nth-child(1) strong", t.voice);
  setText(".home-metrics div:nth-child(2) strong", t.text);
  setText(".home-metrics div:nth-child(3) strong", t.calendar);
  setText(".home-metrics div:nth-child(1) span", t.ready);
  setText(".home-metrics div:nth-child(2) span", t.ready);
  setText(".home-metrics div:nth-child(3) span", t.optional);
  setText(".status-card .status-label", t.robotStatus);
  if (statusOutput?.textContent === HOME_TRANSLATIONS.en.statusIdle || statusOutput?.textContent === HOME_TRANSLATIONS.es.statusIdle) {
    statusOutput.textContent = t.statusIdle;
  }
  setText("#statusBtn", t.refreshStatus);
  setText(".feature-strip .face-callout:nth-child(1) .label", t.liveFace);
  setText(".feature-strip .face-callout:nth-child(1) .subtitle", t.faceSubtitle);
  setText(".feature-strip .face-callout:nth-child(2) .label", t.voiceChat);
  setText(".feature-strip .face-callout:nth-child(2) .subtitle", t.voiceSubtitle);
  setText("#openFaceView", t.openFace);
  setText("#openVoiceChatView", t.openVoiceChat);
  setText(".chat-settings-panel .label", t.audioReply);
  setText(".chat-settings-panel .helper-text", t.audioReplyHelp);
  setText("#googleToolsPanel h2", t.googleTools);
  setText("#googleToolsPanel .history-header .helper-text", t.googleToolsHelp);
  setText("#googleToolsRefresh", t.refresh);
  setText("#disconnectGoogleCalendar", t.disconnect);
  setText("#googleCalendarHelp", t.enableCalendar);
  setText("#developerModePanel h2", t.developerMode);
  setText("#developerView .console-nav span", t.developerMode);
  setText("#developerModePanel .history-header .helper-text", t.developerModeHelp);
  setText("#developerModeRequest", developerModeState?.developerRequested ? t.requestedAccess : t.requestAccess);
  setText("#developerModeSave", t.saveDeveloperApis);
  setText("#developerModeReset", t.resetDeveloperApis);
  setText("#profileSetup h2", t.profileTitle);
  setText("#profileSetup .helper-text", t.profileHelp);
  setText("#profileSetupForm label:nth-of-type(1) span", t.profileName);
  setText("#profileSetupForm label:nth-of-type(2) span", t.profileAge);
  setText("#profileSetupForm label:nth-of-type(3) span", t.profileGender);
  setText("#profileMorePronouns", t.profileMorePronouns);
  setText("#profileExtraPronounsWrap span", t.profileExtraPronouns);
  setText("#profileCustomGenderWrap span", t.profileCustomLabel);
  setText("#profileSkip", t.profileSkip);
  setText("#profileSave", t.profileSave);
  renderProfileGenderOptions();
  setText("#supportPanel h2", t.supportTitle);
  setText("#supportPanel .support-popover-head .helper-text", t.supportHelp);
  setText(".support-launcher-text", t.supportLauncher);
  if (supportLauncher) {
    supportLauncher.title = t.supportTitle;
  }
  if (supportClose) {
    supportClose.setAttribute("aria-label", t.supportClose);
  }
  setText("#supportNewChat", t.supportNewChat);
  setText("#supportSend", t.supportSend);
  if (supportInput) {
    supportInput.placeholder = t.supportPlaceholder;
  }
  renderSupportChatSelect();
  if (supportMessages && supportMessages.children.length === 0) {
    renderSupportConversation();
  }
  setText("#konamiEyebrow", t.konamiEyebrow);
  setText("#konamiTitle", t.konamiTitle);
  setText("#konamiSubtitle", t.konamiSubtitle);
  setText("#closeKonamiView", t.backHome);
  if (
    konamiStatus?.textContent === HOME_TRANSLATIONS.en.konamiIdle ||
    konamiStatus?.textContent === HOME_TRANSLATIONS.es.konamiIdle ||
    konamiStatus?.textContent === HOME_TRANSLATIONS.en.konamiPlaying ||
    konamiStatus?.textContent === HOME_TRANSLATIONS.es.konamiPlaying ||
    konamiStatus?.textContent === HOME_TRANSLATIONS.en.konamiAutoplayBlocked ||
    konamiStatus?.textContent === HOME_TRANSLATIONS.es.konamiAutoplayBlocked
  ) {
    konamiStatus.textContent = konamiView?.classList.contains("hidden") ? t.konamiIdle : t.konamiPlaying;
  }
  setText(".credit-meter .label", t.aiCredits);
  renderCreditMeter();
  setText(".section-heading .eyebrow", t.talkEyebrow);
  setText(".section-heading h2", t.nextMessage);
  setText(".console-input-grid .input-panel:nth-child(1) h2", t.audioInput);
  setText(".console-input-grid .input-panel:nth-child(1) .step-pill", t.voicePill);
  setText(".console-input-grid .input-panel:nth-child(1) .helper-text", t.audioHelp);
  setAllText(".file-input span", t.chooseAudio);
  setText("#uploadSend", t.sendUpload);
  setText("#recordBtn", t.record);
  setText("#stopBtn", t.stop);
  setText("#faceUploadSend", t.sendUpload);
  setText("#faceRecordBtn", t.record);
  setText("#faceStopBtn", t.stop);
  setText("#voiceChatRecordBtn", t.record);
  setText("#voiceChatStopBtn", t.stop);
  setText("#voiceChatTextSend", t.send);
  if (recordState?.textContent === HOME_TRANSLATIONS.en.idle || recordState?.textContent === HOME_TRANSLATIONS.es.idle) {
    setRecordingState(t.idle);
  }
  setText(".console-input-grid .input-panel:nth-child(2) h2", t.textInput);
  setText(".console-input-grid .input-panel:nth-child(2) .step-pill", t.textPill);
  setText(".console-input-grid .input-panel:nth-child(2) .helper-text", t.textHelp);
  if (textInput) {
    textInput.placeholder = t.textPlaceholder;
  }
  if (faceTextInput) {
    faceTextInput.placeholder = t.textPlaceholder;
  }
  if (voiceChatTextInput) {
    voiceChatTextInput.placeholder = t.textPlaceholder;
  }
  setText("#textSend", t.sendText);
  setText("#faceTextSend", t.sendText);
  setText("#mainView > .panel:last-of-type h2", t.result);
  setText("#mainView > .panel:last-of-type .step-pill", t.output);
  setText("#mainView > .panel:last-of-type .grid > div:nth-child(1) .label", t.transcript);
  setText("#mainView > .panel:last-of-type .grid > div:nth-child(2) .label", t.answer);
  if (transcriptOutput?.textContent === HOME_TRANSLATIONS.en.waitingAudio || transcriptOutput?.textContent === HOME_TRANSLATIONS.es.waitingAudio) {
    transcriptOutput.textContent = t.waitingAudio;
  }
  if (answerOutput?.textContent === HOME_TRANSLATIONS.en.waitingResponse || answerOutput?.textContent === HOME_TRANSLATIONS.es.waitingResponse) {
    answerOutput.textContent = t.waitingResponse;
  }
  setText("#closeFaceView", t.backHome);
  setText("#closeVoiceChatView", t.backHome);
  setText("#adminView .hero-copy h1", t.adminConsoleTitle);
  setText("#adminView .hero-copy .subtitle", t.adminConsoleSubtitle);
  setText("#adminPanel h2", t.adminPanelTitle);
  setText("#adminPanel .helper-text", t.adminPanelHelp);
  setText("#adminRefresh", t.refreshUsers);
  setText("#adminSupportPanel h2", t.adminSupportTitle);
  setText("#adminSupportPanel .helper-text", t.adminSupportHelp);
  setText("#adminSupportRefresh", t.refreshSupport);
  setText(".api-health-panel h2", t.apiHealthTitle);
  setText(".api-health-panel .helper-text", t.apiHealthHelp);
  setText("#apiHealthRefresh", t.checkApis);
  setText("#adminView > .panel:last-of-type h2", t.chatHistoryTitle);
  setText("#adminView > .panel:last-of-type .helper-text", t.chatHistoryHelp);
  setText("#clearHistory", t.clearHistory);
  renderGoogleToolsStatus(googleToolsState);
  renderDeveloperModeStatus(developerModeState);
  if (lastAdminUsers.length) {
    renderAdminUsers(lastAdminUsers);
  }
  renderAllHistoryViews();
}

function loadLanguagePreference() {
  try {
    const saved = localStorage.getItem(LANGUAGE_STORAGE_KEY);
    applyLanguage(saved || "en");
  } catch (error) {
    console.warn("Failed to load language preference.", error);
    applyLanguage("en");
  }
}

function toggleLanguage() {
  const nextLanguage = currentLanguage === "en" ? "es" : "en";
  try {
    localStorage.setItem(LANGUAGE_STORAGE_KEY, nextLanguage);
  } catch (error) {
    console.warn("Failed to save language preference.", error);
  }
  applyLanguage(nextLanguage);
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
  loadUserProfile();
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
  currentUserProfile = null;
  renderCreditMeter();
  adminConsoleOverride = false;
  developerViewOverride = false;
  supportWidgetOpen = false;
  supportConversation = [];
  supportChats = [];
  activeSupportChatId = "";
  if (supportMessages) {
    supportMessages.innerHTML = "";
  }
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
  const canUseDeveloper = isSignedIn && isAuthorized && developerViewOverride;
  const canUseApp = isSignedIn && isAuthorized && !developerViewOverride && (!isAdmin || !adminConsoleOverride);
  const canUseAdmin = isSignedIn && isAuthorized && isAdmin && adminConsoleOverride && !developerViewOverride;
  renderProfileSetup();
  if (!canUseApp) {
    closeFacePanel();
    closeVoiceChatPanel();
  }
  loginView?.classList.toggle("hidden", canUseApp || canUseAdmin || canUseDeveloper);
  mainView?.classList.toggle("hidden", !canUseApp);
  mainView?.setAttribute("aria-hidden", canUseApp ? "false" : "true");
  developerView?.classList.toggle("hidden", !canUseDeveloper);
  developerView?.setAttribute("aria-hidden", canUseDeveloper ? "false" : "true");
  adminView?.classList.toggle("hidden", !canUseAdmin);
  adminView?.setAttribute("aria-hidden", canUseAdmin ? "false" : "true");
  adminPanel?.classList.toggle("hidden", !canUseAdmin);
  adminSupportPanel?.classList.toggle("hidden", !canUseAdmin);
  supportLauncher?.classList.toggle("hidden", !canUseApp);
  supportPanel?.classList.toggle("hidden", !canUseApp || !supportWidgetOpen);
  supportPanel?.setAttribute("aria-hidden", canUseApp && supportWidgetOpen ? "false" : "true");
  supportLauncher?.setAttribute("aria-expanded", canUseApp && supportWidgetOpen ? "true" : "false");
  googleToolsPanel?.classList.toggle("hidden", !canUseApp);
  developerModePanel?.classList.toggle("hidden", !canUseDeveloper);
  loginSignOutBtn?.classList.toggle("hidden", !isSignedIn);
  googleLoginButton?.classList.toggle("hidden", isSignedIn);
  openAdminConsole?.classList.toggle("hidden", !isAdmin);
  if (canUseApp || canUseDeveloper) {
    startGoogleToolsMonitor();
    loadDeveloperModeStatus();
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
    renderCreditMeter();
    loadSupportConversation();
    await loadHistory();
    renderAllHistoryViews();
    if (!data.user.authorized) {
      showLoginError(tr("pendingAuthorization"));
    }
    if (data.user.isAdmin) {
      loadAdminUsers();
      loadAdminSupportRequests();
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
    showLoginError(tr("googleClientMissing"));
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
  const t = HOME_TRANSLATIONS[currentLanguage] || HOME_TRANSLATIONS.en;
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
      googleCalendarStatus.textContent = t.notConnected;
    }
  }
  if (googleCalendarHelp) {
    googleCalendarHelp.href = calendar.helpUrl || "#";
    googleCalendarHelp.classList.toggle("hidden", !calendar.helpUrl);
  }
  if (connectGoogleCalendar) {
    connectGoogleCalendar.textContent = connected || needsReconnect ? t.reconnectCalendar : t.connectCalendar;
  }
  if (disconnectGoogleCalendar) {
    disconnectGoogleCalendar.disabled = !configured;
  }
}


function renderDeveloperModeStatus(status) {
  developerModeState = status || null;
  if (!developerModeStatus || !developerModeRequest || !developerModeForm) {
    return;
  }
  const approved = Boolean(status?.developerMode);
  const requested = Boolean(status?.developerRequested);
  const ready = Boolean(status?.ready);
  developerModeForm.classList.toggle("hidden", !approved);
  developerModeRequest.disabled = requested || approved;
  developerModeRequest.textContent = requested || approved ? tr("requestedAccess") : tr("requestAccess");
  if (ready) {
    developerModeStatus.textContent = tr("developerReady");
  } else if (approved) {
    developerModeStatus.textContent = tr("developerApproved");
  } else if (requested) {
    developerModeStatus.textContent = tr("developerWaiting");
  } else {
    developerModeStatus.textContent = tr("developerNotRequested");
  }
}


function formatTemplate(template, values) {
  return Object.entries(values).reduce(
    (text, [key, value]) => text.replaceAll(`{${key}}`, String(value)),
    template
  );
}


function renderCreditMeter() {
  const statusTargets = Array.from(document.querySelectorAll(".credit-status"));
  const detailTargets = Array.from(document.querySelectorAll(".credit-detail"));
  const setCredits = (status, detail) => {
    for (const target of statusTargets) {
      target.textContent = status;
    }
    for (const target of detailTargets) {
      target.textContent = detail;
    }
  };
  if (!currentQuota) {
    setCredits(tr("aiCreditsLoading"), tr("aiCreditsDetail"));
    return;
  }
  if (!currentQuota.limited) {
    setCredits(tr("aiCreditsUnlimited"), currentQuota.developerMode ? tr("developerReady") : tr("aiCreditsDetail"));
    return;
  }
  const status = formatTemplate(tr("aiCreditsReady"), {
    remaining: currentQuota.remaining ?? 0,
  });
  setCredits(status, tr("aiCreditsDetail"));
}


function clearDeveloperModeInputs() {
  developerCompletionsUrl.value = "";
  developerCompletionsKey.value = "";
  developerReplicateUrl.value = "";
  developerReplicateKey.value = "";
  developerFallbackUrl.value = "";
  developerFallbackKey.value = "";
  developerLlmModel.value = "";
  developerTtsModel.value = "";
}


async function loadDeveloperModeStatus() {
  if (!currentUser?.token) {
    return;
  }
  try {
    const response = await fetch(`${API_BASE}/api/developer-mode`, { headers: authHeaders() });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to load developer mode.");
    }
    renderDeveloperModeStatus(data);
  } catch (error) {
    console.warn("Failed to load developer mode.", error);
  }
}


async function requestDeveloperMode() {
  try {
    const response = await fetch(`${API_BASE}/api/developer-mode/request`, {
      method: "POST",
      headers: authHeaders(),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to request developer mode.");
    }
    if (data.user) {
      saveUserSession({ ...data.user, token: currentUser.token, sessionExpiresAt: currentUser.sessionExpiresAt || "" });
    }
    await loadDeveloperModeStatus();
  } catch (error) {
    showError(error.message || "Failed to request developer mode.");
  }
}


async function saveDeveloperModeSettings(event) {
  event.preventDefault();
  try {
    const payload = {
      openrouterBaseUrl: developerCompletionsUrl.value.trim(),
      openrouterApiToken: developerCompletionsKey.value.trim(),
      replicateBaseUrl: developerReplicateUrl.value.trim(),
      replicateApiToken: developerReplicateKey.value.trim(),
      pioneerBaseUrl: developerFallbackUrl.value.trim(),
      pioneerApiKey: developerFallbackKey.value.trim(),
      llmModel: developerLlmModel.value.trim(),
      ttsModel: developerTtsModel.value.trim(),
    };
    const response = await fetch(`${API_BASE}/api/developer-mode/settings`, {
      method: "POST",
      headers: { "Content-Type": "application/json", ...authHeaders() },
      body: JSON.stringify(payload),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to save developer APIs.");
    }
    clearDeveloperModeInputs();
    await loadDeveloperModeStatus();
  } catch (error) {
    showError(error.message || "Failed to save developer APIs.");
  }
}

async function resetDeveloperModeSettings() {
  if (!window.confirm(tr("resetDeveloperApisConfirm"))) {
    return;
  }
  developerModeReset.disabled = true;
  try {
    const response = await fetch(`${API_BASE}/api/developer-mode/settings`, {
      method: "DELETE",
      headers: authHeaders(),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to reset developer APIs.");
    }
    clearDeveloperModeInputs();
    renderDeveloperModeStatus({
      ...(developerModeState || {}),
      ready: false,
      settings: data.settings || {},
    });
    await loadDeveloperModeStatus();
  } catch (error) {
    showError(error.message || "Failed to reset developer APIs.");
  } finally {
    developerModeReset.disabled = false;
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
      renderCreditMeter();
      loadSupportConversation();
    } catch (error) {
      currentUser = null;
      localStorage.removeItem(AUTH_STORAGE_KEY);
      showLoginError(tr("sessionExpired"));
    }
  }
  renderAuthState();
  if (currentUser?.authorized) {
    loadSupportConversation();
  }
  renderGoogleButton();
  if (currentUser?.isAdmin) {
    loadAdminUsers();
    loadAdminSupportRequests();
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
    descriptionKey: "apiDescBackend",
  },
  {
    name: "Robot status",
    path: "/api/status",
    method: "GET",
    description: "Bender runtime status",
    descriptionKey: "apiDescRobot",
  },
  {
    name: "Chat history",
    path: "/api/history",
    method: "GET",
    description: "Saved conversations",
    descriptionKey: "apiDescHistory",
  },
  {
    name: "API schema",
    path: "/openapi.json",
    method: "GET",
    description: "Backend route registry",
    descriptionKey: "apiDescSchema",
  },
  {
    name: "Public config",
    path: "/api/config",
    method: "GET",
    description: "Google Sign-In configuration",
    descriptionKey: "apiDescConfig",
  },
  {
    name: "AI services",
    path: "/api/services/status",
    method: "GET",
    description: "STT, LLM, and TTS configuration",
    descriptionKey: "apiDescServices",
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
    detail: isChecking ? tr("checking") : tr("notChecked"),
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
    meta.textContent = `${item.method} ${item.path} - ${item.descriptionKey ? tr(item.descriptionKey) : item.description}`;
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
              ? `${service.detail || tr("configured")} - ${service.base_url || tr("noBaseUrl")}`
              : service.detail || data.error || tr("serviceNotConfigured"),
          });
        }
        continue;
      }
      results.push({
        ...check,
        status: "ok",
        detail: `${tr("respondingIn")} ${elapsed} ms`,
      });
    } catch (error) {
      results.push({
        ...check,
        status: "fail",
        detail: error.message || tr("failedFetch"),
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
  adminUsersList.innerHTML = `<p class="helper-text">${escapeHtml(tr("loadingUsers"))}</p>`;
  try {
    const response = await fetch(`${API_BASE}/api/admin/users`, { headers: authHeaders() });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to load users.");
    }
    lastAdminUsers = data.users || [];
    renderAdminUsers(lastAdminUsers);
  } catch (error) {
    adminUsersList.innerHTML = `<p class="error">${escapeHtml(error.message || "Failed to load users.")}</p>`;
  }
}

function renderAdminSupportTickets(requests = []) {
  if (!adminSupportList) {
    return;
  }
  if (!requests.length) {
    adminSupportList.innerHTML = `<p class="helper-text">${escapeHtml(tr("noSupportRequests"))}</p>`;
    return;
  }
  adminSupportList.innerHTML = requests.map((ticket) => {
    const status = ticket.emailSent ? tr("supportEmailSent") : tr("supportEmailSaved");
    const createdAt = ticket.createdAt ? new Date(ticket.createdAt).toLocaleString() : "";
    return `
      <article class="support-ticket">
        <div class="support-ticket-head">
          <div>
            <strong>${escapeHtml(ticket.userName || ticket.userEmail || tr("user"))}</strong>
            <span>${escapeHtml(ticket.userEmail || "")}</span>
          </div>
          <small>${escapeHtml(status)}</small>
        </div>
        <p class="helper-text">${escapeHtml(createdAt)}</p>
        <p><b>${escapeHtml(tr("lastQuestion"))}:</b> ${escapeHtml(ticket.question || "")}</p>
        <p><b>${escapeHtml(tr("lastAnswer"))}:</b> ${escapeHtml(ticket.answer || "")}</p>
      </article>
    `;
  }).join("");
}

async function loadAdminSupportRequests() {
  if (!adminSupportList || !currentUser?.isAdmin) {
    return;
  }
  adminSupportList.innerHTML = `<p class="helper-text">${escapeHtml(tr("loadingSupport"))}</p>`;
  try {
    const response = await fetch(`${API_BASE}/api/admin/support`, { headers: authHeaders() });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to load support requests.");
    }
    renderAdminSupportTickets(Array.isArray(data.requests) ? data.requests : []);
  } catch (error) {
    adminSupportList.innerHTML = `<p class="error">${escapeHtml(error.message)}</p>`;
  }
}

function renderAdminUsers(users) {
  if (!adminUsersList) {
    return;
  }
  const pendingDeveloperRequests = users.filter((user) => user.developerRequested && !user.developerMode).length;
  if (developerRequestsNotice) {
    developerRequestsNotice.classList.toggle("hidden", pendingDeveloperRequests === 0);
    developerRequestsNotice.textContent = pendingDeveloperRequests
      ? `${pendingDeveloperRequests} ${pendingDeveloperRequests === 1 ? tr("developerRequestNotice") : tr("developerRequestsNotice")}`
      : "";
  }
  if (!users.length) {
    adminUsersList.innerHTML = `<p class="helper-text">${escapeHtml(tr("noUsers"))}</p>`;
    return;
  }
  adminUsersList.innerHTML = users
    .sort((a, b) => {
      const requestDelta = Number(Boolean(b.developerRequested && !b.developerMode)) - Number(Boolean(a.developerRequested && !a.developerMode));
      return requestDelta || String(a.email).localeCompare(String(b.email));
    })
    .map((user) => {
      const quota = user.quota || {};
      const creditText = user.isAdmin
        ? `∞ ${tr("aiCredits")}`
        : quota.limited
        ? `${formatTemplate(tr("adminCreditsLine"), {
            remaining: quota.remaining ?? 0,
          })}`
        : tr("aiCreditsUnlimited");
      return `
      <div class="admin-user${user.developerRequested && !user.developerMode ? " developer-requested" : ""}" data-email="${escapeHtml(user.email)}">
        <img src="${escapeHtml(user.picture || "")}" alt="" class="${user.picture ? "" : "hidden"}" />
        <div>
          <p>${escapeHtml(user.name || user.email)}</p>
          <span>${escapeHtml(user.email)}</span>
          ${user.developerRequested && !user.developerMode ? `<span class="developer-request-badge">${escapeHtml(tr("approveDeveloper"))}</span>` : ""}
          <span class="admin-user-credits">${escapeHtml(creditText)}</span>
        </div>
        <div class="admin-user-roles">
          <label>
            <input class="admin-user-authorized" type="checkbox" ${user.authorized ? "checked" : ""} />
            ${escapeHtml(tr("authorized"))}
          </label>
          <label>
            <input class="admin-user-admin" type="checkbox" ${user.isAdmin ? "checked" : ""} />
            ${escapeHtml(tr("adminRole"))}
          </label>
          <label class="developer-admin-toggle" title="${user.developerRequested ? escapeHtml(tr("approveDeveloper")) : ""}">
            <input class="admin-user-developer" type="checkbox" ${user.developerMode ? "checked" : ""} />
            ${escapeHtml(tr("developerModeLabel"))}
          </label>
        </div>
        <button class="admin-user-chats ghost" type="button">${escapeHtml(tr("chats"))}</button>
        <button class="admin-user-reset-quota ghost" type="button">${escapeHtml(tr("resetFiveChats"))}</button>
        <div class="admin-credit-grant">
          <button class="admin-user-remove-credits ghost danger" type="button" title="${escapeHtml(tr("removeCreditsLabel"))}" aria-label="${escapeHtml(tr("removeCreditsLabel"))}">${escapeHtml(tr("removeCredits"))}</button>
          <input class="admin-credit-amount" type="text" inputmode="numeric" pattern="[0-9]*" maxlength="7" placeholder="${escapeHtml(tr("creditAmountPlaceholder"))}" />
          <button class="admin-user-add-credits ghost" type="button" title="${escapeHtml(tr("addCreditsLabel"))}" aria-label="${escapeHtml(tr("addCreditsLabel"))}">${escapeHtml(tr("addCredits"))}</button>
        </div>
      </div>
    `;
    })
    .join("");
  bindAdminUserControls();
}

function bindAdminUserControls() {
  for (const row of adminUsersList.querySelectorAll(".admin-user")) {
    const email = row.dataset.email;
    const authorized = row.querySelector(".admin-user-authorized");
    const isAdmin = row.querySelector(".admin-user-admin");
    const developerMode = row.querySelector(".admin-user-developer");
    const chatsButton = row.querySelector(".admin-user-chats");
    const resetQuotaButton = row.querySelector(".admin-user-reset-quota");
    const creditAmount = row.querySelector(".admin-credit-amount");
    const addCreditsButton = row.querySelector(".admin-user-add-credits");
    const removeCreditsButton = row.querySelector(".admin-user-remove-credits");
    authorized?.addEventListener("change", () => updateAdminUser(email, { authorized: authorized.checked }));
    isAdmin?.addEventListener("change", () => updateAdminUser(email, { isAdmin: isAdmin.checked }));
    developerMode?.addEventListener("change", () => updateAdminUser(email, { developerMode: developerMode.checked }));
    chatsButton?.addEventListener("click", () => loadAdminUserChats(email, row));
    resetQuotaButton?.addEventListener("click", () => resetAdminUserQuota(email, resetQuotaButton));
    addCreditsButton?.addEventListener("click", () => addAdminUserCredits(email, creditAmount, addCreditsButton));
    removeCreditsButton?.addEventListener("click", () => changeAdminUserCredits(email, creditAmount, removeCreditsButton, "remove"));
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
  panel.innerHTML = `<p class="helper-text">${escapeHtml(tr("loadingChatSummary"))}</p>`;
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
      <p><strong>${data.chatCount || 0}</strong> ${escapeHtml(plural(data.chatCount || 0, "chatSingular", "chatPlural"))}, <strong>${data.messageCount || 0}</strong> ${escapeHtml(plural(data.messageCount || 0, "messageSingular", "messagePlural"))}</p>
      ${chats.length ? chats.map((chat) => `
        <div class="admin-chat-item" data-chat-id="${escapeHtml(chat.id || "")}">
          <div class="admin-chat-head">
            <p>${escapeHtml(chat.title || tr("newChat"))} <span>${escapeHtml(chat.updatedAt || "")}</span></p>
            <button class="admin-chat-delete ghost" type="button">${escapeHtml(tr("delete"))}</button>
          </div>
          <small>${Number(chat.messageCount || 0)} ${escapeHtml(plural(chat.messageCount || 0, "messageSingular", "messagePlural"))}</small>
          ${Array.isArray(chat.items) && chat.items.length ? chat.items.map((item) => `
            <div class="admin-chat-turn" data-item-index="${Number(item.index || 0)}">
              <div class="admin-chat-turn-head">
                <small>${escapeHtml(item.timestamp || "")}</small>
                <button class="admin-chat-turn-delete ghost" type="button">${escapeHtml(tr("delete"))}</button>
              </div>
              <p><strong>${escapeHtml(tr("user"))}:</strong> ${escapeHtml(item.question || tr("none"))}</p>
              <p><strong>Bender:</strong> ${escapeHtml(item.answer || tr("none"))}</p>
            </div>
          `).join("") : `
            <small>${escapeHtml(tr("lastQuestion"))}: ${escapeHtml(chat.lastQuestion || tr("none"))}</small>
            <small>${escapeHtml(tr("lastAnswer"))}: ${escapeHtml(chat.lastAnswer || tr("none"))}</small>
          `}
        </div>
      `).join("") : `<p class="helper-text">${escapeHtml(tr("noChatsYet"))}</p>`}
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

async function addAdminUserCredits(email, input, button) {
  await changeAdminUserCredits(email, input, button, "add");
}

async function changeAdminUserCredits(email, input, button, action) {
  const amount = Number(String(input?.value || "").replace(/[^\d]/g, ""));
  if (!Number.isFinite(amount) || amount <= 0) {
    showError("Credit amount must be greater than 0.");
    return;
  }
  if (button) {
    button.disabled = true;
  }
  try {
    const response = await fetch(`${API_BASE}/api/admin/users/${encodeURIComponent(email)}/credits/${action}`, {
      method: "POST",
      headers: { "Content-Type": "application/json", ...authHeaders() },
      body: JSON.stringify({ amount: Math.floor(amount) }),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Failed to update credits.");
    }
    if (input) {
      input.value = "";
    }
    await loadAdminUsers();
  } catch (error) {
    showError(error.message || "Failed to update credits.");
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
  const context = [...chatHistory].reverse().map((entry) => ({
    question: entry.question || "",
    answer: entry.answer || "",
    timestamp: entry.timestamp || "",
  }));
  const profileEntry = userProfileContextEntry();
  return profileEntry ? [profileEntry, ...context] : context;
}

function renderAllHistoryViews() {
  getActiveChat();
  renderHistory();
  renderChatSessions();
}

function queuePendingTextMessage(question) {
  pendingQuestion = question;
  pendingHistoryEntryId = `pending-${Date.now()}-${Math.random().toString(16).slice(2)}`;
  const entry = {
    id: pendingHistoryEntryId,
    question,
    answer: "",
    audioUrl: "",
    timestamp: new Date().toLocaleTimeString(),
    pending: true,
  };
  chatHistory.unshift(entry);
  syncActiveChat();
  renderAllHistoryViews();
  return entry;
}

function clearTextInputs() {
  if (textInput) {
    textInput.value = "";
  }
  if (faceTextInput) {
    faceTextInput.value = "";
  }
  if (voiceChatTextInput) {
    voiceChatTextInput.value = "";
  }
}

function updatePendingTextMessage(data) {
  if (!pendingHistoryEntryId) {
    return false;
  }
  const entry = chatHistory.find((item) => item.id === pendingHistoryEntryId);
  if (!entry) {
    return false;
  }
  entry.answer = data.answer || "";
  entry.audioUrl = data.audio_url ? `${API_BASE}${data.audio_url}` : "";
  entry.pending = false;
  entry.timestamp = new Date().toLocaleTimeString();
  pendingHistoryEntryId = "";
  pendingQuestion = "";
  syncActiveChat();
  renderAllHistoryViews();
  persistHistory();
  return true;
}

function failPendingTextMessage(message) {
  if (!pendingHistoryEntryId) {
    return;
  }
  const entry = chatHistory.find((item) => item.id === pendingHistoryEntryId);
  if (entry) {
    entry.answer = message || "Text request failed";
    entry.pending = false;
    entry.failed = true;
    syncActiveChat();
    renderAllHistoryViews();
  }
  pendingHistoryEntryId = "";
  pendingQuestion = "";
}

function drawKonamiRoundedRect(ctx, x, y, w, h, r) {
  const radius = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + radius, y);
  ctx.arcTo(x + w, y, x + w, y + h, radius);
  ctx.arcTo(x + w, y + h, x, y + h, radius);
  ctx.arcTo(x, y + h, x, y, radius);
  ctx.arcTo(x, y, x + w, y, radius);
  ctx.closePath();
}

function drawKonamiLine(ctx, points, width, color = "#b8c5cb") {
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.lineCap = "round";
  ctx.lineJoin = "round";
  ctx.beginPath();
  ctx.moveTo(points[0][0], points[0][1]);
  for (let index = 1; index < points.length; index += 1) {
    const point = points[index];
    if (point.length === 4) {
      ctx.quadraticCurveTo(point[0], point[1], point[2], point[3]);
    } else {
      ctx.lineTo(point[0], point[1]);
    }
  }
  ctx.stroke();
}

function drawKonamiHand(ctx, x, y, angle = 0) {
  ctx.save();
  ctx.translate(x, y);
  ctx.rotate(angle);
  ctx.fillStyle = "#c4d0d6";
  ctx.strokeStyle = "#05070a";
  ctx.lineWidth = 7;
  ctx.beginPath();
  ctx.ellipse(0, 0, 21, 18, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  drawKonamiLine(ctx, [[-6, -3], [-20, -14]], 5, "#05070a");
  drawKonamiLine(ctx, [[0, -4], [0, -20]], 5, "#05070a");
  drawKonamiLine(ctx, [[7, -2], [20, -12]], 5, "#05070a");
  ctx.restore();
}

function drawKonamiMic(ctx, x, y, angle = 0) {
  ctx.save();
  ctx.translate(x, y);
  ctx.rotate(angle);
  ctx.fillStyle = "#e8f0f8";
  ctx.strokeStyle = "#05070a";
  ctx.lineWidth = 7;
  ctx.beginPath();
  ctx.ellipse(0, 0, 16, 20, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  drawKonamiLine(ctx, [[8, 15], [25, 42]], 11, "#05070a");
  drawKonamiLine(ctx, [[8, 15], [25, 42]], 5, "#e8f0f8");
  ctx.restore();
}

function drawKonamiFace(ctx, x, y, tilt, look, mouthOpen) {
  ctx.save();
  ctx.translate(x, y);
  ctx.rotate(tilt);
  ctx.shadowColor = "rgba(0, 0, 0, 0.38)";
  ctx.shadowBlur = 14;
  ctx.shadowOffsetY = 10;
  drawKonamiRoundedRect(ctx, -160, -66, 320, 132, 66);
  ctx.fillStyle = "#e8f0f8";
  ctx.strokeStyle = "#05070a";
  ctx.lineWidth = 10;
  ctx.fill();
  ctx.stroke();
  ctx.shadowColor = "transparent";
  drawKonamiRoundedRect(ctx, -144, -51, 288, 102, 51);
  ctx.fillStyle = "#05070a";
  ctx.fill();
  ctx.fillStyle = "#f8fbff";
  ctx.strokeStyle = "#05070a";
  ctx.lineWidth = 6;
  ctx.beginPath();
  ctx.ellipse(-64, -2, 58, 39, -0.05, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.beginPath();
  ctx.ellipse(66, -2, 58, 39, 0.05, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.save();
  ctx.translate(-64 + look * 8, -4);
  ctx.rotate(0.15);
  ctx.fillStyle = "#05070a";
  ctx.fillRect(-8, -8, 16, 16);
  ctx.restore();
  ctx.save();
  ctx.translate(66 + look * 8, -4);
  ctx.rotate(-0.12);
  ctx.fillStyle = "#05070a";
  ctx.fillRect(-8, -8, 16, 16);
  ctx.restore();
  ctx.translate(0, 118);
  drawKonamiRoundedRect(ctx, -82, -38, 164, mouthOpen ? 82 : 70, 34);
  ctx.fillStyle = "#f8fbff";
  ctx.strokeStyle = "#05070a";
  ctx.lineWidth = 8;
  ctx.fill();
  ctx.stroke();
  drawKonamiLine(ctx, [[-74, -12], [74, -12]], 5, "#05070a");
  drawKonamiLine(ctx, [[-74, 14], [74, 14]], 5, "#05070a");
  [-42, 0, 42].forEach((lineX) => drawKonamiLine(ctx, [[lineX, -37], [lineX, 39]], 5, "#05070a"));
  ctx.restore();
}

function drawKonamiBody(ctx, x, y, lean, bounce) {
  ctx.save();
  ctx.translate(x, y + bounce);
  ctx.rotate(lean);
  ctx.shadowColor = "rgba(0, 0, 0, 0.28)";
  ctx.shadowBlur = 12;
  ctx.shadowOffsetY = 8;
  drawKonamiRoundedRect(ctx, -96, -108, 192, 226, 58);
  ctx.fillStyle = "#9faab0";
  ctx.strokeStyle = "#05070a";
  ctx.lineWidth = 10;
  ctx.fill();
  ctx.stroke();
  ctx.shadowColor = "transparent";
  ctx.fillStyle = "#e8f0f8";
  ctx.strokeStyle = "#05070a";
  ctx.lineWidth = 5;
  ctx.beginPath();
  ctx.moveTo(-46, -90);
  ctx.lineTo(-12, 112);
  ctx.lineTo(12, 112);
  ctx.lineTo(46, -90);
  ctx.closePath();
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = "#10151d";
  ctx.beginPath();
  ctx.moveTo(0, -78);
  ctx.lineTo(-18, -18);
  ctx.lineTo(0, 38);
  ctx.lineTo(18, -18);
  ctx.closePath();
  ctx.fill();
  ctx.stroke();
  drawKonamiLine(ctx, [[-55, -34], [54, -24]], 5, "rgba(5, 7, 10, 0.48)");
  drawKonamiLine(ctx, [[-58, 22], [58, 34]], 5, "rgba(5, 7, 10, 0.48)");
  drawKonamiLine(ctx, [[-88, -88], [-114, -6, -72, 116], [-42, 32, -44, -76]], 8, "#05070a");
  drawKonamiLine(ctx, [[88, -88], [114, -6, 72, 116], [42, 32, 44, -76]], 8, "#05070a");
  ctx.restore();
}

function easeKonamiDance(value) {
  return value < 0.5 ? 4 * value * value * value : 1 - ((-2 * value + 2) ** 3) / 2;
}

function lerpKonamiValue(a, b, amount) {
  return a + (b - a) * amount;
}

function lerpKonamiArray(a, b, amount) {
  return a.map((value, index) => Array.isArray(value)
    ? lerpKonamiArray(value, b[index], amount)
    : lerpKonamiValue(value, b[index], amount));
}

function lerpKonamiPose(a, b, amount) {
  return {
    x: lerpKonamiValue(a.x, b.x, amount),
    lean: lerpKonamiValue(a.lean, b.lean, amount),
    head: lerpKonamiValue(a.head, b.head, amount),
    look: lerpKonamiValue(a.look, b.look, amount),
    bounce: lerpKonamiValue(a.bounce, b.bounce, amount),
    mouth: amount < 0.5 ? a.mouth : b.mouth,
    lHand: lerpKonamiArray(a.lHand, b.lHand, amount),
    rHand: lerpKonamiArray(a.rHand, b.rHand, amount),
    mic: lerpKonamiArray(a.mic, b.mic, amount),
    lArm: lerpKonamiArray(a.lArm, b.lArm, amount),
    rArm: lerpKonamiArray(a.rArm, b.rArm, amount),
    lLeg: lerpKonamiArray(a.lLeg, b.lLeg, amount),
    rLeg: lerpKonamiArray(a.rLeg, b.rLeg, amount),
  };
}

function drawKonamiDancerFrame(ctx, framePosition, timeSeconds) {
  const poses = [
    { x: -24, lean: -0.08, head: 0.1, look: -1, bounce: 0, mouth: 0, lHand: [100, 284, 0.55], rHand: [442, 142, -0.75], mic: [468, 104, -0.72], lArm: [[176, 260], [138, 276, 100, 284]], rArm: [[344, 258], [392, 212, 442, 142]], lLeg: [[226, 402], [198, 444, 158, 486], [112, 506]], rLeg: [[302, 402], [332, 438, 388, 476], [452, 486]] },
    { x: -8, lean: -0.02, head: 0.03, look: -0.3, bounce: -20, mouth: 1, lHand: [130, 238, 0.08], rHand: [428, 184, -0.32], mic: [462, 154, -0.35], lArm: [[176, 254], [150, 244, 130, 238]], rArm: [[344, 254], [384, 226, 428, 184]], lLeg: [[226, 402], [206, 432, 196, 468], [166, 500]], rLeg: [[302, 402], [326, 428, 350, 464], [400, 498]] },
    { x: 18, lean: 0.08, head: -0.09, look: 1, bounce: -2, mouth: 0, lHand: [92, 204, -0.18], rHand: [416, 256, 0.25], mic: [450, 232, 0.12], lArm: [[176, 260], [132, 230, 92, 204]], rArm: [[344, 260], [384, 264, 416, 256]], lLeg: [[226, 402], [226, 440, 214, 478], [174, 506]], rLeg: [[302, 402], [320, 448, 350, 482], [414, 506]] },
    { x: 28, lean: 0.05, head: -0.05, look: 0.7, bounce: -14, mouth: 1, lHand: [112, 292, 0.5], rHand: [458, 132, -0.9], mic: [480, 92, -0.84], lArm: [[176, 258], [140, 278, 112, 292]], rArm: [[344, 256], [400, 208, 458, 132]], lLeg: [[226, 402], [204, 452, 152, 482], [112, 488]], rLeg: [[302, 402], [330, 432, 368, 466], [430, 492]] },
    { x: 20, lean: 0.02, head: 0.03, look: 0, bounce: 0, mouth: 0, lHand: [96, 252, 0.22], rHand: [442, 202, -0.1], mic: [474, 174, -0.2], lArm: [[176, 258], [136, 258, 96, 252]], rArm: [[344, 258], [390, 234, 442, 202]], lLeg: [[226, 402], [214, 440, 196, 480], [146, 512]], rLeg: [[302, 402], [336, 430, 390, 452], [456, 466]] },
    { x: 0, lean: -0.02, head: 0.02, look: -0.2, bounce: -22, mouth: 1, lHand: [144, 210, -0.08], rHand: [414, 244, 0.25], mic: [448, 218, 0.12], lArm: [[176, 254], [156, 224, 144, 210]], rArm: [[344, 258], [384, 254, 414, 244]], lLeg: [[226, 402], [212, 426, 228, 462], [188, 498]], rLeg: [[302, 402], [314, 428, 300, 462], [342, 500]] },
    { x: -28, lean: -0.09, head: 0.1, look: -1, bounce: -1, mouth: 0, lHand: [78, 142, -0.55], rHand: [428, 288, 0.55], mic: [462, 264, 0.38], lArm: [[176, 260], [124, 210, 78, 142]], rArm: [[344, 260], [386, 276, 428, 288]], lLeg: [[226, 402], [192, 434, 150, 458], [104, 470]], rLeg: [[302, 402], [334, 444, 376, 480], [438, 504]] },
    { x: -12, lean: -0.02, head: 0.01, look: -0.4, bounce: -14, mouth: 1, lHand: [112, 286, 0.45], rHand: [438, 142, -0.78], mic: [468, 104, -0.72], lArm: [[176, 258], [142, 276, 112, 286]], rArm: [[344, 256], [398, 202, 438, 142]], lLeg: [[226, 402], [208, 448, 176, 486], [124, 506]], rLeg: [[302, 402], [322, 436, 352, 470], [404, 502]] },
  ];
  const baseFrame = Math.floor(framePosition) % poses.length;
  const nextFrame = (baseFrame + 1) % poses.length;
  const pose = lerpKonamiPose(poses[baseFrame], poses[nextFrame], easeKonamiDance(framePosition % 1));
  const originX = 260 + pose.x;
  const originY = 326;
  ctx.clearRect(0, 0, 520, 560);
  ctx.fillStyle = "rgba(0, 0, 0, 0.34)";
  ctx.beginPath();
  ctx.ellipse(260 + pose.x * 0.35, 508, 142 - Math.abs(pose.bounce) * 2.2, 22, 0, 0, Math.PI * 2);
  ctx.fill();
  const sparklePulse = Math.sin(timeSeconds * 7) * 0.5 + 0.5;
  ctx.fillStyle = `rgba(246, 196, 69, ${0.48 + sparklePulse * 0.34})`;
  [[82, 300, 26], [438, 332, 22], [424, 214, 6], [112, 208, 5]].forEach(([x, y, size]) => {
    ctx.beginPath();
    ctx.moveTo(x, y - size);
    ctx.lineTo(x + size * 0.34, y - size * 0.34);
    ctx.lineTo(x + size, y);
    ctx.lineTo(x + size * 0.34, y + size * 0.34);
    ctx.lineTo(x, y + size);
    ctx.lineTo(x - size * 0.34, y + size * 0.34);
    ctx.lineTo(x - size, y);
    ctx.lineTo(x - size * 0.34, y - size * 0.34);
    ctx.closePath();
    ctx.fill();
  });
  drawKonamiLine(ctx, pose.lLeg, 18);
  drawKonamiLine(ctx, [pose.lLeg[2], [pose.lLeg[2][0] + 48, pose.lLeg[2][1] + 8]], 14);
  drawKonamiLine(ctx, pose.rLeg, 18);
  drawKonamiLine(ctx, [pose.rLeg[2], [pose.rLeg[2][0] + 52, pose.rLeg[2][1] - 6]], 14);
  drawKonamiLine(ctx, pose.lArm, 18);
  drawKonamiHand(ctx, pose.lHand[0], pose.lHand[1], pose.lHand[2]);
  drawKonamiLine(ctx, pose.rArm, 18);
  drawKonamiHand(ctx, pose.rHand[0], pose.rHand[1], pose.rHand[2]);
  drawKonamiMic(ctx, pose.mic[0], pose.mic[1], pose.mic[2]);
  drawKonamiBody(ctx, originX, originY, pose.lean, pose.bounce);
  drawKonamiFace(ctx, originX, 122 + pose.bounce * 0.45, pose.head, pose.look, pose.mouth);
}

function tickKonamiDance() {
  if (!konamiDanceCanvas || !konamiView || konamiView.classList.contains("hidden")) {
    konamiDanceAnimation = 0;
    return;
  }
  const ctx = konamiDanceCanvas.getContext("2d");
  const currentTime = konamiAudioPlayer?.currentTime || performance.now() / 1000;
  drawKonamiDancerFrame(ctx, (currentTime / 0.42) % 8, currentTime);
  konamiDanceAnimation = requestAnimationFrame(tickKonamiDance);
}

function startKonamiDance() {
  if (konamiDanceAnimation || !konamiDanceCanvas) {
    return;
  }
  tickKonamiDance();
}

function stopKonamiDance() {
  if (konamiDanceAnimation) {
    cancelAnimationFrame(konamiDanceAnimation);
    konamiDanceAnimation = 0;
  }
}

function openKonamiView() {
  if (!konamiView) {
    return;
  }
  closeFacePanel();
  closeVoiceChatPanel();
  pauseAllAudio();
  konamiView.classList.remove("hidden");
  konamiView.setAttribute("aria-hidden", "false");
  document.body.classList.add("konami-open");
  startKonamiDance();
  if (konamiAudioPlayer) {
    konamiAudioPlayer.loop = true;
    konamiAudioPlayer.src = KONAMI_AUDIO_SRC;
    konamiAudioPlayer.load();
    konamiAudioPlayer.play().then(() => {
      if (konamiStatus) {
        konamiStatus.textContent = tr("konamiPlaying");
      }
    }).catch(() => {
      if (konamiStatus) {
        konamiStatus.textContent = tr("konamiAutoplayBlocked");
      }
    });
  }
}

function closeKonamiPanel() {
  if (!konamiView) {
    return;
  }
  if (konamiAudioPlayer) {
    konamiAudioPlayer.pause();
    konamiAudioPlayer.currentTime = 0;
  }
  stopKonamiDance();
  konamiView.classList.add("hidden");
  konamiView.setAttribute("aria-hidden", "true");
  document.body.classList.remove("konami-open");
}

function handleKonamiKey(event) {
  if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) {
    return;
  }
  const expected = KONAMI_CODE[konamiIndex];
  if (event.key === expected) {
    konamiIndex += 1;
    if (konamiIndex === KONAMI_CODE.length) {
      konamiIndex = 0;
      event.preventDefault();
      openKonamiView();
    }
    return;
  }
  konamiIndex = event.key === KONAMI_CODE[0] ? 1 : 0;
}

function submitTextFromInput(input, target) {
  activePlaybackTarget = target;
  const message = input.value.trim();
  if (!message) {
    showError(tr("typeMessageFirst"));
    return;
  }
  const context = getActiveChatContext();
  queuePendingTextMessage(message);
  clearTextInputs();
  sendTextMessage(message, context);
}

function updateResult(data) {
  if (data?.quota) {
    currentQuota = data.quota;
    renderCreditMeter();
  }
  if (data?.credit_notice && aiCreditsDetail) {
    for (const target of document.querySelectorAll(".credit-detail")) {
      target.textContent = data.credit_notice;
    }
  }
  if (data.transcript !== undefined) {
    transcriptOutput.textContent = data.transcript || tr("emptyTranscript");
    faceTranscriptOutput.textContent = transcriptOutput.textContent;
  }
  if (data.answer !== undefined) {
    answerOutput.textContent = data.answer || tr("emptyAnswer");
    faceAnswerOutput.textContent = answerOutput.textContent;
  }
  if (data.answer && updatePendingTextMessage(data)) {
    // The text chat already rendered the user's message immediately.
  } else if (data.answer || data.transcript) {
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
          setRecordingState(tr("audioReady"));
        });
        if (!startMouthSync(voiceAudio)) {
          setSpeaking(true);
        }
      }
    } else if (activePlaybackTarget === "face") {
      pauseAllAudio(faceAudioPlayer);
      faceAudioPlayer.play().catch(() => {
        setRecordingState(tr("audioReady"));
      });
      if (!startMouthSync(faceAudioPlayer)) {
        setSpeaking(true);
      }
    } else {
      pauseAllAudio(audioPlayer);
      audioPlayer.play().catch(() => {
        setRecordingState(tr("audioReady"));
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
    setRecordingState(tr("textAnswerReady"));
  }
}

function renderHistory() {
  const items = chatHistory.map((entry, index) => {
    return `
      <div class="history-item" data-history-index="${index}">
        <div class="history-meta">
          <div class="label">${escapeHtml(entry.timestamp)}</div>
          <button class="history-delete" type="button" aria-label="${escapeHtml(tr("delete"))}">${escapeHtml(tr("delete"))}</button>
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
            <span class="voice-chat-session-title">${escapeHtml(chat.title || tr("newChat"))}</span>
            <span class="voice-chat-session-meta">${count} ${escapeHtml(plural(count, "messageSingular", "messagePlural"))}</span>
          </button>
          <button class="voice-chat-session-delete" type="button" aria-label="${escapeHtml(tr("delete"))}">${escapeHtml(tr("delete"))}</button>
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
      const answer = entry.pending ? tr("waitingResponse") : (entry.answer || "");
      const audio = entry.audioUrl
        ? `<audio class="voice-chat-audio" controls src="${escapeHtml(entry.audioUrl)}"></audio>`
        : "";
      return `
        <div class="voice-chat-message user">
          <div class="label">${escapeHtml(tr("you"))}</div>
          <p>${escapeHtml(question)}</p>
        </div>
        <div class="voice-chat-message assistant${entry.pending ? " pending" : ""}${entry.failed ? " failed" : ""}" data-history-index="${index}">
          <div class="voice-chat-message-top">
            <div class="label">Bender</div>
            <button class="voice-chat-message-delete" type="button">${escapeHtml(tr("delete"))}</button>
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
  if (blob.durationSeconds) {
    formData.append("audioDurationSeconds", String(blob.durationSeconds));
  }
  setLoading(true, tr("sendingAudio"), "audio");
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
    setLoading(false, tr("idle"));
  }
}

async function sendTextMessage(message, context = getActiveChatContext()) {
  await refreshCalendarToolIfNeeded();
  setLoading(true, tr("sendingText"));
  showError("");

  try {
    const response = await fetch(`${API_BASE}/api/chat/text`, {
      method: "POST",
      headers: { "Content-Type": "application/json", ...authHeaders() },
      body: JSON.stringify({
        message,
        context,
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
    failPendingTextMessage(error.message || "Text request failed");
    showError(error.message || "Text request failed");
  } finally {
    setLoading(false, tr("idle"));
  }
}

async function refreshStatus() {
  showError("");
  try {
    const response = await fetch(`${API_BASE}/api/status`);
    const data = await response.json();
    statusOutput.textContent = JSON.stringify(data, null, 2);
  } catch (error) {
    showError(tr("failedStatus"));
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
      blob.durationSeconds = recordingStartedAt ? Math.max(1, (Date.now() - recordingStartedAt) / 1000) : 0;
      recordingStartedAt = 0;
      sendAudioBlob(blob, "recording.webm");
      stream.getTracks().forEach((track) => track.stop());
    });
    mediaRecorder.start();
    recordingStartedAt = Date.now();
    setRecordingState(tr("recording"));
    recordBtn.disabled = true;
    faceRecordBtn.disabled = true;
    voiceChatRecordBtn.disabled = true;
    stopBtn.disabled = false;
    faceStopBtn.disabled = false;
    voiceChatStopBtn.disabled = false;
  } catch (error) {
    showError(tr("microphoneDenied"));
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
    setRecordingState(tr("processingRecording"));
  }
}

uploadSend.addEventListener("click", () => {
  activePlaybackTarget = "main";
  const file = uploadInput.files[0];
  if (!file) {
    showError(tr("chooseAudioFirst"));
    return;
  }
  pendingQuestion = "";
  sendAudioBlob(file, file.name || "upload.wav");
});

faceUploadSend.addEventListener("click", () => {
  activePlaybackTarget = "face";
  const file = faceUploadInput.files[0];
  if (!file) {
    showError(tr("chooseAudioFirst"));
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
  submitTextFromInput(textInput, "main");
});

faceTextSend.addEventListener("click", () => {
  submitTextFromInput(faceTextInput, "face");
});

voiceChatTextSend.addEventListener("click", () => {
  submitTextFromInput(voiceChatTextInput, "voice");
});

for (const [input, target] of [
  [textInput, "main"],
  [faceTextInput, "face"],
  [voiceChatTextInput, "voice"],
]) {
  input?.addEventListener("keydown", (event) => {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      submitTextFromInput(input, target);
    }
  });
}

statusBtn.addEventListener("click", refreshStatus);
apiHealthRefresh.addEventListener("click", checkApiHealth);
signOutBtn.addEventListener("click", clearUserSession);
loginSignOutBtn.addEventListener("click", clearUserSession);
adminSignOutBtn.addEventListener("click", clearUserSession);
languageToggle?.addEventListener("click", toggleLanguage);
editProfileInfo?.addEventListener("click", openProfileSetupEditor);
adminLanguageToggle?.addEventListener("click", toggleLanguage);
developerLanguageToggle?.addEventListener("click", toggleLanguage);
homeBenderButton.addEventListener("click", annoyHomeBender);
openSarcasmConsole.addEventListener("click", () => {
  rotateHomeSubtitle();
  developerViewOverride = false;
  adminConsoleOverride = false;
  renderAuthState();
});
openDeveloperMode?.addEventListener("click", () => {
  adminConsoleOverride = false;
  developerViewOverride = true;
  renderAuthState();
  loadDeveloperModeStatus();
});
closeDeveloperMode?.addEventListener("click", () => {
  developerViewOverride = false;
  renderAuthState();
});
openAdminConsole.addEventListener("click", () => {
  rotateHomeSubtitle();
  developerViewOverride = false;
  adminConsoleOverride = true;
  renderAuthState();
  loadAdminSupportRequests();
});
adminRefresh.addEventListener("click", loadAdminUsers);
adminSupportRefresh?.addEventListener("click", loadAdminSupportRequests);
googleToolsRefresh.addEventListener("click", () => loadGoogleToolsStatus({ check: true }));
connectGoogleCalendar.addEventListener("click", connectCalendarTool);
disconnectGoogleCalendar.addEventListener("click", disconnectCalendarTool);
developerModeRequest?.addEventListener("click", requestDeveloperMode);
developerModeForm?.addEventListener("submit", saveDeveloperModeSettings);
developerModeReset?.addEventListener("click", resetDeveloperModeSettings);
profileGender?.addEventListener("change", () => {
  profileExtraPronounsWrap?.classList.add("hidden");
  profileMorePronouns?.classList.remove("active");
  syncProfileCustomGenderVisibility();
});
profileMorePronouns?.addEventListener("click", toggleProfileExtraPronouns);
profileSkip?.addEventListener("click", () => saveUserProfile({ skipped: true }));
profileSetupForm?.addEventListener("submit", (event) => {
  event.preventDefault();
  saveUserProfile({
    preferredName: profileDisplayName?.value || "",
    age: profileAge?.value || "",
    gender: selectedProfileGenderValue(),
    customGender: profileCustomGender?.value || "",
    skipped: false,
  });
});

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
  rotateHomeSubtitle();
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
  rotateHomeSubtitle();
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
closeKonamiView?.addEventListener("click", closeKonamiPanel);
supportLauncher?.addEventListener("click", () => setSupportWidgetOpen(!supportWidgetOpen));
supportClose?.addEventListener("click", () => setSupportWidgetOpen(false));
supportNewChat?.addEventListener("click", createNewSupportChat);
supportChatSelect?.addEventListener("change", () => {
  activeSupportChatId = supportChatSelect.value;
  getActiveSupportChat();
  persistSupportConversation();
  renderSupportConversation();
});
supportForm?.addEventListener("submit", handleSupportSubmit);

faceView.addEventListener("click", (event) => {
  if (event.target === faceView && !isBusy) {
    closeFacePanel();
  }
});

document.addEventListener("keydown", (event) => {
  handleKonamiKey(event);
  if (event.key === "Escape" && !faceView.classList.contains("hidden") && !isBusy) {
    closeFacePanel();
  }
  if (event.key === "Escape" && !voiceChatView.classList.contains("hidden") && !isBusy) {
    closeVoiceChatPanel();
  }
  if (event.key === "Escape" && konamiView && !konamiView.classList.contains("hidden")) {
    closeKonamiPanel();
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
  rotateHomeSubtitle();
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
  rotateHomeSubtitle();
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
  chooseHomeSubtitleIndex();
  loadLanguagePreference();
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
