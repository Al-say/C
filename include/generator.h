#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "parser.h"

// 错误处理枚举
typedef enum {
    GEN_SUCCESS = 0,
    GEN_ERROR_TIMEOUT,
    GEN_ERROR_MEMORY,
    GEN_ERROR_IO,
    GEN_ERROR_NETWORK
} GeneratorError;

// 博客配置结构体
typedef struct {
    char* blog_title;
    char* blog_description;
    char* author;
    char* base_url;
    char* theme;
    
    // 性能优化配置
    int enable_compression;     // 启用Gzip压缩
    int enable_incremental;     // 启用增量构建
    int parallel_workers;       // 并行处理线程数
    size_t chunk_size;         // 文件分块处理大小(KB)
    int timeout_seconds;       // 操作超时时间
    int retry_count;           // 重试次数
    int retry_delay;           // 重试延迟(秒)
} BlogConfig;

// 文章元数据结构体
typedef struct {
    char* title;
    char* date;
    char* author;
    char* description;
    char* permalink;
    char** tags;
    int tag_count;
} PostMetadata;

// 生成器上下文结构体
typedef struct {
    MemPool* pool;
    BlogConfig* config;
    char* output_dir;
    char* template_dir;
    time_t start_time;        // 添加开始时间字段
    GeneratorError last_error; // 添加错误状态字段
} GeneratorContext;

// 生成器上下文操作
GeneratorContext* create_generator_context(const BlogConfig* config, const char* output_dir);
void destroy_generator_context(GeneratorContext* ctx);

// 目录结构操作
void create_directory_structure(const char* base_dir);

// 文章处理函数
PostMetadata* extract_post_metadata(const char* markdown_content);
char* generate_permalink(const char* title, const char* date);
int process_posts(GeneratorContext* ctx, const char* posts_dir);

// 页面生成函数
int generate_post_page(GeneratorContext* ctx, const char* markdown_content, PostMetadata* metadata);
int generate_index_page(GeneratorContext* ctx);
int generate_tag_pages(GeneratorContext* ctx);
int generate_archive_page(GeneratorContext* ctx);
int generate_rss_feed(GeneratorContext* ctx);
int generate_sitemap(GeneratorContext* ctx);

// 模板处理函数
char* apply_template(const char* template_content, const char* content, const PostMetadata* metadata);

// 错误处理和优化函数
GeneratorError get_last_error(GeneratorContext* ctx);
const char* get_error_message(GeneratorError error);
int has_operation_timeout(GeneratorContext* ctx);
void sleep_with_backoff(int retry_count, int base_delay);
int needs_rebuild(const char* source_file, const char* target_file);
int compress_file(const char* input_path, const char* output_path);

#endif /* GENERATOR_H */
