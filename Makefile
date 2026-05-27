# Makefile for PA4 - Analog Floorplan

# 編譯器與旗標設定
CXX = g++
# 使用 -O3 進行效能優化，並開啟 -Wall 顯示所有警告
CXXFLAGS = -std=c++11 -O3
# 連結時需要數學函式庫
LDFLAGS = -lm

# 執行檔名稱
TARGET = 113522090_PA4

# 原始碼檔案
# 假設您的主程式檔案是 113522090_PA4.cpp
# 如果您有其他 .cpp 檔案放在 src/ 目錄下，可以像這樣加入
# SRCS = src/file1.cpp src/file2.cpp 113522090_PA4.cpp
SRCS = 113522090_PA4.cpp

# 將 .cpp 原始碼檔案對應到 .o 物件檔案
OBJS = $(SRCS:.cpp=.o)

# 預設目標：make all
all: $(TARGET)

# 連結目標：將所有 .o 物件檔案連結成最終的執行檔
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "編譯完成，執行檔為: $(TARGET)"

# 編譯目標：將每個 .cpp 原始碼檔案編譯成 .o 物件檔案
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# 執行目標：允許透過命令列傳遞 input 和 output 參數
# 用法: make run input=case1.block output=out1.output
run: all
	@echo "執行程式..."
	./$(TARGET) $(input) $(output)

# 清理目標：移除所有編譯產生的檔案和輸出結果
clean:
	@echo "清理專案目錄..."
	rm -f $(TARGET) $(OBJS) *.o *.output *.txt
	@echo "清理完成。"

# 偽目標：告訴 make，all, run, clean 不是真正的檔案名稱
.PHONY: all run clean
