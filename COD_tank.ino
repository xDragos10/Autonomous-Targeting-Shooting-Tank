#include <Servo.h>

//DEFINIREA PINILOR TURELEI

const int TRIG_PIN = 8;
const int ECHO_PIN = 9;
const int SERVO_PIN = 11;
const int SERVO_TRAGACI_PIN = 7;  // servomotor pentru tragaci (MG996R continuu)
//Sunt definiți pinii folosiți de senzorul ultrasonic (TRIG/ECHO), de servomotorul care rotește turela și de servomotorul continuu care acționează trăgaciul.

//SETARILE GENERALE PENTRU DETECȚIE ȘI SCANARE
const int DISTANTA_DETECTIE = 50;  // cm
const int UNGHI_STANGA = -60;      
const int UNGHI_DREAPTA = 60;
const int VITEZA_SCANARE = 5;      
const int VITEZA_LENTA = 1;        
const int DELAY_SCANARE = 20;      
const int DELAY_LENT = 50;         
//Aceste constante stabilesc distanța maximă la care este considerat un obiect țintă, limitele de rotire ale turelui și vitezele/pașii de scanare rapidă și lentă.

//SETARILE SERVO-ULUI CONTINUU (TRĂGACI)
const int TIMP_ROTIRE = 300;       
const int TIMP_OPRIRE = 200;       
const int VITEZA_SERVO = 70;       
const int VITEZA_INAPOI = 100;     
//Parametrii de mai sus controlează cât timp și cu ce „viteză” este rotit servomotorul continuu pentru a apăsa și elibera trăgaciul

//DECLARAREA OBIECTELOR SERVO ȘI VARIABILELOR GLOBALE
Servo servo;
Servo servoTragaci;
int unghiCurent = 0;
//Se creează două obiecte Servo: unul pentru rotirea turelui și unul pentru trăgaci, plus o variabilă care memorează unghiul curent al turelui.

//SETUP – INIȚIALIZAREA TURELEI
void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  servo.attach(SERVO_PIN);
  servoTragaci.attach(SERVO_TRAGACI_PIN);

  servo.write(90);          // pozitia 0 (centru)
  servoTragaci.write(90);   // STOP pentru servo continuu
  delay(1000);

  Serial.println("  SISTEM TURELA PORNIT  ");
  Serial.println("Servo tragaci: MG996R   ");
}
//Inițializează comunicația serială, senzorul ultrasonic și cele două servomotoare. Turela este adusă în poziția centrală, iar servo-ul continuu este oprit.

//LOOP – PORNIREA SECVENȚEI DE CĂUTARE
void loop() {
  cautaObiect();
  delay(500);
}
//Bucla principală a programului apelează periodic funcția `cautaObiect()`, care se ocupă de scanarea și atacarea țintelor detectate.

//FUNCȚIA PRINCIPALĂ: CĂUTĂREA OBIECTULUI
void cautaObiect() {
  Serial.println(">>> Incep scanare rapida...");
  
  int margineStanga = -1;
  int margineDreapta = -1;
  int directie = 1;
  int unghi = UNGHI_STANGA;
  
  while (margineStanga == -1 || margineDreapta == -1) {
    
    miscaServoSmooth(unghi);
    smartDelay(DELAY_SCANARE);
    
    int distanta = masoaraDistanta();
    
    if (unghi % 10 == 0) {
      Serial.print("Unghi: ");
      Serial.print(unghi);
      Serial.print(" | Distanta: ");
      Serial.println(distanta);
    }
    
    // ===== FILTRU ANTI FALSE POSITIVE LA DETECTIA INITIALA =====
    if (distanta < DISTANTA_DETECTIE && margineStanga == -1) {

      int confirmari = 0;
      for (int i = 0; i < 3; i++) {
        smartDelay(30);
        int d2 = masoaraDistanta();
        Serial.print("   Confirmare ");
        Serial.print(i + 1);
        Serial.print("/3: ");
        Serial.println(d2);

        if (d2 < DISTANTA_DETECTIE) {
          confirmari++;
        }
      }

      if (confirmari >= 2) {
        Serial.println("! Obiect detectat - caut marginea stanga");
        margineStanga = gasesteMargineStanga(unghi);
        Serial.print("Margine stanga la: ");
        Serial.println(margineStanga);
      } else {
        Serial.println("False positive ignorat, continui scanarea.");
      }
    }
    
    if (distanta >= DISTANTA_DETECTIE && margineStanga != -1 && margineDreapta == -1) {
      Serial.println("! Caut marginea dreapta");
      margineDreapta = gasesteMargineaDreapta(unghi);
      Serial.print("Margine dreapta la: ");
      Serial.println(margineDreapta);
      break;
    }
    
    unghi += VITEZA_SCANARE * directie;
    
    if (unghi >= UNGHI_DREAPTA) {
      unghi = UNGHI_DREAPTA;
      directie = -1;
    }
    if (unghi <= UNGHI_STANGA) {
      unghi = UNGHI_STANGA;
      directie = 1;
    }
  }
  
  if (margineStanga == -1 || margineDreapta == -1) {
    Serial.println("Niciun obiect complet detectat");
    return;
  }
  
  Serial.println(">>> Verificare obiect - MISCARE LENTA...");
  
  int margineStangaNoua = -1;
  int margineDreaptaNoua = -1;
  
  // ÎNTOARCERE LENTĂ GRAD CU GRAD 
  int u = margineDreapta;
  while (u >= margineStanga) {
    miscaServoSmooth(u);
    smartDelay(DELAY_LENT);  // Delay lung pentru mișcare foarte lentă
    
    int distanta = masoaraDistanta();
    
    Serial.print("Verificare unghi ");
    Serial.print(u);
    Serial.print(": ");
    Serial.print(distanta);
    Serial.println(" cm");
    
    if (distanta < DISTANTA_DETECTIE && margineDreaptaNoua == -1) {
      margineDreaptaNoua = u;
      Serial.print(">>> Margine dreapta confirmata: ");
      Serial.println(u);
    }
    
    if (distanta >= DISTANTA_DETECTIE && margineDreaptaNoua != -1 && margineStangaNoua == -1) {
      margineStangaNoua = u + VITEZA_LENTA; // Compensare pentru ultimul pas
      Serial.print(">>> Margine stanga confirmata: ");
      Serial.println(margineStangaNoua);
      break;
    }
    
    u -= VITEZA_LENTA;  // Decrementăm manual pentru control total
  }
  
  if (margineStangaNoua != -1 && margineDreaptaNoua != -1) {
    margineStanga = margineStangaNoua;
    margineDreapta = margineDreaptaNoua;
    Serial.println("Obiect confirmat!");
  }
  
  int unghiMijloc = (margineStanga + margineDreapta) / 2;
  Serial.println("=====================================");
  Serial.print("Margine STANGA finala: ");
  Serial.println(margineStanga);
  Serial.print("Margine DREAPTA finala: ");
  Serial.println(margineDreapta);
  Serial.print(">>> MIJLOC CALCULAT: ");
  Serial.println(unghiMijloc);
  Serial.println("=====================================");
  
  // Centrare LENTĂ la mijloc
  Serial.println(">>> Centrare LENTA la mijloc...");
  int paseCatreMijloc = abs(unghiCurent - unghiMijloc);
  int directieMijloc = (unghiMijloc > unghiCurent) ? 1 : -1;
  
  for (int pas = 0; pas < paseCatreMijloc; pas++) {
    unghiCurent += directieMijloc;
    miscaServoSmooth(unghiCurent);
    smartDelay(30);  // Mișcare foarte lentă către mijloc
  }
  
  smartDelay(500);
  
  Serial.println(">>> Verificare finala inainte de tragere...");
  int verificari = 0;
  for (int i = 0; i < 3; i++) {
    smartDelay(100);
    int dist = masoaraDistanta();
    Serial.print("Verificare ");
    Serial.print(i+1);
    Serial.print("/3: ");Serial.println(">>> Incep scanare rapida...");
  
  int margineStanga = -1;
  int margineDreapta = -1;
  int directie = 1;
  int unghi = UNGHI_STANGA;
  
  while (margineStanga == -1 || margineDreapta == -1) {
    
    miscaServoSmooth(unghi);
    smartDelay(DELAY_SCANARE);
    
    int distanta = masoaraDistanta();
    
    if (unghi % 10 == 0) {
      Serial.print("Unghi: ");
      Serial.print(unghi);
      Serial.print(" | Distanta: ");
      Serial.println(distanta);
    }
    
    // ===== FILTRU ANTI FALSE POSITIVE LA DETECTIA INITIALA =====
    if (distanta < DISTANTA_DETECTIE && margineStanga == -1) {

      int confirmari = 0;
      for (int i = 0; i < 3; i++) {
        smartDelay(30);
        int d2 = masoaraDistanta();
        Serial.print("   Confirmare ");
        Serial.print(i + 1);
        Serial.print("/3: ");
        Serial.println(d2);

        if (d2 < DISTANTA_DETECTIE) {
          confirmari++;
        }
      }

      if (confirmari >= 2) {
        Serial.println("! Obiect detectat - caut marginea stanga");
        margineStanga = gasesteMargineStanga(unghi);
        Serial.print("Margine stanga la: ");
        Serial.println(margineStanga);
      } else {
        Serial.println("False positive ignorat, continui scanarea.");
      }
    }
    
    if (distanta >= DISTANTA_DETECTIE && margineStanga != -1 && margineDreapta == -1) {
      Serial.println("! Caut marginea dreapta");
      margineDreapta = gasesteMargineaDreapta(unghi);
      Serial.print("Margine dreapta la: ");
      Serial.println(margineDreapta);
      break;
    }
    
    unghi += VITEZA_SCANARE * directie;
    
    if (unghi >= UNGHI_DREAPTA) {
      unghi = UNGHI_DREAPTA;
      directie = -1;
    }
    if (unghi <= UNGHI_STANGA) {
      unghi = UNGHI_STANGA;
      directie = 1;
    }
  }
  
  if (margineStanga == -1 || margineDreapta == -1) {
    Serial.println("Niciun obiect complet detectat");
    return;
  }
  
  Serial.println(">>> Verificare obiect - MISCARE LENTA...");
  
  int margineStangaNoua = -1;
  int margineDreaptaNoua = -1;
  
  // ÎNTOARCERE LENTĂ GRAD CU GRAD - nu folosim for cu pași mari!
  int u = margineDreapta;
  while (u >= margineStanga) {
    miscaServoSmooth(u);
    smartDelay(DELAY_LENT);  // Delay lung pentru mișcare foarte lentă
    
    int distanta = masoaraDistanta();
    
    Serial.print("Verificare unghi ");
    Serial.print(u);
    Serial.print(": ");
    Serial.print(distanta);
    Serial.println(" cm");
    
    if (distanta < DISTANTA_DETECTIE && margineDreaptaNoua == -1) {
      margineDreaptaNoua = u;
      Serial.print(">>> Margine dreapta confirmata: ");
      Serial.println(u);
    }
    
    if (distanta >= DISTANTA_DETECTIE && margineDreaptaNoua != -1 && margineStangaNoua == -1) {
      margineStangaNoua = u + VITEZA_LENTA; // Compensare pentru ultimul pas
      Serial.print(">>> Margine stanga confirmata: ");
      Serial.println(margineStangaNoua);
      break;
    }
    
    u -= VITEZA_LENTA;  // Decrementăm manual pentru control total
  }
  
  if (margineStangaNoua != -1 && margineDreaptaNoua != -1) {
    margineStanga = margineStangaNoua;
    margineDreapta = margineDreaptaNoua;
    Serial.println("Obiect confirmat!");
  }
  
  int unghiMijloc = (margineStanga + margineDreapta) / 2;
  Serial.println("=====================================");
  Serial.print("Margine STANGA finala: ");
  Serial.println(margineStanga);
  Serial.print("Margine DREAPTA finala: ");
  Serial.println(margineDreapta);
  Serial.print(">>> MIJLOC CALCULAT: ");
  Serial.println(unghiMijloc);
  Serial.println("=====================================");
  
  // Centrare LENTĂ la mijloc
  Serial.println(">>> Centrare LENTA la mijloc...");
  int paseCatreMijloc = abs(unghiCurent - unghiMijloc);
  int directieMijloc = (unghiMijloc > unghiCurent) ? 1 : -1;
  
  for (int pas = 0; pas < paseCatreMijloc; pas++) {
    unghiCurent += directieMijloc;
    miscaServoSmooth(unghiCurent);
    smartDelay(30);  // Mișcare foarte lentă către mijloc
  }
  
  smartDelay(500);
  
  Serial.println(">>> Verificare finala inainte de tragere...");
  int verificari = 0;
  for (int i = 0; i < 3; i++) {
    smartDelay(100);
    int dist = masoaraDistanta();
    Serial.print("Verificare ");
    Serial.print(i+1);
    Serial.print("/3: ");
    Serial.println(dist);
    
    // tragere DOAR dacă ținta e între DIST_MIN_TRAGERE și DIST_MAX_TRAGERE
    if (dist >= DIST_MIN_TRAGERE && dist <= DIST_MAX_TRAGERE) {
      verificari++;
    }
  }
  
  if (verificari < 2) {
    Serial.println("!!! ANULAT - obiect disparut sau distanta in afara intervalului");
    return;
  }
  
  Serial.println(">>> Obiect CONFIRMAT in intervalul de tragere!");
  Serial.println(">>> FOOOC!");
  trageTinta();
  asteaptaDispareTinta();
    Serial.println(dist);
    
    // tragere DOAR dacă ținta e între DIST_MIN_TRAGERE și DIST_MAX_TRAGERE
    if (dist >= DIST_MIN_TRAGERE && dist <= DIST_MAX_TRAGERE) {
      verificari++;
    }
  }
  
  if (verificari < 2) {
    Serial.println("!!! ANULAT - obiect disparut sau distanta in afara intervalului");
    return;
  }
  
  Serial.println(">>> Obiect CONFIRMAT in intervalul de tragere!");
  Serial.println(">>> FOOOC!");
  trageTinta();
  asteaptaDispareTinta();
}

//`cautaObiect()` scanează între limitele stânga–dreapta, găsește marginea stângă și dreaptă a obiectului, confirmă ținta printr-o scanare lentă, calculează unghiul de mijloc, verifică de mai multe ori dacă obiectul este încă prezent, apoi declanșează tragerea și așteaptă să dispară ținta înainte de o nouă scanare.

//FUNCȚII DE DETECȚIE A MARGINILOR ȚINTEI
int gasesteMargineStanga(int unghiStart) {
  Serial.println("-> Caut margine stanga LENT...");
  for (int u = unghiStart; u >= UNGHI_STANGA; u -= VITEZA_LENTA) {
    miscaServoSmooth(u);
    smartDelay(DELAY_LENT);

    int distanta = masoaraDistanta();
    Serial.print("  Unghi ");
    Serial.print(u);
    Serial.print(": ");
    Serial.println(distanta);

    if (distanta >= DISTANTA_DETECTIE) {
      int margine = u + VITEZA_LENTA;
      Serial.print("-> Margine gasita la: ");
      Serial.println(margine);
      return margine;
    }
  }
  return unghiStart;
}

int gasesteMargineaDreapta(int unghiStart) {
  Serial.println("-> Caut margine dreapta LENT...");
  for (int u = unghiStart; u <= UNGHI_DREAPTA; u += VITEZA_LENTA) {
    miscaServoSmooth(u);
    smartDelay(DELAY_LENT);

    int distanta = masoaraDistanta();
    Serial.print("  Unghi ");
    Serial.print(u);
    Serial.print(": ");
    Serial.println(distanta);

    if (distanta >= DISTANTA_DETECTIE) {
      int margine = u - VITEZA_LENTA;
      Serial.print("-> Margine gasita la: ");
      Serial.println(margine);
      return margine;
    }
  }
  return unghiStart;
}

//Aceste funcții mișcă turela încet către stânga sau dreapta și citesc distanța de la senzorul ultrasonic, pentru a identifica poziția exactă unde începe și unde se termină obiectul detectat


//FUNCȚII DE ACȚIUNE: TRAGERE ȘI LOCK PE ȚINTĂ
void trageTinta() {
Serial.println("=== ACTIVARE TRAGACI (SERVO CONTINUU) ===");

  Serial.println("-> Rotire inainte...");
  servoTragaci.write(VITEZA_SERVO);
  smartDelay(TIMP_ROTIRE);

  servoTragaci.write(90);
  Serial.println("-> STOP (compensare inertie)");
  smartDelay(TIMP_OPRIRE);

  Serial.println("-> Rotire inapoi...");
  servoTragaci.write(VITEZA_INAPOI);
  smartDelay(TIMP_ROTIRE);

  servoTragaci.write(90);
  Serial.println("-> STOP FINAL (compensare inertie)");
  smartDelay(TIMP_OPRIRE);

  Serial.println("=== TRAGACI FINALIZAT ===");
}

void asteaptaDispareTinta() {
  Serial.println(">>> Lock activ - astept sa dispara obiectul...");

  int contor = 0;
  while (contor < 5) {
    int distanta = masoaraDistanta();

    if (distanta >= DISTANTA_DETECTIE) {
      contor++;
      Serial.print("Verificare ");
      Serial.print(contor);
      Serial.println("/5");
    } else {
      contor = 0;
    }

//`trageTinta()` comandă servo-ul continuu al trăgaciului să apese și să elibereze exact o dată, compensând inerția
//`asteptaDispareTinta()` blochează sistemul până când ținta este detectată ca dispărută de mai multe ori la rând, pentru siguranță.


//FUNCȚII AUXILIARE: MIȘCAREA SERVO-ULUI ȘI MĂSURAREA DISTANȚEI
void miscaServoSmooth(int unghi) {
  servo.write(90 + unghi);
  unghiCurent = unghi;
}

int masoaraDistanta() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long durata = pulseIn(ECHO_PIN, HIGH, 30000);  
  int distanta = durata * 0.034 / 2;

  if (distanta <= 0 || distanta > 400) {
    return 999;
  }

  return distanta;
}
