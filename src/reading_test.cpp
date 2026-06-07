// #include <Arduino.h>
// #include <Wire.h>
// #include <Adafruit_ADS1X15.h>

// Adafruit_ADS1115 ads;

// void setup() {
//   Serial.begin(115200);
//   Wire.begin();

//   if (!ads.begin()) {
//     Serial.println("Falha ao inicializar o ADS1115!");
//     while (1);
//   }

//   // Mantém o ganho em 2x (Mede até +/-2.048V). Perfeito para o nosso offset de 1.65V
//   ads.setGain(GAIN_TWO); 
// }

// void loop() {
//   int16_t adc0;
//   float tensao;

//   // Lê o pino A0
//   adc0 = ads.readADC_SingleEnded(0);
  
//   // No GAIN_TWO, cada unidade do ADC equivale a 0.0625 mV
//   tensao = ads.computeVolts(adc0);

//   // Printa apenas o valor da tensao para o Serial Plotter conseguir desenhar o gráfico
//   Serial.print(">V:");
//   Serial.println(tensao, 4); // Imprime com 4 casas decimais para não perder resolução

//   delayMicroseconds(6500); 
// }