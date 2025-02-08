#include "../include/generator.h"
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#define PATH_SEPARATOR '\\'
#define MKDIR(path) _mkdir(path)
#define F_OK 0
#define access _access
#define MAX_PATH 260
#else
#include <unistd.h>
#include <dirent.h>
#define PATH_SEPARATOR '/'
#define MKDIR(path) mkdir(path, 0755)
#endif

// ��������������ļ��Ƿ����
static int file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

// ��������������Ƿ���Markdown�ļ�
static int is_markdown_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    return ext && strcmp(ext, ".md") == 0;
}

// ��������������·��
static char* join_path(const char* dir, const char* file) {
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    char* path = malloc(dir_len + file_len + 2);
    
    if (path) {
        strcpy(path, dir);
        if (dir[dir_len - 1] != PATH_SEPARATOR) {
            path[dir_len] = PATH_SEPARATOR;
            strcpy(path + dir_len + 1, file);
        } else {
            strcpy(path + dir_len, file);
        }
    }
    return path;
}

// �����������Ĳ���
GeneratorContext* create_generator_context(const BlogConfig* config, const char* output_dir) {
    GeneratorContext* ctx = (GeneratorContext*)malloc(sizeof(GeneratorContext));
    if (!ctx) return NULL;
    
    ctx->pool = create_memory_pool(1024 * 1024);  // 1MB
    if (!ctx->pool) {
        free(ctx);
        return NULL;
    }
    
    ctx->config = (BlogConfig*)pool_alloc(ctx->pool, sizeof(BlogConfig));
    if (!ctx->config) {
        destroy_memory_pool(ctx->pool);
        free(ctx);
        return NULL;
    }
    
    memcpy(ctx->config, config, sizeof(BlogConfig));
    
    size_t dir_len = strlen(output_dir);
    ctx->output_dir = (char*)pool_alloc(ctx->pool, dir_len + 1);
    if (!ctx->output_dir) {
        destroy_memory_pool(ctx->pool);
        free(ctx);
        return NULL;
    }
    strcpy(ctx->output_dir, output_dir);
    
    ctx->template_dir = NULL;  // ���ں�������
    
    return ctx;
}

void destroy_generator_context(GeneratorContext* ctx) {
    if (ctx) {
        destroy_memory_pool(ctx->pool);
        free(ctx);
    }
}

// ����Ŀ¼�ṹ
void create_directory_structure(const char* base_dir) {
    char* dirs[] = {
        "posts",
        "assets",
        "assets/css",
        "assets/js",
        "assets/images",
        "templates"
    };
    
    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); i++) {
        char* path = join_path(base_dir, dirs[i]);
        if (path) {
            MKDIR(path);
            free(path);
        }
    }
}

// ��ȡ����Ԫ����
PostMetadata* extract_post_metadata(const char* markdown_content) {
    if (!markdown_content) return NULL;
    
    PostMetadata* metadata = malloc(sizeof(PostMetadata));
    if (!metadata) return NULL;
    
    // ��ʼ��Ĭ��ֵ
    metadata->title = NULL;
    metadata->date = NULL;
    metadata->author = NULL;
    metadata->description = NULL;
    metadata->tags = NULL;
    metadata->tag_count = 0;
    metadata->permalink = NULL;
    
    char line[1024];
    const char* ptr = markdown_content;
    int in_metadata = 0;
    
    // ����Ԫ���ݿ� (YAML front matter)
    if (strncmp(ptr, "---\n", 4) == 0) {
        in_metadata = 1;
        ptr += 4;
        
        while (*ptr && in_metadata) {
            size_t i = 0;
            while (*ptr && *ptr != '\n' && i < sizeof(line)-1) {
                line[i++] = *ptr++;
            }
            line[i] = '\0';
            if (*ptr) ptr++;
            
            if (strcmp(line, "---") == 0) {
                in_metadata = 0;
                break;
            }
            
            char* key = line;
            char* value = strchr(line, ':');
            if (value) {
                *value = '\0';
                value++;
                while (isspace(*value)) value++;
                
                if (strcmp(key, "title") == 0) {
                    metadata->title = strdup(value);
                } else if (strcmp(key, "date") == 0) {
                    metadata->date = strdup(value);
                } else if (strcmp(key, "author") == 0) {
                    metadata->author = strdup(value);
                } else if (strcmp(key, "description") == 0) {
                    metadata->description = strdup(value);
                }
            }
        }
    }
    
    return metadata;
}

// ������������
char* generate_permalink(const char* title, const char* date) {
    if (!title || !date) return NULL;
    
    char* permalink = malloc(strlen(title) + strlen(date) + 2);
    if (!permalink) return NULL;
    
    // �򻯵�������������
    sprintf(permalink, "%s-%s", date, title);
    
    // ���ո�ת��Ϊ���ַ�
    for (char* p = permalink; *p; p++) {
        if (isspace(*p)) *p = '-';
    }
    
    return permalink;
}

// ��������
void process_posts(GeneratorContext* ctx, const char* posts_dir) {
#ifdef _WIN32
    WIN32_FIND_DATA find_data;
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*.md", posts_dir);
    
    HANDLE find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) return;
    
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char* post_path = join_path(posts_dir, find_data.cFileName);
            if (post_path && is_markdown_file(post_path)) {
                FILE* fp = fopen(post_path, "r");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long size = ftell(fp);
                    rewind(fp);
                    
                    char* content = malloc(size + 1);
                    if (content) {
                        if (fread(content, 1, size, fp) == (size_t)size) {
                            content[size] = '\0';
                            PostMetadata* metadata = extract_post_metadata(content);
                            if (metadata) {
                                generate_post_page(ctx, content, metadata);
                                free(metadata);
                            }
                        }
                        free(content);
                    }
                    fclose(fp);
                }
                free(post_path);
            }
        }
    } while (FindNextFile(find_handle, &find_data));
    
    FindClose(find_handle);
#else
    DIR* dir = opendir(posts_dir);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        char* post_path = join_path(posts_dir, entry->d_name);
        if (post_path && is_markdown_file(post_path)) {
            FILE* fp = fopen(post_path, "r");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);
                rewind(fp);
                
                char* content = malloc(size + 1);
                if (content) {
                    if (fread(content, 1, size, fp) == (size_t)size) {
                        content[size] = '\0';
                        PostMetadata* metadata = extract_post_metadata(content);
                        if (metadata) {
                            generate_post_page(ctx, content, metadata);
                            free(metadata);
                        }
                    }
                    free(content);
                }
                fclose(fp);
            }
            free(post_path);
        }
    }
    closedir(dir);
#endif
}

// ��������ҳ��
void generate_post_page(GeneratorContext* ctx, const char* markdown_content, PostMetadata* metadata) {
    ParserContext* parser_ctx = create_parser_context(NULL);
    if (!parser_ctx) return;
    
    if (parse_markdown_with_context(parser_ctx, markdown_content)) {
        char* html_content = get_html_output(parser_ctx);
        
        if (html_content) {
            char* output_path = join_path(ctx->output_dir, 
                metadata->permalink ? metadata->permalink : "post.html");
            
            if (output_path) {
                FILE* fp = fopen(output_path, "w");
                if (fp) {
                    char* page = apply_template("templates/post.html", html_content, metadata);
                    if (page) {
                        fputs(page, fp);
                        free(page);
                    }
                    fclose(fp);
                }
                free(output_path);
            }
            free(html_content);
        }
    }
    
    destroy_parser_context(parser_ctx);
}

// ������վ
void generate_blog(const char* output_dir) {
    BlogConfig config = {
        .site_title = "My C Blog",
        .site_description = "A static blog generated by C",
        .author = "Anonymous",
        .theme = "default",
        .base_url = "http://localhost"
    };
    
    GeneratorContext* ctx = create_generator_context(&config, output_dir);
    if (!ctx) return;
    
    create_directory_structure(output_dir);
    process_posts(ctx, "posts");
    generate_index_page(ctx);
    generate_tag_pages(ctx);
    generate_archive_page(ctx);
    generate_rss_feed(ctx);
    generate_sitemap(ctx);
    
    destroy_generator_context(ctx);
}

// RSS��վ���ͼ����
void generate_rss_feed(GeneratorContext* ctx) {
    if (!ctx || !ctx->config) return;
    
    char* rss_path = join_path(ctx->output_dir, "feed.xml");
    if (!rss_path) return;
    
    FILE* fp = fopen(rss_path, "w");
    if (fp) {
        fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
        fprintf(fp, "<rss version=\"2.0\">\n");
        fprintf(fp, "<channel>\n");
        fprintf(fp, "    <title>%s</title>\n", ctx->config->blog_title);
        fprintf(fp, "    <link>%s</link>\n", ctx->config->base_url);
        fprintf(fp, "    <description>%s</description>\n", ctx->config->blog_description);
        fprintf(fp, "    <language>en-us</language>\n");
        fprintf(fp, "    <pubDate>%s</pubDate>\n", "Mon, 01 Jan 2024 00:00:00 GMT");
        fprintf(fp, "</channel>\n");
        fprintf(fp, "</rss>\n");
        fclose(fp);
    }
    
    free(rss_path);
}

void generate_sitemap(GeneratorContext* ctx) {
    char* sitemap_path = join_path(ctx->output_dir, "sitemap.xml");
    if (!sitemap_path) return;
    
    FILE* fp = fopen(sitemap_path, "w");
    if (fp) {
        fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(fp, "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");
        fprintf(fp, "</urlset>\n");
        fclose(fp);
    }
    
    free(sitemap_path);
}

// ����ҳ�͹鵵ҳ����
void generate_index_page(GeneratorContext* ctx) {
    if (!ctx || !ctx->config) return;
    
    char* index_path = join_path(ctx->output_dir, "index.html");
    if (!index_path) return;
    
    FILE* fp = fopen(index_path, "w");
    if (fp) {
        fprintf(fp, "<!DOCTYPE html>\n");
        fprintf(fp, "<html>\n<head>\n");
        fprintf(fp, "<title>%s</title>\n", ctx->config->blog_title);
        fprintf(fp, "</head>\n<body>\n");
        fprintf(fp, "<h1>%s</h1>\n", ctx->config->blog_title);
        fprintf(fp, "</body>\n</html>\n");
        fclose(fp);
    }
    
    free(index_path);
}

void generate_tag_pages(GeneratorContext* ctx) {
    char* tags_dir = join_path(ctx->output_dir, "tags");
    if (!tags_dir) return;
    
    MKDIR(tags_dir);
    free(tags_dir);
}

void generate_archive_page(GeneratorContext* ctx) {
    char* archive_path = join_path(ctx->output_dir, "archive.html");
    if (!archive_path) return;
    
    FILE* fp = fopen(archive_path, "w");
    if (fp) {
        fprintf(fp, "<html><body><h1>Archives</h1></body></html>\n");
        fclose(fp);
    }
    
    free(archive_path);
}

// 模板处理函数
char* apply_template(const char* template_content, const char* content, const PostMetadata* metadata) {
    if (!template_content || !content || !metadata) return NULL;
    
    // 预估需要的缓冲区大小
    size_t buffer_size = strlen(template_content) + strlen(content) * 2;
    if (metadata->title) buffer_size += strlen(metadata->title);
    if (metadata->date) buffer_size += strlen(metadata->date);
    if (metadata->author) buffer_size += strlen(metadata->author);
    if (metadata->description) buffer_size += strlen(metadata->description);
    
    char* result = malloc(buffer_size);
    if (!result) return NULL;
    
    // 简单的模板替换
    char* current = result;
    const char* template_ptr = template_content;
    
    while (*template_ptr) {
        if (strncmp(template_ptr, "{{content}}", 10) == 0) {
            strcpy(current, content);
            current += strlen(content);
            template_ptr += 10;
        } else if (strncmp(template_ptr, "{{title}}", 8) == 0 && metadata->title) {
            strcpy(current, metadata->title);
            current += strlen(metadata->title);
            template_ptr += 8;
        } else if (strncmp(template_ptr, "{{date}}", 8) == 0 && metadata->date) {
            strcpy(current, metadata->date);
            current += strlen(metadata->date);
            template_ptr += 8;
        } else if (strncmp(template_ptr, "{{author}}", 10) == 0 && metadata->author) {
            strcpy(current, metadata->author);
            current += strlen(metadata->author);
            template_ptr += 10;
        } else if (strncmp(template_ptr, "{{description}}", 14) == 0 && metadata->description) {
            strcpy(current, metadata->description);
            current += strlen(metadata->description);
            template_ptr += 14;
        } else {
            *current++ = *template_ptr++;
        }
    }
    *current = '\0';
    
    return result;
}
