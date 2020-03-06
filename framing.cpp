#include <string>
#include "esp32-hal-log.h"
#include <HardwareSerial.h>

#include "framing.h"
#include "u2f.h"

#define ERR_INVALID_CMD 0x01   // The command in the request is unknown/invalid
#define ERR_INVALID_PAR 0x02   // The parameter(s) of the command is/are invalid or missing
#define ERR_INVALID_LEN 0x03   // The length of the request is invalid
#define ERR_INVALID_SEQ 0x04   // The sequence number is invalid
#define ERR_REQ_TIMEOUT 0x05   // The request timed out
#define ERR_OTHER       0x7f   // Other, unspecified error

#define CMD_PING  0x81
#define CMD_MSG   0x83
#define CMD_ERROR 0xbf

#define STATUS_EMPTY   0
#define STATUS_FRAMING 1


std::string data;
static int16_t missing_data;
static uint8_t CMD;
static uint8_t status = STATUS_EMPTY;
static uint8_t expected_frame_sequence;


void set_error(uint8_t error) {
  response = "01234";
  response[0] = CMD_ERROR;
  response[1] = 0;
  response[2] = 1;
  response[3] = error;
}

void processCMD() {
  log_d("*** Process command: %02x", CMD);
  if (CMD == CMD_PING) {
    response = "012";
    response[0] = CMD;
    response[1] = (data.length() >> 8);
    response[2] = data.length();
    response += data;
  }
  else if (CMD == CMD_MSG) {
    // U2F
    std::string response_u2f = u2f_process(data);
    response = "012";    
    response[0] = CMD;
    response[1] = response_u2f.length() >> 8;
    response[2] = response_u2f.length();
    response += response_u2f;
    //log_d("*** Response:");
    //for (int i=0; i<response.length(); i++) log_d("      %02x", response[i]);
  }
  else {
    set_error(ERR_INVALID_CMD);
  }
}

uint8_t update(std::string value) {
  uint16_t i, value_len, LEN;
  value_len = value.length();
  log_d("*** Framing: Update called:");
  log_d("Value: ");
  for (i=0; i<value_len; i++) log_d("      %02x", value[i]);
  
  if (status == STATUS_EMPTY) {
    if (value_len < 3) {
      set_error(ERR_INVALID_PAR);
      return UPDATE_ERROR;
    } else {
      LEN = (value[1] << 8) + value[2];
      if (value_len > LEN+3) {
        set_error(ERR_INVALID_LEN);
        return UPDATE_ERROR;
      }
      else {
        CMD = value[0];
        data = value.substr(3, value_len);
        missing_data = LEN - (value_len-3);
        if (missing_data == 0) {
          return UPDATE_SUCCESS;
        }
        else {
          status = STATUS_FRAMING;
          expected_frame_sequence = 0;
          return UPDATE_FRAMING;
        }
      }
    }
  }
  else if ((status == STATUS_FRAMING) && (value_len > 0)) {
    if (value[0] != expected_frame_sequence) {
      status = STATUS_EMPTY;
      set_error(ERR_INVALID_SEQ);
      return UPDATE_ERROR;
    } else if (missing_data < value_len-1) {
      status = STATUS_EMPTY;
      set_error(ERR_INVALID_LEN);
      return UPDATE_ERROR;
    }
    else {
      data += value.substr(1, value_len);
      missing_data -= value_len-1;
      if (missing_data == 0) {
        status = STATUS_EMPTY;
        return UPDATE_SUCCESS;
      }
      else {
        if (expected_frame_sequence == 0xff) {
          status = STATUS_EMPTY;
          set_error(ERR_INVALID_SEQ);
          return UPDATE_ERROR;
        }
        else {
          expected_frame_sequence += 1;
          return UPDATE_FRAMING;
        }
      }
    }
  }
  status = STATUS_EMPTY;
  set_error(ERR_OTHER);
  return UPDATE_ERROR;
}
