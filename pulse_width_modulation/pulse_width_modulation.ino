/*******************************************************************
   So my idea for how we do PWM is that we have the sender send
   pulses, and each length of pulse corresponds to a different
   sequence of bits. Like, pulse of length 200ms is a 00, 300ms is a
   01, 400 = 10, 500 = 11. And we need a preamble to know how to
   divide up the bits we get, so maybe the preamble can be a pulse
   of width 600?
 ******************************************************************/
/********* TESTING VARIABLES **********/
#define TESTING_PRINT 1


/********* IO VARIABLES *********/
#define INPUT_PIN A0
#define OUTPUT_LED_PIN 8
#define OUTPUT_LED_PIN_TWO 9
#define OUTPUT_LED_PIN_THREE 7
#define OUTPUT_LED_PIN_FOUR 10
#define OUTPUT_LED_PIN_FIVE 6
#define OUTPUT_LED_PIN_SIX 5
#define OUTPUT_LED_PIN_SEVEN 4
/*********BLUE RECEIVER THRESHOLD*********/
//#define ANALOG_READ_THRESHOLD 185
/*********RED RECEIVER THRESHOLD**********/
#define ANALOG_READ_THRESHOLD 170

/******** SENDING VARIABLES ********/
//MAX_MESSAGE_BYTECOUNT includes the null terminator
#define MAX_MESSAGE_BYTECOUNT 10

//Wait this long after finishing one pulse to send the next one
#define DELAY_BETWEEN_PULSES 20

//The difference between a pulse width for one bit sequence (eg 00) and the pulse width for the next bit sequence (eg 01)
#define PULSE_WIDTH_DELTA 20
//Kinda arbitrary value, but - as we're trying to interpret a received pulse width, how far off from the ideal width of data X do we allow it to be and still interpret it as X?
constexpr int MAX_ALLOWED_PULSE_WIDTH_ERROR = PULSE_WIDTH_DELTA / 4;

//The pulse for sending 0 will have this width
#define SHORTEST_PULSE_WIDTH 100

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
#define DEBUG_PRINT_SENT_PULSES 0
#define DEBUG_PRINT_RECEIVED_BITS_AND_HILOS 0
#define DEBUG_PRINT_RECEIVED_HILOS_EVERY_ITER 0

unsigned long current_time;
unsigned long next_send_time_test = 0;

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

void sending_send_pulse(int width) {
#if DEBUG_PRINT_SENT_PULSES == 1
  Serial.print("Sending a pulse of width ");
  Serial.println(width, DEC);
#endif
  digitalWriteAll(HIGH);
  sending.next_tick_time = current_time + width;
  sending.state = SENDING_PULSE;
}

enum receiving_pulse_state {
  WAITING_FOR_PULSE,
  RECEIVING_PULSE
};

enum receiving_message_state {
  WAITING_FOR_PREAMBLE,
  RECEIVING_DATA
};

struct receiving {
  unsigned long cur_pulse_start_time;
  unsigned long debounce_cur_pulse_tentative_end_time;
  int debounce_num_lows_read;
  enum receiving_pulse_state pulse_state;
  enum receiving_message_state message_state;
} receiving;

struct bytes {
  byte cur_byte;
  byte num_bits_in_cur_byte;
} bytes;

struct packets {
  int terminator_idx;
  char contents[MAX_MESSAGE_BYTECOUNT];
} packets;

void receiving_start_waiting_for_packet() {
  receiving.message_state = WAITING_FOR_PREAMBLE;
  bytes.cur_byte = 0;
  bytes.num_bits_in_cur_byte = 0;
  packets.terminator_idx = 0;
  packets.contents[0] = '\0';
}

void setup() {
  // put your setup code here, to run once:
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  pinMode(INPUT_PIN, INPUT);
  Serial.begin(9600);
  Serial.println("BEGIN RECEIVING");

  //////////// Setup sending ////////////
  pinMode(OUTPUT_LED_PIN, OUTPUT);
  pinMode(OUTPUT_LED_PIN_TWO, OUTPUT);
  pinMode(OUTPUT_LED_PIN_THREE, OUTPUT);
  pinMode(OUTPUT_LED_PIN_FOUR, OUTPUT);
  pinMode(OUTPUT_LED_PIN_FIVE, OUTPUT);
  pinMode(OUTPUT_LED_PIN_SIX, OUTPUT);
  pinMode(OUTPUT_LED_PIN_SEVEN, OUTPUT);

  sending.state = WAITING_FOR_DATA_TO_SEND;
  sending.next_tick_time = 0;
  for (int data = 0; data < NUM_PULSE_WIDTHS; data++) {
    lookup_pulse_width[data] = SHORTEST_PULSE_WIDTH + PULSE_WIDTH_DELTA * data;
  }
  for (int data = 0; data < NUM_PULSE_WIDTHS; data++) {
    Serial.print("lookup_pulse_width[");
    Serial.print(data);
    Serial.print("] = ");
    Serial.print(lookup_pulse_width[data]);
    Serial.println();
  }
  Serial.print("Preamble pulse width = ");
  Serial.println(PREAMBLE_PULSE_WIDTH);

  //////////// Setup receiving /////////////
  receiving.pulse_state = WAITING_FOR_PULSE;
  receiving_start_waiting_for_packet();
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

void digitalWriteAll(int state)
{
  digitalWrite(OUTPUT_LED_PIN, state);
  digitalWrite(OUTPUT_LED_PIN_TWO, state);
  digitalWrite(OUTPUT_LED_PIN_THREE, state);
  digitalWrite(OUTPUT_LED_PIN_FOUR, state);
  digitalWrite(OUTPUT_LED_PIN_FIVE, state);
  digitalWrite(OUTPUT_LED_PIN_SIX, state);
  digitalWrite(OUTPUT_LED_PIN_SEVEN, state);
}

void loop() {
  current_time = millis();

  ///////////// SENDING //////////////

  if (current_time >= sending.next_tick_time) {
    switch (sending.state) {
      case WAITING_FOR_DATA_TO_SEND: {
          int bytes_waiting = Serial.available();
#if TESTING_PRINT == 1
          if (current_time >= next_send_time_test) {
            bytes_waiting = 10;
            for (int i = 0; i < bytes_waiting; i++) {
              sending.message[i] = (int)'a' + i;
            }
            sending.message[bytes_waiting] = '\0';
            sending.cur_message_bytecount = bytes_waiting + 1;
            sending_send_pulse(PREAMBLE_PULSE_WIDTH);
            //Act like we just finished sending the -1st byte in the message
            sending.cur_message_idx = -1;
            sending.num_pulses_sent_for_cur_byte = PULSES_PER_BYTE;
            next_send_time_test = current_time + 10000;
          }
#endif
          if (bytes_waiting > 0) {
            if (bytes_waiting >= MAX_MESSAGE_BYTECOUNT) {
              bytes_waiting = MAX_MESSAGE_BYTECOUNT - 1;
            }
            for (int i = 0; i < bytes_waiting; i++) {
              sending.message[i] = Serial.read();
            }
            sending.message[bytes_waiting] = '\0';
            sending.cur_message_bytecount = bytes_waiting + 1;

            //Serial.print("Sending Message [bytecount:");
            //Serial.print(sending.cur_message_bytecount);
            //Serial.print("]: ");
            //Serial.println(sending.message);

            sending_send_pulse(PREAMBLE_PULSE_WIDTH);
            //Act like we just finished sending the -1st byte in the message
            sending.cur_message_idx = -1;
            sending.num_pulses_sent_for_cur_byte = PULSES_PER_BYTE;
          } else {
            sending.next_tick_time = current_time + DELAY_BETWEEN_PULSES; //we don't want to check for serial data too fast
          }
          break;
        }
      case SENDING_PULSE:
        //  Serial.println("sending pulse state");
        //We just finished sending a pulse.
        digitalWriteAll(LOW);

        //Will there be a next pulse?
        if (sending.cur_message_idx == sending.cur_message_bytecount - 1 && sending.num_pulses_sent_for_cur_byte == PULSES_PER_BYTE) {
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
  //Serial.println(hilo);
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
//  Serial.print("Got a pulse of width ");
  Serial.println(width, DEC);

  switch (receiving.message_state) {
    case WAITING_FOR_PREAMBLE: {
        int diff_from_preamble_width = width - PREAMBLE_PULSE_WIDTH;
        if (abs(diff_from_preamble_width) < MAX_ALLOWED_PULSE_WIDTH_ERROR) {
          Serial.println("Got preamble!");
          receiving.message_state = RECEIVING_DATA;
        }
        break;
      }
    case RECEIVING_DATA:
      //TODO is there a better way to do this than a for loop?? I feel like there should be but idk what it is
      for (int data = 0; data < NUM_PULSE_WIDTHS; data++) {
        int diff_from_data_width = width - lookup_pulse_width[data];
        if (abs(diff_from_data_width) < MAX_ALLOWED_PULSE_WIDTH_ERROR) {
          on_read_data(data);
          return;
        }
      }
      //If we got to here, then we didn't have valid data
      Serial.println("Got a bad pulse width, discarding packet");
      receiving_start_waiting_for_packet();
      break;
  }
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
  if (packets.terminator_idx == MAX_MESSAGE_BYTECOUNT - 1 || b == '\0') {
    Serial.print("Finished receiving packet! Contents: ");
    Serial.println(packets.contents);
    packets.terminator_idx = 0;
    packets.contents[0] = '\0';
    // We finished receiving a packet, so go back to waiting for the preamble
    //receiving_start_waiting_for_packet();
  }
}

