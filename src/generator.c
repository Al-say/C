#include "../include/generator.h"
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
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

// 函数声明
static int process_single_post(GeneratorContext* ctx, const char* post_path);
static char* read_utf8_file(const char* path, size_t* size_out);

// ļǷ
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
    
    ctx->template_dir = NULL;
    ctx->start_time = time(NULL);
    ctx->last_error = GEN_SUCCESS;
    
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
    
    // Initialize default values
    metadata->title = NULL;
    metadata->date = NULL;
    metadata->author = NULL;
    metadata->description = NULL;
    metadata->tags = NULL;
    metadata->tag_count = 0;
    metadata->permalink = NULL;
    
    // Check for YAML front matter
    if (strncmp(markdown_content, "---\n", 4) != 0) {
        return metadata;
    }
    
    const char* ptr = markdown_content + 4;
    char line[1024];
    int in_metadata = 1;
    
    while (*ptr && in_metadata) {
        // Read line
        const char* eol = strchr(ptr, '\n');
        if (!eol) break;
        
        size_t line_len = eol - ptr;
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        strncpy(line, ptr, line_len);
        line[line_len] = '\0';
        
        // Move to next line
        ptr = eol + 1;
        
        // Check for end of front matter
        if (strcmp(line, "---") == 0) {
            in_metadata = 0;
            break;
        }
        
        // Parse key-value pair
        char* key = line;
        char* value = strchr(line, ':');
        if (!value) continue;
        
        *value = '\0';  // Split key and value
        value++;
        
        // Trim whitespace from key
        while (isspace(*key)) key++;
        char* key_end = key + strlen(key) - 1;
        while (key_end > key && isspace(*key_end)) {
            *key_end = '\0';
            key_end--;
        }
        
        // Trim whitespace from value
        while (isspace(*value)) value++;
        char* value_end = value + strlen(value) - 1;
        while (value_end > value && isspace(*value_end)) {
            *value_end = '\0';
            value_end--;
        }
        
        // Store metadata
        char* str_value = strdup(value);
        if (!str_value) continue;
        
        if (strcmp(key, "title") == 0) {
            metadata->title = str_value;
        } else if (strcmp(key, "date") == 0) {
            metadata->date = str_value;
        } else if (strcmp(key, "author") == 0) {
            metadata->author = str_value;
        } else if (strcmp(key, "description") == 0) {
            metadata->description = str_value;
        } else {
            free(str_value);  // Not a recognized field
        }
    }
    
    // Generate permalink if title exists
    if (metadata->title && metadata->date) {
        metadata->permalink = generate_permalink(metadata->title, metadata->date);
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
int process_posts(GeneratorContext* ctx, const char* posts_dir) {
    if (!ctx || !posts_dir) {
        if (ctx) ctx->last_error = GEN_ERROR_MEMORY;
        printf("Error: Invalid context or posts directory\n");
        return 0;
    }
    
    printf("Processing posts from directory: %s\n", posts_dir);
    
    int success = 1;
    int retry_count = 0;
    
    while (retry_count < ctx->config->retry_count) {
        if (has_operation_timeout(ctx)) {
            ctx->last_error = GEN_ERROR_TIMEOUT;
            printf("Error: Operation timed out\n");
            return 0;
        }
        
#ifdef _WIN32
        WIN32_FIND_DATA find_data;
        char search_path[MAX_PATH];
        snprintf(search_path, sizeof(search_path), "%s\\*.md", posts_dir);
        
        printf("Searching for markdown files in: %s\n", search_path);
        
        HANDLE find_handle = FindFirstFile(search_path, &find_data);
        if (find_handle == INVALID_HANDLE_VALUE) {
            ctx->last_error = GEN_ERROR_IO;
            printf("Error: Could not find any markdown files (error code: %lu)\n", GetLastError());
            return 0;
        }
        
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char* post_path = join_path(posts_dir, find_data.cFileName);
                if (post_path && is_markdown_file(post_path)) {
                    printf("Processing file: %s\n", post_path);
                    if (!process_single_post(ctx, post_path)) {
                        printf("Error: Failed to process post: %s\n", post_path);
                        success = 0;
                        break;
                    }
                }
                free(post_path);
            }
        } while (FindNextFile(find_handle, &find_data));
        
        FindClose(find_handle);
#else
        DIR* dir = opendir(posts_dir);
        if (!dir) {
            ctx->last_error = GEN_ERROR_IO;
            printf("Error: Could not open posts directory\n");
            return 0;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (has_operation_timeout(ctx)) {
                closedir(dir);
                ctx->last_error = GEN_ERROR_TIMEOUT;
                printf("Error: Operation timed out while processing directory\n");
                return 0;
            }
            
            char* post_path = join_path(posts_dir, entry->d_name);
            if (post_path && is_markdown_file(post_path)) {
                printf("Processing file: %s\n", post_path);
                if (!process_single_post(ctx, post_path)) {
                    printf("Error: Failed to process post: %s\n", post_path);
                    success = 0;
                    break;
                }
            }
            free(post_path);
        }
        closedir(dir);
#endif
        
        if (success) break;
        
        printf("Retrying after failure (attempt %d)...\n", retry_count + 1);
        sleep_with_backoff(++retry_count, ctx->config->retry_delay);
    }
    
    return success;
}

// 生成文章页面
int generate_post_page(GeneratorContext* ctx, const char* markdown_content, PostMetadata* metadata) {
    if (!ctx || !markdown_content || !metadata) {
        printf("Error: Invalid parameters for generate_post_page\n");
        if (ctx) ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    printf("Creating parser context...\n");
    ParserContext* parser_ctx = create_parser_context(NULL);
    if (!parser_ctx) {
        printf("Error: Could not create parser context\n");
        ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    int success = 0;
    
    // Skip YAML front matter
    const char* content_start = markdown_content;
    if (strncmp(content_start, "---\n", 4) == 0) {
        content_start += 4;
        while (*content_start) {
            const char* eol = strchr(content_start, '\n');
            if (!eol) break;
            
            size_t line_len = eol - content_start;
            if (line_len == 3 && strncmp(content_start, "---", 3) == 0) {
                content_start = eol + 1;
                while (*content_start == '\n') content_start++;  // Skip extra newlines
                break;
            }
            content_start = eol + 1;
        }
    }
    
    printf("Parsing markdown content...\n");
    if (parse_markdown_with_context(parser_ctx, content_start)) {
        printf("Getting HTML output...\n");
        char* html_content = get_html_output(parser_ctx);
        
        if (html_content) {
            printf("HTML content generated successfully\n");
            
            // Generate output filename
            char output_name[256];
            if (metadata->title) {
                char* clean_title = strdup(metadata->title);
                if (clean_title) {
                    // Replace spaces with hyphens
                    for (char* p = clean_title; *p; p++) {
                        if (isspace(*p)) *p = '-';
                    }
                    snprintf(output_name, sizeof(output_name), "%s.html", clean_title);
                    free(clean_title);
                } else {
                    strcpy(output_name, "post.html");
                }
            } else {
                strcpy(output_name, "post.html");
            }
            
            printf("Output filename: %s\n", output_name);
            char* output_path = join_path(ctx->output_dir, output_name);
            
            if (output_path) {
                printf("Opening output file: %s\n", output_path);
                FILE* fp = fopen(output_path, "w");
                if (fp) {
                    printf("Reading template file...\n");
                    char* template_path = join_path("templates", "post.html");
                    if (template_path) {
                        size_t template_size;
                        char* template_content = read_utf8_file(template_path, &template_size);
                        if (template_content) {
                            printf("Applying template...\n");
                            char* page = apply_template(template_content, html_content, metadata);
                            if (page) {
                                printf("Writing output file...\n");
                                if (fputs(page, fp) >= 0) {
                                    success = 1;
                                    printf("Post page generated successfully\n");
                                } else {
                                    printf("Error: Could not write to output file\n");
                                }
                                free(page);
                            } else {
                                printf("Error: Could not apply template\n");
                            }
                            free(template_content);
                        } else {
                            printf("Error: Could not read template file\n");
                        }
                        free(template_path);
                    } else {
                        printf("Error: Could not create template path\n");
                    }
                    fclose(fp);
                } else {
                    printf("Error: Could not create output file\n");
                }
                free(output_path);
            } else {
                printf("Error: Could not create output path\n");
            }
            free(html_content);
        } else {
            printf("Error: Could not generate HTML content\n");
        }
    } else {
        printf("Error: Could not parse markdown content\n");
    }
    
    destroy_parser_context(parser_ctx);
    return success;
}

// 生成索引页面
int generate_index_page(GeneratorContext* ctx) {
    if (!ctx || !ctx->config) {
        if (ctx) ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    char* index_path = join_path(ctx->output_dir, "index.html");
    if (!index_path) {
        ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    FILE* fp = fopen(index_path, "w");
    if (!fp) {
        free(index_path);
        ctx->last_error = GEN_ERROR_IO;
        return 0;
    }
    
    fprintf(fp, "<!DOCTYPE html>\n");
    fprintf(fp, "<html>\n<head>\n");
    fprintf(fp, "<title>%s</title>\n", ctx->config->blog_title);
    fprintf(fp, "</head>\n<body>\n");
    fprintf(fp, "<h1>%s</h1>\n", ctx->config->blog_title);
    fprintf(fp, "</body>\n</html>\n");
    
    fclose(fp);
    free(index_path);
    return 1;
}

// 生成标签页面
int generate_tag_pages(GeneratorContext* ctx) {
    if (!ctx) return 0;
    
    char* tags_dir = join_path(ctx->output_dir, "tags");
    if (!tags_dir) {
        ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    int result = MKDIR(tags_dir) == 0 || errno == EEXIST;
    free(tags_dir);
    return result;
}

// 生成归档页面
int generate_archive_page(GeneratorContext* ctx) {
    if (!ctx) return 0;
    
    char* archive_path = join_path(ctx->output_dir, "archive.html");
    if (!archive_path) {
        ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    FILE* fp = fopen(archive_path, "w");
    if (!fp) {
        free(archive_path);
        ctx->last_error = GEN_ERROR_IO;
        return 0;
    }
    
    fprintf(fp, "<html><body><h1>Archives</h1></body></html>\n");
    fclose(fp);
    
    free(archive_path);
    return 1;
}

// 生成RSS订阅
int generate_rss_feed(GeneratorContext* ctx) {
    if (!ctx || !ctx->config) return 0;
    
    char* rss_path = join_path(ctx->output_dir, "feed.xml");
    if (!rss_path) {
        ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    FILE* fp = fopen(rss_path, "w");
    if (!fp) {
        free(rss_path);
        ctx->last_error = GEN_ERROR_IO;
        return 0;
    }
    
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
    free(rss_path);
    return 1;
}

// 生成站点地图
int generate_sitemap(GeneratorContext* ctx) {
    if (!ctx) return 0;
    
    char* sitemap_path = join_path(ctx->output_dir, "sitemap.xml");
    if (!sitemap_path) {
        ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    FILE* fp = fopen(sitemap_path, "w");
    if (!fp) {
        free(sitemap_path);
        ctx->last_error = GEN_ERROR_IO;
        return 0;
    }
    
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");
    fprintf(fp, "</urlset>\n");
    
    fclose(fp);
    free(sitemap_path);
    return 1;
}

// 模板处理函数
char* apply_template(const char* template_content, const char* content, const PostMetadata* metadata) {
    if (!template_content || !content || !metadata) return NULL;
    
    // 预估需要的缓冲区大小
    size_t buffer_size = strlen(template_content) + strlen(content) * 2;
    if (metadata->title) buffer_size += strlen(metadata->title) * 2;
    if (metadata->date) buffer_size += strlen(metadata->date) * 2;
    if (metadata->author) buffer_size += strlen(metadata->author) * 2;
    if (metadata->description) buffer_size += strlen(metadata->description) * 2;
    
    char* result = malloc(buffer_size);
    if (!result) return NULL;
    
    // 简单的模板替换
    char* current = result;
    const char* template_ptr = template_content;
    
    while (*template_ptr) {
        if (strncmp(template_ptr, "{{content}}", 10) == 0) {
            if (content) {
                strcpy(current, content);
                current += strlen(content);
            }
            template_ptr += 10;
        } else if (strncmp(template_ptr, "{{title}}", 8) == 0) {
            if (metadata->title) {
                strcpy(current, metadata->title);
                current += strlen(metadata->title);
            }
            template_ptr += 8;
        } else if (strncmp(template_ptr, "{{date}}", 8) == 0) {
            if (metadata->date) {
                strcpy(current, metadata->date);
                current += strlen(metadata->date);
            }
            template_ptr += 8;
        } else if (strncmp(template_ptr, "{{author}}", 10) == 0) {
            if (metadata->author) {
                strcpy(current, metadata->author);
                current += strlen(metadata->author);
            }
            template_ptr += 10;
        } else if (strncmp(template_ptr, "{{description}}", 14) == 0) {
            if (metadata->description) {
                strcpy(current, metadata->description);
                current += strlen(metadata->description);
            }
            template_ptr += 14;
        } else if (strncmp(template_ptr, "}}", 2) == 0) {
            // Skip closing brackets
            template_ptr += 2;
        } else {
            *current++ = *template_ptr++;
        }
    }
    *current = '\0';
    
    return result;
}

// 错误处理函数
GeneratorError get_last_error(GeneratorContext* ctx) {
    return ctx ? ctx->last_error : GEN_ERROR_MEMORY;
}

const char* get_error_message(GeneratorError error) {
    switch (error) {
        case GEN_SUCCESS:
            return "Success";
        case GEN_ERROR_TIMEOUT:
            return "Operation timed out";
        case GEN_ERROR_MEMORY:
            return "Memory allocation failed";
        case GEN_ERROR_IO:
            return "I/O operation failed";
        case GEN_ERROR_NETWORK:
            return "Network operation failed";
        default:
            return "Unknown error";
    }
}

// 超时检查函数
int has_operation_timeout(GeneratorContext* ctx) {
    if (!ctx || ctx->config->timeout_seconds <= 0) return 0;
    return (time(NULL) - ctx->start_time) > ctx->config->timeout_seconds;
}

// 重试延迟函数
void sleep_with_backoff(int retry_count, int base_delay) {
    int delay = base_delay * (1 << (retry_count - 1));
#ifdef _WIN32
    Sleep(delay * 1000);
#else
    sleep(delay);
#endif
}

// 增量构建检查
int needs_rebuild(const char* source_file, const char* target_file) {
    struct stat source_stat, target_stat;
    
    if (stat(source_file, &source_stat) != 0) {
        return 1;  // 源文件不存在，需要构建
    }
    
    if (stat(target_file, &target_stat) != 0) {
        return 1;  // 目标文件不存在，需要构建
    }
    
    return source_stat.st_mtime > target_stat.st_mtime;
}

// 文件压缩函数
int compress_file(const char* input_path, const char* output_path) {
    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "powershell Compress-Archive -Path \"%s\" -DestinationPath \"%s.zip\"",
             input_path, output_path);
#else
    snprintf(cmd, sizeof(cmd), "gzip -9 -c \"%s\" > \"%s.gz\"", input_path, output_path);
#endif
    return system(cmd) == 0;
}

// 读取UTF-8文件
static char* read_utf8_file(const char* path, size_t* size_out) {
    if (!path || !size_out) return NULL;
    
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        printf("Error: Could not open file %s\n", path);
        return NULL;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    
    // Allocate memory
    char* content = malloc(size + 1);
    if (!content) {
        printf("Error: Could not allocate memory for file content\n");
        fclose(fp);
        return NULL;
    }
    
    // Read file content
    size_t read_size = fread(content, 1, size, fp);
    fclose(fp);
    
    if (read_size != (size_t)size) {
        printf("Error: Could not read entire file (read %zu of %ld bytes)\n", read_size, size);
        free(content);
        return NULL;
    }
    
    // Remove UTF-8 BOM if present
    if (size >= 3 && 
        (unsigned char)content[0] == 0xEF &&
        (unsigned char)content[1] == 0xBB &&
        (unsigned char)content[2] == 0xBF) {
        memmove(content, content + 3, size - 3);
        size -= 3;
    }
    
    // Ensure null termination
    content[size] = '\0';
    *size_out = size;
    
    return content;
}

// 处理单个文章
static int process_single_post(GeneratorContext* ctx, const char* post_path) {
    if (!ctx || !post_path) {
        printf("Error: Invalid context or post path\n");
        if (ctx) ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    printf("Opening file: %s\n", post_path);
    
    size_t content_size;
    char* content = read_utf8_file(post_path, &content_size);
    if (!content) {
        printf("Error: Could not read file (errno: %d)\n", errno);
        ctx->last_error = GEN_ERROR_IO;
        return 0;
    }
    
    printf("File size: %zu bytes\n", content_size);
    printf("File content preview: %.100s...\n", content);
    
    printf("Extracting metadata...\n");
    PostMetadata* metadata = extract_post_metadata(content);
    if (!metadata) {
        printf("Error: Could not extract metadata from file\n");
        free(content);
        ctx->last_error = GEN_ERROR_MEMORY;
        return 0;
    }
    
    printf("Generating post page...\n");
    int result = generate_post_page(ctx, content, metadata);
    if (!result) {
        printf("Error: Failed to generate post page\n");
    }
    
    free(metadata);
    free(content);
    
    return result;
}
