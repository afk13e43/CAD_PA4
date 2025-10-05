import os
from graphviz import Source

def visualize_from_dot(input_filename="best_tree_structure.txt", output_filename="b_star_tree_graph"):
    """
    直接從 Graphviz DOT 格式的文字檔產生視覺化圖表。
    """
    try:
        # 讀取整個 DOT 檔案
        with open(input_filename, 'r', encoding='utf-8') as f:
            dot_source = f.read()
        
        # 使用 graphviz.Source 來解析 DOT 語言並建立圖
        s = Source(dot_source, filename=output_filename, format="png")
        
        # 產生圖檔，不再嘗試自動開啟
        s.render(cleanup=True, view=False)
        
        print(f"成功！B*-Tree 圖表已儲存為 '{output_filename}.png'")
        print("請手動打開該檔案檢視。")

    except FileNotFoundError:
        print(f"錯誤: 找不到輸入檔案 '{input_filename}'。")
    except Exception as e:
        print("\n--- 錯誤 ---")
        print("無法產生圖檔。請確認您已正確安裝 Graphviz 系統軟體。")
        print(f"詳細錯誤訊息: {e}")

if __name__ == '__main__':
    visualize_from_dot()