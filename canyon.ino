/*
    Project:    Canyon
    Purpose:    OPL3 ISA Sound Card controller
    Author:     Andrew Greenwood
    Date:       July 2018

    Designed for Yamaha Audician sound card
*/

#define IO_WAIT     20

class ISABus {
    public:
        ISABus(
            int outputPin,  // To first shift register's serial input (SER)
            int inputPin,   // From final shift register's serial output (Qh')
            int clockPin,   // Shift clock pin (RCLK,RCLK,CLK)
            int loadPin,    // To S1 pin of final shift register
            int ioWritePin, // To IOW pin of device (active low)
            int ioReadPin,  // To IOR pin of device (active low)
            int resetPin);  // To RESET pin of device (active low)

        void reset();

        void write(
            uint16_t address,
            uint8_t data);

        uint8_t read(
            uint16_t address);

    private:
        int m_outputPin;
        int m_inputPin;
        int m_clockPin;
        int m_loadPin;
        int m_ioWritePin;
        int m_ioReadPin;
        int m_resetPin;
};

ISABus::ISABus(
    int outputPin,
    int inputPin,
    int clockPin,
    int loadPin,
    int ioWritePin,
    int ioReadPin,
    int resetPin)
: m_outputPin(outputPin), m_inputPin(inputPin), m_clockPin(clockPin),
  m_loadPin(loadPin), m_ioWritePin(ioWritePin), m_ioReadPin(ioReadPin),
  m_resetPin(resetPin)
{
    reset();
}

void ISABus::reset()
{
    // Raise IOW and IOR on the ISA bus
    pinMode(m_ioWritePin, OUTPUT);
    pinMode(m_ioReadPin, OUTPUT);
    digitalWrite(m_ioWritePin, HIGH);
    digitalWrite(m_ioReadPin, HIGH);

    // Lower RESET on the ISA bus
    pinMode(m_resetPin, OUTPUT);
    digitalWrite(m_resetPin, LOW);

    pinMode(m_outputPin, OUTPUT);
    pinMode(m_inputPin, INPUT);
    pinMode(m_clockPin, OUTPUT);
    pinMode(m_loadPin, OUTPUT);

    // Ready to go
    digitalWrite(m_resetPin, HIGH);
}

#define DO_LSB 1

void ISABus::write(
    uint16_t address,
    uint8_t data)
{
    // Put the data shift register into 'shift' mode
    digitalWrite(m_loadPin, LOW);

#ifdef DO_LSB
    // The data goes first so it ends up in the bi-directional shift register
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, data);

    // Next, we write the address
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, address & 0xff);
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, (address & 0xff00) >> 8);
#else
    // The data goes first so it ends up in the bi-directional shift register
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, data);

    // Next, we write the address
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, (address & 0xff00) >> 8);
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, address & 0xff);
#endif

    // The output pin is wired up to the SRCLK on the 595s as well as the SER
    // input, so we flip it here to perform a store
    digitalWrite(m_outputPin, LOW);
    digitalWrite(m_outputPin, HIGH);

    // Lower the IOW pin on the ISA bus to indicate we're writing data
    digitalWrite(m_ioWritePin, LOW);
    delay(IO_WAIT);
    digitalWrite(m_ioWritePin, HIGH);
}

uint8_t ISABus::read(
    uint16_t address)
{
#ifdef DO_LSB
    // Send out the address
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, address & 0xff);
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, (address & 0xff00) >> 8);
#else
    // Send out the address
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, (address & 0xff00) >> 8);
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, address & 0xff);
#endif

    // The output pin is wired up to the SRCLK on the 595s as well as the SER
    // input, so we flip it here to perform a store
    digitalWrite(m_outputPin, LOW);
    digitalWrite(m_outputPin, HIGH);

    // Put the data shift register into 'load' mode
    digitalWrite(m_loadPin, HIGH);

    // Lower the IOR pin on the ISA bus to indicate we're writing data
    //Serial.println("IOR LOW");
    digitalWrite(m_ioReadPin, LOW);

    // Give the device some time to respond
    delay(IO_WAIT);

    // Load the ISA data pin content
//    Serial.println("Receiving data");
    digitalWrite(m_clockPin, LOW);
    digitalWrite(m_clockPin, HIGH);
//    delay(500);

    // Stop writing
//    Serial.println("IOR HIGH");
    digitalWrite(m_ioReadPin, HIGH);

    // Put the data shift register into 'shift' mode
    digitalWrite(m_loadPin, LOW);

//    Serial.println("Shifting data in");
    //return shiftIn(m_inputPin, m_clockPin, MSBFIRST);
    uint8_t data = 0;
    data = digitalRead(m_inputPin);

    for (int i = 1; i < 8; ++ i) {
        digitalWrite(m_clockPin, LOW);
        digitalWrite(m_clockPin, HIGH);
        data |= digitalRead(m_inputPin) << i;
    }
    digitalWrite(m_clockPin, LOW);

    return data;
}


const int isaWritePin = 2;  // nc
const int isaInputPin = 3;
const int isaOutputPin = 4;
const int isaReadPin = 5;   // nc
const int isaLoadPin = 6;
const int isaClockPin = 7;
const int isaResetPin = 8;  // nc

ISABus isaBus(isaOutputPin, isaInputPin, isaClockPin, isaLoadPin,
              isaWritePin, isaReadPin, isaResetPin);

void pnp_write(
  byte reg,
  byte data)
{
  delayMicroseconds(4);
  isaBus.write(0x279, reg);
  delayMicroseconds(4);
  isaBus.write(0xa79, data);
}

void setup()
{
    int i;
    Serial.begin(9600);

    delay(1000);
    Serial.println("Starting");

    byte key[32] = {
        0xb1, 0xd8, 0x6c, 0x36, 0x9b, 0x4d, 0xa6, 0xd3,
        0x69, 0xb4, 0x5a, 0xad, 0xd6, 0xeb, 0x75, 0xba,
        0xdd, 0xee, 0xf7, 0x7b, 0x3d, 0x9e, 0xcf, 0x67,
        0x33, 0x19, 0x8c, 0x46, 0xa3, 0x51, 0xa8, 0x54
    };

    pnp_write(0x02, 0x01);

    Serial.println("Sending key");
    isaBus.write(0x279, 0x00);
    isaBus.write(0x279, 0x00);
    for (i = 0; i < 32; ++ i) {
        isaBus.write(0x0279, key[i]);
        delay(1);
    }

    // Set read address 0x0203
    pnp_write(0x00, 0x203 >> 2);

    // Wake
    Serial.println("Wake");
    pnp_write(0x03, 0x81);

    // Select logical device 0
    pnp_write(0x07, 0x00);

    // Deactivate
    pnp_write(0x30, 0x00);

    // Assign address 0
    pnp_write(0x60, 0x02);
    pnp_write(0x61, 0x20);

    Serial.println("Reading return value");

    isaBus.write(0x0279, 0x60);
    delay(1);
    Serial.print(isaBus.read(0x0203), HEX);
    Serial.println("");

    isaBus.write(0x0279, 0x61);
    delay(1);
    Serial.print(isaBus.read(0x0203), HEX);
    Serial.println("");
    Serial.println("--------------------------------------");
}

void loop()
{
    int i;
  
    for (i = 0; i < 256; ++ i) {
        //isaBus.write(0x279);
        Serial.print(isaBus.read(0x220), HEX);
        Serial.println("");
        delay(2000);
    }
}
