#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// �ڴ�׷�ٺ�
#ifdef DEBUG
    #define ALLOC(size) tracked_malloc(size, __FILE__, __LINE__)
    #define FREE(ptr) tracked_free(ptr, __FILE__, __LINE__)
#else
    #define ALLOC(size) malloc(size)
    #define FREE(ptr) free(ptr)
#endif

// �ڴ�׷�ٺ���
void* tracked_malloc(size_t size, const char* file, int line);
void tracked_free(void* ptr, const char* file, int line);

// Ŀ¼��������
void mkdir_p(const char* path);
void copy_directory(const char* src, const char* dest);

// �ַ���������
void trim_whitespace(char* str);
char* format_rss_date(const char* date);
size_t safe_strncpy(char* dest, const char* src, size_t n);

// ·��������
char* get_file_ext(const char* filename);
int is_markdown_file(const char* filename);

// �ļ�ϵͳ��������
int file_exists(const char* path);
long get_file_size(const char* path);

// HTML��URL���뺯��
void html_encode(const char* src, char* dest, size_t dest_size);
void url_encode(const char* src, char* dest, size_t dest_size);

// ��־����
void log_debug(const char* fmt, ...);
void log_error(const char* fmt, ...);

#endif /* UTILS_H */
