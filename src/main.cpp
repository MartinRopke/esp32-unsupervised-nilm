#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial); // Aguarda a abertura do monitor serial
  
  Serial.println("\n--- Inicializando Varredura I2C ---");
  // Inicializa o barramento I2C nos pinos padrões do ESP32 (SDA=21, SCL=22)
  Wire.begin(); 
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Escaneando barramento...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Dispositivo I2C encontrado no endereco 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("!");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Erro desconhecido no endereco 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println("Nenhum dispositivo I2C encontrado.\n");
  } else {
    Serial.println("Varredura concluida.\n");
  }

  delay(5000); // Executa o escaneamento a cada 5 segundos
}