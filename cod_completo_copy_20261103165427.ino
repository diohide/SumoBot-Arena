/*

 * ROBÔ SUMÔ AUTÔNOMO - ARDUINO (C++)
 *
 * SEQUÊNCIA DE FUNCIONAMENTO:
 *
 * 1. INICIALIZAÇÃO:
 *    Aguarda o botão ser pressionado e inicia
 *    após 5 segundos.
 *
 * 2. MOVIMENTO INICIAL:
 *    Avança por 1,5 s e gira à direita por 1,2 s.
 *
 * 3. DETECÇÃO DE BORDA (PRIORIDADE MÁXIMA):
 *    Ao detectar a linha da arena, interrompe
 *    os movimentos, executa uma ré de 1,5 s
 *    e aguarda sair da borda.
 *
 * 4. DETECÇÃO DO ADVERSÁRIO (2ª PRIORIDADE):
 *    O ultrassônico identifica o oponente e
 *    ajusta a velocidade conforme a distância:
 *    - Até 40 cm: ataque leve
 *    - Até 25 cm: ataque médio
 *    - Até 10 cm: ataque máximo
 *
 * 5. VARREDURA (3ª PRIORIDADE):
 *    Sem detectar borda ou adversário, realiza
 *    movimentos de busca pela arena.
 *
 * 6. REPETIÇÃO:
 *    Continua monitorando os sensores e alternando
 *    automaticamente entre defesa, ataque e busca.
 */


// ============================================================
// 1. PINOS (padrao do codigo do ultrassonico)
// ============================================================
const int PINO_BOTAO = 2;
const int MOTOR_ESQ_A = 5;
const int MOTOR_ESQ_B = 6;
const int MOTOR_DIR_A = 9;
const int MOTOR_DIR_B = 10;
const int ECHO = 7;
const int TRIGGER = 8;
const int SENSOR_DIR = 11;
const int SENSOR_ESQ = 12;
const int SENSOR_TRAS = 13;

// ============================================================
// 2. VELOCIDADES
// ============================================================
const int VEL_FRENTE_ESQ = 255;
const int VEL_FRENTE_DIR = 255;
const int VELOCIDADE_GIRO = 255;
const int ATAQUE_LEVE_ESQ = 128;
const int ATAQUE_LEVE_DIR = 113;
const int ATAQUE_MEDIO_ESQ = 190;
const int ATAQUE_MEDIO_DIR = 200;
const int ATAQUE_MAX_ESQ = 255;
const int ATAQUE_MAX_DIR = 240;
const int VEL_ESQ_RE = 255;
const int VEL_DIR_RE = 255;

// ============================================================
// 3. TEMPOS (milissegundos, exceto TIMEOUT_ECHO)
// ============================================================
const unsigned long TEMPO_INICIO = 5000;
const unsigned long TEMPO_ABERTURA_FRENTE = 1500; // valor real do codigo 2
const unsigned long TEMPO_GIRO_130 = 1200;
const unsigned long TEMPO_FRENTE = 500;
const unsigned long TEMPO_GIRO_45 = 500;
const unsigned long INTERVALO_SENSOR = 50;
const unsigned long TIMEOUT_ECHO = 12000; // microssegundos
const unsigned long TEMPO_RE = 1500; // solicitado anteriormente; codigo 1 tinha 900
const unsigned long CONFIRMACAO_BORDA = 30;
const unsigned long TEMPO_REARME = 100;
const unsigned long INTERVALO_SERIAL = 500;

// ============================================================
// 4. VARIAVEIS DE ESTADO
// ============================================================
int COR_DIR = HIGH, COR_ESQ = HIGH, COR_TRAS = HIGH;
int ultimaLeituraDir = HIGH, ultimaLeituraEsq = HIGH;
unsigned long mudancaDir = 0, mudancaEsq = 0;
unsigned long inicioRearme = 0;
bool aguardandoRearme = false;

enum Modo { ABERTURA_FRENTE, ABERTURA_DIREITA, BUSCA, RE_BORDA, ESPERA_PISO };
Modo modo = ABERTURA_FRENTE;
unsigned long inicioModo = 0;

int etapa = 0;
unsigned long inicioEtapa = 0;
unsigned long ultimaLeitura = 0;
unsigned long ultimoSerial = 0;
float distancia = -1;
bool atacando = false;
int nivelAtaque = 0;

// ============================================================
// 5. MOTORES
//    No codigo 2, a FRENTE do motor direito usa o pino B.
//    Esta convencao evita que um lado gire invertido.
// ============================================================
void controlarMotores(int velEsq, int velDir) {
  velEsq = constrain(velEsq, -255, 255);
  velDir = constrain(velDir, -255, 255);

  if (velEsq >= 0) {
    analogWrite(MOTOR_ESQ_A, velEsq);
    analogWrite(MOTOR_ESQ_B, 0);
  } else {
    analogWrite(MOTOR_ESQ_A, 0);
    analogWrite(MOTOR_ESQ_B, -velEsq);
  }

  // DIREITO INVERTIDO EM RELACAO AO ESQUERDO
  if (velDir >= 0) {
    analogWrite(MOTOR_DIR_A, 0);
    analogWrite(MOTOR_DIR_B, velDir);
  } else {
    analogWrite(MOTOR_DIR_A, -velDir);
    analogWrite(MOTOR_DIR_B, 0);
  }
}

void moverFrente(int velEsq, int velDir) { controlarMotores(velEsq, velDir); }
void direita() { controlarMotores(VELOCIDADE_GIRO, -VELOCIDADE_GIRO); }
void esquerda() { controlarMotores(-VELOCIDADE_GIRO, VELOCIDADE_GIRO); }
void reMaxima() { controlarMotores(-VEL_ESQ_RE, -VEL_DIR_RE); }
void parar() { controlarMotores(0, 0); }

// ============================================================
// 6. CALIBRACAO DIGITAL (DO) - 100 AMOSTRAS POR SENSOR
// ============================================================
void calibrarSensores() {
  parar();
  const int AMOSTRAS = 100;
  int contDir = 0, contEsq = 0, contTras = 0;

  Serial.println("CALIBRANDO: MANTENHA SENSORES SOBRE O PISO NORMAL");
  for (int i = 0; i < AMOSTRAS; i++) {
    if (digitalRead(SENSOR_DIR) == HIGH) contDir++;
    if (digitalRead(SENSOR_ESQ) == HIGH) contEsq++;
    if (digitalRead(SENSOR_TRAS) == HIGH) contTras++;
    delay(5);
  }
  COR_DIR = (contDir >= 50) ? HIGH : LOW;
  COR_ESQ = (contEsq >= 50) ? HIGH : LOW;
  COR_TRAS = (contTras >= 50) ? HIGH : LOW;

  Serial.println("===== CALIBRACAO (0=LOW, 1=HIGH) =====");
  Serial.print("S11 direito: "); Serial.print(COR_DIR);
  Serial.print(" | HIGH em "); Serial.print(contDir); Serial.println("/100");
  Serial.print("S12 esquerdo: "); Serial.print(COR_ESQ);
  Serial.print(" | HIGH em "); Serial.print(contEsq); Serial.println("/100");
  Serial.print("S13 traseiro: "); Serial.print(COR_TRAS);
  Serial.print(" | HIGH em "); Serial.print(contTras); Serial.println("/100");
  if ((contDir > 10 && contDir < 90) ||
      (contEsq > 10 && contEsq < 90)) {
    Serial.println("AVISO: calibracao instavel nos sensores dianteiros!");
  }
  ultimaLeituraDir = digitalRead(SENSOR_DIR);
  ultimaLeituraEsq = digitalRead(SENSOR_ESQ);
  mudancaDir = millis();
  mudancaEsq = millis();
}

// ============================================================
// 7. DETECCAO DE BORDA COM CONFIRMACAO DE 30 ms
// ============================================================
void atualizarLeiturasLinha() {
  unsigned long agora = millis();
  int leituraDir = digitalRead(SENSOR_DIR);
  int leituraEsq = digitalRead(SENSOR_ESQ);
  if (leituraDir != ultimaLeituraDir) {
    ultimaLeituraDir = leituraDir;
    mudancaDir = agora;
  }
  if (leituraEsq != ultimaLeituraEsq) {
    ultimaLeituraEsq = leituraEsq;
    mudancaEsq = agora;
  }
}

bool detectarBorda() {
  unsigned long agora = millis();
  bool bordaDir = (ultimaLeituraDir != COR_DIR) &&
                   (agora - mudancaDir >= CONFIRMACAO_BORDA);
  bool bordaEsq = (ultimaLeituraEsq != COR_ESQ) &&
                   (agora - mudancaEsq >= CONFIRMACAO_BORDA);
  if (bordaDir || bordaEsq) {
    Serial.print("BORDA CONFIRMADA: ");
    if (bordaDir) Serial.print("DIREITO(11) ");
    if (bordaEsq) Serial.print("ESQUERDO(12)");
    Serial.println();
  }
  return bordaDir || bordaEsq;
}

void iniciarReBorda() {
  // Interrompe abertura, ataque ou varredura imediatamente.
  atacando = false;
  nivelAtaque = 0;
  modo = RE_BORDA;
  inicioModo = millis();
  reMaxima();
  Serial.println("RE MAXIMA: 1500 ms");
}

void atualizarReBorda() {
  if (millis() - inicioModo >= TEMPO_RE) {
    parar();
    modo = ESPERA_PISO;
    inicioRearme = 0;
    aguardandoRearme = true;
    Serial.println("RE FINALIZADA. AGUARDANDO PISO CALIBRADO.");
  }
}

void atualizarEsperaPiso() {
  // Enquanto estiver suspenso ou sobre a cor contraria, permanece PARADO.
  if (ultimaLeituraDir == COR_DIR && ultimaLeituraEsq == COR_ESQ) {
    if (inicioRearme == 0) inicioRearme = millis();
    if (millis() - inicioRearme >= TEMPO_REARME) {
      aguardandoRearme = false;
      modo = BUSCA;
      etapa = 0;
      inicioEtapa = millis();
      ultimaLeitura = millis() - INTERVALO_SENSOR;
      Serial.println("PISO NORMAL. RETOMANDO VARREDURA.");
    }
  } else {
    inicioRearme = 0;
    parar();
  }
}

// ============================================================
// 8. ABERTURA UNICA: FRENTE 1500 ms, DIREITA 130 graus (1200 ms)
//    Mantida exatamente com os TEMPOS efetivos do codigo 2.
//    Sem delay: a borda pode interromper a abertura.
// ============================================================
void executarAbertura() {
  unsigned long agora = millis();
  if (modo == ABERTURA_FRENTE) {
    moverFrente(VEL_FRENTE_ESQ, VEL_FRENTE_DIR);
    if (agora - inicioModo >= TEMPO_ABERTURA_FRENTE) {
      parar();
      modo = ABERTURA_DIREITA;
      inicioModo = millis();
      Serial.println("ABERTURA: GIRANDO DIREITA 130 graus (estimado)");
    }
  } else if (modo == ABERTURA_DIREITA) {
    direita();
    if (agora - inicioModo >= TEMPO_GIRO_130) {
      parar();
      modo = BUSCA;
      etapa = 0;
      inicioEtapa = millis();
      ultimaLeitura = millis() - INTERVALO_SENSOR;
      Serial.println("ABERTURA FINALIZADA. BUSCA INICIADA.");
    }
  }
}

// ============================================================
// 9. SENSOR ULTRASSONICO
// ============================================================
float lerDistancia() {
  digitalWrite(TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER, LOW);
  unsigned long duracao = pulseIn(ECHO, HIGH, TIMEOUT_ECHO);
  if (duracao == 0) return -1;
  return duracao * 0.0343 / 2.0;
}

// ============================================================
// 10. ATAQUE ULTRASSONICO
// ============================================================
void executarAtaque(float cm) {
  int novoNivel = 0;
  if (cm > 25 && cm <= 40) {
    novoNivel = 1;
    moverFrente(ATAQUE_LEVE_ESQ, ATAQUE_LEVE_DIR);
  } else if (cm > 10 && cm <= 25) {
    novoNivel = 2;
    moverFrente(ATAQUE_MEDIO_ESQ, ATAQUE_MEDIO_DIR);
  } else if (cm > 0 && cm <= 10) {
    novoNivel = 3;
    moverFrente(ATAQUE_MAX_ESQ, ATAQUE_MAX_DIR);
  }
  if (novoNivel != nivelAtaque) {
    nivelAtaque = novoNivel;
    Serial.print("DISTANCIA: "); Serial.print(cm); Serial.println(" cm");
    if (novoNivel == 1) Serial.println("ATAQUE LEVE");
    else if (novoNivel == 2) Serial.println("ATAQUE MEDIO");
    else if (novoNivel == 3) Serial.println("ATAQUE MAXIMO");
  }
}

void atualizarUltrassonico() {
  unsigned long agora = millis();
  if (agora - ultimaLeitura < INTERVALO_SENSOR) return;
  ultimaLeitura = agora;
  distancia = lerDistancia();
  if (distancia > 0 && distancia <= 40) {
    atacando = true;
    executarAtaque(distancia);
  } else if (atacando) {
    atacando = false;
    nivelAtaque = 0;
    parar();
    etapa = 0;
    inicioEtapa = millis();
    Serial.println("ALVO PERDIDO. VOLTANDO A VARREDURA.");
  }
}

// ============================================================
// 11. VARREDURA (6 ETAPAS DO CODIGO 2)
//     Frente -> Dir -> Esq -> Frente -> Esq -> Dir
// ============================================================
void executarVarredura() {
  unsigned long agora = millis();
  unsigned long duracao = 0;
  switch (etapa) {
    case 0: moverFrente(VEL_FRENTE_ESQ, VEL_FRENTE_DIR);
            duracao = TEMPO_FRENTE; break;
    case 1: direita(); duracao = TEMPO_GIRO_45; break;
    case 2: esquerda(); duracao = TEMPO_GIRO_45; break;
    case 3: moverFrente(VEL_FRENTE_ESQ, VEL_FRENTE_DIR);
            duracao = TEMPO_FRENTE; break;
    case 4: esquerda(); duracao = TEMPO_GIRO_45; break;
    case 5: direita(); duracao = TEMPO_GIRO_45; break;
  }
  if (agora - inicioEtapa >= duracao) {
    parar();
    etapa++;
    if (etapa > 5) {
      etapa = 0;
      Serial.println("REPETINDO VARREDURA");
    }
    inicioEtapa = millis();
    Serial.print("ETAPA: "); Serial.println(etapa);
  }
}

// ============================================================
// 12. MONITOR SERIAL
// ============================================================
void mostrarDiagnostico() {
  if (millis() - ultimoSerial < INTERVALO_SERIAL) return;
  ultimoSerial = millis();
  Serial.print("S11="); Serial.print(ultimaLeituraDir);
  Serial.print(" REF="); Serial.print(COR_DIR);
  Serial.print(" | S12="); Serial.print(ultimaLeituraEsq);
  Serial.print(" REF="); Serial.print(COR_ESQ);
  Serial.print(" | MODO="); Serial.print((int)modo);
  Serial.print(" | DIST="); Serial.println(distancia);
}

// ============================================================
// 13. SETUP (UNICO)
// ============================================================
void setup() {
  Serial.begin(9600);
  pinMode(PINO_BOTAO, INPUT_PULLUP);
  pinMode(MOTOR_ESQ_A, OUTPUT);
  pinMode(MOTOR_ESQ_B, OUTPUT);
  pinMode(MOTOR_DIR_A, OUTPUT);
  pinMode(MOTOR_DIR_B, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(TRIGGER, OUTPUT);
  digitalWrite(TRIGGER, LOW);
  pinMode(SENSOR_DIR, INPUT);
  pinMode(SENSOR_ESQ, INPUT);
  pinMode(SENSOR_TRAS, INPUT);
  parar();

  Serial.println("ROBO SUMO - CODIGO UNIFICADO");
  Serial.println("AGUARDANDO BOTAO...");
  while (digitalRead(PINO_BOTAO) == HIGH) delay(20);
  delay(30);
  while (digitalRead(PINO_BOTAO) == LOW) delay(20);

  for (int i = 5; i > 0; i--) {
    Serial.print("COMECA EM: "); Serial.println(i);
    delay(1000);
  }
  calibrarSensores();
  modo = ABERTURA_FRENTE;
  inicioModo = millis();
  Serial.println("ABERTURA: FRENTE");
}

// ============================================================
// 14. LOOP (UNICO) - PRIORIDADES
// ============================================================
void loop() {
  atualizarLeiturasLinha();
  mostrarDiagnostico();

  // 1. Se esta fazendo re, termina os 1500 ms sem reiniciar.
  if (modo == RE_BORDA) {
    atualizarReBorda();
    return;
  }

  // 2. Depois da re, nao se movimenta enquanto houver borda.
  if (modo == ESPERA_PISO) {
    atualizarEsperaPiso();
    return;
  }

  // 3. Sensores dianteiros interrompem qualquer outro movimento.
  if (detectarBorda()) {
    iniciarReBorda();
    return;
  }

  // 4. Abertura unica, interrompivel por borda.
  if (modo == ABERTURA_FRENTE || modo == ABERTURA_DIREITA) {
    executarAbertura();
    return;
  }

  // 5. Ultrassonico com prioridade sobre a varredura.
  atualizarUltrassonico();
  if (atacando) return;

  // 6. Sem borda e sem alvo: executa varredura.
  executarVarredura();

  // Sensor traseiro (13) calibrado, mas SEM ACAO.
}
