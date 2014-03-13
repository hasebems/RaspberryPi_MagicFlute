//
//  raspi_main.c
//
//  Created by Masahiko Hasebe on 2013/05/18.
//  Copyright (c) 2013 by Masahiko Hasebe(hasebems). All rights reserved.
//
#include	"raspi.h"
#ifdef RASPI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <math.h>

#include <alsa/asoundlib.h>
#include <pthread.h>

#include	"raspi_cwrap.h"
#include	"raspi_magicflute.h"

//-------------------------------------------------------------------------
//			Variables
//-------------------------------------------------------------------------
static const char *device = "plughw:0,0";               /* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;    /* sample format */
static unsigned int samplingRate = 44100;               /* stream rate */
static unsigned int channels = 1;                       /* count of channels */
static unsigned int buffer_time = 60000;                /* ring buffer length in us */
static unsigned int period_time = 15000;                /* period time in us */
static double freq = 440;                /* sinusoidal wave frequency in Hz */
static int verbose = 0;                  /* verbose flag */
static int resample = 1;                 /* enable alsa-lib resampling */
static int period_event = 0;             /* produce poll event after each period */
static int transpose = 0;

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static snd_output_t *output = NULL;

//-------------------------------------------------------------------------
//		Generate Wave : MSGF
//-------------------------------------------------------------------------
static void generate_wave(const snd_pcm_channel_area_t *areas,
                          snd_pcm_uframes_t offset,
                          int maxCount, double *_phase)
{
	unsigned char *samples[channels];
	int steps[channels];
	unsigned int chn;
	int count = 0;
	int format_bits = snd_pcm_format_width(format);
	unsigned int maxval = (1 << (format_bits - 1)) - 1;
	int bps = format_bits / 8;  /* bytes per sample */
	int phys_bps = snd_pcm_format_physical_width(format) / 8;
	int big_endian = snd_pcm_format_big_endian(format) == 1;
	int to_unsigned = snd_pcm_format_unsigned(format) == 1;
	int is_float = (format == SND_PCM_FORMAT_FLOAT_LE ||
					format == SND_PCM_FORMAT_FLOAT_BE);
	
	/* verify and prepare the contents of areas */
	for (chn = 0; chn < channels; chn++) {
		if ((areas[chn].first % 8) != 0) {
			printf("areas[%i].first == %i, aborting...\n", chn, areas[chn].first);
			exit(EXIT_FAILURE);
		}
		samples[chn] = /*(signed short *)*/(((unsigned char *)areas[chn].addr) + (areas[chn].first / 8));
		if ((areas[chn].step % 16) != 0) {
			printf("areas[%i].step == %i, aborting...\n", chn, areas[chn].step);
			exit(EXIT_FAILURE);
		}
		steps[chn] = areas[chn].step / 8;
		samples[chn] += offset * steps[chn];
	}
	
	//	get wave data
	int16_t* buf = (int16_t*)malloc(sizeof(int16_t) * maxCount);
	raspiaudio_Process( buf, maxCount );
	
	/* fill the channel areas */
	while (count < maxCount) {
		int i;
		int16_t res = buf[count++];
		
		if (to_unsigned)
			res ^= 1U << (format_bits - 1);
		
		for (chn = 0; chn < channels; chn++) {
			/* Generate data in native endian format */
			if (big_endian) {
				for (i = 0; i < bps; i++)
					*(samples[chn] + phys_bps - 1 - i) = (res >> i * 8) & 0xff;
			} else {
				for (i = 0; i < bps; i++)
					*(samples[chn] + i) = (res >>  i * 8) & 0xff;
			}
			samples[chn] += steps[chn];
		}
	}
	free(buf);
}

//-------------------------------------------------------------------------
//			ALSA Hardware Parameters
//-------------------------------------------------------------------------
static int set_hwparams(snd_pcm_t *handle,
                        snd_pcm_hw_params_t *params)
{
	unsigned int rrate;
	snd_pcm_uframes_t size;
	int err, dir;
	
	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
		return err;
	}
	
	/* set hardware resampling */
	err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
	if (err < 0) {
		printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
		return err;
	}
	
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_MMAP_INTERLEAVED );
	if (err < 0) {
		printf("Access type not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		printf("Sample format not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	
	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, channels);
	if (err < 0) {
		printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
		return err;
	}
	
	/* set the stream rate */
	rrate = samplingRate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
	if (err < 0) {
		printf("Rate %iHz not available for playback: %s\n", samplingRate, snd_strerror(err));
		return err;
	}
	if (rrate != samplingRate) {
		printf("Rate doesn't match (requested %iHz, get %iHz)\n", samplingRate, err);
		return -EINVAL;
	}
	
	/* set the buffer time */
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
	if (err < 0) {
		printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_buffer_size(params, &size);
	if (err < 0) {
		printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
		return err;
	}
	buffer_size = size;
	
	/* set the period time */
	err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
	if (err < 0) {
		printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
	if (err < 0) {
		printf("Unable to get period size for playback: %s\n", snd_strerror(err));
		return err;
	}
	period_size = size;
	
	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

//-------------------------------------------------------------------------
//			ALSA Software Parameters
//-------------------------------------------------------------------------
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
	int err;
	
	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
		return err;
	}
	
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
	if (err < 0) {
		printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
		return err;
	}
	
	/* allow the transfer when at least period_size samples can be processed */
	/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size);
	if (err < 0) {
		printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
		return err;
	}
	
	/* enable period events when requested */
	if (period_event) {
		err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
		if (err < 0) {
			printf("Unable to set period event: %s\n", snd_strerror(err));
			return err;
		}
	}
	
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

//-------------------------------------------------------------------------
//			Underrun and suspend recovery
//-------------------------------------------------------------------------
static int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (verbose)
		printf("stream recovery\n");
	if (err == -EPIPE) {    /* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);       /* wait until the suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
		}
		return 0;
	}
	return err;
}

//-------------------------------------------------------------------------
//		Audio Theread  /  Transfer method - direct write only
//-------------------------------------------------------------------------
#define		BEGIN_TRUNCATE			80	//	percent
//-------------------------------------------------------------------------
static void writeAudioToDriver( snd_pcm_t* handle, double* phase, int* first )
{
	const snd_pcm_channel_area_t *my_areas;
	snd_pcm_uframes_t	offset, frames, size;
	snd_pcm_sframes_t	commitres;
	int err;

	//	Time Measurement
	struct	timeval ts;
	struct	timeval te;
	long	startTime, endTime, execTime, latency, limit;
	gettimeofday(&ts, NULL);
	
	size = period_size;
	while (size > 0) {
		frames = size;
		err = snd_pcm_mmap_begin(handle, &my_areas, &offset, &frames);
		if (err < 0) {
			if ((err = xrun_recovery(handle, err)) < 0) {
				printf("MMAP begin avail error: %s\n", snd_strerror(err));
				exit(EXIT_FAILURE);
			}
			*first = 1;
		}
	
		//	Call MSGF
		generate_wave(my_areas, offset, frames, phase);

		commitres = snd_pcm_mmap_commit(handle, offset, frames);
		if (commitres < 0 || (snd_pcm_uframes_t)commitres != frames) {
			if ((err = xrun_recovery(handle, commitres >= 0 ? -EPIPE : commitres)) < 0) {
				printf("MMAP commit error: %s\n", snd_strerror(err));
				exit(EXIT_FAILURE);
			}
			*first = 1;
		}
		size -= frames;
	}

	//	Time Measurement
	gettimeofday(&te, NULL);
	startTime = ts.tv_sec * 1000 + ts.tv_usec/1000;
	endTime = te.tv_sec * 1000 + te.tv_usec/1000;
	execTime = endTime - startTime;
	latency = period_size*1000/samplingRate;
	limit = period_size*BEGIN_TRUNCATE*10/samplingRate;

	//	Reduce Resource
	if ( limit < execTime ){
		raspiaudio_ReduceResource();
		printf("processing time = %d/%d[msec]\n", execTime, latency);
	}
}

//-------------------------------------------------------------------------
static int soundGenerateLoop( snd_pcm_t *handle )
{
	double phase = 0;
	snd_pcm_sframes_t avail;
	snd_pcm_state_t state;
	int err, first = 1;

	INIT_PRM	prm;
	prm.transpose = transpose;
	
	eventLoopInit(&prm);
	
	while (1) {
		eventLoop();
		
		state = snd_pcm_state(handle);
		if (state == SND_PCM_STATE_XRUN) {
			err = xrun_recovery(handle, -EPIPE);
			if (err < 0) {
				printf("XRUN recovery failed: %s\n", snd_strerror(err));
				goto END_OF_THREAD;
				//return err;
			}
			first = 1;
		} else if (state == SND_PCM_STATE_SUSPENDED) {
			err = xrun_recovery(handle, -ESTRPIPE);
			if (err < 0) {
				printf("SUSPEND recovery failed: %s\n", snd_strerror(err));
				goto END_OF_THREAD;
				//return err;
			}
		}
		
		avail = snd_pcm_avail_update(handle);
		if (avail < 0) {
			err = xrun_recovery(handle, avail);
			if (err < 0) {
				printf("avail update failed: %s\n", snd_strerror(err));
				goto END_OF_THREAD;
				//return err;
			}
			first = 1;
			continue;
		}

		if (avail < period_size) {
			if (first) {
				first = 0;
				err = snd_pcm_start(handle);
				if (err < 0) {
					printf("Start error: %s\n", snd_strerror(err));
					exit(EXIT_FAILURE);
				}
			} else {
				err = snd_pcm_wait(handle, -1);
				if (err < 0) {
					if ((err = xrun_recovery(handle, err)) < 0) {
						printf("snd_pcm_wait error: %s\n", snd_strerror(err));
						exit(EXIT_FAILURE);
					}
					first = 1;
				}
			}
			continue;
		}

		writeAudioToDriver( handle, &phase, &first );
	}
	
END_OF_THREAD:
	return -1;
}


//-------------------------------------------------------------------------
//			HELP
//-------------------------------------------------------------------------
static void help(void)
{
	int k;
	printf(
		   "Usage: pcm [OPTION]... [FILE]...\n"
		   "-h,--help      help\n"
		   "-D,--device    playback device\n"
		   "-r,--rate      stream rate in Hz\n"
		   "-c,--channels  count of channels in stream\n"
		   "-f,--frequency sine wave frequency in Hz\n"
		   "-b,--buffer    ring buffer size in us\n"
		   "-p,--period    period size in us\n"
		   "-o,--format    sample format\n"
		   "-v,--verbose   show the PCM setup parameters\n"
		   "-n,--noresample  do not resample\n"
		   "-e,--pevent    enable poll event after each period\n"
		   "\n");
	printf("Recognized sample formats are:");
	for (k = 0; k < SND_PCM_FORMAT_LAST; ++k) {
		const char *s = (const char *)snd_pcm_format_name((snd_pcm_format_t)k);
		if (s)
			printf(" %s", s);
	}
	printf("\n");
}
//-------------------------------------------------------------------------
//			Option Command
//-------------------------------------------------------------------------
static int optionCommand(int morehelp, int argc, char *argv[])
{
	struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},
		{"device", 1, NULL, 'D'},
		{"samplingRate", 1, NULL, 'r'},
		{"channels", 1, NULL, 'c'},
		{"frequency", 1, NULL, 'f'},
		{"buffer", 1, NULL, 'b'},
		{"period", 1, NULL, 'p'},
		{"format", 1, NULL, 'o'},
		{"verbose", 1, NULL, 'v'},
		{"noresample", 1, NULL, 'n'},
		{"pevent", 1, NULL, 'e'},
		{"transpose", 1, NULL, 't'},
		{NULL, 0, NULL, 0},
	};

	while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "hD:r:c:f:b:p:m:o:vnet:", long_option, NULL)) < 0)
			break;
		switch (c) {
			case 'h':
				morehelp++;
				break;
			case 'D':
				device = strdup(optarg);
				break;
			case 'r':
				samplingRate = atoi(optarg);
				samplingRate = samplingRate < 4000 ? 4000 : samplingRate;
				samplingRate = samplingRate > 196000 ? 196000 : samplingRate;
				break;
			case 'c':
				channels = atoi(optarg);
				channels = channels < 1 ? 1 : channels;
				channels = channels > 1024 ? 1024 : channels;
				break;
			case 'f':
				freq = atoi(optarg);
				freq = freq < 50 ? 50 : freq;
				freq = freq > 5000 ? 5000 : freq;
				break;
			case 'b':
				buffer_time = atoi(optarg);
				buffer_time = buffer_time < 1000 ? 1000 : buffer_time;
				buffer_time = buffer_time > 1000000 ? 1000000 : buffer_time;
				break;
			case 'p':
				period_time = atoi(optarg);
				period_time = period_time < 1000 ? 1000 : period_time;
				period_time = period_time > 1000000 ? 1000000 : period_time;
				break;
			case 'o':
				int _format;
				for (_format = 0; _format < SND_PCM_FORMAT_LAST; _format++) {
					const char *format_name = (const char *)snd_pcm_format_name((snd_pcm_format_t)_format);
					if (format_name)
						if (!strcasecmp(format_name, optarg))
							break;
				}
				if (format == SND_PCM_FORMAT_LAST)
					format = SND_PCM_FORMAT_S16;
				if (!snd_pcm_format_linear(format) &&
					!(format == SND_PCM_FORMAT_FLOAT_LE ||
					  format == SND_PCM_FORMAT_FLOAT_BE)) {
						printf("Invalid (non-linear/float) format %s\n",
							   optarg);
						return 1;
					}
				break;
			case 'v':
				verbose = 1;
				break;
			case 'n':
				resample = 0;
				break;
			case 'e':
				period_event = 1;
				break;
			case 't':
				transpose = atoi(optarg);
				break;
			default: break;
		}
	}
	return morehelp;
}
//-------------------------------------------------------------------------
//			MAIN
//-------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	snd_pcm_t *handle;
	int err, morehelp;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	signed short *samples;
	unsigned int chn;
	snd_pcm_channel_area_t *areas;
	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_sw_params_alloca(&swparams);

	//--------------------------------------------------------
	//	Check Initialize Parameter
	morehelp = 0;
	morehelp = optionCommand( morehelp, argc, argv );
	
	if (morehelp) {
		help();
		return 0;
	}
	
	err = snd_output_stdio_attach(&output, stdout, 0);
	if (err < 0) {
		printf("Output failed: %s\n", snd_strerror(err));
		return 0;
	}
	
	printf("Playback device is %s\n", device);
	printf("Stream parameters are %iHz, %s, %i channels\n", samplingRate, snd_pcm_format_name(format), channels);
	printf("Sine wave rate is %.4fHz\n", freq);
	

	//--------------------------------------------------------
	//	ALSA Settings
	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		return 0;
	}

	if ((err = set_hwparams(handle, hwparams)) < 0) {
		printf("Setting of hwparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = set_swparams(handle, swparams)) < 0) {
		printf("Setting of swparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	
	printf("Buffer Size is %d\n", buffer_size);
	printf("Period Size is %d\n", period_size);

	//	reserve memory
	if (verbose > 0)
		snd_pcm_dump(handle, output);
	samples = (signed short *)malloc((period_size * channels * snd_pcm_format_physical_width(format)) / 8);
	if (samples == NULL) {
		printf("No enough memory\n");
		exit(EXIT_FAILURE);
	}
	
	//	create information by snd_pcm_channel_area_t
	areas = (snd_pcm_channel_area_t *)calloc(channels, sizeof(snd_pcm_channel_area_t));
	if (areas == NULL) {
		printf("No enough memory\n");
		exit(EXIT_FAILURE);
	}
	for (chn = 0; chn < channels; chn++) {
		areas[chn].addr = samples;
		areas[chn].first = chn * snd_pcm_format_physical_width(format);
		areas[chn].step = channels * snd_pcm_format_physical_width(format);
	}

	//--------------------------------------------------------
	//	Call Init MSGF
	raspiaudio_Init();	

	//--------------------------------------------------------
	//	Init Hardware
	initHw();

	//--------------------------------------------------------
	//	Main Loop
	err = soundGenerateLoop(handle);
	if (err < 0)
		printf("Transfer failed: %s\n", snd_strerror(err));
	free(areas);
	free(samples);
	snd_pcm_close(handle);

	//--------------------------------------------------------
	//	Quit Hardware
	quitHw();

	//--------------------------------------------------------
	//	Call End of MSGF
	raspiaudio_End();
	
	return 0;
}
#endif
