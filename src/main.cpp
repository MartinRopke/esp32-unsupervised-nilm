#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

unsigned long ultimoTempo = 0;

// 1.000.000 microssegundos / 860 SPS = ~1163 microssegundos por amostra
const unsigned long intervaloAmostra = 1163; 

void setup() {
  // Mantém a Serial ultraveloz que você configurou
  Serial.begin(921600);
  
  if (!ads.begin()) {
    Serial.println("Falha ao iniciar o ADS1115!");
    while (1);
  }
  
  // Define a taxa máxima no hardware do chip
  ads.setDataRate(RATE_ADS1115_860SPS);
  
  // ATIVA O MODO CONTÍNUO: O chip fica medindo o canal A0 sem parar
  ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);
  
  // TURBO NO I2C: Muda a comunicação de 100kHz para 400kHz
  Wire.setClock(400000);
}

void loop() {
  unsigned long tempoAtual = micros();
  
  // Ritmo de amostragem controlado precisamente em 1163 microssegundos
  if (tempoAtual - ultimoTempo >= intervaloAmostra) {
    ultimoTempo = tempoAtual;
    
    // 1. Leitura instantânea (não-bloqueante) do valor bruto
    int16_t valorBruto = ads.getLastConversionResults();
    
    // 2. Conversão ultra-rápida para float (Volts) usando o hardware do ESP32
    float tensao = ads.computeVolts(valorBruto);
    
    // 3. Envio para o Teleplot mantendo o nome da variável consistente
    Serial.print(">V:");
    Serial.println(tensao, 4); // Imprime com 4 casas decimais para não perder resolução
  }
}