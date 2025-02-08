#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

// �������ýṹ��
typedef struct {
    char* site_title;       // ��վ����
    char* site_description; // ��վ����
    char* author;          // ������Ϣ
    char* theme;           // ��������
    char* base_url;        // ��վ����URL
} BlogConfig;

// ����Ԫ���ݽṹ��
typedef struct {
    char* title;           // ���±���
    char* date;            // ��������
    char* author;          // ����
    char* description;     // ��������
    char** tags;          // ��ǩ����
    int tag_count;        // ��ǩ����
    char* permalink;      // ��������
} PostMetadata;

// �����������Ľṹ��
typedef struct {
    BlogConfig* config;    // ��������
    char* output_dir;      // ���Ŀ¼
    char* template_dir;    // ģ��Ŀ¼
    MemPool* pool;        // �ڴ��
} GeneratorContext;

// ��Ҫ���ɺ���
void generate_blog(const char* output_dir);

// �����������Ĳ���
GeneratorContext* create_generator_context(const BlogConfig* config, const char* output_dir);
void destroy_generator_context(GeneratorContext* ctx);

// ���´�����
void process_posts(GeneratorContext* ctx, const char* posts_dir);
void generate_post_page(GeneratorContext* ctx, const char* markdown_content, PostMetadata* metadata);
void generate_index_page(GeneratorContext* ctx);
void generate_tag_pages(GeneratorContext* ctx);
void generate_archive_page(GeneratorContext* ctx);

// ģ�崦����
char* apply_template(const char* template_path, const char* content, const PostMetadata* metadata);
char* load_template(const char* template_path);

// ���ߺ���
void copy_static_assets(const char* src_dir, const char* dest_dir);
PostMetadata* extract_post_metadata(const char* markdown_content);
void create_directory_structure(const char* base_dir);
char* generate_permalink(const char* title, const char* date);

// RSS��վ���ͼ����
void generate_rss_feed(GeneratorContext* ctx);
void generate_sitemap(GeneratorContext* ctx);

#endif /* GENERATOR_H */
