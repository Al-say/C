# C语言开发环境配置说明

## 编译器
- 使用 Apple Clang 作为默认编译器
- 版本：16.0.0
- 支持C17标准

## 编码配置
- 使用UTF-8编码
- 已在VSCode中配置相关设置
- 支持中文输入和显示

## VSCode配置
已配置以下VSCode设置：
- 默认使用UTF-8编码
- 禁用自动编码检测
- 启用格式化功能
- 显示空白字符
- 自动移除行尾空格

## 编译运行示例
```bash
# 编译C程序
gcc -o 程序名 源文件.c

# 运行程序
./程序名
```

## 测试结果
- UTF-8编码测试通过
- 中文显示正常
- 编译器工作正常

注：如果需要使用清华镜像源下载其他工具或库，可以使用以下地址：
- Homebrew镜像：https://mirrors.tuna.tsinghua.edu.cn/help/homebrew/
- LLVM镜像：https://mirrors.tuna.tsinghua.edu.cn/help/llvm-apt/
