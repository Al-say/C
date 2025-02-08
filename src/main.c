#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#include "../include/generator.h"
#include "../include/parser.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <output_dir>\n", argv[0]);
        return 1;
    }
    
    const char* output_dir = argv[1];
    printf("Output directory: %s\n", output_dir);
    
    // 创建输出目录
    if (mkdir(output_dir, 0755) != 0) {
        if (errno != EEXIST) {
            printf("Error: Could not create output directory (errno: %d)\n", errno);
            return 1;
        }
        printf("Output directory already exists\n");
    } else {
        printf("Created output directory\n");
    }
    
    // 创建博客配置
    BlogConfig config = {
        .blog_title = "My C Blog",
        .blog_description = "A static blog generator written in C",
        .author = "Alsay",
        .base_url = "https://example.com",
        .theme = "default",
        
        // 性能优化配置
        .enable_compression = 1,
        .enable_incremental = 1,
        .parallel_workers = 4,
        .chunk_size = 4096,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay = 1
    };
    
    printf("Creating generator context...\n");
    
    // 创建生成器上下文
    GeneratorContext* ctx = create_generator_context(&config, output_dir);
    if (!ctx) {
        printf("Error: Could not create generator context\n");
        return 1;
    }
    
    printf("Creating directory structure...\n");
    
    // 创建目录结构
    create_directory_structure(output_dir);
    
    int success = 1;
    int retry_count = 0;
    
    while (retry_count < config.retry_count) {
        printf("Processing posts (attempt %d)...\n", retry_count + 1);
        
        // 处理文章
        if (!process_posts(ctx, "posts")) {
            GeneratorError error = get_last_error(ctx);
            printf("Error processing posts: %s\n", get_error_message(error));
            
            if (error == GEN_ERROR_TIMEOUT) {
                printf("Warning: Operation timed out, retrying...\n");
                sleep_with_backoff(++retry_count, config.retry_delay);
                continue;
            } else {
                printf("Error: %s\n", get_error_message(error));
                success = 0;
                break;
            }
        }
        
        printf("Generating pages...\n");
        
        // 生成其他页面
        if (!generate_index_page(ctx) ||
            !generate_tag_pages(ctx) ||
            !generate_archive_page(ctx) ||
            !generate_rss_feed(ctx) ||
            !generate_sitemap(ctx)) {
            
            GeneratorError error = get_last_error(ctx);
            printf("Warning: Failed to generate pages (%s), retrying...\n",
                   get_error_message(error));
            sleep_with_backoff(++retry_count, config.retry_delay);
            continue;
        }
        
        // 如果启用了压缩，压缩所有HTML和CSS文件
        if (config.enable_compression) {
            printf("Compressing static files...\n");
            compress_file("public/style.css", "public/style.css");
            // TODO: 压缩其他静态文件
        }
        
        printf("All operations completed successfully\n");
        break;  // 所有操作成功完成
    }
    
    // 清理资源
    printf("Cleaning up...\n");
    destroy_generator_context(ctx);
    
    if (!success || retry_count >= config.retry_count) {
        printf("Error: Failed to generate blog after %d retries\n", retry_count);
        return 1;
    }
    
    printf("Blog generation completed successfully!\n");
    return 0;
}
