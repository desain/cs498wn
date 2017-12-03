/*******************************************************************
 * So my idea for how we do PWM is that we have the sender send
 * pulses, and each length of pulse corresponds to a different
 * sequence of bits. Like, pulse of length 20ms is a 00, 30ms is a
 * 01, 40 = 10, 50 = 11. And we need a preamble to know how to
 * divide up the bits we get, so maybe the preamble can be a pulse
 * of width 60?
 ******************************************************************/


/********* IO VARIABLES *********/
#define INPUT_PIN A0
#define OUTPUT_LED_PIN 8
#define ANALOG_READ_THRESHOLD 90

/******** SENDING VARIABLES ********/
//MAX_MESSAGE_BYTECOUNT includes the null terminator
#define MAX_MESSAGE_BYTECOUNT 10

//Wait this long after finishing one pulse to send the next one
#define DELAY_BETWEEN_PULSES 10

//The difference between a pulse width for one bit sequence (eg 00) and the pulse width for the next bit sequence (eg 01)
#define PULSE_WIDTH_DELTA 10

//The pulse for sending 0 will have this width
#define SHORTEST_PULSE_WIDTH 20

//How many bits do we want to convey with a single pulse? Should be a power of 2
#define BITS_PER_PULSE 2
constexpr int PULSES_PER_BYTE = 8 / BITS_PER_PULSE;
constexpr int NUM_PULSE_WIDTHS = 1 << BITS_PER_PULSE;
constexpr int DATA_MASK = (1 << BITS_PER_PULSE) - 1; //produces an int with a number of 1s equal to BITS_PER_PULSE
constexpr int PREAMBLE_PULSE_WIDTH = SHORTEST_PULSE_WIDTH + NUM_PULSE_WIDTHS * PULSE_WIDTH_DELTA;
int lookup_pulse_width[NUM_PULSE_WIDTHS]; //Will be initialized in setup()

/********* RECEIVING VARIABLES ********/
#define DEBOUNCE_THRESHOLD 10

/******* PRINT DEBUGGING VARIABLES *******/
// Serial printing tends to make loops take a lot longer, so let's not use these with short gaps
#define DEBUG_PRINT_TIME 0
#define DEBUG_PRINT_SENT_BITS_AND_HILOS 0
#define DEBUG_PRINT_RECEIVED_BITS_AND_HILOS 0
#define DEBUG_PRINT_RECEIVED_HILOS_EVERY_ITER 0

unsigned long current_time;

enum sending_state {
  WAITING_FOR_DATA_TO_SEND,
  SENDING_PULSE,
  WAITING_TO_SEND_NEXT_PULSE,
};

struct sending {
  unsigned long next_tick_time;
  int cur_message_idx;
  int cur_message_bytecount;
  int num_pulses_sent_for_cur_byte;
  enum sending_state state;
  char message[MAX_MESSAGE_BYTECOUNT];
} sending;

enum receiving_pulse_state {
  WAITING_FOR_PULSE,
  RECEIVING_PULSE
};

struct receiving {
  unsigned long cur_pulse_start_time;
  unsigned long debounce_cur_pulse_tentative_end_time;
  int debounce_num_lows_read;
  enum receiving_pulse_state pulse_state;
} receiving;

struct bytes {
  byte cur_byte;
  byte num_bits_in_cur_byte;
} bytes;

struct packets {
  int terminator_idx;
  char contents[MAX_MESSAGE_BYTECOUNT];
} packets;

void setup() {
  // put your setup code here, to run once:
  pinMode(INPUT_PIN, INPUT);
  Serial.begin(9600);
  Serial.println("BEGIN RECEIVING");

  //////////// Setup sending ////////////
  pinMode(OUTPUT_LED_PIN, OUTPUT);
  sending.state = WAITING_FOR_DATA_TO_SEND;
  sending.next_tick_time = 0;
  for (int data = 0; data < NUM_PULSE_WIDTHS; data++) {
    lookup_pulse_width[data] = SHORTEST_PULSE_WIDTH + PULSE_WIDTH_DELTA * data;
  }

  //////////// Setup receiving /////////////
  receiving.pulse_state = WAITING_FOR_PULSE;
  bytes.cur_byte = 0;
  bytes.num_bits_in_cur_byte = 0;
  packets.terminator_idx = 0;
  packets.contents[0] = '\0';
}

void debug_print_time() {
  #if DEBUG_PRINT_TIME == 1
  Serial.print("[Current time = ");
  Serial.print(current_time);
  Serial.print(", next send time = ");
  Serial.print(sending.next_tick_time);
  Serial.print(", next sample time = ");
  Serial.print(hilos.next_sample_time);
  Serial.print("] ");
  #endif
}

inline void sending_send_pulse(int width) {
  digitalWrite(OUTPUT_LED_PIN, HIGH);
  sending.next_tick_time = current_time + width;
  sending.state = SENDING_PULSE;
}

void loop() {

  current_time = millis();

  ///////////// SENDING //////////////

  if (current_time >= sending.next_tick_time) {
    switch (sending.state) {
    case WAITING_FOR_DATA_TO_SEND: {
      int bytes_waiting = Serial.available();
      if (bytes_waiting > 0) {
        if (bytes_waiting >= MAX_MESSAGE_BYTECOUNT-1) {
          bytes_waiting = MAX_MESSAGE_BYTECOUNT-1;
        }
        for (int i = 0; i < bytes_waiting; i++) {
          sending.message[i] = Serial.read();
        }
        sending.message[bytes_waiting] = '\0';
        sending.cur_message_bytecount = bytes_waiting+1;

        sending_send_pulse(PREAMBLE_PULSE_WIDTH);
        //Act like we just finished sending the -1st byte in the message
        sending.cur_message_idx = -1;
        sending.num_pulses_sent_for_cur_byte = PULSES_PER_BYTE;
        
        Serial.print("Sending Message [bytecount:");
        Serial.print(sending.cur_message_bytecount);
        Serial.print("]: ");
        Serial.println(sending.message);
      }
      break;
    }
    case SENDING_PULSE:
      //We just finished sending a pulse.
      digitalWrite(OUTPUT_LED_PIN, LOW);
      //Will there be a next pulse?
      if (sending.cur_message_idx == sending.cur_message_bytecount-1 && sending.num_pulses_sent_for_cur_byte == PULSES_PER_BYTE) {
        //We just sent the last pulse in the message. Wait for a new message.
        sending.state = WAITING_FOR_DATA_TO_SEND;
      } else {
        //There will be another one
        sending.state = WAITING_TO_SEND_NEXT_PULSE;
        sending.next_tick_time = current_time + DELAY_BETWEEN_PULSES;
      }
      break;
    case WAITING_TO_SEND_NEXT_PULSE:
      if (sending.num_pulses_sent_for_cur_byte == PULSES_PER_BYTE) {
        sending.cur_message_idx++;
        sending.num_pulses_sent_for_cur_byte = 0;
      }
      int pulses_after_this = PULSES_PER_BYTE - sending.num_pulses_sent_for_cur_byte - 1;
      int bits_after_this = BITS_PER_PULSE * pulses_after_this;
      int data = (sending.message[sending.cur_message_idx] >> bits_after_this) & DATA_MASK;
      int width = lookup_pulse_width[data];
      sending_send_pulse(width);
      sending.num_pulses_sent_for_cur_byte++;
      break;
    }
  }

  ////////////// RECEIVING /////////////////
  
  bool hilo = analogRead(INPUT_PIN) > ANALOG_READ_THRESHOLD;

  switch (receiving.pulse_state) {
  case WAITING_FOR_PULSE:
    if (hilo) {
      receiving.pulse_state = RECEIVING_PULSE;
      receiving.cur_pulse_start_time = current_time;
      receiving.debounce_num_lows_read = 0;
    }
    break;
  case RECEIVING_PULSE:
    if (hilo) {
      receiving.debounce_num_lows_read = 0;
    } else {
      receiving.debounce_cur_pulse_tentative_end_time = current_time;
      receiving.debounce_num_lows_read++;
    }

    if (receiving.debounce_num_lows_read > DEBOUNCE_THRESHOLD) {
      receiving.pulse_state = WAITING_FOR_PULSE;
      unsigned long pulse_width = receiving.debounce_cur_pulse_tentative_end_time - receiving.cur_pulse_start_time;
      on_read_pulse(pulse_width);
    }
  }
}

void on_read_pulse(int width) {
  Serial.print("Got a pulse of width ");
  Serial.println(width, DEC);

  //TODO figure out which possible pulse width is closest to the actual width

  int data = /* ??? */ 0;
  on_read_data(data);
}

void on_read_data(int data) {
  bytes.cur_byte = (bytes.cur_byte << BITS_PER_PULSE) | data;
  bytes.num_bits_in_cur_byte += BITS_PER_PULSE;
  if (bytes.num_bits_in_cur_byte == 8) {
    byte byte_read = bytes.cur_byte;
    bytes.cur_byte = 0;
    bytes.num_bits_in_cur_byte = 0;
    on_read_byte(byte_read);
  }
}

void on_read_byte(byte b) {

  Serial.print("Got byte ");
  Serial.println((char)b);
  
  packets.contents[packets.terminator_idx++] = b;
  packets.contents[packets.terminator_idx] = '\0';
  if (packets.terminator_idx == MAX_MESSAGE_BYTECOUNT-1 || b == '\0') {
    Serial.print("Finished receiving packet! Contents: ");
    Serial.println(packets.contents);
    packets.terminator_idx = 0;
    packets.contents[0] = '\0';
    // We finished receiving a packet, so go back to waiting for the preamble
    //receiving_start_waiting_for_packet();
  }
}

