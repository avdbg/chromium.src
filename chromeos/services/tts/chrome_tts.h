// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_CHROME_TTS_H_
#define CHROMEOS_SERVICES_TTS_CHROME_TTS_H_

#include <cstddef>
#include <cstdint>

void GoogleTtsSetLogger(void (*logger_func)(int severity, const char* message));

bool GoogleTtsInit(const char* pipeline_path, const char* path_prefix);

void GoogleTtsShutdown();

bool GoogleTtsInstallVoice(const char* voice_name,
                           const uint8_t* voice_bytes,
                           int size);

bool GoogleTtsInitBuffered(const uint8_t* text_jspb,
                           const char* speaker_name,
                           int text_jspb_len);

int GoogleTtsReadBuffered(float* audio_channel_buffer, size_t* frames_written);

void GoogleTtsFinalizeBuffered();

size_t GoogleTtsGetTimepointsCount();

float GoogleTtsGetTimepointsTimeInSecsAtIndex(size_t index);

int GoogleTtsGetTimepointsCharIndexAtIndex(size_t index);

char* GoogleTtsGetEventBufferPtr();

size_t GoogleTtsGetEventBufferLen();

size_t GoogleTtsGetFramesInAudioBuffer();

#endif  // CHROMEOS_SERVICES_TTS_CHROME_TTS_H_
