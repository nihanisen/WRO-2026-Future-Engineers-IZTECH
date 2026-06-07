#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Servo.h>

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// ============================================================
//   PİN TANIMLAMALARI
// ============================================================
const int TRIG_PIN  = 2;   // DEĞİŞTİ
const int ECHO_PIN  = 3;   // DEĞİŞTİ
const int SERVO_PIN = 9;
Servo direksiyonServosu;

const int IN3 = 4;
const int IN4 = 7;
const int ENB = 6;

// ============================================================
//   HIZ VE AÇI AYARLARI
// ============================================================
const int HIZ_TAM_GAZ  = 240;

const int SERVO_MERKEZ = 98;
const int SERVO_SAG    = 118;
const int SERVO_SOL    = 78;

// ============================================================
//   MESAFE VE ZAMAN AYARLARI
// ============================================================
const int ACIL_DURMA_MESAFESI    = 30;
const unsigned long GERI_GITME_SURESI    = 1200;
const unsigned long DONUS_SURESI         = 900;
const unsigned long KACIS_MANEVRA_SURESI = 1200;

// ============================================================
//   SETUP
// ============================================================
void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 0);

  // Önce attach et, bekle, sonra merkeze al
  direksiyonServosu.attach(SERVO_PIN);
  delay(200);
  direksiyonServosu.write(SERVO_MERKEZ);
  delay(1000);  // Merkeze gelmesi için yeterli süre ver

  if (tcs.begin()) {
    Serial.println("Renk Sensoru OK");
  } else {
    //Serial.println("UYARI: Renk Sensoru Bulunamadi!");
  }

  //Serial.println("=== ROBOT 3 SANIYE SONRA BASLIYOR ===");
  delay(3000);
}

// ============================================================
//   ANA DÖNGÜ
// ============================================================
void loop() {
  long onMesafe = mesafeOlc();
  char okunanRenk = renkOku();

  //Serial.print("Mesafe: "); Serial.print(onMesafe);
  //Serial.print(" cm | Renk: "); Serial.println(okunanRenk);

  // 1. DUVAR TEHLİKESİ - mesafe öncelikli
  if (onMesafe > 0 && onMesafe <= ACIL_DURMA_MESAFESI) {
    //Serial.println(">>> DUVAR! Dur + Geri + Don <<<");
    acilGeriCikVeDon();
  }
  // 2. KIRMIZI DİREK → Solundan geç
  else if (okunanRenk == 'K') {
    Serial.println("KIRMIZI -> Sola Kac!");
    solaKacManevrasi();
  }
  // 3. YOL AÇIK → İleri
  else {
    ileriGit();
  }
}

// ============================================================
//   SÜRÜŞ FONKSİYONLARI
// ============================================================
void ileriGit() {
  direksiyonServosu.write(SERVO_MERKEZ);
  delay(50);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, HIZ_TAM_GAZ);
}

void dur() {
  analogWrite(ENB, 0);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  delay(200);
}

void acilGeriCikVeDon() {
  // 1. Tamamen dur
  dur();

  // 2. Düz geri git
  direksiyonServosu.write(SERVO_MERKEZ);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENB, HIZ_TAM_GAZ);
  delay(GERI_GITME_SURESI);

  // 3. Dur
  dur();

  // 4. Sağa dönerek ilerle
  direksiyonServosu.write(SERVO_SAG);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, HIZ_TAM_GAZ);
  delay(DONUS_SURESI);

  // 5. Düzelt ve dur
  dur();
  direksiyonServosu.write(SERVO_MERKEZ);
  delay(200);
}

void solaKacManevrasi() {
  direksiyonServosu.write(SERVO_SOL);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, HIZ_TAM_GAZ);
  delay(KACIS_MANEVRA_SURESI);
  direksiyonServosu.write(SERVO_MERKEZ);
}

// ============================================================
//   SENSÖR FONKSİYONLARI
// ============================================================
long mesafeOlc() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long sure = pulseIn(ECHO_PIN, HIGH, 20000);
  if (sure == 0) return 999;
  return sure / 58;
}

char renkOku() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  // Zemin değerleri: R~210, G~245, B~181
  // Kırmızı direk: R~600-780 (çok yüksek)
  // R 500'ün üstündeyse ve G ile B'den belirgin şekilde büyükse kırmızı
  if (r > 500 && r > g * 2) {
    return 'K'; // Kırmızı direk
  }

  return 'B'; // Zemin veya bilinmeyen
}
