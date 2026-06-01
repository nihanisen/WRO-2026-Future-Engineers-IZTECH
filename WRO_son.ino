// ============================================================
//  WRO Future Engineers 2026 — KUSURSUZ OTONOM KODU
//  Donanım: Arduino Uno/Nano, TCS34725 (x2), HC-SR04 (x2),
//           MPU6050, L298N, Servo (Direksiyon), TCA9548A (Mux)
// ============================================================

#include <Wire.h>
#include <Servo.h>
#include <Adafruit_TCS34725.h>
#include <MPU6050_light.h> // Jiroskop için bu hafif ve stabil kütüphaneyi kullanın

// ─── SERVO & MOTOR PİNLERİ ──────────────────────────────────
Servo direksiyon;
const int SERVO_PIN   = 9;
const int SERVO_DUZCE = 90;   // Düz ileri
const int SERVO_SOL   = 60;   // Tam sol dönüş
const int SERVO_SAG   = 120;  // Tam sağ dönüş
const int SERVO_HAFIF_SOL = 75;
const int SERVO_HAFIF_SAG = 105;

const int ENA = 5;
const int IN1 = 6;
const int IN2 = 7;

const int HIZ_NORMAL  = 160;  // 0-255 arası hız değerleri
const int HIZ_YAVAS   = 100;
const int HIZ_DONME   = 130;
const int HIZ_DUR     = 0;

// ─── ULTRASONİK PİNLER (Mekanik Yerleşime Göre) ─────────────
const int TRIG_ON  = 2, ECHO_ON  = 3; // Ön tampona takılacak (Duvar takibi için)
const int TRIG_YAN = 4, ECHO_YAN = 8; // Sağ kapıya takılacak (Paralel park için)

float mesafe_on_cm  = 0;
float mesafe_yan_cm = 0;

const float DUVAR_KRITIK_CM  = 25.0;  // Ön duvar manevra mesafesi
const float PARK_BOSLUK_CM   = 36.0;  // Otopark cebi boşluğu (Araç boyu x 1.5)

// ─── RENK SENSÖRÜ (TCA9548A Çoklayıcı İle) ──────────────────
#define TCAADDR 0x70
// İki sensör de varsayılan adreste (0x29). Çoklayıcı üzerinden kanal seçeceğiz.
Adafruit_TCS34725 renk_sensor = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// Kalibrasyon Değerleri (Laboratuvarda güncellenecek)
uint16_t KIRMIZI_R_MIN = 250, KIRMIZI_G_MAX = 120, KIRMIZI_B_MAX = 120;
uint16_t YESIL_G_MIN   = 200, YESIL_R_MAX   = 130, YESIL_B_MAX   = 150;

enum Renk { RENK_YOK, RENK_KIRMIZI, RENK_YESIL };
Renk renk_sol_sonuc = RENK_YOK;
Renk renk_sag_sonuc = RENK_YOK;
Renk son_gordugun_direk = RENK_YOK; 

// ─── MPU6050 (Jiroskop) ─────────────────────────────────────
MPU6050 mpu(Wire);

// Tur sayma değişkenleri
int tur_sayisi       = 0;
int viraj_sayisi     = 0;    
float viraj_baz_yaw  = 0.0;  
bool viraj_bekleniyor = false;
const float VIRAJ_ESIK = 80.0; // 90° viraj için töleranslı eşik

// ─── OYUN DURUMU (STATE MACHINE) ────────────────────────────
enum OyunModu { MOD_ACIK, MOD_ENGELLI };
OyunModu oyun_modu = MOD_ENGELLI; 

enum DurumMakinesi {
    DURUM_BASLANGIC,
    DURUM_NORMAL_SUR,
    DURUM_VIRAJ_DON,
    DURUM_U_DONUSU,
    DURUM_PARK_ARA,
    DURUM_PARK_YAP,
    DURUM_BITIS
};
DurumMakinesi durum = DURUM_BASLANGIC;

int yon = 1;  // +1 saat yönü, -1 saat yönünün tersi

// Zamanlayıcılar
unsigned long u_donusu_baslangic_zaman = 0;
float u_donus_baslangic_yaw = 0.0;
bool u_donusu_aktif = false;

// ============================================================
//  YARDIMCI FONKSİYONLAR
// ============================================================

void motorIleri(int hiz) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, constrain(hiz, 0, 255));
}

void motorGeri(int hiz) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    analogWrite(ENA, constrain(hiz, 0, 255));
}

void motorDur() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, 0);
}

float mesafeOku(int trig, int echo) {
    digitalWrite(trig, LOW);  delayMicroseconds(2);
    digitalWrite(trig, HIGH); delayMicroseconds(10);
    digitalWrite(trig, LOW);
    long sure = pulseIn(echo, HIGH, 25000); 
    if (sure == 0) return 999.0;
    return sure * 0.0343 / 2.0;
}

// I2C Çoklayıcı Kanal Seçimi (Sol sensör Kanal 0, Sağ Sensör Kanal 1'e takılmalı)
void tcaselect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

Renk renkBelirle(uint16_t r, uint16_t g, uint16_t b) {
    if (r > KIRMIZI_R_MIN && g < KIRMIZI_G_MAX && b < KIRMIZI_B_MAX) return RENK_KIRMIZI;
    if (g > YESIL_G_MIN && r < YESIL_R_MAX && b < YESIL_B_MAX) return RENK_YESIL;
    return RENK_YOK;
}

void renkOku() {
    uint16_t r, g, b, c;

    // SOL Sensörü Oku (TCA Kanal 0)
    tcaselect(0); 
    renk_sensor.getRawData(&r, &g, &b, &c);
    renk_sol_sonuc = renkBelirle(r, g, b);

    // SAĞ Sensörü Oku (TCA Kanal 1)
    tcaselect(1);
    renk_sensor.getRawData(&r, &g, &b, &c);
    renk_sag_sonuc = renkBelirle(r, g, b);

    // Son görülen direk hafızası (U-Dönüşü kapanı için)
    if (renk_sol_sonuc != RENK_YOK) son_gordugun_direk = renk_sol_sonuc;
    if (renk_sag_sonuc != RENK_YOK) son_gordugun_direk = renk_sag_sonuc;
}

// ============================================================
//  MODÜLLER
// ============================================================

void sensorOkuVeYazdir() {
    mesafe_on_cm  = mesafeOku(TRIG_ON, ECHO_ON);
    mesafe_yan_cm = mesafeOku(TRIG_YAN, ECHO_YAN);
    renkOku();
    mpu.update(); // Jiroskop verisini arka planda sürekli günceller

    Serial.print("ON_CM:"); Serial.print(mesafe_on_cm, 1);
    Serial.print(" YAN_CM:"); Serial.print(mesafe_yan_cm, 1);
    Serial.print(" YAW:"); Serial.print(mpu.getAngleZ(), 1);
    Serial.print(" TUR:"); Serial.println(tur_sayisi);
}

bool renkKacisKontrol() {
    if (oyun_modu != MOD_ENGELLI) return false;

    if (renk_sag_sonuc == RENK_KIRMIZI) {
        direksiyon.write(SERVO_SOL); motorIleri(HIZ_NORMAL); return true;
    }
    if (renk_sol_sonuc == RENK_YESIL) {
        direksiyon.write(SERVO_SAG); motorIleri(HIZ_NORMAL); return true;
    }
    if (renk_sag_sonuc == RENK_YESIL) {
        direksiyon.write(SERVO_HAFIF_SAG); motorIleri(HIZ_NORMAL); return true;
    }
    if (renk_sol_sonuc == RENK_KIRMIZI) {
        direksiyon.write(SERVO_HAFIF_SOL); motorIleri(HIZ_NORMAL); return true;
    }
    return false; 
}

void duvarTakibiVeViraj() {
    if (mesafe_on_cm < DUVAR_KRITIK_CM) {
        durum = DURUM_VIRAJ_DON;
        viraj_baz_yaw = mpu.getAngleZ();
        viraj_bekleniyor = true;
        return;
    }
    // Tehlike yoksa düz git
    direksiyon.write(SERVO_DUZCE);
    motorIleri(HIZ_NORMAL);
}

void turSaymaGuncelle() {
    if (!viraj_bekleniyor) return;

    float donulen = abs(mpu.getAngleZ() - viraj_baz_yaw);
    if (donulen >= VIRAJ_ESIK) {
        viraj_sayisi++;
        viraj_bekleniyor = false;

        if (viraj_sayisi % 4 == 0) {
            tur_sayisi++;
        }
        if (durum == DURUM_VIRAJ_DON) {
            durum = DURUM_NORMAL_SUR;
        }
    }
}

void virajDon() {
    if (yon == 1) direksiyon.write(SERVO_SAG);
    else          direksiyon.write(SERVO_SOL);
    
    motorIleri(HIZ_DONME);
    turSaymaGuncelle();
}

bool uDonusuKontrol() {
    if (tur_sayisi == 2 && son_gordugun_direk == RENK_KIRMIZI && !u_donusu_aktif) {
        u_donusu_aktif = true;
        durum = DURUM_U_DONUSU;
        u_donus_baslangic_yaw = mpu.getAngleZ();
        return true;
    }
    return false;
}

void uDonusuYap() {
    // Ackermann U-Dönüşü: Direksiyonu tam kır ve ileri kavis çiz
    float donulen = abs(mpu.getAngleZ() - u_donus_baslangic_yaw);

    if (donulen < 175.0) {
        direksiyon.write(SERVO_SAG); // Pisti ortalayacak şekilde kavis yönü seçilmeli
        motorIleri(HIZ_DONME);
    } else {
        yon = -yon; // Sistemi ters yöne çevir
        u_donusu_aktif = false;
        viraj_sayisi = 0; 
        durum = DURUM_NORMAL_SUR;
    }
}

// Paralel Park Değişkenleri
bool parkBoslukuBulundu = false;
unsigned long park_bosluk_baslangic = 0;
const unsigned long PARK_BOSLUK_SURE_MS = 500; 

void parkAra() {
    motorIleri(HIZ_YAVAS);
    direksiyon.write(SERVO_DUZCE);

    // Yan sensör sağdaki boşluğu tarar
    if (mesafe_yan_cm > PARK_BOSLUK_CM && mesafe_yan_cm < 100) {
        if (!parkBoslukuBulundu) {
            parkBoslukuBulundu = true;
            park_bosluk_baslangic = millis();
        } else if (millis() - park_bosluk_baslangic > PARK_BOSLUK_SURE_MS) {
            durum = DURUM_PARK_YAP;
        }
    } else {
        parkBoslukuBulundu = false;
    }
}

int park_adim = 0;
unsigned long park_adim_zaman = 0;

void parkYap() {
    unsigned long simdi = millis();

    switch (park_adim) {
        case 0: // Boşluğu ortalamak için hafif ileri
            motorIleri(HIZ_YAVAS); direksiyon.write(SERVO_DUZCE);
            if (simdi - park_adim_zaman > 600) { park_adim = 1; park_adim_zaman = simdi; }
            break;
        case 1: // Geri + Sağa Kır
            motorGeri(HIZ_YAVAS); direksiyon.write(SERVO_SAG);
            if (simdi - park_adim_zaman > 800) { park_adim = 2; park_adim_zaman = simdi; }
            break;
        case 2: // Geri + Sola Kır (Toparlama)
            motorGeri(HIZ_YAVAS); direksiyon.write(SERVO_SOL);
            if (simdi - park_adim_zaman > 500) { park_adim = 3; park_adim_zaman = simdi; }
            break;
        case 3: // Düzle ve Dur
            motorIleri(HIZ_YAVAS); direksiyon.write(SERVO_DUZCE);
            if (simdi - park_adim_zaman > 300) { park_adim = 4; park_adim_zaman = simdi; }
            break;
        case 4: // BİTİŞ
            motorDur();
            durum = DURUM_BITIS;
            break;
    }
}

// ============================================================
//  KURULUM VE ANA DÖNGÜ
// ============================================================
void setup() {
    Serial.begin(9600);
    Wire.begin();

    pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
    pinMode(TRIG_ON, OUTPUT); pinMode(ECHO_ON, INPUT);
    pinMode(TRIG_YAN, OUTPUT); pinMode(ECHO_YAN, INPUT);

    direksiyon.attach(SERVO_PIN);
    direksiyon.write(SERVO_DUZCE);

    // MPU6050 Başlatma ve Kalibrasyon (Araba Hareketsiz Olmalı)
    mpu.begin();
    Serial.println("Jiroskop Kalibre Ediliyor... LUTFEN ARACA DOKUNMAYIN!");
    delay(1000);
    mpu.calcGyroOffsets(); 
    Serial.println("Jiroskop Hazir.");

    // Sensörleri Başlat
    tcaselect(0); renk_sensor.begin();
    tcaselect(1); renk_sensor.begin();

    delay(2000);
    durum = DURUM_NORMAL_SUR;
    Serial.println("=== YARIS BASLADI ===");
}

void loop() {
    sensorOkuVeYazdir(); // MPU update fonksiyonu dahil olduğu için bloklanmamalı

    if (durum == DURUM_BITIS) {
        motorDur();
        return;
    }

    if (oyun_modu == MOD_ENGELLI) uDonusuKontrol();

    if (tur_sayisi >= 3 && durum == DURUM_NORMAL_SUR) durum = DURUM_PARK_ARA;

    switch (durum) {
        case DURUM_NORMAL_SUR:
            if (oyun_modu == MOD_ENGELLI) {
                if (!renkKacisKontrol()) duvarTakibiVeViraj();
            } else {
                duvarTakibiVeViraj();
            }
            break;
        case DURUM_VIRAJ_DON: virajDon(); break;
        case DURUM_U_DONUSU:  uDonusuYap(); break;
        case DURUM_PARK_ARA:  parkAra(); break;
        case DURUM_PARK_YAP:  parkYap(); break;
        default: motorDur(); break;
    }
}