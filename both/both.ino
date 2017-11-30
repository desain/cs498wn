#define SAMPLE_GAP_MILLIS 300
#define FUDGE 10
#define THRESHOLD 90
//PACKET_SIZE_BYTES doesn't include the null terminator
#define PACKET_SIZE_BYTES 2
#define DECAY 1
#define PREAMBLE 0b10000
#define OUTPUT_LED_PIN 8
#define INPUT_PIN 0



#define DEBUG_PRINT_TIME 0
#define DEBUG_SEND 0
#define DEBUG_LOOPBACK 0
#define DEBUG_HILOS 0


/************* SENDING **************/
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

char message[] = "hi";
int message_len = strlen(message);

struct sending {
  byte cur_5b_block = 0;
  int bits_sent_in_5b_block = 0;
  bool next_4b_block_highorder = true;
  int cur_message_idx = 0;
  bool prev_hilo;
  unsigned long next_send_time;
} sending;

/************* RECEIVING ***************/
int lookup5b[32] = {
  /* 00000 */ -1,
  /* 00001 */ -1,
  /* 00010 */ -1,
  /* 00011 */ -1,
  /* 00100 */ -1,
  /* 00101 */ -1,
  /* 00110 */ -1,
  /* 00111 */ -1,
  /* 01000 */ -1,
  /* 01001 */ 0b0001,
  /* 01010 */ 0b0100,
  /* 01011 */ 0b0101,
  /* 01100 */ -1,
  /* 01101 */ -1,
  /* 01110 */ 0b0110,
  /* 01111 */ 0b0111,
  /* 10000 */ -1,
  /* 10001 */ -1,
  /* 10010 */ 0b1000,
  /* 10011 */ 0b1001,
  /* 10100 */ 0b0010,
  /* 10101 */ 0b0011,
  /* 10110 */ 0b1010,
  /* 10111 */ 0b1011,
  /* 11000 */ -1,
  /* 11001 */ -1,
  /* 11010 */ 0b1100,
  /* 11011 */ 0b1101,
  /* 11100 */ 0b1110,
  /* 11101 */ 0b1111,
  /* 11110 */ 0b0000,
  /* 11111 */ -1
};

struct hilos {
  unsigned long next_sample_time;
  byte val;
  bool prev_val_hi;
} hilos;

enum blocks_state {
  WAIT_FOR_PREAMBLE,
  RECEIVING_DATA
};

struct blocks {
  enum blocks_state state;
  byte cur_block_size;
  byte cur_block;
  bool prev_hilo;
} blocks;

struct bytes {
  byte prev_block;
  bool has_prev_block;
} bytes;

struct packets {
  int terminator_idx;
  char contents[PACKET_SIZE_BYTES+1];
} packets;

void setup() {
  // put your setup code here, to run once:
  pinMode(INPUT_PIN, INPUT);
  Serial.begin(9600);
  Serial.println("BEGIN RECEIVING");

  //////////// Setup sending ////////////
  pinMode(OUTPUT_LED_PIN, OUTPUT);
  //Start as if we just finished sending
  sending.cur_message_idx = message_len;
  sending.bits_sent_in_5b_block = 5;
  sending.next_send_time = SAMPLE_GAP_MILLIS;

  //////////// Setup receiving /////////////
  hilos.next_sample_time = 0-1; //set to max value because we want to anchor it later
  hilos.val = 0;
  hilos.prev_val_hi = 0;

  blocks.state = WAIT_FOR_PREAMBLE;
  blocks.cur_block_size = 0;
  blocks.cur_block = 0;
  blocks.prev_hilo = 0;

  bytes.prev_block = 0;
  bytes.has_prev_block = false;

  packets.terminator_idx = 0;
  packets.contents[0] = '\0';
}

void loop() {

  unsigned long current_time = millis();
  #if DEBUG_PRINT_TIME == 1
  Serial.print("Current time = ");
  Serial.print(current_time);
  Serial.print(", next send time = ");
  Serial.print(sending.next_send_time);
  Serial.print(", next sample time = ");
  Serial.println(hilos.next_sample_time);
  #endif

  ///////////// SENDING //////////////

  if (current_time >= sending.next_send_time) {
    sending.next_send_time = current_time + SAMPLE_GAP_MILLIS;
    
    if (sending.bits_sent_in_5b_block == 5) {
      //setup for sending next block
      sending.bits_sent_in_5b_block = 0;
      
      //get next block
      if (sending.cur_message_idx == message_len) {
        sending.cur_5b_block = PREAMBLE;
        sending.cur_message_idx = 0;
        sending.next_4b_block_highorder = true;
      } else {
        byte cur_4b_block;
        if (sending.next_4b_block_highorder) {
          //Send the high order bits of the current character
          cur_4b_block = (message[sending.cur_message_idx] >> 4) & 0b1111;
        } else {
          //Send the low order bits of the current character. After this we'll need to move to the next character
          cur_4b_block = message[sending.cur_message_idx++] & 0b1111;
        }
        sending.next_4b_block_highorder = !sending.next_4b_block_highorder;
        sending.cur_5b_block = lookup4b[cur_4b_block];
      }
    }
  
    //get current hilo to send
    bool cur_bit = (sending.cur_5b_block >> (4-sending.bits_sent_in_5b_block)) & 1;
    bool cur_hilo = sending.prev_hilo ^ cur_bit; //NRZI encoding
    sending.prev_hilo = cur_hilo;
    //send current bit

    #if DEBUG_SEND == 1
    Serial.print("Sending ");
    Serial.print(cur_hilo ? "HIGH" : "LOW ");
    Serial.print(" to represent bit ");
    Serial.print(cur_bit, BIN);
    Serial.print(" which is the bit at index ");
    Serial.print(4-sending.bits_sent_in_5b_block);
    Serial.print(" in ");
    Serial.print(sending.cur_5b_block, BIN);
    Serial.println();
    #endif

    
    digitalWrite(OUTPUT_LED_PIN, cur_hilo ? HIGH : LOW);
    sending.bits_sent_in_5b_block++;
  }

  ////////////// RECEIVING /////////////////
    
  //Update the ticker
  bool hilo = analogRead(INPUT_PIN) > THRESHOLD;
  #if DEBUG_LOOPBACK == 1
  hilo = sending.prev_hilo;
  #endif

  #if DEBUG_HILOS == 1
  Serial.print("Got hilo ");
  Serial.print(hilo);
  Serial.print(" (prev was ");
  Serial.print(blocks.prev_hilo);
  Serial.println(")");
  #endif
  
  if (hilo) {
    //We saw a HI, so set the value to maximum, since we don't often get spurious 1s
    hilos.val = 255;
    if (!hilos.prev_val_hi) {
      //If we just transitioned low-to-high, re-anchor the clock
      hilos.next_sample_time = current_time + SAMPLE_GAP_MILLIS-FUDGE;
      #if DEBUG_HILOS == 1
      Serial.println("Re-anchored clock");
      #endif
    }
  } else {
    //We saw a LO, so decrement our hilo tracker
    if (hilos.val < DECAY) {
      hilos.val = 0;
    } else {
      hilos.val -= DECAY;
    }
  }
  #if DEBUG_HILOS == 1
  Serial.print("Hilos val = ");\
  Serial.println(hilos.val);
  #endif

  hilos.prev_val_hi = hilos.val >= 128;

  //Emit sample if needed
  if (current_time >= hilos.next_sample_time) {
    hilos.next_sample_time += SAMPLE_GAP_MILLIS;
    on_read_hilo(hilos.prev_val_hi);
  }
}

void reset() {
  blocks.cur_block_size = 0;
  blocks.state = WAIT_FOR_PREAMBLE;
  bytes.has_prev_block = false;
}

void on_read_hilo(bool hilo) {

  // Save the new bit
  bool cur_bit = blocks.prev_hilo ^ hilo;
  blocks.cur_block = ((blocks.cur_block << 1) | cur_bit) & 0b11111;
  blocks.prev_hilo = hilo;

  Serial.print("Got bit ");
  Serial.println(cur_bit, BIN);

  switch (blocks.state) {
  case WAIT_FOR_PREAMBLE:
    if (blocks.cur_block == PREAMBLE) {
      Serial.println("Got preamble!");
      blocks.state = RECEIVING_DATA;
      blocks.cur_block = 0;
      blocks.cur_block_size = 0;
    }
    break;
  case RECEIVING_DATA:
    if (++blocks.cur_block_size == 5) {
      byte block = blocks.cur_block;
      blocks.cur_block = 0;
      blocks.cur_block_size = 0;
      on_read_block(block);
    }
    break;
  }
}

void on_read_block(byte five_bit_block) {
  if (lookup5b[five_bit_block] == -1) {
    Serial.print("Got a bad block, marking packet as lost\n");
    reset();
  } else {
    byte four_bit_block = lookup5b[five_bit_block];

    Serial.print("Got 4 bit block ");
    Serial.print(four_bit_block, BIN);
    Serial.println(bytes.has_prev_block ? " lower" : " upper");
    
    if (bytes.has_prev_block) {
      byte b = (bytes.prev_block << 4) | four_bit_block;
      bytes.has_prev_block = false;
      on_read_byte(b);
    } else {
      bytes.has_prev_block = true;
      bytes.prev_block = four_bit_block;
    }
  }
}

void on_read_byte(byte b) {

  Serial.print("Got byte ");
  Serial.println((char)b);
  
  packets.contents[packets.terminator_idx++] = b;
  packets.contents[packets.terminator_idx] = '\0';
  if (packets.terminator_idx == PACKET_SIZE_BYTES) {
    Serial.print("Finished receiving packet! Contents: ");
    Serial.println(packets.contents);
    packets.terminator_idx = 0;
    packets.contents[0] = '\0';
    // We finished receiving a packet, so go back to waiting for the preamble
    reset();
  }

}




