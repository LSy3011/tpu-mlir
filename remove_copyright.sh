#!/bin/bash
# 严格匹配目标注释（支持行首/尾空格）
TARGET="// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved."

# 遍历所有代码文件（根据仓库语言增减后缀）
find . -type f \( \
  -name "*.h" -o -name "*.c" -o -name "*.cpp" -o -name "*.cc" \
  -o -name "*.py" -o -name "*.java" -o -name "*.sh" -o -name "*.go" \
\) -and ! -path "*/.git/*" | while read -r file; do
  # 备份并删除匹配行（--in-place 直接修改文件）
  sed -i.bak "/^[[:space:]]*$TARGET[[:space:]]*$/d" "$file"
  # 仅保留有变更的文件（减少无效备份）
  if ! diff -q "$file" "$file.bak" >/dev/null; then
    echo "修改: $file"
  else
    rm "$file.bak"  # 无变更则删除备份
  fi
done

echo -e "\n操作完成！共$(git diff --name-only | wc -l)个文件变更，备份文件已自动清理无效项"
