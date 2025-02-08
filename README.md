# C语言静态博客生成器

一个使用纯C语言实现的静态博客生成器，支持Markdown解析、模板渲染、自动部署等功能。

## 特性

- 纯C语言实现，无外部依赖
- Markdown转HTML支持
- 内存池优化
- 跨平台支持（Windows/Linux/macOS）
- 自动部署到GitHub Pages

## 构建

```bash
# 克隆仓库
git clone https://github.com/yourusername/c-static-blog.git
cd c-static-blog

# 编译
make

# 调试构建
make debug

# 运行测试
make test
```

## 使用方法

1. 创建新文章：
```markdown
---
title: 文章标题
date: YYYY-MM-DD
author: 作者名
description: 文章描述
tags: [标签1, 标签2]
---

文章内容...
```

2. 生成网站：
```bash
./blog-generator _site
```

3. 本地预览：
```bash
cd _site
python3 -m http.server 8000
```

## 开发

### 目录结构

```
my-c-blog/
├── src/           # 源代码
├── include/       # 头文件
├── posts/         # 文章
├── templates/     # HTML模板
└── .github/       # GitHub Actions
```

### 构建目标

- `make` - 构建正常版本
- `make debug` - 构建调试版本
- `make clean` - 清理构建文件
- `make test` - 运行测试
- `make help` - 显示帮助信息

## 许可证

MIT License

## 作者

Alsay
