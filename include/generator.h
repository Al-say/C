#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

// 博客配置结构体
typedef struct {
    char* blog_title;
    char* blog_description;
    char* author;
    char* base_url;
    char* theme;
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
} GeneratorContext;

// 生成器上下文操作
GeneratorContext* create_generator_context(const BlogConfig* config, const char* output_dir);
void destroy_generator_context(GeneratorContext* ctx);

// 目录结构操作
void create_directory_structure(const char* base_dir);

// 文章处理函数
PostMetadata* extract_post_metadata(const char* markdown_content);
char* generate_permalink(const char* title, const char* date);
void process_posts(GeneratorContext* ctx, const char* posts_dir);

// 页面生成函数
void generate_post_page(GeneratorContext* ctx, const char* markdown_content, PostMetadata* metadata);
void generate_index_page(GeneratorContext* ctx);
void generate_tag_pages(GeneratorContext* ctx);
void generate_archive_page(GeneratorContext* ctx);
void generate_rss_feed(GeneratorContext* ctx);
void generate_sitemap(GeneratorContext* ctx);

// 模板处理函数
char* apply_template(const char* template_content, const char* content, const PostMetadata* metadata);

#endif /* GENERATOR_H */
