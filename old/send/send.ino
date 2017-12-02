#define STAY_TIME 300
#define PREAMBLE 0b10000
#define LED_PIN 8

const int lookup4b[16] = {
  /* 0b0000 */ 0b11110,
  /* 0b0001 */ 0b01001,
  /* 0b0010 */ 0b10100,
  /* 0b0011 */ 0b10101,
  /* 0b0100 */ 0b01010,
  /* 0b0101 */ 0b01011,
  /* 0b0110 */ 0b01110,
  /* 0b0111 */ 0b01111,
  /* 0b1000 */ 0b10010,
  /* 0b1001 */ 0b10011,
  /* 0b1010 */ 0b10110,
  /* 0b1011 */ 0b10111,
  /* 0b1100 */ 0b11010,
  /* 0b1101 */ 0b11011,
  /* 0b1110 */ 0b11100,
  /* 0b1111 */ 0b11101
};

char message[] = "hello from the other side";
int message_len = strlen(message);
// msg is 0110 1000
//send: 10000 01110 10010
bool next_4b_block_highorder = true;
int cur_message_idx = 0;

byte cur_5b_block = 0;
int bits_sent_in_5b_block = 0;

bool prev_hilo;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  //Start as if we just finished sending
  cur_message_idx = message_len;
  bits_sent_in_5b_block = 5;
  
}

void loop() {
  if (bits_sent_in_5b_block == 5) {
    //setup for sending next block
    bits_sent_in_5b_block = 0;
    
    //get next block
    if (cur_message_idx == message_len) {
      cur_5b_block = PREAMBLE;
      cur_message_idx = 0;
    } else {
      byte cur_4b_block;
      if (next_4b_block_highorder) {
        //Send the high order bits of the current character
        cur_4b_block = (message[cur_message_idx] >> 4) & 0b1111;
      } else {
        //Send the low order bits of the current character. After this we'll need to move to the next character
        cur_4b_block = message[cur_message_idx++] & 0b1111;
      }
      next_4b_block_highorder = !next_4b_block_highorder;
      cur_5b_block = lookup4b[cur_4b_block];
    }
  }

  //get current hilo to send
  bool cur_bit = (cur_5b_block >> (4-bits_sent_in_5b_block)) & 1;

  /*
  Serial.print("Sending bit ");
  Serial.print(cur_bit, BIN);
  Serial.print(" which is the bit at index ");
  Serial.print(4-bits_sent_in_5b_block);
  Serial.print(" in ");
  Serial.print(cur_5b_block, BIN);
  Serial.println();
  */
  
  bool cur_hilo = prev_hilo ^ cur_bit; //NRZI encoding
  prev_hilo = cur_hilo;
  //send current bit
  digitalWrite(LED_PIN, cur_hilo ? HIGH : LOW);
  delay(STAY_TIME);
  bits_sent_in_5b_block++;
}
