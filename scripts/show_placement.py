import matplotlib.pyplot as plt
import matplotlib.patches as patches
import re
import argparse
import random

def visualize_floorplan(input_filepath, output_filepath):
    """
    讀取 floorplan 的 output 檔案並產生可視化圖片。

    :param input_filepath: 輸入的 layout.txt 檔案路徑
    :param output_filepath: 輸出的圖片檔案路徑 (例如: layout.png)
    """
    modules = []
    chip_width = 0
    chip_height = 0
    chip_area = 0

    # 使用正規表示法來解析每一行模組的資料
    # 格式: NAME X Y (WIDTH HEIGHT ...)
    module_pattern = re.compile(
        r"(\S+)\s+([\d\.]+)\s+([\d\.]+)\s+\(([\d\.]+)\s+([\d\.]+).*\)"
    )

    try:
        with open(input_filepath, 'r') as f:
            lines = f.readlines()

            # 解析前三行的 meta data
            chip_area = float(lines[0].strip())
            chip_width, chip_height = map(float, lines[1].strip().split())
            
            # 解析後續的模組資料
            for line in lines[3:]:
                match = module_pattern.match(line.strip())
                if match:
                    name, x, y, w, h = match.groups()
                    modules.append({
                        'name': name,
                        'x': float(x),
                        'y': float(y),
                        'w': float(w),
                        'h': float(h)
                    })
    except FileNotFoundError:
        print(f"錯誤: 找不到檔案 '{input_filepath}'")
        return
    except (IOError, IndexError, ValueError) as e:
        print(f"錯誤: 讀取或解析檔案時發生錯誤: {e}")
        return

    # --- 開始繪圖 ---
    
    # 建立一個圖形和一個座標軸
    fig, ax = plt.subplots(1, figsize=(10, 10 * (chip_height / chip_width) if chip_width > 0 else 10))

    # 設定座標軸的範圍，並加上 5% 的邊界讓圖更好看
    ax.set_xlim(0, chip_width * 1.05)
    ax.set_ylim(0, chip_height * 1.05)
    
    # 確保 x 和 y 軸的比例是 1:1，這樣矩形才不會變形
    ax.set_aspect('equal', adjustable='box')

    # 產生一組隨機顏色供不同模組使用
    colors = [f"#{random.randint(0, 0xFFFFFF):06x}" for _ in range(len(modules))]

    # 遍歷所有模組並繪製
    for i, module in enumerate(modules):
        # 建立矩形物件
        rect = patches.Rectangle(
            (module['x'], module['y']),  # 左下角座標
            module['w'],                 # 寬度
            module['h'],                 # 高度
            linewidth=1.2,
            edgecolor='black',
            facecolor=colors[i % len(colors)], # 從顏色列表中循環選取
            alpha=0.75 # 設定透明度
        )

        # 將矩形加入到座標軸
        ax.add_patch(rect)

        # 在矩形中央加上模組名稱
        # 根據矩形大小調整字體大小，避免文字太大或太小
        fontsize = max(6, min(12, int(min(module['w'], module['h']))))
        ax.text(
            module['x'] + module['w'] / 2,
            module['y'] + module['h'] / 2,
            module['name'],
            ha='center', # 水平置中
            va='center', # 垂直置中
            fontsize=fontsize,
            color='black'
        )

    # 設定圖形標題和座標軸標籤
    plt.title(f"Floorplan Visualization\nArea: {chip_area:.2f}, Size: {chip_width:.2f} x {chip_height:.2f}")
    plt.xlabel("X coordinate")
    plt.ylabel("Y coordinate")
    plt.grid(True, linestyle='--', alpha=0.6) # 加上格線

    # 儲存圖檔
    plt.savefig(output_filepath, dpi=300, bbox_inches='tight')
    print(f"可視化結果已成功儲存至: {output_filepath}")
    
    # 顯示圖形 (如果需要)
    # plt.show()


if __name__ == '__main__':
    # 使用 argparse 來處理命令列參數，讓腳本更具通用性
    parser = argparse.ArgumentParser(description="Visualize a floorplan layout from an output file.")
    parser.add_argument("input_file", help="Path to the input layout file (e.g., layout.txt)")
    parser.add_argument("-o", "--output", default="figures/floorplan_visualization.png", help="Path to save the output image file (default: figures/floorplan_visualization.png)")
    
    args = parser.parse_args()
    
    visualize_floorplan(args.input_file, args.output)