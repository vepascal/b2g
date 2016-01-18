/*
 * Xtender 1
 */

#include <scom_data_link.h>
#include <scom_port_c99.h>
#include <scom_property.h>

char buffer[256]; //From datasheet protocol Studer (max buffer size)
scom_error_t exchange_frame(scom_frame_t* frame);
float studer_return_value;

//---------------------------------------------------------//
/**
 * \brief empty the rx and tx buffer before a new exchange
 */
void clear_serial_port() {
  Serial.flush();
}

/**
 * \brief read in a blocking way on the serial port
 *
 * This function must implement the proper timeout mechanism.
 * * \return number of byte read
 */
size_t read_serial_port(char* buffer, size_t byte_count) {
  return Serial.readBytes(buffer, byte_count);
}

/**
 * \brief write in a blocking way on the serial port
 *
 * This function must implement the proper timeout mechanism.
 * \return number of byte written
 */
size_t write_serial_port(const char* buffer, size_t byte_count) {
  //Debug se lo tolgo "error when writing to the com port"
  //mySerial.print("buffer content: ");
  //for (size_t i = 0; i < byte_count; ++i) {
  //  mySerial.print("0x");
  //  mySerial.print(((const unsigned char*)buffer)[i], HEX);
  //  mySerial.print(' ');
  //}
  //mySerial.println("");

  //  mySerial.print("byte_count: ");
  //  mySerial.print(byte_count);
  //  mySerial.println("");
  //
  //  mySerial.print("written: ");
  //  mySerial.print(Serial.write(buffer, byte_count));
  //  mySerial.println("");
  //  mySerial.println("___\n");
  return Serial.write(buffer, byte_count);
}

int studer_xt1(char mode, int object_id, int value_length, byte *data, int data_len)
{
  scom_frame_t frame;
  scom_property_t property;

  /* initialize the structures */
  scom_initialize_frame(&frame, buffer, sizeof(buffer));
  scom_initialize_property(&property, &frame);

  frame.src_addr = 1;     /* my address, could be anything */
  frame.dst_addr = 101;   /* the first inverter */
  property.object_id = object_id;

  switch (mode) {
    case 'r':
      property.object_type = SCOM_USER_INFO_OBJECT_TYPE;  /* read a user info  "0x1" */
      property.property_id = 1 ;  /* to read */
      scom_encode_read_property(&property);
      break;

    case 'w':
      property.value_length = value_length;
      property.value_buffer_size = data_len;
      property.object_type = SCOM_PARAMETER_OBJECT_TYPE;  /* write a user info "0x2" */
      memcpy(property.value_buffer, data, value_length);

      property.property_id = 5 ;  /* 5 to write */
      scom_encode_write_property(&property);
      break;

    default:
      return -1;
  }


  if (frame.last_error != SCOM_ERROR_NO_ERROR) {
    return -1;
  }

  /* do the exchange of frames */
  if (exchange_frame(&frame) != SCOM_ERROR_NO_ERROR) {
    return -1;
  }

  /* reuse the structure to save space */
  scom_initialize_property(&property, &frame);

  switch (mode) {
    case 'r':
      /* decode the read property service part */
      scom_decode_read_property(&property);
      break;

    case 'w':
      /* decode the write property service part */
      scom_decode_write_property(&property);
      break;
    default:
      //      mySerial.println("invalid mode parameter ");
      return -1;
  }

  if (frame.last_error != SCOM_ERROR_NO_ERROR) {
    //        printf("read property decoding failed with error %d\n", (int) frame.last_error);
    return -1;
  }

  switch (mode) {
    case 'r':
      /* check the the size reading */
      if (property.value_length != 4) {
        return -1;
      }
      break;

    case 'w':
      if (property.value_length == 0) { // OK
        return 0;
      }

      else {
        return -1;
      }

    default:
      return -1;
  }
  studer_return_value = scom_read_le_float(property.value_buffer);
  //printf("uBat = %.2f V\n", scom_read_le_float(property.value_buffer));
  return 0;
}

//--------------------------------------------------------------------//
/**
 * \brief example code to exchange a frame and print possible error on standard output
 *
 * \param frame an initialized frame configured for a service
 * \return a possible error that occurred or SCOM_NO_ERROR
 */
scom_error_t exchange_frame(scom_frame_t* frame)
{
  size_t byte_count;

  clear_serial_port();

  scom_encode_request_frame(frame);

  if (frame->last_error != SCOM_ERROR_NO_ERROR) {
    return frame->last_error;
  }

  /* send the request on the com port */

  byte_count = write_serial_port(frame->buffer, scom_frame_length(frame));
  if (byte_count != scom_frame_length(frame)) {
    //printf("error when writing to the com port\n");
    return SCOM_ERROR_STACK_PORT_WRITE_FAILED;
  }

  /* reuse the structure to save space */
  scom_initialize_frame(frame, frame->buffer, frame->buffer_size);

  /* clear the buffer for debug purpose */
  memset(frame->buffer, 0, frame->buffer_size);

  /* Read the fixed size header.
     A platform specific handling of a timeout mechanism should be
     provided in case of the possible lack of response */

  byte_count = read_serial_port(&frame->buffer[0], SCOM_FRAME_HEADER_SIZE);

  if (byte_count != SCOM_FRAME_HEADER_SIZE) {
    return SCOM_ERROR_STACK_PORT_READ_FAILED;
  }

  /* decode the header */
  scom_decode_frame_header(frame);
  if (frame->last_error != SCOM_ERROR_NO_ERROR) {
    return frame->last_error;
  }

  /* read the data part */

  byte_count = read_serial_port(&frame->buffer[SCOM_FRAME_HEADER_SIZE],  scom_frame_length(frame) - SCOM_FRAME_HEADER_SIZE);
  if (byte_count != (scom_frame_length(frame) - SCOM_FRAME_HEADER_SIZE)) {
    return SCOM_ERROR_STACK_PORT_READ_FAILED;
  }

  /* decode the data */
  scom_decode_frame_data(frame);
  if (frame->last_error != SCOM_ERROR_NO_ERROR) {
    return frame->last_error;
  }
  return SCOM_ERROR_NO_ERROR;
}

//*********************************************************//


int write_xt1_enable_grid_injection() {
  //mySerial.println("enable_grid-injection");
  int data = 1;
  if (studer_xt1('w', 1127, 1, (byte*)&data, sizeof(data))) {
    //mySerial.println("error in enable_grid-injection");
    return -1;
  }
  return 0;
}

int write_xt1_disable_grid_injection() {
  //mySerial.println("disable_grid-injection");
  int data = 0;
  if (studer_xt1('w', 1127, 1, (byte*)&data, sizeof(data))) {
    //mySerial.println("error in disable_grid-injection");
    return -1;
  }
  return 0;
}

int write_xt1_enable_charging() {
  //mySerial.println("enable_charging");
  int data = 1;
  if (studer_xt1('w', 1125, 1, (byte*)&data, sizeof(data))) {
    //mySerial.println("error in enable_charging");
    return -1;
  }
  return 0;
}

int write_xt1_disable_charging() {
  //mySerial.println("disable_charging");
  int data = 0;
  if (studer_xt1('w', 1125, 1, (byte*)&data, sizeof(data))) {
    //mySerial.println("error in disable_charging");
    return -1;
  }
  return 0;
}

int write_xt1_set_injection_current(float data) {
  //mySerial.println("set-injection-current");
  //mySerial.println(data);
  if (studer_xt1('w', 1523, 4, (byte*)&data, sizeof(data))) {
    //mySerial.println(" error in set-injection-current");
    return -1;
  }
  return 0;
}

int write_xt1_set_input_current(float data) { // must be at least as injection current
  //mySerial.println("set-input-current");
  //mySerial.println(data);
  if (studer_xt1('w', 1107, 4, (byte*)&data, sizeof(data))) {
    //mySerial.println(" error in set_input_current");
    return -1;
  }
  return 0;
}

int write_xt1_qsp_value(float data) {
  //mySerial.println("write qsp_value");
  if (studer_xt1('w', 1138, 4, (byte*)&data, sizeof(data))) {
    //mySerial.println(" error in write qsp_value");
    return -1;
  }
  return 0;
}
