#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Preferences.h>
#include <HTTPClient.h> // Ajouté pour récupérer le JSON sur GitHub

// Configuration de l'écran
#define TFT_CS         15
#define TFT_RST         4                                            
#define TFT_DC          2
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Configuration Wi-Fi et Telegram
const char* ssid = "NOM_WIFI";
const char* password = "MOT_DE_PASSE";
#define BOTtoken "TOKEN_TELEGRAM_ICI"

const char* GITHUB_JSON_URL = "https://raw.githubusercontent.com/jasongauvin/happy-box/main/quotes.json";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

int tempsAttenteBot = 2000; 
unsigned long derniereFoisBotTourne = 0;
unsigned long dernierCheckMinuit = 0;

// Mémoire interne pour sauvegarder le message + heure
Preferences memoire;
String dernierMessageRecu = "";
String heureReception = "";

// Variable pour s'assurer de ne faire le reset de minuit qu'une seule fois par jour
int dernierJourReset = -1;

// ==========================================
//   1. DÉCLARATION DES ÉMOJIS
// ==========================================

const unsigned char epd_bitmap_coeur [] PROGMEM = {
  0x00, 0x00, 0x18, 0x18, 0x3c, 0x3c, 0x7e, 0x7e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x7e, 0x7e, 0x7e, 0x7e, 0x3c, 0x3c, 0x3c, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00
};

const unsigned char epd_bitmap_clin_doeil [] PROGMEM = {
  0x07, 0xe0, 0x18, 0x18, 0x21, 0x84, 0x42, 0x42, 0x82, 0x01, 0x88, 0x11, 0x80, 0x01, 0x80, 0x01,
  0x84, 0x21, 0x83, 0xc1, 0x40, 0x02, 0x40, 0x02, 0x20, 0x04, 0x18, 0x18, 0x07, 0xe0, 0x00, 0x00
};

const unsigned char epd_bitmap_sourire [] PROGMEM = {
  0x07, 0xe0, 0x18, 0x18, 0x20, 0x04, 0x44, 0x22, 0x84, 0x21, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
  0x88, 0x11, 0x84, 0x21, 0x43, 0xc2, 0x40, 0x02, 0x20, 0x04, 0x18, 0x18, 0x07, 0xe0, 0x00, 0x00
};

const unsigned char epd_bitmap_bisou [] PROGMEM = {
  0x07, 0xe0, 0x19, 0x98, 0x26, 0x64, 0x46, 0x62, 0x83, 0xc1, 0x81, 0x81, 0x80, 0x01, 0x80, 0x19,
  0x84, 0x3c, 0x83, 0x3c, 0x40, 0x18, 0x40, 0x00, 0x20, 0x04, 0x18, 0x18, 0x07, 0xe0, 0x00, 0x00
};

const unsigned char epd_bitmap_soleil [] PROGMEM = {
  0x01, 0x80, 0x01, 0x80, 0x90, 0x09, 0x47, 0xe2, 0x2f, 0xf4, 0x1f, 0xf8, 0x7f, 0xfe, 0x7f, 0xfe,
  0x7f, 0xfe, 0x7f, 0xfe, 0x1f, 0xf8, 0x2f, 0xf4, 0x47, 0xe2, 0x90, 0x09, 0x01, 0x80, 0x01, 0x80
};

// ==========================================
//   RÉCUPÉRATION DE MAXIME DEPUIS GITHUB
// ==========================================
String recupererPhraseDepuisGithub() {
  String phraseSelectionnee = "Chaque jour est une nouvelle chance d'être heureux. 😊"; // Phrase de secours (fallback)
  
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure(); // Évite les soucis de certificats SSL expirés ou invalides pour GitHub
    
    Serial.println("Connexion à GitHub pour obtenir les citations...");
    http.begin(httpsClient, GITHUB_JSON_URL);
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();
      
      // On alloue de l'espace pour parser le JSON. 16 Ko suffisent amplement pour ~120 phrases.
      DynamicJsonDocument doc(16384); 
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        JsonArray arr = doc.as<JsonArray>();
        int taille = arr.size();
        if (taille > 0) {
          int indexAleatoire = esp_random() % taille;
          phraseSelectionnee = arr[indexAleatoire].as<String>();
          Serial.print("Citation GitHub récupérée avec succès : ");
          Serial.println(phraseSelectionnee);
        }
      } else {
        Serial.print("Erreur de parsing JSON : ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.print("Erreur HTTP de récupération : Code ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("Impossible de récupérer la citation sur GitHub : WiFi déconnecté.");
  }
  return phraseSelectionnee;
}

// ==========================================
//   DÉCODEUR UTF-8 ACCENTS FRANÇAIS
// ==========================================
String simplifierAccents(String str) {
  String resultat = "";
  for (size_t i = 0; i < str.length(); i++) {
    unsigned char c = str[i];
    if (c == 0xC3) { // Préfixe des accents UTF-8 courants
      if (i + 1 < str.length()) {
        unsigned char c2 = str[++i];
        switch (c2) {
          case 0xA0: resultat += "a"; break; // à
          case 0xA2: resultat += "a"; break; // â
          case 0xA4: resultat += "a"; break; // ä
          case 0xA7: resultat += "c"; break; // ç
          case 0xA8: resultat += "e"; break; // è
          case 0xA9: resultat += "e"; break; // é
          case 0xAA: resultat += "e"; break; // ê
          case 0xAB: resultat += "e"; break; // ë
          case 0xAE: resultat += "i"; break; // î
          case 0xAF: resultat += "i"; break; // ï
          case 0xB4: resultat += "o"; break; // ô
          case 0xB9: resultat += "u"; break; // ù
          case 0xBB: resultat += "u"; break; // û
          case 0xBC: resultat += "u"; break; // ü
          
          case 0x80: resultat += "A"; break; // À
          case 0x82: resultat += "A"; break; // Â
          case 0x87: resultat += "C"; break; // Ç
          case 0x88: resultat += "E"; break; // È
          case 0x89: resultat += "E"; break; // É
          case 0x8A: resultat += "E"; break; // Ê
          case 0x8B: resultat += "E"; break; // Ë
          case 0x8E: resultat += "I"; break; // Î
          case 0x8F: resultat += "I"; break; // Ï
          case 0x94: resultat += "O"; break; // Ô
          case 0x99: resultat += "U"; break; // Ù
          case 0x9B: resultat += "U"; break; // Û
          default:   resultat += "?"; break;
        }
      }
    } else if (c == 0xC2) { // Autres symboles UTF-8 courants
      if (i + 1 < str.length()) {
        unsigned char c2 = str[++i];
        if (c2 == 0xB0) resultat += "o"; // Symbole degré ° -> o
        else resultat += " ";
      }
    } else if (c == 0xE2) { // Ponctuation 3 octets (ex: apostrophe courbe)
      if (i + 2 < str.length()) {
        unsigned char c2 = str[i+1];
        unsigned char c3 = str[i+2];
        i += 2;
        if (c2 == 0x80 && (c3 == 0x99 || c3 == 0x98)) {
          resultat += "'"; // Apostrophe penchée -> droite
        } else {
          resultat += " ";
        }
      }
    } else if (c < 128) {
      resultat += (char)c; // Caractère standard ASCII
    }
  }
  return resultat;
}

// ==========================================
//   FONCTIONS DE MISE EN PAGE DU TEXTE
// ==========================================

// Découpe le texte proprement en lignes (sans couper de mots) dans un tableau de chaînes
int decouperTexteEnLignes(String text, int textSize, int maxWidth, String outputLines[], int maxLines) {
  int charWidth = 6 * textSize;
  int lineCount = 0;
  String currentLine = "";
  int startIndex = 0;
  
  while (startIndex < text.length() && lineCount < maxLines) {
    int nextSpace = text.indexOf(' ', startIndex);
    int nextNewLine = text.indexOf('\n', startIndex);
    int splitIndex = -1;
    bool isNewLine = false;
    
    if (nextSpace == -1 && nextNewLine == -1) {
      splitIndex = -1;
    } else if (nextSpace != -1 && nextNewLine == -1) {
      splitIndex = nextSpace;
    } else if (nextSpace == -1 && nextNewLine != -1) {
      splitIndex = nextNewLine;
      isNewLine = true;
    } else {
      if (nextSpace < nextNewLine) {
        splitIndex = nextSpace;
      } else {
        splitIndex = nextNewLine;
        isNewLine = true;
      }
    }
    
    String word;
    if (splitIndex == -1) {
      word = text.substring(startIndex);
      startIndex = text.length();
    } else {
      word = text.substring(startIndex, splitIndex);
      startIndex = splitIndex + 1;
    }
    
    if (word.length() == 0) {
      if (isNewLine) {
        if (currentLine != "") {
          outputLines[lineCount++] = currentLine;
          currentLine = "";
        } else {
          outputLines[lineCount++] = " "; // Ligne vide
        }
      }
      continue;
    }
    
    // Si un seul mot dépasse la largeur totale, on doit quand même le découper
    if (word.length() * charWidth > maxWidth) {
      if (currentLine != "") {
        outputLines[lineCount++] = currentLine;
        currentLine = "";
      }
      for (size_t i = 0; i < word.length() && lineCount < maxLines; i++) {
        if ((currentLine.length() + 1) * charWidth > maxWidth) {
          outputLines[lineCount++] = currentLine;
          currentLine = "";
        }
        currentLine += word[i];
      }
      if (isNewLine && lineCount < maxLines) {
        outputLines[lineCount++] = currentLine;
        currentLine = "";
      }
      continue;
    }
    
    String testLine = currentLine;
    if (testLine != "") testLine += " ";
    testLine += word;
    
    if (testLine.length() * charWidth > maxWidth) {
      if (lineCount < maxLines) {
        outputLines[lineCount++] = currentLine;
      }
      currentLine = word;
    } else {
      currentLine = testLine;
    }
    
    if (isNewLine && lineCount < maxLines) {
      outputLines[lineCount++] = currentLine;
      currentLine = "";
    }
  }
  
  if (currentLine != "" && lineCount < maxLines) {
    outputLines[lineCount++] = currentLine;
  }
  
  return lineCount;
}

// Détermine la plus grande taille de police (de 3 à 1) qui fait rentrer tout le texte
int calculerMeilleureTaille(String text, int maxWidth, int maxHeight, String outputLines[], int maxLines) {
  for (int size = 3; size >= 1; size--) {
    int linesNeeded = decouperTexteEnLignes(text, size, maxWidth, outputLines, maxLines);
    int totalHeight = linesNeeded * 10 * size - 2 * size; // 10px par hauteur de ligne incluant l'interligne
    if (totalHeight <= maxHeight && linesNeeded <= maxLines) {
      return size; // Ça rentre !
    }
  }
  return 1; // Fallback sécurisé en taille minimale 1
}

// Dessinateur d'émojis centralisé
void dessinerEmoji(int type, int x, int y, int zoom) {
  const unsigned char* bitmap = nullptr;
  uint16_t couleur = ST7735_YELLOW;
  
  if (type == 1) { bitmap = epd_bitmap_coeur; couleur = tft.color565(255, 77, 109); }
  else if (type == 2) { bitmap = epd_bitmap_coeur; couleur = tft.color565(0, 180, 216); }
  else if (type == 3) { bitmap = epd_bitmap_soleil; couleur = tft.color565(255, 158, 0); }
  else if (type == 4) { bitmap = epd_bitmap_bisou; couleur = tft.color565(255, 77, 109); }
  else if (type == 5) { bitmap = epd_bitmap_sourire; couleur = tft.color565(255, 214, 10); }
  else if (type == 6) { bitmap = epd_bitmap_clin_doeil; couleur = tft.color565(255, 214, 10); }
  
  if (bitmap != nullptr) {
    tft.drawBitmap(x, y, bitmap, 16, 16, couleur, zoom);
  }
}

// ==========================================
//   2. AFFICHAGE DU MESSAGE CENTRÉ ET INTELLIGENT
// ==========================================

void afficherMessage(String text, String heure) {
  // Fond et Cadre gris arrondi
  tft.fillScreen(ST7735_BLACK);
  tft.drawRoundRect(4, 4, 152, 120, 6, tft.color565(80, 80, 80)); 
  
  // En-tête (Header) "BOÎTE À MOTS"
  tft.fillRoundRect(6, 6, 148, 22, 4, tft.color565(201, 24, 74)); 
  tft.setCursor(35, 13);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.print("BOITE A MOTS");

  // Détection des émojis
  boolean coeur = (text.indexOf("❤️") != -1 || text.indexOf("<3") != -1 || text.indexOf("💖") != -1);
  boolean coeurBleu = (text.indexOf("💙") != -1);
  boolean soleil = (text.indexOf("☀️") != -1 || text.indexOf("☀") != -1 || text.indexOf("soleil") != -1);
  boolean bisou = (text.indexOf("😘") != -1);
  boolean sourire = (text.indexOf("😊") != -1 || text.indexOf("🙂") != -1);
  boolean clindoeil = (text.indexOf("😉") != -1);

  // Nettoyage des émojis bruts du texte pour l'affichage pur
  text.replace("❤️", ""); text.replace("<3", ""); text.replace("💖", "");
  text.replace("💙", "");
  text.replace("☀️", ""); text.replace("☀", ""); text.replace("soleil", "");
  text.replace("😘", "");
  text.replace("😊", ""); text.replace("🙂", "");
  text.replace("😉", "");
  text.trim();

  // Simplification et remplacement des accents UTF-8 pour l'écran LCD
  String textPropre = simplifierAccents(text);

  // Définir l'émoji actif
  int emojiActif = 0;
  if (coeur) emojiActif = 1;
  else if (coeurBleu) emojiActif = 2;
  else if (soleil) emojiActif = 3;
  else if (bisou) emojiActif = 4;
  else if (sourire) emojiActif = 5;
  else if (clindoeil) emojiActif = 6;

  // Initialisation des variables de mise en page dynamique
  int maxWidth = 140; // 160px d'écran - 10px de marge de chaque côté
  int maxLines = 10;
  String lignes[10];
  int lineCount = 0;
  int tailleChoisie = 2;
  int yTextStart = 32;
  int yTextEnd = 110;
  int zoomEmoji = 3; // Taille par défaut de l'émoji : 3x (48x48)

  if (emojiActif > 0) {
    // Si émoji, espace de texte par défaut de Y=32 à Y=65 (Hauteur max = 33px)
    int maxHeight = 33;
    tailleChoisie = calculerMeilleureTaille(textPropre, maxWidth, maxHeight, lignes, maxLines);
    lineCount = decouperTexteEnLignes(textPropre, tailleChoisie, maxWidth, lignes, maxLines);
    
    // Si la phrase est un peu trop longue et ne tient pas en taille 1, on réduit l'émoji à 2x (32x32)
    int totalHeight = lineCount * 10 * tailleChoisie - 2 * tailleChoisie;
    if (totalHeight > maxHeight && tailleChoisie == 1) {
      zoomEmoji = 2;
      maxHeight = 46; // Nouvel espace de texte de Y=32 à Y=78
      tailleChoisie = calculerMeilleureTaille(textPropre, maxWidth, maxHeight, lignes, maxLines);
      lineCount = decouperTexteEnLignes(textPropre, tailleChoisie, maxWidth, lignes, maxLines);
    }
    yTextEnd = (zoomEmoji == 3) ? 66 : 78;
  } else {
    // Pas d'émoji : plein écran disponible pour le texte, de Y=32 à Y=110 (Hauteur max = 78px)
    int maxHeight = 78;
    tailleChoisie = calculerMeilleureTaille(textPropre, maxWidth, maxHeight, lignes, maxLines);
    lineCount = decouperTexteEnLignes(textPropre, tailleChoisie, maxWidth, lignes, maxLines);
    yTextEnd = 110;
  }

  // Centrage vertical précis du bloc complet de lignes de texte
  int textHeight = lineCount * 10 * tailleChoisie - 2 * tailleChoisie;
  int verticalOffset = (yTextEnd - yTextStart - textHeight) / 2;
  if (verticalOffset < 0) verticalOffset = 0; // Sécurité anti-débordement
  int currentY = yTextStart + verticalOffset;

  // Affichage de chaque ligne centrée horizontalement
  tft.setTextSize(tailleChoisie);
  tft.setTextColor(tft.color565(255, 179, 179)); // Superbe rose pastel

  for (int idx = 0; idx < lineCount; idx++) {
    int lineWidth = lignes[idx].length() * 6 * tailleChoisie - 1 * tailleChoisie;
    int startX = (160 - lineWidth) / 2;
    if (startX < 10) startX = 10; // Sécurité marge
    
    tft.setCursor(startX, currentY);
    tft.print(lignes[idx]);
    currentY += 10 * tailleChoisie;
  }

  // Dessin de l'émoji s'il est présent
  if (emojiActif > 0) {
    int emojiSize = zoomEmoji * 16;
    int emojiX = (160 - emojiSize) / 2;
    int emojiY = (zoomEmoji == 3) ? 68 : 82;
    dessinerEmoji(emojiActif, emojiX, emojiY, zoomEmoji);
  }

  // Affichage de l'heure de réception (En bas à droite, en gris discret)
  if (heure != "") {
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(150, 150, 150)); 
    tft.setCursor(118, 112);
    tft.print(heure);
  }
}

// ==========================================
//   3. RÉCEPTION TELEGRAM
// ==========================================

void gererNouveauxMessages(int numNouveauxMessages) {
  for (int i=0; i<numNouveauxMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    // Récupération de l'heure locale actuelle lors de la réception
    struct tm timeinfo;
    heureReception = "";
    if (getLocalTime(&timeinfo)) {
      char timeStr[10];
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
      heureReception = String(timeStr);
    }

    // On sauvegarde le message et son heure dans la mémoire Flash
    memoire.putString("dernierMsg", text);
    memoire.putString("heureMsg", heureReception);
    dernierMessageRecu = text;
    
    // On met à jour l'écran
    afficherMessage(text, heureReception);

    // Confirmation sur le téléphone
    bot.sendMessage(chat_id, "Le mot doux est affiché ! 💌", "");
  }
}

// ==========================================
//   4. VÉRIFICATION DU NETTOYAGE À MINUIT
// ==========================================

void verifierMinuit() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Si nous ne connaissons pas encore le jour de référence, on l'initialise
    if (dernierJourReset == -1) {
      dernierJourReset = timeinfo.tm_mday;
      return;
    }

    // Si l'horloge passe à 00:00 pile, et que nous n'avons pas encore réinitialisé aujourd'hui
    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
      if (dernierJourReset != timeinfo.tm_mday) {
        dernierJourReset = timeinfo.tm_mday; // On bloque la réinitialisation pour le reste de la journée

        // On va chercher une pensée aléatoire en direct sur GitHub !
        String phraseJour = recupererPhraseDepuisGithub();

        // On écrase l'ancienne note et l'heure dans la mémoire Flash
        memoire.putString("dernierMsg", phraseJour);
        memoire.putString("heureMsg", ""); // Pas d'heure spécifique pour la pensée matinale
        
        dernierMessageRecu = phraseJour;
        heureReception = "";
        
        // On affiche directement la phrase (elle s'auto-stylisera grâce aux détecteurs d'émojis)
        afficherMessage(dernierMessageRecu, heureReception);
        
        Serial.print("C'est minuit ! Affichage de la pensee positive : ");
        Serial.println(dernierMessageRecu);
      }
    }
  }
}

// ==========================================
//   5. SETUP ET CONNEXION ROBUSTE
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(500); 
  
  // Initialisation de l'écran
  tft.initR(INITR_BLACKTAB);      
  tft.setRotation(1); 
  
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(10, 30);
  tft.setTextColor(ST7735_YELLOW);
  tft.println("Connexion WiFi...");

  // 1. Connexion WiFi : Stratégie du double coup
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true); // On purge la mémoire fantôme de l'antenne
  delay(1000);           
  WiFi.setSleep(false);  // On désactive la mise en veille
  WiFi.begin(ssid, password);

  int tentatives = 0;
  while (WiFi.status() != WL_CONNECTED && tentatives < 40) { 
    delay(500);
    Serial.print(".");
    tentatives++;
    
    if (tentatives == 20) {
      Serial.println("\nLa box est lente, on force la reconnexion...");
      WiFi.reconnect();
    }
  }

  // 2. Une fois le réseau stabilisé, on charge la mémoire Flash
  memoire.begin("boite", false);
  
  // On récupère le message et son heure s'ils existent
  if (memoire.isKey("dernierMsg")) {
    dernierMessageRecu = memoire.getString("dernierMsg", ""); 
  } else {
    dernierMessageRecu = "";
  }
  
  if (memoire.isKey("heureMsg")) {
    heureReception = memoire.getString("heureMsg", "");
  } else {
    heureReception = "";
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Synchronisation précise du fuseau horaire français (Paris) avec Daylight Saving Time automatique
    Serial.print("Mise a l'heure locale...");
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    while (now < 24 * 3600) {
      delay(100);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println("\nHeure synchronisee !");

    // Initialisation du jour de réinitialisation lors du démarrage
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      dernierJourReset = timeinfo.tm_mday;
    }

    client.setInsecure(); // Sécurité activée pour Telegram
    
    // Test de connexion direct à Telegram
    Serial.print("Test de connexion aux serveurs Telegram... ");
    if (client.connect("api.telegram.org", 443)) {
      Serial.println("SUCCES ! Le pare-feu autorise Telegram.");
      client.stop(); 
    } else {
      Serial.println("ECHEC ! Impossible de joindre Telegram (TLS ou Pare-feu).");
    }

    // Si on a un message sauvegardé en mémoire, on l'affiche direct !
    if (dernierMessageRecu != "") {
      afficherMessage(dernierMessageRecu, heureReception);
    } 
    // Sinon on récupère la première citation sur GitHub au démarrage !
    else {
      String premiereCitation = recupererPhraseDepuisGithub();
      memoire.putString("dernierMsg", premiereCitation);
      dernierMessageRecu = premiereCitation;
      afficherMessage(dernierMessageRecu, "");
    }
  } else {
    tft.fillScreen(ST7735_BLACK);
    tft.setCursor(10, 30);
    tft.setTextColor(ST7735_RED);
    tft.println("Echec WiFi...");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Tâche 1 : Interroger Telegram toutes les 2 secondes
    if (millis() > derniereFoisBotTourne + tempsAttenteBot)  {
      int numNouveauxMessages = bot.getUpdates(bot.last_message_received + 1);
      
      if (numNouveauxMessages > 0) {
        Serial.print("BINGO ! Message(s) detecte(s) : ");
        Serial.println(numNouveauxMessages);
        
        while(numNouveauxMessages) {
          gererNouveauxMessages(numNouveauxMessages);
          numNouveauxMessages = bot.getUpdates(bot.last_message_received + 1);
        }
      } 
      derniereFoisBotTourne = millis();
    }

    // Tâche 2 : Vérification automatique de l'heure d'effacement (toutes les 10 secondes)
    if (millis() > dernierCheckMinuit + 10000) {
      verifierMinuit();
      dernierCheckMinuit = millis();
    }
  }
}
