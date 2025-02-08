#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define PATH_SEPARATOR '\\'
#define MKDIR_CMD "mkdir "
#else
#include <unistd.h>
#define PATH_SEPARATOR '/'
#define MKDIR_CMD "mkdir -p "
#endif

// �ڴ�׷�ٺ���
static size_t total_allocated = 0;
static size_t allocation_count = 0;

void* tracked_malloc(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    if (ptr) {
        total_allocated += size;
        allocation_count++;
        fprintf(stderr, "[MEM] Allocated %zu bytes at %s:%d (total: %zu, count: %zu)\n",
                size, file, line, total_allocated, allocation_count);
    } else {
        fprintf(stderr, "[MEM] Failed to allocate %zu bytes at %s:%d\n",
                size, file, line);
    }
    return ptr;
}

void tracked_free(void* ptr, const char* file, int line) {
    if (ptr) {
        allocation_count--;
        fprintf(stderr, "[MEM] Freed memory at %s:%d (count: %zu)\n",
                file, line, allocation_count);
        free(ptr);
    }
}

// Ŀ¼��������
void mkdir_p(const char* path) {
    char tmp[1024];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == PATH_SEPARATOR)
        tmp[len - 1] = 0;
    
    for (p = tmp + 1; *p; p++) {
        if (*p == PATH_SEPARATOR) {
            *p = 0;
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = PATH_SEPARATOR;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

void copy_directory(const char* src, const char* dest) {
#ifdef _WIN32
    char command[1024];
    snprintf(command, sizeof(command), "xcopy /E /I \"%s\" \"%s\"", src, dest);
#else
    char command[1024];
    snprintf(command, sizeof(command), "cp -r \"%s/\" \"%s/\"", src, dest);
#endif
    if (system(command) != 0) {
        fprintf(stderr, "Failed to copy directory from %s to %s\n", src, dest);
    }
}

// ��ʽ�����ں���
char* format_rss_date(const char* date) {
    // ���������ʽΪ YYYY-MM-DD
    struct tm tm = {0};
    static char formatted[50];
    
    if (sscanf(date, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
        tm.tm_year -= 1900;  // ��ݴ�1900��ʼ
        tm.tm_mon -= 1;      // �·ݴ�0��ʼ
        
        strftime(formatted, sizeof(formatted),
                "%a, %d %b %Y %H:%M:%S +0000",
                &tm);
        return formatted;
    }
    return NULL;
}

// �ַ�����������
void trim_whitespace(char* str) {
    char* end;

    // ������ͷ�հ�
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return;

    // �Ӻ���ǰ�Ƴ��հ�
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

// ·����������
char* get_file_ext(const char* filename) {
    char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

int is_markdown_file(const char* filename) {
    char* ext = get_file_ext(filename);
    return strcmp(ext, "md") == 0 || strcmp(ext, "markdown") == 0;
}

// �ļ�ϵͳ��������
int file_exists(const char* path) {
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

long get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// ���Ժ���־����
#ifdef DEBUG
void log_debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[DEBUG] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
#else
void log_debug(const char* fmt, ...) {
    (void)fmt; // 避免未使用参数的警告
}
#endif

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// ��ȫ�ַ�������
size_t safe_strncpy(char* dest, const char* src, size_t n) {
    if (!dest || !src || n == 0) return 0;
    
    size_t i;
    for (i = 0; i < n - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return i;
}

// HTML��ȫ����
void html_encode(const char* src, char* dest, size_t dest_size) {
    size_t i = 0, j = 0;
    
    while (src[i] && j < dest_size - 1) {
        switch (src[i]) {
            case '<':
                if (j + 4 >= dest_size) break;
                memcpy(&dest[j], "&lt;", 4);
                j += 4;
                break;
            case '>':
                if (j + 4 >= dest_size) break;
                memcpy(&dest[j], "&gt;", 4);
                j += 4;
                break;
            case '&':
                if (j + 5 >= dest_size) break;
                memcpy(&dest[j], "&amp;", 5);
                j += 5;
                break;
            case '"':
                if (j + 6 >= dest_size) break;
                memcpy(&dest[j], "&quot;", 6);
                j += 6;
                break;
            default:
                dest[j++] = src[i];
                break;
        }
        i++;
    }
    dest[j] = '\0';
}

// URL��ȫ����
void url_encode(const char* src, char* dest, size_t dest_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t i = 0, j = 0;
    
    while (src[i] && j < dest_size - 1) {
        if (isalnum((unsigned char)src[i]) || src[i] == '-' || src[i] == '_' || src[i] == '.' || src[i] == '~') {
            dest[j++] = src[i];
        } else {
            if (j + 3 >= dest_size) break;
            dest[j++] = '%';
            dest[j++] = hex[(unsigned char)src[i] >> 4];
            dest[j++] = hex[(unsigned char)src[i] & 15];
        }
        i++;
    }
    dest[j] = '\0';
}
