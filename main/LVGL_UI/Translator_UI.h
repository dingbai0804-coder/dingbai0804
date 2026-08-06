#pragma once

#include "lvgl.h"

void Translator_UI_Create(void);
void Translator_UI_SetListening(bool listening);
void Translator_UI_SetTranscript(const char *source, const char *translation);

