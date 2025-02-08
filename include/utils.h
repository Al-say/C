#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 内存追踪宏
#ifdef DEBUG
    #define ALLOC(size) tracked_malloc(size, __FILE__, __LINE__)
    #define FREE(ptr) tracked_free(ptr, __FILE__, __LINE__)
#else
    #define ALLOC(size) malloc(size)
    #define FREE(ptr) free(ptr)
#endif

// 内存追踪函数
void* tracked_malloc(size_t size, const char* file, int line);
void tracked_free(void* ptr, const char* file, int line);

// 目录操作函数
void mkdir_p(const char* path);
void copy_directory(const char* src, const char* dest);

// 字符串处理函数
void trim_whitespace(char* str);
char* format_rss_date(const char* date);
size_t safe_strncpy(char* dest, const char* src, size_t n);

// 路径处理函数
char* get_file_ext(const char* filename);
int is_markdown_file(const char* filename);

// 文件系统辅助函数
int file_exists(const char* path);
long get_file_size(const char* path);

// HTML和URL编码函数
void html_encode(const char* src, char* dest, size_t dest_size);
void url_encode(const char* src, char* dest, size_t dest_size);

// 日志函数
void log_debug(const char* fmt, ...);
void log_error(const char* fmt, ...);

#endif /* UTILS_H */
