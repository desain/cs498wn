#define SAMPLE_GAP_MILLIS 1000
#define FUDGE 10
#define THRESHOLD 300
//PACKET_SIZE_BYTES doesn't include the null terminator
#define PACKET_SIZE_BYTES 2
#define DECAY 1
#define NOTHING 0b00000
#define PREAMBLE 0b10000
#define OUTPUT_LED_PIN 8
#define INPUT_PIN A0
#define MAX_MESS_LEN 30
#define HIGH_TICKS 128
#define OUTPUT_LED_TIME_PIN 9

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

bool waiting_for_sending_data = true;
char message [MAX_MESS_LEN];
int message_len = strlen(message);
// msg is 0110 1000
//send: 10000 01110 10010
bool next_4b_block_highorder = true;
int cur_message_idx = 0;

byte cur_5b_block = 0;
int bits_sent_in_5b_block = 0;
int block_counter = 0;

bool prev_hilo;
bool timeonoff = true;
unsigned long next_send_time;

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
  pinMode(OUTPUT_LED_TIME_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("BEGIN RECEIVING");

  //////////// Setup sending ////////////
  pinMode(OUTPUT_LED_PIN, OUTPUT);
  //Start as if we just finished sending
  cur_message_idx = message_len;
  bits_sent_in_5b_block = 5;
  next_send_time = SAMPLE_GAP_MILLIS;

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

  ///////////// SENDING //////////////

  if (current_time >= next_send_time) {
//    next_send_time += SAMPLE_GAP_MILLIS;
    next_send_time = current_time + SAMPLE_GAP_MILLIS;

    if (waiting_for_sending_data) {
      int bytes_waiting = Serial.available();
      if (bytes_waiting > 0) {
        if (bytes_waiting > MAX_MESS_LEN) {
          bytes_waiting = MAX_MESS_LEN-1;
        }
        waiting_for_sending_data = false;
        for (int i = 0; i < bytes_waiting; i++) {
          message[i] = Serial.read();
        }
        message[bytes_waiting] = '\0';
        message_len = bytes_waiting+1;
        sending_reset();
        cur_5b_block = PREAMBLE;
        Serial.print("Sending Message [len:");
        Serial.print(message_len);
        Serial.print("]: ");
        Serial.println(message);
      } else {
        if (bits_sent_in_5b_block == 5) {
          //setup for sending next block
          bits_sent_in_5b_block = 0;
          cur_5b_block = NOTHING;
        }
      }
    }
    else {
      if (bits_sent_in_5b_block == 5) {
        Serial.print("Sent block # ");
        Serial.println(block_counter++);
        //setup for sending next block
        bits_sent_in_5b_block = 0;
        
        //get next block
        if (cur_message_idx == message_len) {
          cur_5b_block = NOTHING;
          waiting_for_sending_data = true;
          Serial.println("Finished sending message");
          block_counter = 0;
        } else {
          byte cur_4b_block;
          if (next_4b_block_highorder) {
            //Send the high order bits of the current character
            Serial.print("Sending high order of ");
            Serial.println(message[cur_message_idx]);
            cur_4b_block = (message[cur_message_idx] >> 4) & 0b1111;
          } else {
            //Send the low order bits of the current character. After this we'll need to move to the next character
            cur_4b_block = message[cur_message_idx++] & 0b1111;
          }
          next_4b_block_highorder = !next_4b_block_highorder;
          cur_5b_block = lookup4b[cur_4b_block];
        }
      }

    }
      
    //get current hilo to send
    bool cur_bit = (cur_5b_block >> (4-bits_sent_in_5b_block)) & 1;

    if (!waiting_for_sending_data){
      Serial.print("Sending bit ");
      Serial.print(cur_bit, BIN);
      Serial.print(" which is the bit at index ");
      Serial.print(4-bits_sent_in_5b_block);
      Serial.print(" in ");
      Serial.print(cur_5b_block, BIN);
      Serial.println();
    }
    bool cur_hilo = prev_hilo ^ cur_bit; //NRZI encoding
    prev_hilo = cur_hilo;
    //send current bit
    digitalWrite(OUTPUT_LED_PIN, cur_hilo ? HIGH : LOW);
    bits_sent_in_5b_block++;
    digitalWrite(OUTPUT_LED_TIME_PIN, timeonoff ? HIGH:LOW);
    timeonoff = ! timeonoff;
  }

  ////////////// RECEIVING /////////////////
    
  //Update the ticker
  int analog_read_input = analogRead(INPUT_PIN);
  bool hilo = analogRead(INPUT_PIN) > THRESHOLD;
  if (hilo) {
//      Serial.println(analog_read_input);

    //We saw a HI, so set the value to maximum, since we don't often get spurious 1s
    hilos.val = 255;
    if (!hilos.prev_val_hi) {
      //If we just transitioned low-to-high, re-anchor the clock
      hilos.next_sample_time = current_time + SAMPLE_GAP_MILLIS-FUDGE;
    }
  } else {
//    Serial.println("");
    //We saw a LO, so decrement our hilo tracker
    if (hilos.val < DECAY) {
      hilos.val = 0;
    } else {
      hilos.val -= DECAY;
    }
  }

  hilos.prev_val_hi = hilos.val >= HIGH_TICKS;

  //Emit sample if needed
  if (current_time >= hilos.next_sample_time) {
    hilos.next_sample_time += SAMPLE_GAP_MILLIS;
    on_read_hilo(hilos.prev_val_hi);
  }
}

void sending_reset() {
  cur_message_idx = 0;
  bits_sent_in_5b_block = 0;
  block_counter = 0;
}

void receiving_reset() {
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
    receiving_reset();
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
    blocks.state = WAIT_FOR_PREAMBLE;
  }


}
