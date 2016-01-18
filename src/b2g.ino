#include <stdio.h>
#include <string.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Ethernet2.h>
#include <aREST.h>
#include <avr/wdt.h>
#include <mcp_can.h>
#include <mcp_can_dfs.h>
#include "mcp_can.h"
#include <extender.h>

#define READ_TIMEOUT_MILLISECONDS 2000 //From datasheet protocol Studer (Chapter 3.5 response delay)
//The response delay of the Xcom-232i can be up to 2 seconds.

#define REST_RET_OK 0
#define REST_RET_LOW_BAT -1
#define REST_RET_BLOCK_DISCHARGE -2
#define REST_RET_DEVICE_ERROR -3
#define REST_RET_POWER_TO_HIGH -4

float battery_soc;
float battery_Vdc=30;
float charge=0;
float charge_real=0;
float batteryLowLimits[2]= {17,20};

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 9;
MCP_CAN CAN(SPI_CS_PIN);                                    // Set CS pin 10

//Ethernet shield 195.176.65.221
byte mac[] = { 0x90, 0xA2, 0xDA, 0x10, 0x0A, 0xA6 };
byte ip[] = { 195, 176, 65, 221 };
byte gateway[] = { 195, 176, 65, 193 };
byte dns1[] = { 195, 176, 176, 129 };
byte subnet[] = { 195, 176, 65, 192 };

// Ethernet server
EthernetServer server(80);

// Create aREST instance
aREST rest = aREST();

SoftwareSerial mySerial(5, 4); // RX, TX (Inverted! NOT like welded on the arduino)

//---------------------------------------------------------//
void setup() {

  // Open serial communications with te studer inverter:
  Serial.begin(38400, SERIAL_8E1);
  /***********Studer***********
  USART configuration
  The RS-232 is defined with :
  · A fixed baudrate of 38400 bps
  · 1 start bit
  · 8 bit of data, LSB first
  · 1 parity bit
  · even parity
  · 1 stop bit
  ***************************/

  Serial.setTimeout(READ_TIMEOUT_MILLISECONDS);

  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600); //Debug with PC
  mySerial.println("Boot");

  //---------------------------------------------------------//
  // Ethernet
  //---------------------------------------------------------//
  // Init variables and expose them to REST API

  rest.variable("SOC", &battery_soc);
  rest.variable("VDC", &battery_Vdc);
  rest.variable("Real_Watts", &charge_real);
  rest.variable("Desired_Watts", &charge);

  // Function to be exposed
  rest.function("battery", batteryControl);

  // Give name and ID to device
  rest.set_id("0001");
  rest.set_name("Battery_API");

  Ethernet.begin(mac, ip, dns1, gateway, subnet);
  server.begin();
  //  mySerial.print("server is at ");
  //  mySerial.println(Ethernet.localIP());

  // Start watchdog
  wdt_enable(WDTO_4S); 

  //---------------------------------------------------------//
  // Canbus
  //---------------------------------------------------------//

START_INIT:

  if (CAN_OK == CAN.begin(CAN_125KBPS)) // init can bus : baudrate = 125k
  {
    // mySerial.println("CAN BUS Shield init ok!");
  }
  else
  {
    //mySerial.println("CAN BUS Shield init fail");
    //mySerial.println("Init CAN BUS Shield again");
    delay(1000);
    goto START_INIT;
  }

  // SET BATTERY LIMITS
  write_xt1_set_input_current(10.0);
}

//*********************************************************//
void loop() {
  //---------------------------------------------------------//
  // Control
  //---------------------------------------------------------//

  if (battery_soc < batteryLowLimits[0] && round(charge_real/230.0) <= 0 ) { // automatic charge  
    charge_battery(9);
    charge_real = 2070; // Continus charge current 100 A
  }
  else if (battery_soc >= batteryLowLimits[1] && (charge_real != charge) && charge_real!=0)
  {
    idle_battery();
    charge_real=0;
  }
  


  //---------------------------------------------------------//
  // Ethernet
  //---------------------------------------------------------//
  // listen for incoming clients

  EthernetClient client = server.available();
  if (client) {
    rest.handle(client);
  }

  wdt_reset(); // Watchdog 


  //---------------------------------------------------------//
  // Canbus
  //---------------------------------------------------------//
  unsigned char len = 0;
  unsigned char buf[8];

  if (CAN_MSGAVAIL == CAN.checkReceive())           // check if data coming
  {
    CAN.readMsgBuf(&len, buf);                    // read data,  len: data length, buf: data buf
    unsigned long canId = CAN.getCanId();
    /*********************************************************************************************************
    SOC is the pack state of charge. It shall be unsigned 16-bits, with a resolution of 1/65535 (%)
    per bit, and a range of 0% to 100%.
    *********************************************************************************************************/
    if (canId == 0x18FF000B)
    {
      battery_soc = (float)((0x0000ff00 & (buf[0] << 8)) | (0x000000ff & buf[1])) / 65535.0 * 100.0; // SOC percentual
      // DEBUG print SOC
      //mySerial.println("");
      //mySerial.print("battery_soc: ");
      //mySerial.print(battery_soc);
      //mySerial.print(" %");
      //mySerial.println("");
    }
    else if (canId == 0x18FF010B)
    {
      battery_Vdc = (float)((0x0000ff00 & (buf[0] << 8)) | (0x000000ff & buf[1])) / 100.0; // VDC
      //mySerial.println(battery_Vdc);
    }
  }
}

//--------------------------------------------------------------------//
int charge_battery(float current) {
  mySerial.print("cha ");
  mySerial.print(current);
  mySerial.print(" Aac, ");
  
  write_xt1_disable_grid_injection();
  
  // set maxCurrent DC
  float currentDC=0.875*current*230.0/battery_Vdc;
  currentDC = (currentDC>55) ? 55 : currentDC;
  mySerial.print(currentDC);
  mySerial.println(" Adc");
  write_xt1_qsp_value(currentDC);
  
  write_xt1_enable_charging(); 
  return 0;
}

int discharge_battery(float current) {
  mySerial.print("dis ");
  mySerial.print(current);
  mySerial.println(" A");
  write_xt1_disable_charging();
  write_xt1_set_injection_current(current);
  write_xt1_enable_grid_injection();
  return 0;
}

int idle_battery()
{
  mySerial.println("idle");
  write_xt1_disable_grid_injection();
  write_xt1_disable_charging();
  return 0;
}

//--------------------------------------------------------------------//
//Ethernet
// http://195.176.65.221/batteryControl?params=0
//--------------------------------------------------------------------//

// Custom function accessible by the API
int batteryControl(String command) {
  float current;
  // Get state from command
  charge = command.toFloat();
  mySerial.print("Control: ");
  mySerial.print(charge);
  mySerial.println(" W");
  

  if (battery_soc < batteryLowLimits[0]) { // automatic charge
    //mySerial.println("AUTOMATIC charge activated");
    charge_real = 2070; // Continus charge current 100 A
    charge_battery(9); 

    return REST_RET_LOW_BAT;
  }
  
  //--------------------------------------------------------------
  // Negative watts = inject
  //--------------------------------------------------------------
  
  else if (charge < 0 ) //24V, Max 100A
  {
    if (battery_soc < batteryLowLimits[1]) { // do not discharge
      //mySerial.println("AUTOMATIC block discharge activated");
      return REST_RET_BLOCK_DISCHARGE;
    }

    else {

      //mySerial.print("Inject Mode: ");
      //mySerial.print(charge);
      //mySerial.println(" Watts");

      //--------------------------------------------------------------------->>>>>>>>>>leggere valore ac????
      current = (-1.0) * charge / 230.0;
      if (current > 9) {
        return REST_RET_POWER_TO_HIGH;
      }
      else {
        discharge_battery(current);
        charge_real = charge;
      }
    };


  //--------------------------------------------------------------
  // Charge Mode
  //--------------------------------------------------------------

  }
  else if (charge > 0 ) //24V, Input Max 10A
  {

    //mySerial.print("Charge Mode: ");
    //mySerial.print(charge);
    //mySerial.println(" Watts");

    current = charge / 230.0;
    //--------------------------------------------------------------------->>>>>>>>>>leggere valore ac????
    if (current > 9) {
      return REST_RET_POWER_TO_HIGH;
    }
    else {
      //mySerial.print(current);
      //mySerial.println(" A");
      charge_battery(current);
      charge_real = charge;
    }
  }
  
  //--------------------------------------------------------------
  // Idle Mode
  //--------------------------------------------------------------
  
  else // Power = 0
  {
    //mySerial.println("Idle mode:");
    //mySerial.print(charge);
    //mySerial.println(" Watts");
    idle_battery();
    charge_real = charge;

  }
  return REST_RET_OK;
}



