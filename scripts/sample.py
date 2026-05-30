import pandas as pd
import matplotlib.pyplot as plt
import re

def parse_log_file(file_content):
    """
    解析模擬退火演算法的日誌檔案內容。

    Args:
        file_content (str): 包含日誌數據的整個字串。

    Returns:
        pandas.DataFrame: 包含 'Temp', 'Best Cost', 'probability_avg' 的 DataFrame。
    """
    cleaned_text = re.sub(r'\\', '', file_content)
    cleaned_text = cleaned_text.replace('\n', ' ')

    # 每個有效的紀錄都以 "Temp:" 開頭，以此為基準分割資料
    records = cleaned_text.split('Temp:')
    records = [rec.strip() for rec in records if rec.strip()]

    data = []
    # 使用正規表示法從每條紀錄中提取數值
    for rec in records:
        try:
            temp_match = re.search(r'^([\d\.]+)', rec)
            cost_match = re.search(r'Best Cost: ([\d\.]+)', rec)
            prob_match = re.search(r'probability_avg : ([\d\.]+)', rec)

            if temp_match and cost_match and prob_match:
                temp = float(temp_match.group(1))
                cost = float(cost_match.group(1))
                prob = float(prob_match.group(1))
                data.append([temp, cost, prob])
        except (AttributeError, ValueError) as e:
            print(f"無法解析此行，已跳過: {rec[:50]}... Error: {e}")
            continue
            
    # 建立 DataFrame
    df = pd.DataFrame(data, columns=['Temp', 'Best Cost', 'probability_avg'])
    return df

def generate_plots(df):
    """
    根據提供的 DataFrame 產生並儲存圖表。

    Args:
        df (pandas.DataFrame): 包含 'Temp', 'Best Cost', 'probability_avg' 的 DataFrame。
    """
    if df.empty:
        print("DataFrame 是空的，無法產生圖表。")
        return

    # --- 圖表一：溫度 vs. 成本 ---
    plt.style.use('seaborn-v0_8-whitegrid') # 使用較美觀的樣式
    fig1, ax1 = plt.subplots(figsize=(12, 7))

    ax1.plot(df['Temp'], df['Best Cost'], marker='o', linestyle='-', color='b', markersize=4, label='Best Cost')
    
    # 設定圖表標題與座標軸標籤
    ax1.set_title('Best Cost vs. Temperature', fontsize=16)
    ax1.set_xlabel('Temperature', fontsize=12)
    ax1.set_ylabel('Best Cost', fontsize=12)
    
    # 將 Y 軸設定為科學記號表示法
    ax1.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
    
    # X 軸（溫度）從高到低顯示，符合退火過程
    ax1.invert_xaxis()
    ax1.legend()
    
    # 儲存圖表
    plt.savefig('figures/temperature_vs_cost.png')
    print("圖表 'figures/temperature_vs_cost.png' 已儲存。")
    plt.close(fig1)


    # --- 圖表二：溫度 vs. 平均機率 (Moving Average) ---
    fig2, ax2 = plt.subplots(figsize=(12, 7))

    MA_WINDOW = 30
    prob_ma = df['probability_avg'].rolling(window=MA_WINDOW, min_periods=1).mean()
    ax2.plot(df['Temp'], prob_ma, linestyle='-', color='r', linewidth=2, label=f'Average Probability (MA window={MA_WINDOW})')

    # 設定圖表標題與座標軸標籤
    ax2.set_title(f'Average Probability vs. Temperature (Moving Avg, window={MA_WINDOW})', fontsize=16)
    ax2.set_xlabel('Temperature', fontsize=12)
    ax2.set_ylabel('Average Probability', fontsize=12)

    # X 軸（溫度）從高到低顯示
    ax2.invert_xaxis()
    ax2.legend()
    
    # 儲存圖表
    plt.savefig('figures/temperature_vs_probability.png')
    print("圖表 'figures/temperature_vs_probability.png' 已儲存。")
    plt.close(fig2)


if __name__ == '__main__':
    # 讀取 case.txt 檔案
    # 請確保 'case.txt' 檔案與此 python 檔在同一個資料夾中
    try:
        with open('cases/case.txt', 'r', encoding='utf-8') as f:
            file_content = f.read()
        
        # 解析檔案並產生圖表
        data_df = parse_log_file(file_content)
        generate_plots(data_df)
        print("\n所有圖表已成功產生。")

    except FileNotFoundError:
        print("錯誤：找不到 'case.txt' 檔案。請確認檔案名稱與路徑是否正確。")
    except Exception as e:
        print(f"處理過程中發生錯誤: {e}")