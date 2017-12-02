#define SAMPLE_GAP_MILLIS 300
#define FUDGE 10
#define THRESHOLD 90
//PACKET_SIZE_BYTES doesn't include the null terminator
#define PACKET_SIZE_BYTES 25
#define DECAY 1
#define PREAMBLE 0b10000

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
  unsigned long current_time;
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
  pinMode(0, INPUT);
  Serial.begin(9600);
  
  hilos.current_time = 0;
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
  hilos.current_time = millis();
  
  //Update the ticker
  bool hilo = analogRead(0) > THRESHOLD;
  if (hilo) {
    //We saw a HI, so set the value to maximum, since we don't often get spurious 1s
    hilos.val = 255;
    if (!hilos.prev_val_hi) {
      //If we just transitioned low-to-high, re-anchor the clock
      hilos.next_sample_time = hilos.current_time + SAMPLE_GAP_MILLIS-FUDGE;
    }
  } else {
    //We saw a LO, so decrement our hilo tracker
    if (hilos.val < DECAY) {
      hilos.val = 0;
    } else {
      hilos.val -= DECAY;
    }
  }

  hilos.prev_val_hi = hilos.val >= 128;

  //Emit sample if needed
  if (hilos.current_time >= hilos.next_sample_time) {
    hilos.next_sample_time += SAMPLE_GAP_MILLIS;
    on_read_hilo(hilos.prev_val_hi);
  }
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
    //TODO WE RECEIVED A BAD BLOCK
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

