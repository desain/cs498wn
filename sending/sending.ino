#include <Crc16.h>

int cycle = 100;
int preamble_length = 8;
int led = 13;
char *codes[16] = {"11110", "01001", "10100", "10101", "01010", "01011", "01110", "01111", "10010", "10011", "10110", "10111", "11010", "11011", "11100", "11101"};

String message = "";

// the setup function runs once when you press reset or power the board
void setup() {
  pinMode(led, OUTPUT);
  Serial.begin(9600);
}


/**
 * Using NRZI with 4b5b encoding
 * 
 * 1. send the preamble (on-off-on-off, etc)
 * 2. send the message length
 * 3. send the message (encoded 4b5b)
 * 4. send the CRC
 * 
 * NRZI - 1 on a voltage change
 *      - 0 on no change
 * Starts on high voltage
 * HIGH is 1, LOW is 0
 */
void loop() {
  if (Serial.available() > 0) {
    int incomingByte = Serial.read();
    if (incomingByte == (int)'\n') {
      Serial.println("Sending message...");
      Serial.println(message.length());
      Serial.println(message);

      Crc16 crc = Crc16();
      crc.clearCrc();

      Serial.println("Preamble...");
      preamble();
      
      for (int i = 0; i < message.length(); i++) {
        char currentByte = message[i];
        crc.updateCrc(currentByte);
        Serial.println(currentByte);
        send_char(currentByte);
      }
      uint16_t crc_final = crc.getCrc();
      Serial.print("CRC: ");
      Serial.println(crc_final, HEX);
      send_crc(crc_final);
      
      message = "";
    } else {
      Serial.print("I received: ");
      Serial.println(incomingByte, DEC);
      Serial.println(incomingByte, HEX);
      message += (char)incomingByte;
    }
    
  } else {
    
    digitalWrite(led, LOW);
  }
}

/**
 * Takes the input and outputs the 5 bits to send
 */
void encode(int input, int *output) {
  char *code = codes[input];
  for (int i = 0; i < 5; i++) {
    if (code[i] == '0') {
      output[i] = 0;
    } else {
      output[i] = 1;
    }
  }
}

void preamble() {
  for (int i = 0; i < preamble_length; i++) {
    digitalWrite(led, (i+1) % 2);
    delay(cycle);
  }
}

void send_char(char currentByte) {
  int top = currentByte >> 4;
  int bottom = currentByte & 0xF;
  
  Serial.println(top, HEX);
  Serial.println(bottom, HEX);
  
  int encoded[10];
  encode(top, encoded);
  encode(bottom, &(encoded[5]));
  
  digitalWrite(led, HIGH);
  delay(cycle);
  int last = HIGH;
  for(int i = 0; i < 10; i++) {
    Serial.print(encoded[i]);
    if(encoded[i] == 1) {
      last = (last+1)%2;
      digitalWrite(led, last);
      delay(cycle);
    } else {
      digitalWrite(led, last);
      delay(cycle);
    }
  }
  Serial.print("\n");
}

void send_crc(uint16_t crc_final) {
  uint8_t hi_lo[] = { (uint8_t)(crc_final >> 8), (uint8_t)crc_final }; // { 0xAA, 0xFF }
//  uint8_t lo_hi[] = { (uint8_t)crc_final, (uint8_t)(crc_final >> 8) }; // { 0xFF, 0xAA }
  send_char(hi_lo[0]);
  send_char(hi_lo[1]);
}

