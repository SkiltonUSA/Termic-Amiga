#ifndef TERMIC_PROMPTS_H
#define TERMIC_PROMPTS_H
#include "vendor/cJSON.h"
#define PROMPTS_LIMIT 524288
#define PROMPTS_MAX 48
/* Desktop Termic's delta format: customs, overrides, deletedBuiltins,
 * disabled and order. Unedited defaults follow the shipped library. */
const char *prompt_string(const cJSON *item, const char *name);
cJSON *prompt_find(cJSON *items, const char *id);
int prompt_valid(const cJSON *item);
cJSON *prompts_resolve(cJSON *defaults, cJSON *stored);
cJSON *prompts_delta(cJSON *defaults, cJSON *items);
#endif
