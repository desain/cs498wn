int analogPin = 5;
int last_val = 0;
byte last_byte = 0;

char *codes[17] = {"11110", "01001", "10100", "10101", "01010", "01011", "01110", "01111", "10010", "10011", "10110", "10111", "11010", "11011", "11100", "11101", "00000"};

void setup() {
  // put your setup code here, to run once:
  pinMode(A0, INPUT);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
 Serial.begin(9600);
}

int receive_incoming_signal() {
  int in = analogRead(0);
  if (in >= 80) {
    Serial.println(1);
    return 1;
  } else {
    Serial.println(0);
    return 0;
  }
}

int decode_byte(char *read) {
  for (int i = 0; i < 17; i++) {
    if (strcmp(read, codes[i])==0) {
      return i;
    }
  }
  return -1;
}

char *receive_message() {
  char buff[100];
  int buffer_end = 0;
  char code[5];
  byte hex;
  while(1) {
    for (int b = 0; b < 2; b++) {
      for (int i = 0; i < 5; i++) {
          delay(600);
          int curr_val = receive_incoming_signal();
          if (curr_val ^ last_val != 0) {
            Serial.print("Received a 1\n");
            code[i] = '1';
          } else {
            Serial.print("Received a 0\n");
            code[i] = '0';
          }
          last_val = curr_val;
      }
      Serial.print("Code: ");
      Serial.println(code);
      
      int decoded = decode_byte(code);
      if (decoded == -1) {
        
      } else if (decoded == 16) {
        buff[buffer_end] = '\0';
        last_byte = 0;
        Serial.println("Finished transmitting");
        return;
      } else {
        hex = (hex<<b)&decoded;
      }
      Serial.print("Decoded: ");
      Serial.println(decoded, HEX);
      
    }
    //h -> 0x68 -> 104
    Serial.println(hex, HEX);
    
    buff[buffer_end] = (char)hex;
    buffer_end++;
  }
}

void loop() {
  // put your main code here, to run repeatedly:


  int curr_val = receive_incoming_signal();

  if (curr_val ^ last_val != 0) {
    Serial.print("Received a 1\n");
    last_byte = (last_byte << 1) | 1;
  } else {
    Serial.print("Received a 0\n");
    last_byte = (last_byte << 1);
  }
  last_val = curr_val;
  Serial.println(last_byte, BIN);
  
  if (last_byte == 0b11111111) {
    Serial.print("Received preamble!\n");
    char *message = receive_message();
    Serial.print("Received Message: ");
    Serial.print(message);
  } 
  delay(600);
}
