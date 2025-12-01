// main.c
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif

#include <curl/curl.h>
#include "cjson/cJSON.h"

// ==========================================
// [1] 초간단 인코딩 함수 (utf8, korean)
// ==========================================

// 한글(CP949) -> UTF-8 변환
char* utf8(const char* src) {
#ifdef _WIN32
    if (!src) return NULL;
    int len1 = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
    wchar_t* wstr = (wchar_t*)malloc(sizeof(wchar_t) * len1);
    MultiByteToWideChar(CP_ACP, 0, src, -1, wstr, len1);

    int len2 = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* dest = (char*)malloc(len2);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, dest, len2, NULL, NULL);

    free(wstr);
    return dest;
#else
    return _strdup(src);
#endif
}

// UTF-8 -> 한글(CP949) 변환
char* korean(const char* src) {
#ifdef _WIN32
    if (!src) return NULL;
    int len1 = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    wchar_t* wstr = (wchar_t*)malloc(sizeof(wchar_t) * len1);
    MultiByteToWideChar(CP_UTF8, 0, src, -1, wstr, len1);

    int len2 = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* dest = (char*)malloc(len2);
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, dest, len2, NULL, NULL);

    free(wstr);
    return dest;
#else
    return _strdup(src);
#endif
}

// ==========================================
// [2] 데이터 수신 (Buffer, write_data)
// ==========================================

struct Buffer {
    char* data;
    size_t size;
};

static size_t write_data(void* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t len = size * nmemb;
    struct Buffer* buf = (struct Buffer*)userdata;

    char* new_ptr = (char*)realloc(buf->data, buf->size + len + 1);
    if (!new_ptr) return 0;

    buf->data = new_ptr;
    memcpy(&(buf->data[buf->size]), ptr, len);
    buf->size += len;
    buf->data[buf->size] = 0;

    return len;
}

static void make_dir(const char* path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0777);
#endif
}

// ==========================================
// [3] 프롬프트 만들기 (★깔끔한 출력 요청 추가★)
// ==========================================
static char* make_prompt(
    const char* name, const char* sex, const char* age, const char* height,
    const char* weight, const char* muscle, const char* goal,
    const char* diet, const char* allergy, const char* gym)
{
    // [수정 포인트] 6번 지침과 7번 지침을 추가하여 특수문자를 금지시켰습니다.
    const char* fmt =
        "당신은 운동처방사입니다. 한국인에게 맞는 7일 식단과 운동 루틴을 짜주세요.\n"
        "## 정보\n- 이름:%s, 성별:%s, 나이:%s, 키:%s, 체중:%s, 골격근:%s\n"
        "- 목표:%s, 식단선호:%s, 제한:%s, 장소:%s\n"
        "## 지침\n"
        "1) 7일치 계획을 요일별로 상세하게 설명하세요.\n"
        "2) 식단: 한식 위주, 칼로리/탄단지 포함.\n"
        "3) 운동: 워밍업/메인/쿨다운 상세히.\n"
        "4) JSON 형식을 먼저 출력하고, 그 뒤에 줄글로 상세 설명을 덧붙이세요.\n"
        "5) [중요] 출력 시 '마크다운(Markdown)', '볼드체(**)', '표(|)'를 절대 사용하지 마세요.\n"
        "6) [중요] '이모지(이모티콘)'나 특수문자를 사용하지 마세요. (윈도우 콘솔에서 깨집니다)\n"
        "7) 오직 기호 없는 '순수한 텍스트(Plain Text)'로만 깔끔하게 작성하세요.\n"
        "### JSON 스키마 예\n"
        "{\n"
        "  \"name\":\"%s\",\n"
        "  \"days\": [\n"
        "    {\"day\":1,\n"
        "     \"meals\":{\"breakfast\":\"\",\"lunch\":\"\",\"snack\":\"\",\"dinner\":\"\"},\n"
        "     \"workout\":{\"warmup\":\"\",\"main\":\"\",\"cooldown\":\"\"}\n"
        "    }\n"
        "  ]\n"
        "}\n";

    size_t len = snprintf(NULL, 0, fmt, name, sex, age, height, weight, muscle, goal, diet, allergy, gym, name) + 1;
    char* txt = (char*)malloc(len);
    if (!txt) return NULL;

    snprintf(txt, len, fmt, name, sex, age, height, weight, muscle, goal, diet, allergy, gym, name);

    char* res = utf8(txt);
    free(txt);
    return res;
}

// ==========================================
// [4] Gemini에게 요청 (ask_gemini)
// ==========================================
static char* ask_gemini(const char* key, const char* prompt) {
    const char* url = "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
    const char* model = "gemini-2.5-flash";

    CURL* curl = curl_easy_init();
    if (!curl) return NULL;

    struct curl_slist* list = NULL;
    struct Buffer res = { 0 };

    list = curl_slist_append(list, "Content-Type: application/json");
    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", key);
    list = curl_slist_append(list, auth);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", model);
    
    cJSON* msgs = cJSON_CreateArray();
    cJSON* sys = cJSON_CreateObject();
    cJSON_AddStringToObject(sys, "role", "system");
    cJSON_AddStringToObject(sys, "content", "You are a Korean trainer. Do NOT use Markdown, Bold(**), or Emojis.");
    cJSON_AddItemToArray(msgs, sys);

    cJSON* usr = cJSON_CreateObject();
    cJSON_AddStringToObject(usr, "role", "user");
    cJSON_AddStringToObject(usr, "content", prompt);
    cJSON_AddItemToArray(msgs, usr);

    cJSON_AddItemToObject(root, "messages", msgs);
    cJSON_AddNumberToObject(root, "max_tokens", 15000); 
    cJSON_AddNumberToObject(root, "temperature", 0.7);

    char* body = cJSON_PrintUnformatted(root);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);

    CURLcode code = curl_easy_perform(curl);

    cJSON_Delete(root);
    free(body);
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        if (res.data) free(res.data);
        return NULL;
    }
    return res.data;
}

// JSON에서 내용 뽑기 (parse_answer)
static char* parse_answer(const char* json) {
    cJSON* root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (!choices) { cJSON_Delete(root); return NULL; }
    
    cJSON* item = cJSON_GetArrayItem(choices, 0);
    cJSON* msg = cJSON_GetObjectItem(item, "message");
    cJSON* content = cJSON_GetObjectItem(msg, "content");

    char* txt = NULL;
    if (cJSON_IsString(content) && content->valuestring) {
        txt = _strdup(content->valuestring);
    }

    cJSON_Delete(root);
    return txt;
}

// 파일 저장 (save_file)
static void save_file(const char* name, const char* content) {
    make_dir("plans");
    
    char date[16];
    time_t t = time(NULL);
    strftime(date, sizeof(date), "%Y%m%d", localtime(&t));

    char path_txt[256];
    char path_json[256];

    snprintf(path_txt, sizeof(path_txt), "plans/%s_%s.txt", name, date);
    snprintf(path_json, sizeof(path_json), "plans/%s_%s.json", name, date);

    // 1. 텍스트 파일 저장
    FILE* fp = fopen(path_txt, "wb");
    if (fp) {
        unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        fwrite(bom, 1, 3, fp);
        fwrite(content, 1, strlen(content), fp);
        fclose(fp);
        printf("[저장됨] %s\n", path_txt);
    }

    // 2. JSON 부분만 따로 저장
    const char* start = strchr(content, '{');
    if (start) {
        int cnt = 0; const char* end = NULL;
        for (const char* p = start; *p; ++p) {
            if (*p == '{') cnt++;
            else if (*p == '}') { cnt--; if (cnt == 0) { end = p; break; } }
        }
        if (end) {
            FILE* fj = fopen(path_json, "wb");
            if (fj) {
                unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
                fwrite(bom, 1, 3, fj);
                fwrite(start, 1, (size_t)(end - start + 1), fj);
                fclose(fj);
                printf("[저장됨] %s (JSON 블록)\n", path_json);
            }
        }
    }
}

int main(void) {
    printf("=== AI 헬스 플래너 (Clean Text Ver) ===\n");

    char name[64], sex[16], age[16], height[16], weight[16], muscle[16];
    char goal[16], diet[128], allergy[128], gym[128];

    printf("이름: "); fgets(name, sizeof(name), stdin);
    printf("성별: "); fgets(sex, sizeof(sex), stdin);
    printf("나이: "); fgets(age, sizeof(age), stdin);
    printf("키(cm): ");   fgets(height, sizeof(height), stdin);
    printf("체중(kg): "); fgets(weight, sizeof(weight), stdin);
    printf("골격근(kg): "); fgets(muscle, sizeof(muscle), stdin);
    printf("목표: "); fgets(goal, sizeof(goal), stdin);
    printf("식단: "); fgets(diet, sizeof(diet), stdin);
    printf("알러지: "); fgets(allergy, sizeof(allergy), stdin);
    printf("장소: "); fgets(gym, sizeof(gym), stdin);

#define TRIM(s) s[strcspn(s, "\n")] = 0
    TRIM(name); TRIM(sex); TRIM(age); TRIM(height);
    TRIM(weight); TRIM(muscle); TRIM(goal);
    TRIM(diet); TRIM(allergy); TRIM(gym);

    char* prompt = make_prompt(name, sex, age, height, weight, muscle, goal, diet, allergy, gym);
    if (!prompt) return 1;

    char* key = getenv("GEMINI_API_KEY");
    if (!key) { printf("키 없음\n"); return 1; }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    printf("\n요청 중...\n");
    char* json = ask_gemini(key, prompt);
    free(prompt);

    if (!json) { printf("실패\n"); return 1; }

    char* text = parse_answer(json);
    if (!text) { printf("파싱 실패\n"); return 1; }

    char* kor_text = korean(text);
    if (kor_text) {
        printf("\n=== 결과 ===\n%s\n", kor_text);
        free(kor_text);
    }

    save_file(name, text);

    free(text);
    free(json);
    curl_global_cleanup();
    return 0;
}