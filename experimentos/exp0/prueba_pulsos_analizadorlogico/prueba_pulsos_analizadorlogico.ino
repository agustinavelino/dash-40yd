#define PIN_PULSO 4

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PULSO, OUTPUT);
  digitalWrite(PIN_PULSO, LOW);
}

void loop() {
  digitalWrite(PIN_PULSO, HIGH);
  delayMicroseconds(200);
  digitalWrite(PIN_PULSO, LOW);
  delay(500);
}