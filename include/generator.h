#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

// 博客配置结构体
typedef struct {
    char* site_title;       // 网站标题
    char* site_description; // 网站描述
    char* author;          // 作者信息
    char* theme;           // 主题名称
    char* base_url;        // 网站基础URL
} BlogConfig;

// 文章元数据结构体
typedef struct {
    char* title;           // 文章标题
    char* date;            // 发布日期
    char* author;          // 作者
    char* description;     // 文章描述
    char** tags;          // 标签数组
    int tag_count;        // 标签数量
    char* permalink;      // 永久链接
} PostMetadata;

// 生成器上下文结构体
typedef struct {
    BlogConfig* config;    // 博客配置
    char* output_dir;      // 输出目录
    char* template_dir;    // 模板目录
    MemPool* pool;        // 内存池
} GeneratorContext;

// 主要生成函数
void generate_blog(const char* output_dir);

// 生成器上下文操作
GeneratorContext* create_generator_context(const BlogConfig* config, const char* output_dir);
void destroy_generator_context(GeneratorContext* ctx);

// 文章处理函数
void process_posts(GeneratorContext* ctx, const char* posts_dir);
void generate_post_page(GeneratorContext* ctx, const char* markdown_content, PostMetadata* metadata);
void generate_index_page(GeneratorContext* ctx);
void generate_tag_pages(GeneratorContext* ctx);
void generate_archive_page(GeneratorContext* ctx);

// 模板处理函数
char* apply_template(const char* template_path, const char* content, const PostMetadata* metadata);
char* load_template(const char* template_path);

// 工具函数
void copy_static_assets(const char* src_dir, const char* dest_dir);
PostMetadata* extract_post_metadata(const char* markdown_content);
void create_directory_structure(const char* base_dir);
char* generate_permalink(const char* title, const char* date);

// RSS和站点地图生成
void generate_rss_feed(GeneratorContext* ctx);
void generate_sitemap(GeneratorContext* ctx);

#endif /* GENERATOR_H */
