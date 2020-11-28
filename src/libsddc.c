/*
 * libsddc.c - low level functions for wideband SDR receivers like
 *             BBRF103, RX-666, RX888, HF103, etc
 *
 * Copyright (C) 2020 by Franco Venturi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libsddc.h"
#include "logging.h"
#include "usb_device.h"
#include "streaming.h"

typedef struct sddc sddc_t;

/* internal functions */
static int sddc_set_clock_source(sddc_t *this, double adc_frequency,
                                 double tuner_frequency);


typedef struct sddc {
  enum SDDCStatus status;
  enum SDDCHWModel model;
  uint16_t firmware;
  enum RFMode rf_mode;
  usb_device_t *usb_device;
  streaming_t *streaming;
  int has_clock_source;
  int has_vhf_tuner;
  int hf_attenuator_levels;
  double sample_rate;
  double tuner_frequency;
  double freq_corr_ppm;
} sddc_t;


static const double DEFAULT_SAMPLE_RATE = 64e6;       /* 64Msps */
static const double DEFAULT_TUNER_FREQUENCY = 999e3;  /* MW station in Turin */
static const double DEFAULT_FREQ_CORR_PPM = 0.0;      /* frequency correction PPM */


/******************************
 * basic functions
 ******************************/

int sddc_get_device_count()
{
  return usb_device_count_devices();
}


int sddc_get_device_info(struct sddc_device_info **sddc_device_infos)
{
  int ret_val = -1; 

  /* no more info to add from usb_device_get_device_list() for now */
  struct usb_device_info *list;
  int ret = usb_device_get_device_list(&list);
  if (ret < 0) {
    goto FAIL0;
  }

  int count = ret;
  struct sddc_device_info *device_infos = (struct sddc_device_info *) malloc((count + 1) * sizeof(struct sddc_device_info));
  /* use the first element to save the pointer to the underlying list,
     so we can use it to free it later on */
  *((void **) device_infos) = list;
  device_infos++;
  for (int i = 0; i < count; ++i) {
    device_infos[i].manufacturer = list[i].manufacturer;
    device_infos[i].product = list[i].product;
    device_infos[i].serial_number = list[i].serial_number;
  }

  *sddc_device_infos = device_infos;
  ret_val = count;

FAIL0:
  return ret_val;
}


int sddc_free_device_info(struct sddc_device_info *sddc_device_infos)
{
  /* just free our structure and call usb_device_free_device_list() to free
     underlying data structure */
  /* retrieve the underlying usb_device list pointer first */
  sddc_device_infos--;
  struct usb_device_info *list = (struct usb_device_info *) *((void **) sddc_device_infos);
  free(sddc_device_infos);
  int ret = usb_device_free_device_list(list);
  return ret;
}


sddc_t *sddc_open(int index, const char* imagefile)
{
  sddc_t *ret_val = 0;

  usb_device_t *usb_device = usb_device_open(index, imagefile, 0);
  if (usb_device == 0) {
    fprintf(stderr, "ERROR - usb_device_open() failed\n");
    goto FAIL0;
  }
  uint8_t data[4];
  int ret = usb_device_control(usb_device, TESTFX3, 0, 0, data, sizeof(data));
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(TESTFX3) failed\n");
    goto FAIL1;
  }

  sddc_t *this = (sddc_t *) malloc(sizeof(sddc_t));
  this->status = SDDC_STATUS_READY;
  this->model = (enum SDDCHWModel) data[0];
  this->firmware = (data[1] << 8) | data[2];
  this->rf_mode = HF_MODE;
  this->usb_device = usb_device;
  this->streaming = 0;
  switch (this->model) {
    case HW_BBRF103:
    case HW_RX888:
      this->has_clock_source = 1;
      this->has_vhf_tuner = 1;
      this->hf_attenuator_levels = 3;
      break;
    case HW_HF103:
      this->has_clock_source = 0;
      this->has_vhf_tuner = 0;
      this->hf_attenuator_levels = 32;
      break;
    default:
      this->has_clock_source = 0;
      this->has_vhf_tuner = 0;
      this->hf_attenuator_levels = 0;
      break;
  }
  this->sample_rate = DEFAULT_SAMPLE_RATE;         /* default sample rate */
  this->tuner_frequency = DEFAULT_TUNER_FREQUENCY; /* default tuner frequency */
  this->freq_corr_ppm = DEFAULT_FREQ_CORR_PPM;     /* default frequency correction PPM */

  ret_val = this;
  return ret_val;

FAIL1:
  usb_device_close(usb_device);
FAIL0:
  return ret_val;
}


void sddc_close(sddc_t *this)
{
  int ret = usb_device_control(this->usb_device, RESETFX3, 0, 0, 0, 0);
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(RESETFX3) failed\n");
  }
  usb_device_close(this->usb_device);
  free(this);
  return;
}


enum SDDCStatus sddc_get_status(sddc_t *this)
{
  return this->status;
}


enum SDDCHWModel sddc_get_hw_model(sddc_t *this)
{
  return this->model;
}


uint16_t sddc_get_firmware(sddc_t *this)
{
  return this->firmware;
}


enum RFMode sddc_get_rf_mode(sddc_t *this)
{
  return this->rf_mode;
}


int sddc_set_rf_mode(sddc_t *this, enum RFMode rf_mode)
{
  switch (rf_mode) {
    case HF_MODE:
      this->rf_mode = HF_MODE;
      break;
    case VHF_MODE:
      if (!this->has_vhf_tuner) {
        fprintf(stderr, "WARNING - no VHF/UHF tuner found\n");
        return -1;
      }
      this->rf_mode = VHF_MODE;
      break;
    default:
      fprintf(stderr, "WARNING - invalid RF mode: %d\n", rf_mode);
      return -1;
  }
  return 0;
}


/******************************
 * GPIO related functions
 ******************************/

enum GPIOBits {
  GPIO_ADC_SHDN   = 0x0020,
  GPIO_ADC_DITH   = 0x0040,
  GPIO_ADC_RAND   = 0x0080,
  GPIO_BIAS_HF    = 0x0100,
  GPIO_BIAS_VHF   = 0x0200,
  GPIO_LED_YELLOW = 0x0400,
  GPIO_LED_RED    = 0x0800,
  GPIO_LED_BLUE   = 0x1000,
  GPIO_ATT_SEL0   = 0x2000,
  GPIO_ATT_SEL1   = 0x4000,
  GPIO_VHF_EN     = 0x8000
};

static const uint16_t GPIO_LED_SHIFT = 10;



int sddc_led_on(sddc_t *this, uint8_t led_pattern)
{
  if (led_pattern & ~(LED_YELLOW | LED_RED | LED_BLUE)) {
    fprintf(stderr, "ERROR - invalid LED pattern: 0x%02x\n", led_pattern);
    return -1;
  }
  return usb_device_gpio_on(this->usb_device, (uint16_t) led_pattern << GPIO_LED_SHIFT);
}


int sddc_led_off(sddc_t *this, uint8_t led_pattern)
{
  if (led_pattern & ~(LED_YELLOW | LED_RED | LED_BLUE)) {
    fprintf(stderr, "ERROR - invalid LED pattern: 0x%02x\n", led_pattern);
    return -1;
  }
  return usb_device_gpio_off(this->usb_device, (uint16_t) led_pattern << GPIO_LED_SHIFT);
}


int sddc_led_toggle(sddc_t *this, uint8_t led_pattern)
{
  if (led_pattern & ~(LED_YELLOW | LED_RED | LED_BLUE)) {
    fprintf(stderr, "ERROR - invalid LED pattern: 0x%02x\n", led_pattern);
    return -1;
  }
  return usb_device_gpio_toggle(this->usb_device, (uint16_t) led_pattern << GPIO_LED_SHIFT);
}


int sddc_get_adc_dither(sddc_t *this)
{
  return (usb_device_gpio_get(this->usb_device) & GPIO_ADC_DITH) != 0;
}


int sddc_set_adc_dither(sddc_t *this, int dither)
{
  if (dither) {
    return usb_device_gpio_on(this->usb_device, GPIO_ADC_DITH);
  } else {
    return usb_device_gpio_off(this->usb_device, GPIO_ADC_DITH);
  }
}


int sddc_get_adc_random(sddc_t *this)
{
  return (usb_device_gpio_get(this->usb_device) & GPIO_ADC_RAND) != 0;
}


int sddc_set_adc_random(sddc_t *this, int random)
{
  if (random) {
    return usb_device_gpio_on(this->usb_device, GPIO_ADC_RAND);
  } else {
    return usb_device_gpio_off(this->usb_device, GPIO_ADC_RAND);
  }
}


int sddc_set_hf_attenuation(sddc_t *this, double attenuation)
{
  if (this->hf_attenuator_levels == 0) {
    /* no attenuator */
    return 0;
  } else if (this->hf_attenuator_levels == 3) {
    /* old style attenuator with just 0dB, 10dB, and 20Db */
    uint16_t bit_pattern = 0;
    switch ((int) attenuation) {
      case 0:
        bit_pattern = GPIO_ATT_SEL1;
        break;
      case 10:
        bit_pattern = GPIO_ATT_SEL0 | GPIO_ATT_SEL1;
        break;
      case 20:
        bit_pattern = GPIO_ATT_SEL0;
        break;
      default:
        fprintf(stderr, "ERROR - invalid HF attenuation: %lf\n", attenuation);
        return -1;
    }
    return usb_device_gpio_set(this->usb_device, bit_pattern,
                               GPIO_ATT_SEL0 | GPIO_ATT_SEL1);
  } else if (this->hf_attenuator_levels == 32) {
    /* new style attenuator with 1dB increments */
    if (attenuation < 0.0 || attenuation > 31.0) {
      fprintf(stderr, "ERROR - invalid HF attenuation: %lf\n", attenuation);
      return -1;
    }
    uint8_t data = (31 - (int) attenuation) << 1;
    return usb_device_control(this->usb_device, DAT31FX3, 0, 0, &data,
                              sizeof(data));
  }

  /* should never get here */
  fprintf(stderr, "ERROR - invalid number of HF attenuator levels: %d\n",
          this->hf_attenuator_levels);
  return -1;
}


int sddc_get_hf_bias(sddc_t *this)
{
  return (usb_device_gpio_get(this->usb_device) & GPIO_BIAS_HF) != 0;
}


int sddc_set_hf_bias(sddc_t *this, int bias)
{
  if (bias) {
    return usb_device_gpio_on(this->usb_device, GPIO_BIAS_HF);
  } else {
    return usb_device_gpio_off(this->usb_device, GPIO_BIAS_HF);
  }
}


int sddc_get_vhf_bias(sddc_t *this)
{
  return (usb_device_gpio_get(this->usb_device) & GPIO_BIAS_VHF) != 0;
}


int sddc_set_vhf_bias(sddc_t *this, int bias)
{
  if (bias) {
    return usb_device_gpio_on(this->usb_device, GPIO_BIAS_VHF);
  } else {
    return usb_device_gpio_off(this->usb_device, GPIO_BIAS_VHF);
  }
}


/******************************
 * streaming related functions
 ******************************/

int sddc_set_sample_rate(sddc_t *this, double sample_rate)
{
  /* no checks yet */
  this->sample_rate = sample_rate;
  return 0;
}


int sddc_set_async_params(sddc_t *this, uint32_t frame_size,
                           uint32_t num_frames, sddc_read_async_cb_t callback,
                           void *callback_context)
{
  if (this->streaming) {
    fprintf(stderr, "ERROR - sddc_set_async_params() failed: streaming already configured\n");
    return -1;
  }

  this->streaming = streaming_open_async(this->usb_device, frame_size,
                                         num_frames, callback,
                                         callback_context);
  if (this->streaming == 0) {
    fprintf(stderr, "ERROR - streaming_open_async() failed\n");
    return -1;
  }

  return 0;
}


int sddc_start_streaming(sddc_t *this)
{
  if (this->status != SDDC_STATUS_READY) {
    fprintf(stderr, "ERROR - sddc_start_streaming() called with SDR status not READY: %d\n", this->status);
    return -1;
  }

  /* start the clocks */
  if (this->has_clock_source) {
    int ret = sddc_set_clock_source(this, (double) this->sample_rate,
                                    this->tuner_frequency);
    if (ret < 0) {
      fprintf(stderr, "ERROR - sddc_set_clock_source() failed\n");
      return -1;
    }
  }

  /* tuner in standby */
  if (this->has_vhf_tuner) {
    int ret = usb_device_control(this->usb_device, R820T2STDBY, 0, 0, 0, 0);
    if (ret < 0) {
      fprintf(stderr, "ERROR - usb_device_control(R820T2STDBY) failed\n");
      return -1;
    }
  }

  /* set HF and VHF attenuation to 0 */
  int ret = sddc_set_hf_attenuation(this, 0.0);
  if (ret < 0) {
    fprintf(stderr, "ERROR - sddc_set_hf_attenuation() failed\n");
    return -1;
  }
  if (this->has_vhf_tuner) {
    int ret = sddc_set_tuner_attenuation(this, 0);
    if (ret < 0) {
      fprintf(stderr, "ERROR - sddc_set_tuner_attenuation() failed\n");
      return -1;
    }
  }

  /* start async streaming */
  if (this->streaming) {
    streaming_set_sample_rate(this->streaming, (uint32_t) this->sample_rate);
    int ret = streaming_start(this->streaming);
    if (ret < 0) {
      fprintf(stderr, "ERROR - streaming_start() failed\n");
      return -1;
    }
  }

  /* start the producer */
  ret = usb_device_control(this->usb_device, STARTFX3, 0, 0, 0, 0);
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(STARTFX3) failed\n");
    return -1;
  }

  /* all good */
  this->status = SDDC_STATUS_STREAMING;
  return 0;
}

int sddc_handle_events(sddc_t *this)
{
  return usb_device_handle_events(this->usb_device);
}

int sddc_stop_streaming(sddc_t *this)
{
  if (this->status != SDDC_STATUS_STREAMING) {
    fprintf(stderr, "ERROR - sddc_stop_streaming() called with SDR status not STREAMING: %d\n", this->status);
    return -1;
  }

  /* stop the producer */
  int ret = usb_device_control(this->usb_device, STOPFX3, 0, 0, 0, 0);
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(STOPFX3) failed\n");
    return -1;
  }

  /* stop async streaming */
  if (this->streaming) {
    int ret = streaming_stop(this->streaming);
    if (ret < 0) {
      fprintf(stderr, "ERROR - streaming_stop() failed\n");
      return -1;
    }
  }

  /* stop the clocks */
  ret = sddc_set_clock_source(this, 0, 0);
  if (ret < 0) {
    fprintf(stderr, "ERROR - sddc_set_clock_source(0, 0) failed\n");
    return -1;
  }

  /* all good */
  this->status = SDDC_STATUS_READY;
  return 0;
}


int sddc_reset_status(sddc_t *this)
{
  int ret = streaming_reset_status(this->streaming);
  if (ret < 0) {
    fprintf(stderr, "ERROR - streaming_reset_status() failed\n");
    return -1;
  }
  return 0;
}


int sddc_read_sync(sddc_t *this, uint8_t *data, int length, int *transferred)
{
  return streaming_read_sync(this->streaming, data, length, transferred);
}


/***********************************
 * VHF/UHF tuner related functions
 ***********************************/

double sddc_get_tuner_frequency(sddc_t *this)
{
  return this->tuner_frequency;
}

int sddc_set_tuner_frequency(sddc_t *this, double frequency)
{
  uint32_t data = (uint32_t) frequency;
  int ret = usb_device_control(this->usb_device, R820T2TUNE, 0, 0,
                               (uint8_t *) &data, sizeof(data));
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(R820T2TUNE) failed\n");
    return -1;
  }
  this->tuner_frequency = frequency;
  return 0;
}

/* tuner attenuations */
static const double tuner_attenuations_table[] = {
  0.0, 0.9, 1.4, 2.7, 3.7, 7.7, 8.7, 12.5, 14.4, 15.7, 16.6, 19.7, 20.7,
  22.9, 25.4, 28.0, 29.7, 32.8, 33.8, 36.4, 37.2, 38.6, 40.2, 42.1, 43.4,
  43.9, 44.5, 48.0, 49.6
};

int sddc_get_tuner_attenuations(sddc_t *this __attribute__((unused)),
                                const double *attenuations[])
{
  *attenuations = tuner_attenuations_table;
  return sizeof(tuner_attenuations_table) / sizeof(tuner_attenuations_table[0]);
}

double sddc_get_tuner_attenuation(sddc_t *this)
{
  uint8_t data = 0;
  int ret = usb_device_control(this->usb_device, R820T2GETATT, 0, 0, &data,
                               sizeof(data));
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(R820T2GETATT) failed\n");
    return -1;
  }
  return tuner_attenuations_table[(int) data];
}

int sddc_set_tuner_attenuation(sddc_t *this, double attenuation)
{
  int attenuation_table_size = sizeof(tuner_attenuations_table) /
                               sizeof(tuner_attenuations_table[0]);
  uint8_t idx = 0;
  double idx_att = fabs(attenuation - tuner_attenuations_table[idx]);
  for (int i = 1; i < attenuation_table_size; ++i) {
    double att = fabs(attenuation - tuner_attenuations_table[i]);
    if (att < idx_att) {
      idx = i;
      idx_att = att;
    }
  }

  int ret = usb_device_control(this->usb_device, R820T2SETATT, 0, 0, &idx,
                               sizeof(idx));
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(R820T2SETATT) failed\n");
    return -1;
  }

  fprintf(stderr, "INFO - tuner attenuation set to %.1f\n",
          tuner_attenuations_table[idx]);
  return 0;
}


/******************************
 * Misc functions
 ******************************/

double sddc_get_frequency_correction(sddc_t *this)
{
  return this->freq_corr_ppm;
}

int sddc_set_frequency_correction(sddc_t *this, double correction)
{
  if (this->status == SDDC_STATUS_STREAMING) {
    int ret = sddc_set_clock_source(this, (double) this->sample_rate,
                                    this->tuner_frequency);
    if (ret < 0) {
      fprintf(stderr, "ERROR - sddc_set_clock_source() failed\n");
      return -1;
    }
  }
  this->freq_corr_ppm = correction;
  return 0;
}


/* internal functions */
static int sddc_set_clock_source(sddc_t *this, double adc_frequency,
                                 double tuner_frequency)
{
  uint32_t data[2];
  /* ADC sampling frequency */
  double correction = 1e-6 * this->freq_corr_ppm * adc_frequency;
  data[0] = (uint32_t) (adc_frequency + correction);
  /* tuner reference frequency */
  correction = 1e-6 * this->freq_corr_ppm * tuner_frequency;
  data[1] = (uint32_t) (tuner_frequency + correction);

  int ret = usb_device_control(this->usb_device, SI5351A, 0, 0,
                               (uint8_t *) data, sizeof(data));
  if (ret < 0) {
    fprintf(stderr, "ERROR - usb_device_control(SI5351A) failed\n");
    return -1;
  }
  return 0;
}
