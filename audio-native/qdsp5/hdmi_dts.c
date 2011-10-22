/* hdmi_dts.c - native PCM test application
 *
 * * Copyright (C) 2008 The Android Open Source Project
 * Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/msm_audio.h>
#include <pthread.h>
#include <errno.h>
#include "audiotest_def.h"
#include "control.h"
#include "iec_60958_61937.h"
#include "audio_parsers.h"


const char  *dev_file_name;
static int quit, repeat;

struct config_60958_61937 config_60958_61937;
struct codec_61937_config codec_61937_config;
struct audio_parser_codec_info audio_codec_info;

static unsigned char *dts_61937_burst;
static unsigned char *hdmi_non_l_rep_per;
static unsigned char *dts_frame;


#define DEV_FILE_NAME   "/dev/msm_lpa_if_out"

int hdmi_dts_play(struct audtest_config *config)
{
	struct msm_audio_config audio_config;
	unsigned int dts_file_sz;
	unsigned int actual_read_size;
	unsigned char *dts_file_data;
	FILE *fp;
	FILE *fp1;
	int afd;
	int sz;
	unsigned int i;
	int alsa_control;
	unsigned char *ch;
	int dev_id;
	int max_dts_frame_sz, cur_dts_frame_sz;
	char *device_name = "hdmi_pass_through";
	int rc = 0;
	unsigned int dts_type = 0;
	unsigned int repetition_period = 0;
	unsigned int dma_buf_sz = 0;
	unsigned int tmp = 0;
	unsigned int frame_count = 0;
	unsigned int actual_write_size = 0;


	fprintf(stderr, "%s():\n", __func__);


	/******************** read_file ***************************************/

	fp = fopen(config->file_name,"rb");
	if (fp == NULL) {
		fprintf(stderr, "hdmi_dts : cannot open '%s'\n",
				config->file_name);
		return -1;
	}

	/* Get file size and reset file position */
	fseek(fp, 0, SEEK_END);
	dts_file_sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	fprintf(stderr, "size of dts file %s = %u\n",
			config->file_name, dts_file_sz);

	dts_file_data = (unsigned char *)
		(malloc(sizeof(unsigned char) * dts_file_sz));

	if (dts_file_data == NULL) {
		fprintf(stderr, "could not allocate memeory for dts file data."
			" file length %u\n" , dts_file_sz);
		fclose(fp);
		return -1;
	}

	actual_read_size = fread((void*) dts_file_data, 1, dts_file_sz, fp);

	if (actual_read_size != dts_file_sz) {
		fprintf(stderr, "could not read DTS file %s\n",
				config->file_name);
		free(dts_file_data);
		fclose(fp);
		return -1;
	}

	max_dts_frame_sz = init_audio_parser(dts_file_data, dts_file_sz,
			AUDIO_PARSER_CODEC_DTS);

	dts_frame = (unsigned char *)
		(malloc(sizeof(unsigned char) * max_dts_frame_sz));

	if (dts_frame == NULL) {
		fprintf(stderr, "could not allocate memeory for dts frame."
			" frame size %u\n" ,max_dts_frame_sz);
		free(dts_file_data);
		fclose(fp);
		return -1;
	}
	dts_type = get_dts_type();

	init_60958_61937_framer();

	/********** end of read_file ******************************************/


	alsa_control = msm_mixer_open("/dev/snd/controlC0", 0);
	if(alsa_control < 0) {
		fprintf(stderr, "ERROR opening the ALSA mixer device\n");
		rc = -1;
		goto error_alsa_mixer_open;
	}

	dev_id = msm_get_device(device_name);

	if (msm_en_device(dev_id,1)) {
		fprintf(stderr, "could not enable device %s\n",
				device_name);
		goto error_en_alsa_dev;
	}

	afd = open(DEV_FILE_NAME, O_WRONLY);
	if (afd < 0) {
		fprintf(stderr, "cannot open audio device\n");
		rc = -1;
		goto error_open_dev;
	}

	config_60958_61937.codec_type = dts_type;

	rc = get_60958_61937_config(&config_60958_61937);

	if(rc == -1) {
		fprintf(stderr, "get_60958_61937_config failed\n");
		rc = -1;
		goto error_get_60958_61937_config;
	}
	dma_buf_sz = config_60958_61937.dma_buf_sz;
        fprintf(stderr, "Dma buf_sz %d\n", dma_buf_sz);

	dts_61937_burst =  (unsigned char *)
		(malloc(config_60958_61937.sz_61937_burst));

	if (dts_61937_burst == NULL) {
		fprintf(stderr, "could not allocate memory for dts 61937 burst."
			" burst size %u\n" , config_60958_61937.sz_61937_burst);
		rc = -1;
		goto error_no_mem_dts_61937_burst;
	}

	hdmi_non_l_rep_per  = (unsigned char *)
		(malloc(dma_buf_sz));


	if (hdmi_non_l_rep_per == NULL) {
		fprintf(stderr, "could not allocate memeory for dts 60958 "
			"rep per frames. dts 60958rep per frame size %u\n" ,
			config_60958_61937.rep_per_60958);
		rc = -1;
		goto error_no_mem_hdmi_non_l_rep_per;
	}

	fprintf(stderr, "burst length  %d , rep_per_60958 %d \n",
			config_60958_61937.sz_61937_burst,
			config_60958_61937.rep_per_60958);


	if (ioctl(afd, AUDIO_SET_CONFIG, &dma_buf_sz)) {
		fprintf(stderr, "could not set AUDIO_SET_IEC_CODEC_CONFIG\n");
		rc = -1;
		goto error_ioctl_audio_get_config;
	}
	if (ioctl(afd, AUDIO_GET_CONFIG, &audio_config)) {
		fprintf(stderr, "could not get audio_config\n");
		rc = -1;
		goto error_ioctl_audio_get_config;
	}

	fprintf(stderr, "initiate_play: buffer_size=%d, buffer_count=%d\n",
			audio_config.buffer_size, audio_config.buffer_count);
	fprintf(stderr, "prefill: send Pause Fram\n");
	

	sz = dma_buf_sz;

	 /******************** write_file ***************************************/

        fp1 = fopen("/sdcard/60958_frames","wb");
        if (fp1 == NULL) {
                fprintf(stderr, "hdmi_dts : cannot open file 60958_frames file\n");
                return -1;
        }
	
	for (i = 0; i < audio_config.buffer_count; i++) {
		get_60958_61937_pause_burst(hdmi_non_l_rep_per,
				config_60958_61937.rep_per_60958, &config_60958_61937);
		actual_write_size = fwrite((void*) hdmi_non_l_rep_per, 1, sz, fp1);

		if (actual_write_size != sz) {
			fprintf(stderr, "could not write DTS file 60958_frames\n");
			fclose(fp1);
			return -1;
		}
		frame_count++;
	}


	audio_codec_info.codec_type = AUDIO_PARSER_CODEC_DTS;
	for (;;) {

		rc = get_audio_frame(dts_frame, max_dts_frame_sz, &audio_codec_info);

		if (rc < 0 ) {
			fprintf(stderr, "%s: no more aduio frames\n", __func__);
			rc = 0;
			break;
		}

		codec_61937_config.codec_type = dts_type;

		codec_61937_config.codec_config.dts_fr_config.dts_fr_sz_8bit =
			audio_codec_info.codec_config.dts_fr_info.dts_fr_sz_8bit;

		codec_61937_config.codec_config.dts_fr_config.sample_rate =
			audio_codec_info.codec_config.dts_fr_info.sample_rate;

		codec_61937_config.codec_config.dts_fr_config.dts_type =
			audio_codec_info.codec_config.dts_fr_info.dts_type;

		cur_dts_frame_sz =
			codec_61937_config.codec_config.dts_fr_config.dts_fr_sz_8bit;

		get_61937_burst(dts_61937_burst,
				config_60958_61937.sz_61937_burst,
				dts_frame, cur_dts_frame_sz, &codec_61937_config);
		
		get_60958_frame(hdmi_non_l_rep_per,
				config_60958_61937.rep_per_60958,
				dts_61937_burst,
				config_60958_61937.sz_61937_burst,
				&codec_61937_config);
		
		actual_write_size = fwrite((void*) hdmi_non_l_rep_per, 1, sz, fp1);

		if (actual_write_size != sz) {
			fprintf(stderr, "could not write DTS file 60958_frames\n");
			fclose(fp1);
			return -1;
		}
		frame_count++;
	}
	fclose(fp1);
	fprintf(stderr, "start playback\n");
        fp1 = fopen("/sdcard/60958_frames","rb");
        if (fp1 == NULL) {
                fprintf(stderr, "hdmi_dts : cannot open file 60958_frames file\n");
                return -1;
        }
	fprintf(stderr, "frame no %u\n", frame_count);
	for (i=0; i<frame_count; i++) {
		actual_read_size = fread((void*) hdmi_non_l_rep_per, 1, sz, fp1);

		if (actual_read_size != sz) {
			fprintf(stderr, "could not read 60958_frames  file %u\n", sz);
			fclose(fp1);
			return -1;
		}

		if(i == 2) {
			rc = ioctl(afd, AUDIO_START, 0);

			if (rc < 0 ) {
				fprintf(stderr, "%s: Unable to start driver\n", __func__);
				rc = 1;
				goto error_ioctl_audio_start;
			}
		}
		if (write(afd, hdmi_non_l_rep_per, sz) != sz) {
			fprintf(stderr, "could not write %s\n", DEV_FILE_NAME);
			break;
		}
	}
	fclose(fp1);
	fprintf(stderr, "End of playback\n");

error_ioctl_audio_start:
error_dev_write:

error_ioctl_audio_get_config:
	free(hdmi_non_l_rep_per);

error_no_mem_hdmi_non_l_rep_per:
	free(dts_61937_burst);

error_no_mem_dts_61937_burst:

error_get_60958_61937_config:
	close(afd);

error_open_dev:
	if (msm_en_device(dev_id, 0) < 0)
		fprintf(stderr, "ERROR: could not disable %s\n",
				device_name);

error_en_alsa_dev:
	alsa_control = msm_mixer_close();
	if (alsa_control < 0)
		fprintf(stderr, "Failed to close ALSA MIXER\n");

error_alsa_mixer_open:
	free(dts_frame);
	fclose(fp);
	deinit_60958_61937_framer();
	free(dts_file_data);

	return rc;
}


void* hdmi_dts_thread(void* arg) {
	struct audiotest_thread_context *context =
	(struct audiotest_thread_context*) arg;
	int ret_val;

	ret_val = hdmi_dts_play(&context->config);
	free_context(context);
	pthread_exit((void*) ret_val);

    return NULL;
}

int hdmi_dts_read_params(void) {
	struct audiotest_thread_context *context;
	char *token;
	int ret_val = 0;

	if ((context = get_free_context()) == NULL) {
		ret_val = -1;
	} else {
		context->config.file_name = "/data/data.dts";
		dev_file_name = "/dev/msm_lpa_if_out";
		repeat = 0;
		quit = 0;

		token = strtok(NULL, " ");

		while (token != NULL) {
			if (!memcmp(token,"-id=", (sizeof("-id=")-1))) {
				context->cxt_id = atoi(&token[sizeof("-id=") - 1]);
			} else if (!memcmp(token, "-dev=",
					(sizeof("-dev=") - 1))) {
				dev_file_name = token + (sizeof("-dev=")-1);
			} else {
				context->config.file_name = token;
			}
			token = strtok(NULL, " ");
		}
		context->type = AUDIOTEST_TEST_MOD_PCM_DEC;
		pthread_create( &context->thread, NULL,
				hdmi_dts_thread, (void*) context);
	}

	return ret_val;

}

const char *hdmi_dts_help_txt =
	"To Play dts file on hdmi: type \n"
"echo \"hdmi_dts path_of_file -id=xxx  -dev=/dev/msm_lpa_if_out \" > tmp/audio_test \n";


void hdmi_dts_help_menu(void) {
	printf("%s\n", hdmi_dts_help_txt);
}
