#ifndef MENTAL_OPENROUTER_AI_H
#define MENTAL_OPENROUTER_AI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* ---- Configuration (Braintrust Gateway) ------------------------- */

/*
 * Braintrust Gateway endpoint.
 * Note: /v1/proxy/chat/completions is the legacy proxy path that accepts
 * OpenAI-format requests directly. The newer gateway is at
 * https://gateway.braintrust.dev/v1/chat/completions, but requires the
 * OpenAI SDK-style base URL handling. The proxy path is simpler for raw curl.
 */
#define MENTAL_AI_ENDPOINT "https://openrouter.ai/api/v1/chat/completions"
#define MENTAL_AI_MODEL    "free-claude-haiku-4-5-20251001"

/* ---- Data Types ------------------------------------------------ */

typedef struct MentalAIMessage {
    char *role;     /* "user", "assistant", or "system" */
    char *content;
} MentalAIMessage;

typedef struct MentalAIChat {
    MentalAIMessage *messages;
    int count;
    int capacity;
    char api_key[256];
    char last_error[1024];
} MentalAIChat;

/* ---- Internal: curl write callback ----------------------------- */

static size_t _mental_ai_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    char **response = (char**)userdata;

    size_t old_len = *response ? strlen(*response) : 0;
    char *new_ptr = realloc(*response, old_len + total + 1);
    if (!new_ptr) return 0;

    memcpy(new_ptr + old_len, ptr, total);
    new_ptr[old_len + total] = '\0';
    *response = new_ptr;
    return total;
}

/* ---- Internal: JSON string escape ------------------------------ */

static char* _mental_ai_escape_json(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char *out = malloc(len * 2 + 1);
    if (!out) return NULL;
    char *p = out;
    for (const char *c = s; *c; c++) {
        switch (*c) {
            case '"':  *p++ = '\\'; *p++ = '"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
            default:   *p++ = *c; break;
        }
    }
    *p = '\0';
    return out;
}

/* ---- Internal: simple JSON content extractor ------------------- */

static char* _mental_ai_extract_content(const char *json) {
    if (!json) return NULL;

    const char *key = "\"content\":\"";
    const char *start = strstr(json, key);
    if (!start) {
        key = "\"content\": \"";
        start = strstr(json, key);
    }
    if (!start) return NULL;

    start += strlen(key);

    const char *end = start;
    while (*end) {
        if (*end == '"' && (end == start || *(end-1) != '\\')) break;
        end++;
    }

    size_t len = end - start;
    char *result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, start, len);
    result[len] = '\0';

    char *w = result;
    for (char *r = result; *r; r++) {
        if (*r == '\\' && *(r+1)) {
            r++;
            switch (*r) {
                case 'n': *w++ = '\n'; break;
                case 't': *w++ = '\t'; break;
                case 'r': *w++ = '\r'; break;
                case '"': *w++ = '"'; break;
                case '\\': *w++ = '\\'; break;
                default: *w++ = *r; break;
            }
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
    return result;
}

/* ---- Public API ------------------------------------------------ */

static inline MentalAIChat* mentalAIChatCreate(const char *api_key, const char *system_prompt) {
    if (!api_key) return NULL;

    MentalAIChat *chat = calloc(1, sizeof(MentalAIChat));
    if (!chat) return NULL;

    strncpy(chat->api_key, api_key, sizeof(chat->api_key) - 1);
    chat->capacity = 8;
    chat->messages = calloc(chat->capacity, sizeof(MentalAIMessage));
    if (!chat->messages) { free(chat); return NULL; }

    if (system_prompt && strlen(system_prompt) > 0) {
        chat->messages[0].role = strdup("system");
        chat->messages[0].content = strdup(system_prompt);
        chat->count = 1;
    }

    return chat;
}

static inline char* mentalAIChatSend(MentalAIChat *chat, const char *user_message) {
    if (!chat || !user_message) return NULL;

    /* 1. Append user message to history */
    if (chat->count >= chat->capacity) {
        int new_cap = chat->capacity * 2;
        MentalAIMessage *tmp = realloc(chat->messages, new_cap * sizeof(MentalAIMessage));
        if (!tmp) return NULL;
        chat->messages = tmp;
        chat->capacity = new_cap;
    }

    int user_idx = chat->count++;
    chat->messages[user_idx].role = strdup("user");
    chat->messages[user_idx].content = strdup(user_message);

    /* 2. Build JSON request body */
    char *body = malloc(65536);
    if (!body) return NULL;
    strcpy(body, "{\"model\":\"");
    strcat(body, MENTAL_AI_MODEL);
    strcat(body, "\",\"messages\":[");

    for (int i = 0; i < chat->count; i++) {
        if (i > 0) strcat(body, ",");
        strcat(body, "{\"role\":\"");
        strcat(body, chat->messages[i].role);
        strcat(body, "\",\"content\":\"");
        char *escaped = _mental_ai_escape_json(chat->messages[i].content);
        if (escaped) {
            strcat(body, escaped);
            free(escaped);
        }
        strcat(body, "\"}");
    }
    strcat(body, "]}");

    /* 3. Setup curl */
    CURL *curl = curl_easy_init();
    if (!curl) { free(body); return NULL; }

    char *response = NULL;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", chat->api_key);
    headers = curl_slist_append(headers, auth);

    curl_easy_setopt(curl, CURLOPT_URL, MENTAL_AI_ENDPOINT);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _mental_ai_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(body);

    if (res != CURLE_OK) {
        free(response);
        return NULL;
    }

    /* 4. Extract assistant reply */
    char *reply = _mental_ai_extract_content(response);
    free(response);

    if (!reply) {
        /* Save raw response for debugging */
        if (response)
            strncpy(chat->last_error, response, sizeof(chat->last_error) - 1);
        else
            strncpy(chat->last_error, "(null response)", sizeof(chat->last_error) - 1);

        free(chat->messages[user_idx].role);
        free(chat->messages[user_idx].content);
        chat->count--;
        free(response);
        return NULL;
    }

    /* 5. Append assistant reply to history */
    if (chat->count >= chat->capacity) {
        int new_cap = chat->capacity * 2;
        MentalAIMessage *tmp = realloc(chat->messages, new_cap * sizeof(MentalAIMessage));
        if (!tmp) { free(reply); return NULL; }
        chat->messages = tmp;
        chat->capacity = new_cap;
    }

    int asst_idx = chat->count++;
    chat->messages[asst_idx].role = strdup("assistant");
    chat->messages[asst_idx].content = strdup(reply);

    return reply;
}

static inline void mentalAIChatDestroy(MentalAIChat *chat) {
    if (!chat) return;
    for (int i = 0; i < chat->count; i++) {
        free(chat->messages[i].role);
        free(chat->messages[i].content);
    }
    free(chat->messages);
    free(chat);
}

#endif /* MENTAL_OPENROUTER_AI_H */