#ifndef SCOR_LOADER_H
#define SCOR_LOADER_H

#include "esp_err.h"

esp_err_t scor_load_and_run(const char *path);
void scor_unload(void);

#endif   