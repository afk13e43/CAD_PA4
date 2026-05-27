#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <queue>
#include <set>
#include <cmath>
#include <limits>
#include <random> // HINT: 為了模擬退火中的隨機過程，需要這個標頭檔
#include <list>
#include <chrono>
using namespace std;
// 用於儲存單一形態的幾何資訊
// 格式: (width height col_multiple row_multiple)
struct Shape
{
    double width;
    double height;
    int col_multiple;
    int row_multiple;
};

// 用於儲存一個裝置 (模組) 的資訊
// 包含裝置名稱和一個或多個可能的形態
struct Module
{
    std::string name;
    int id;
    std::vector<Shape> possible_shapes;
};

class Placer
{
public:
    // --- 需要實作 (1): 建構子 ---
    Placer(const std::vector<Module> &modules, string outputfilename) : rng(std::random_device{}())
    {
        outfilename = outputfilename;
        start_time = std::chrono::high_resolution_clock::now();
        // 初始化 root 指標
        root = nullptr;
        Node *prev_node = nullptr;
        // 1. 遍歷傳入的 modules vector
        // 1. 建立所有節點物件，但不建立樹連接

        for (const auto &module : modules)
        {
            const Shape &initial_shape = module.possible_shapes[0];
            Node *new_node = new Node(module.name, module.id, initial_shape.width, initial_shape.height);
            new_node->module_info = &module;
            this->nodes.push_back(new_node);
        }
        // 2. 隨機建立 B*-Tree 的拓撲結構
        root = nodes[0]; // 第一個節點總是根

        // 從第二個節點開始，為每個節點隨機找一個父節點
        for (size_t i = 1; i < nodes.size(); ++i)
        {
            Node *new_node = nodes[i];
            bool placed = false;

            // 持續嘗試，直到為 new_node 找到一個家
            while (!placed)
            {
                // 從已經在樹中的節點 (0 到 i-1) 中隨機選一個當爸爸
                int parent_idx = rand() % i;
                Node *parent_node = nodes[parent_idx];

                // 隨機決定要放左邊還是右邊
                std::uniform_int_distribution<int> distrib(0, 1);
                int random_idx = distrib(rng);
                if (random_idx == 0)
                {
                    // 嘗試放在左邊
                    if (parent_node->left_child == nullptr)
                    {
                        parent_node->left_child = new_node;
                        new_node->parent = parent_node;
                        placed = true;
                    }
                    // 如果左邊被佔了，試試右邊
                    else if (parent_node->right_child == nullptr)
                    {
                        parent_node->right_child = new_node;
                        new_node->parent = parent_node;
                        placed = true;
                    }
                }
                else
                {
                    // 嘗試放在右邊
                    if (parent_node->right_child == nullptr)
                    {
                        parent_node->right_child = new_node;
                        new_node->parent = parent_node;
                        placed = true;
                    }
                    // 如果右邊被佔了，試試左邊
                    else if (parent_node->left_child == nullptr)
                    {
                        parent_node->left_child = new_node;
                        new_node->parent = parent_node;
                        placed = true;
                    }
                }
                // 如果選中的父節點左右都滿了，迴圈會繼續，重新選另一個父節點
            }
        }
        // 初始化最佳解的儲存空間 (可以先不做，等 SA 寫好再加)
        best_root = nullptr;
        this->w_area = 1.0;                                 // 預設值
        this->w_inl = 99;                                   // 預設值
        best_cost_ans = std::numeric_limits<double>::max(); // 初始最佳成本設為無限大
    }
    ~Placer()
    {
        // 釋放主要節點列表中的記憶體
        for (Node *node : nodes)
        {
            delete node;
        }

        // 釋放備份狀態中的節點記憶體
        for (Node *node : best_nodes_state)
        { // best_nodes_state 現在也需要釋放
            delete node;
        }
    }

    void solve()
    {
        packing();
        double current_cost = calculate_cost();
        double best_cost = current_cost;
        int Cost_nodown_times = 0;
        double last_check_best_cost = best_cost; // 記錄上一次檢查時的最佳成本
        int no_improvement_steps = 0;
        // 預先配置備份空間 (這部分正確)
        std::vector<Node *> backup_nodes_state;
        Node *backup_root = nullptr;
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            backup_nodes_state.push_back(new Node("", 0, 0.0, 0.0));
        }
        if (best_nodes_state.empty())
        {
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                best_nodes_state.push_back(new Node("", 0, 0.0, 0.0));
            }
        }

        // 儲存第一個初始解作為當前最佳解
        copy_data_only(nodes, root, best_nodes_state, best_root);
        // 另外備份一份用於熱身後還原
        copy_data_only(nodes, root, backup_nodes_state, backup_root);

        cout << "--- Starting Single-Pass Warm-up & Weight Calculation ---" << endl;
        const int N_SAMPLES = 5000; // 統一的取樣次數
        cout << "Total samples for warm-up: " << N_SAMPLES << endl;
        double total_area_cost = 0;
        double total_inl_cost = 0;

        // 1. 在同一個迴圈中，同時計時並累加兩個成本分量

        auto warmup_start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < N_SAMPLES; ++i)
        {
            perturb(i % 3);
            packing();

            // 一次性取得所有成本分量
            auto costs = get_cost_components();
            total_area_cost += costs.first;
            total_inl_cost += costs.second;
        }
        auto warmup_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> warmup_duration = warmup_end - warmup_start;

        // 2. 計算精確的 time_per_iter 和平均成本
        double time_per_iter = warmup_duration.count() / N_SAMPLES;
        double avg_AreaAR = total_area_cost / N_SAMPLES;
        double avg_INL = total_inl_cost / N_SAMPLES;

        cout << "Accurate time per iteration: " << time_per_iter * 1000 << " ms" << endl;
        cout << "Average Area/AR cost: " << avg_AreaAR << endl;
        cout << "Average INL cost: " << avg_INL << endl;
        if (this->w_inl == 99)
        {
            cout << "First round. " << endl;
            // 3. 計算權重，並存為成員變數
            this->w_area = 1.0;
            if (avg_INL > 1e-9)
            {
                this->w_inl = avg_AreaAR / avg_INL;
            }
            else
            {
                this->w_inl = 1.0; // Fallback
            }
        }
        // if (nodes.size() > 50)
        // {
        //     this->w_inl = 0;
        // }
        cout << "Calculated weights -> w_area: " << w_area << ", w_inl: " << w_inl << endl;

        // 4. 還原到初始狀態，準備進入 SA 主迴圈
        copy_data_only(best_nodes_state, best_root, nodes, root);
        packing();

        // --- 【新的】動態計算 SA 參數 (策略 B) ---
        const double TIME_BUDGET = 585.0;
        double total_affordable_iters = (time_per_iter > 0) ? (TIME_BUDGET / time_per_iter) : 1e9;

        // 1. 優先決定一個合理的 max_iter，確保每個溫層的探索品質
        //    這個值可以和問題規模掛鉤，但給一個下限，避免太小。
        int max_iter = std::max(200, static_cast<int>(nodes.size() * 10)); // 例如，至少100次，或是節點數的2倍

        // 2. 根據總預算，計算我們能負擔多少個溫度階梯
        int num_temp_steps = static_cast<int>(total_affordable_iters / (max_iter + 4));
        if (num_temp_steps < 1)
            num_temp_steps = 1; // 確保至少有 100 個階梯來降溫

        double T = 10000.0;
        double T_min = 0.1;

        // 3. 根據 T, T_min 和計算出的步數，反推出最合適的 cooling_rate
        double cooling_rate;
        if (num_temp_steps > 0)
        {
            cooling_rate = pow(T_min / T, 1.0 / num_temp_steps);
        }
        else
        {
            cooling_rate = 0.99; // 預防除以零的 fallback
        }

        cout << "Adaptive SA params (Strategy B): max_iter=" << max_iter
             << ", cooling_rate=" << fixed << setprecision(6) << cooling_rate
             << ", num_steps=" << num_temp_steps << endl;
        double endtimelimit = 0;
        if (time_per_iter < 1)
        {
            endtimelimit = 3;
        }
        else
        {
            endtimelimit = time_per_iter + 5;
        }
        // write_output(outfilename);
        // 3. 模擬退火主迴圈
        double probability_avg = 0;
        while (T > T_min)
        {
            probability_avg = 0;
            if (get_elapsed_time() > 585.0 - endtimelimit)
            {
                cout << "Time limit approaching. Finishing SA..." << endl;
                break;
            }
            sa_round_time = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < max_iter; ++i)
            {
                // cout << "Iteration: " << i << ", Current cost: " << current_cost << endl;
                // a. 儲存當前狀態 (以便退回)
                copy_data_only(nodes, root, backup_nodes_state, backup_root);

                // b. 擾動 (Perturb)，產生新解
                std::uniform_int_distribution<int> distrib(0, 2);
                int random_idx = distrib(rng);
                perturb(random_idx);

                // c. 計算新解的成本
                packing(); // 擾動後需要重新 packing
                double new_cost = calculate_cost();

                // d. 判斷是否接受新解
                double delta_cost = new_cost - current_cost;
                if (delta_cost < 0)
                { // 新解更好，直接接受
                    current_cost = new_cost;
                    if (new_cost < best_cost)
                    { // 更新全域最佳解
                        best_cost = new_cost;
                        copy_data_only(nodes, root, best_nodes_state, best_root);
                    }
                }
                else
                { // 新解比較差，以一定機率接受
                    double accept_prob = exp(-delta_cost / T);
                    probability_avg += accept_prob; // 累加接受機率
                    std::uniform_real_distribution<double> distrib_real(0.0, 1.0);
                    if (distrib_real(rng) < accept_prob)
                    {
                        current_cost = new_cost;
                    }
                    else
                    {
                        // 不接受，還原到擾動前的狀態
                        copy_data_only(backup_nodes_state, backup_root, nodes, root);
                    }
                }
            }
            // e. 降溫

            cout << "Temp: " << fixed << setprecision(2) << T
                 << ", Best Cost: " << fixed << setprecision(4) << best_cost
                 << ", Round Elapsed time: " << fixed << setprecision(2) << get_sa_elapsed_time() << " s, "
                 << "probability_avg : " << probability_avg / max_iter << endl;
            // --- 懲罰性降溫邏輯 ---
            if (T == 10000 && (probability_avg / max_iter) < 0.1)
            {
                cout << "INL make SA can't find better solution , disable" << endl;
                this->w_inl = 0;
                copy_data_only(backup_nodes_state, backup_root, nodes, root); // 還原到初始狀態
                continue;
            }
            if (best_cost < last_check_best_cost)
            {
                // 成本有改善！重置計數器，並更新檢查點
                no_improvement_steps = 0;
                last_check_best_cost = best_cost;
            }
            else
            {
                // 成本未改善，計數器加1
                no_improvement_steps++;
            }

            // 檢查是否達到觸發條件
            if (no_improvement_steps >= 20)
            {
                cout << "No improvement for 10 steps. Applying punishing cooldown." << endl;
                T *= (cooling_rate - 0.03); // 加速降溫
                no_improvement_steps = 0;   // 重置計數器，避免連續觸發
            }
            else
            {
                T *= cooling_rate;
            }
        }

        // 4. 迴圈結束，還原到找到的最佳解
        copy_data_only(best_nodes_state, best_root, nodes, root);
        packing(); // 確保最終輸出的是最佳解的 packing 結果
        cout << "SA finished. Best cost found: " << best_cost << ", inl_w: " << this->w_inl << endl;
        best_cost_ans = best_cost; // 儲存最佳成本答案
        cout << "Total execution time: " << get_elapsed_time() << " s" << endl;
        for (Node *node : backup_nodes_state)
        {
            delete node;
        }
    }
    double get_sa_elapsed_time() const
    {
        // 取得當前時間
        auto now = std::chrono::high_resolution_clock::now();
        // 計算時間差
        std::chrono::duration<double> elapsed = now - sa_round_time;
        // 將時間差轉換為秒數並回傳
        return elapsed.count();
    }
    double get_elapsed_time() const
    {
        // 取得當前時間
        auto now = std::chrono::high_resolution_clock::now();
        // 計算時間差
        std::chrono::duration<double> elapsed = now - start_time;
        // 將時間差轉換為秒數並回傳
        return elapsed.count();
    }
    int get_best_cost() const
    {
        // 返回最佳成本答案
        return static_cast<double>(best_cost_ans);
    }
    // --- 檔案輸出 ---
    void write_output(const std::string &filename)
    {
        // 1. 計算晶片尺寸
        double chip_width = this->current_chip_width;
        double chip_height = this->current_chip_height;
        double chip_area = chip_width * chip_height;

        // 2. 計算 INL
        double inl = calculate_inl();

        // 3. 開啟 ofstream
        ofstream out_file(filename);
        if (!out_file.is_open())
        {
            cerr << "錯誤: 無法開啟輸出檔案 " << filename << endl;
            return;
        }

        // 4. 寫入前三行 (面積、尺寸、INL)，這部分不變
        out_file << fixed << setprecision(4) << chip_area << endl;
        out_file << fixed << setprecision(2) << chip_width << " " << chip_height << endl;
        out_file << fixed << setprecision(2) << inl << endl;

        // 5. 為了輸出排序好的模組列表，建立一個 nodes 的副本
        std::vector<Node *> sorted_output_nodes = this->nodes;

        // 6. 使用與 INL 計算中完全相同的 "自然排序" 邏輯對副本進行排序
        sort(sorted_output_nodes.begin(), sorted_output_nodes.end(), [](const Node *a, const Node *b)
             {
        size_t first_digit_a = std::string::npos;
        for (size_t i = 0; i < a->name.length(); ++i) if (isdigit(a->name[i])) { first_digit_a = i; break; }

        size_t first_digit_b = std::string::npos;
        for (size_t i = 0; i < b->name.length(); ++i) if (isdigit(b->name[i])) { first_digit_b = i; break; }

        if (first_digit_a == std::string::npos || first_digit_b == std::string::npos) return a->name < b->name;
        
        std::string prefix_a = a->name.substr(0, first_digit_a);
        std::string prefix_b = b->name.substr(0, first_digit_b);

        if (prefix_a != prefix_b) return prefix_a < prefix_b;
        
        int num_a = std::stoi(a->name.substr(first_digit_a));
        int num_b = std::stoi(b->name.substr(first_digit_b));

        return num_a < num_b; });

        // 7. 遍歷排序後的副本 (sorted_output_nodes) 來進行輸出
        for (const auto &node : sorted_output_nodes)
        {
            const Shape &current_shape = node->module_info->possible_shapes[node->current_shape_idx];
            // 這裡的輸出精度控制邏輯保持不變
            out_file << node->name << " "
                     << fixed << setprecision(4) << node->x << " "
                     << fixed << setprecision(4) << node->y << " ("
                     << fixed << setprecision(2) << current_shape.width << " "
                     << fixed << setprecision(2) << current_shape.height << " "
                     << current_shape.col_multiple << " "
                     << current_shape.row_multiple << ")" << endl;
        }

        out_file.close();
    };
    void packing()
    {
        if (root == nullptr)
            return;

        // 1. 重置所有模組的座標
        for (auto &node : this->nodes)
        {
            node->x = 0.0;
            node->y = 0.0;
        }

        // 2. 每次 packing 前，都重置晶片尺寸和 Contour
        DoublyLinkedList contour;
        current_chip_width = 0.0;
        current_chip_height = 0.0;

        // 3. 從根節點開始進行 DFS Packing
        packing_dfs(root, contour);
    }
    void write_best_tree(const std::string &filename)
    {
        ofstream out_file(filename);
        if (!out_file.is_open())
        {
            cerr << "錯誤: 無法開啟樹結構輸出檔案 " << filename << endl;
            return;
        }

        // 寫入 Graphviz 的標頭
        out_file << "digraph BStarTree {" << endl;
        out_file << "    rankdir=TB;" << endl;
        out_file << "    node [shape=circle, style=filled, fillcolor=lightblue];" << endl;
        out_file << "    edge [fontsize=10];" << endl;

        // 標示出根節點
        if (this->root != nullptr)
        {
            out_file << "    \"" << this->root->name << "\" [fillcolor=salmon];" << endl;
            // 呼叫遞迴函式來產生所有的邊
            generate_edge_list_recursive(this->root, out_file);
        }
        else
        {
            out_file << "    label=\"Tree is empty.\";" << endl;
        }

        // 寫入結尾
        out_file << "}" << endl;

        out_file.close();
    }

private:
    struct Node
    {
        // 模組資訊
        std::string name;
        int id;
        double width, height;
        int current_shape_idx;
        const Module *module_info; // 新增一個指標，指向原始 Module

        // 座標
        double x, y;

        // B*-Tree 結構指標
        Node *parent;
        Node *left_child;
        Node *right_child;

        // 建構子
        Node(std::string n, int d, double w, double h) : name(n),
                                                         id(d),
                                                         width(w),
                                                         height(h),
                                                         current_shape_idx(0),
                                                         module_info(nullptr),
                                                         x(0), y(0),
                                                         parent(nullptr), left_child(nullptr), right_child(nullptr)
        {
        }
    };
    double w_area;
    double w_inl;
    Node *root;
    double best_cost_ans;
    std::vector<Node *> nodes;
    Node *best_root;
    std::vector<Node *> best_nodes_state;
    // 新增這兩個成員變數
    string outfilename;
    double current_chip_width;
    double current_chip_height;
    std::mt19937 rng; // 隨機數引擎
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point sa_round_time;
    // --- 實作 Contour 類別 (作為 Placer 的私有內部類別) ---
    struct ContourNode
    {
        // contour 邏輯核心欄位
        string module_id; // 模組 ID (如果需要的話)
        double x_start;   // 線段的起始 x 座標
        double x_end;
        double y; // 線段的高度
    };
    struct ContourDLLNode
    {
        ContourNode data;     // 存放輪廓線段的資料
        ContourDLLNode *prev; // 指向前一個節點
        ContourDLLNode *next; // 指向後一個節點

        // Constructor
        ContourDLLNode(const ContourNode &d) : data(d), prev(nullptr), next(nullptr) {}
    };
    class DoublyLinkedList
    {
    public:
        ContourDLLNode *head;
        ContourDLLNode *tail;

        DoublyLinkedList() : head(nullptr), tail(nullptr) {}

        // Destructor (記得釋放記憶體)
        ~DoublyLinkedList()
        {
            ContourDLLNode *current = head;
            while (current != nullptr)
            {
                ContourDLLNode *next = current->next;
                delete current;
                current = next;
            }
        }

        // 在指定節點之後插入新節點
        void insert_after(ContourDLLNode *node, const ContourNode &data)
        {
            ContourDLLNode *new_node = new ContourDLLNode(data);
            if (node == nullptr)
            { // 插入到開頭
                if (head == nullptr)
                { // 串列為空
                    head = tail = new_node;
                }
                else
                {
                    new_node->next = head;
                    head->prev = new_node;
                    head = new_node;
                }
            }
            else
            { // 在 node 後面插入
                new_node->next = node->next;
                new_node->prev = node;
                if (node->next != nullptr)
                {
                    node->next->prev = new_node;
                }
                else
                {
                    tail = new_node; // 更新 tail
                }
                node->next = new_node;
            }
        }

        // 移除一個節點
        void remove_node(ContourDLLNode *node)
        {
            if (node == nullptr)
                return;

            if (node->prev)
            {
                node->prev->next = node->next;
            }
            else
            { // 移除的是 head
                head = node->next;
            }

            if (node->next)
            {
                node->next->prev = node->prev;
            }
            else
            { // 移除的是 tail
                tail = node->prev;
            }

            delete node;
        }
        // 移除一個節點
        void replace_node(ContourDLLNode *node, const ContourNode &data)
        {
            if (node == nullptr)
            {
                return;
            }
            ContourDLLNode *new_node = new ContourDLLNode(data);

            new_node->next = node->next;
            new_node->prev = node->prev;
            if (node->prev != nullptr)
            {
                node->prev->next = new_node;
            }
            else
            {
                head = new_node; // 更新 head
            }
            if (node->next != nullptr)
            {
                node->next->prev = new_node;
            }
            else
            {
                tail = new_node; // 更新 tail
            }
            delete node;
        }

        bool is_empty() const
        {
            return head == nullptr;
        }
    };
    // --- Packing 演算法的私有遞迴核心 (DFS) ---
    void packing_dfs(Node *node, DoublyLinkedList &contour)
    {
        // Base Case: 如果節點為空，則返回
        if (node == nullptr)
        {
            return;
        }

        // --- 核心邏輯：處理當前節點 (Pre-order Traversal: Node -> Left -> Right) ---

        // 1. 根據父節點決定 X 座標
        if (node->parent == nullptr)
        { // Case: 根節點
            node->x = 0.0;
        }
        else if (node == node->parent->left_child)
        { // Case: 左子節點
            node->x = node->parent->x + node->parent->width;
        }
        else
        { // Case: 右子節點
            node->x = node->parent->x;
        }

        // 2. 根據輪廓線 (Contour) 和父節點決定 Y 座標
        //    (此處假設您已為 std::list<ContourNode> 實現了 get_max_height)
        double y_from_contour = get_max_height_from_contour(contour, node->x, node->x + node->width);

        if (node->parent != nullptr && node == node->parent->right_child)
        {
            // 右子節點必須在父節點的上方
            double y_from_parent = node->parent->y + node->parent->height;
            node->y = std::max(y_from_contour, y_from_parent);
        }
        else
        {
            node->y = y_from_contour;
        }

        // 3. 更新輪廓線和晶片總尺寸
        //    (此處假設您已為 std::list<ContourNode> 實現了 update_contour)
        double new_top_y = node->y + node->height;
        update_contour(contour, node->x, node->x + node->width, new_top_y);

        current_chip_width = std::max(current_chip_width, node->x + node->width);
        current_chip_height = std::max(current_chip_height, node->y + node->height);

        // --- 遞迴呼叫：先走訪左子樹，再走訪右子樹 ---
        packing_dfs(node->left_child, contour);
        packing_dfs(node->right_child, contour);
    }
    double get_max_height_from_contour(DoublyLinkedList &contour, double x_start, double x_end)
    {
        double max_y = 0;
        ContourDLLNode *current = contour.head;

        while (current != nullptr)
        {
            // 檢查是否有重疊
            if (current->data.x_end > x_start && current->data.x_start < x_end)
            {
                // 有重疊，更新最大高度
                max_y = max(max_y, current->data.y);
            }
            current = current->next;
        }
        return max_y;
    }

    void update_contour(DoublyLinkedList &contour, double new_x_start, double new_x_end, double new_y)
    {
        // 用於記錄新線段應該插入在哪個節點的後面
        ContourDLLNode *insert_after_node = nullptr;

        ContourDLLNode *current = contour.head;
        while (current != nullptr)
        {
            // 為了安全地刪除節點，預先保存下一個節點的指標
            ContourDLLNode *next_node = current->next;

            double old_x_start = current->data.x_start;
            double old_x_end = current->data.x_end;

            // --- 檢查重疊 ---
            // 如果沒有重疊，且當前節點在新節點的左側，更新 insert_after_node 並繼續
            if (old_x_end <= new_x_start)
            {
                insert_after_node = current;
                current = next_node;
                continue;
            }
            // 如果沒有重疊，且當前節點在新節點的右側，那麼可以提前結束迴圈
            if (old_x_start >= new_x_end)
            {
                break;
            }

            // --- 至此，代表線段有重疊 ---

            // 情況 1: 舊線段的左邊部分沒有被覆蓋，需要保留
            if (old_x_start < new_x_start)
            {
                current->data.x_end = new_x_start; // 縮短舊線段的長度
                insert_after_node = current;       // 新線段應該插在這個線段後面
            }

            // 情況 2: 舊線段的右邊部分沒有被覆蓋，需要保留
            if (old_x_end > new_x_end)
            {
                current->data.x_start = new_x_end; // 縮短舊線段的起點
            }

            // 情況 3: 舊線段被新線段完全覆蓋 (old_x_start >= new_x_start && old_x_end <= new_x_end)
            // 在這種情況下，上面的兩個if都不會執行，所以舊線段的範圍不變，我們需要刪除它。
            // 或者，舊線段的左邊部分被縮短後，右邊又沒超出，那剩下的部分就是被覆蓋的。
            if (old_x_start >= new_x_start && old_x_end <= new_x_end)
            {
                contour.remove_node(current); // 安全地移除節點
            }

            current = next_node;
        }

        // 迴圈結束後，在紀錄的位置插入代表新模組頂部的新線段
        ContourNode new_segment_data = {"", new_x_start, new_x_end, new_y};
        contour.insert_after(insert_after_node, new_segment_data);
    }
    // --- 需要實作 (5): INL 計算 ---
    double calculate_inl()
    {
        if (nodes.empty())
            return 0.0;

        // 步驟 1 & 2: 計算 Bounding Box 和質心
        double x_min = std::numeric_limits<double>::max();
        double x_max = std::numeric_limits<double>::min();
        double y_min = std::numeric_limits<double>::max();
        double y_max = std::numeric_limits<double>::min();

        for (const auto &node : nodes)
        {
            x_min = std::min(x_min, node->x);
            x_max = std::max(x_max, node->x + node->width);
            y_min = std::min(y_min, node->y);
            y_max = std::max(y_max, node->y + node->height);
        }
        double Xc = (x_min + x_max) / 2.0;
        double Yc = (y_min + y_max) / 2.0;

        // 步驟 3: 計算每個 Block 的平方歐幾里得距離
        std::map<std::string, double> squared_distances;
        for (const auto &node : nodes)
        {
            double block_center_x = node->x + node->width / 2.0;
            double block_center_y = node->y + node->height / 2.0;
            squared_distances[node->name] = pow(block_center_x - Xc, 2) + pow(block_center_y - Yc, 2);
        }

        // 步驟 4: 依名稱升序排序
        std::vector<Node *> sorted_nodes = nodes;
        sort(sorted_nodes.begin(), sorted_nodes.end(), [](const Node *a, const Node *b)
             {
            // 找到 a 和 b 名稱中第一個數字的位置
            // size_t first_digit_a = a->name.find_first_of("0123456789");
            // size_t first_digit_b = b->name.find_first_of("0123456789");

            // // 如果任一名稱中沒有數字，或解析失敗，則退回簡單的字串比較
            // if (first_digit_a == std::string::npos || first_digit_b == std::string::npos) {
            //     return a->name < b->name;
            // }

            // // 提取字母前綴
            // std::string prefix_a = a->name.substr(0, first_digit_a);
            // std::string prefix_b = b->name.substr(0, first_digit_b);

            // // 如果前綴不同，直接比較前綴
            // if (prefix_a != prefix_b) {
            //     return prefix_a < prefix_b;
            // }

            return a->id < b->id; });
        // 步驟 5: 累積平方距離
        std::vector<double> S_actual;
        double current_sum = 0.0;
        for (const auto &node : sorted_nodes)
        {
            current_sum += squared_distances[node->name];
            S_actual.push_back(current_sum);
        }

        // 步驟 6: 線性迴歸
        double n_sum = 0, s_sum = 0, ns_sum = 0, n2_sum = 0;
        int N = sorted_nodes.size();
        for (int i = 0; i < N; ++i)
        {
            double n = i + 1;
            double s = S_actual[i];
            n_sum += n;
            s_sum += s;
            ns_sum += n * s;
            n2_sum += n * n;
        }

        double a = (N * ns_sum - n_sum * s_sum) / (N * n2_sum - n_sum * n_sum);
        double b = (s_sum - a * n_sum) / N;

        // 步驟 7: 計算 INL
        double inl = 0.0;
        for (int i = 0; i < N; ++i)
        {
            double S_ideal = a * (i + 1) + b;
            inl = std::max(inl, std::abs(S_actual[i] - S_ideal));
        }

        return inl;
    };
    std::pair<double, double> get_cost_components()
    {
        // 1. 計算面積與長寬比成本
        double chip_width = this->current_chip_width;
        double chip_height = this->current_chip_height;

        if (chip_width == 0 || chip_height == 0)
        {
            // 回傳一個極大的成本，表示無效解
            return {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
        }

        double chip_area = chip_width * chip_height;
        double AR = chip_width / chip_height;
        double f_AR = 0.0;
        if (AR < 0.5)
            f_AR = 2 * (0.5 - AR);
        else if (AR > 2)
            f_AR = AR - 2;
        double cost_AreaAR = chip_area * (1.0 + f_AR);

        // 2. 計算 INL 成本
        double inl_val = calculate_inl();

        // 3. 將兩個成本分量打包回傳
        return {cost_AreaAR, inl_val};
    }
    // --- 需要實作 (6): 成本函數 ---
    double calculate_cost()
    {
        if (this->w_inl == 0)
        {
            double chip_width = this->current_chip_width;
            double chip_height = this->current_chip_height;

            double chip_area = chip_width * chip_height;
            double AR = chip_width / chip_height;
            double f_AR = 0.0;
            if (AR < 0.5)
                f_AR = 2 * (0.5 - AR);
            else if (AR > 2)
                f_AR = AR - 2;
            double cost_AreaAR = chip_area * (1.0 + f_AR);
            return cost_AreaAR;
        }
        else
        {
            auto components = get_cost_components();
            double cost_AreaAR = components.first;
            double inl_val = components.second;

            // 直接使用成員變數中的權重來計算總成本
            return this->w_area * cost_AreaAR + this->w_inl * inl_val;
        }
    }

    // --- 需要實作 (7): 擾動操作 ---
    void perturb(int type)
    {
        // HINT: 根據傳入的 type，呼叫下面三種操作之一。
        switch (type)
        {
        case 0:
            reshape_node();
            break;
        case 1:
            swap_nodes();
            break;
        case 2:
            delete_and_insert_node();
            break;
        }
    };

    void reshape_node()
    {
        std::uniform_int_distribution<int> distrib(0, nodes.size() - 1);
        int random_idx = distrib(rng);
        Node *nd = nodes[random_idx];
        const auto &shapes = nd->module_info->possible_shapes;
        if (shapes.size() <= 1)
            return;
        std::uniform_int_distribution<int> shape_distrib(0, shapes.size() - 1);
        int new_idx = shape_distrib(rng);
        if (new_idx == nd->current_shape_idx)
            new_idx = (nd->current_shape_idx + 1) % shapes.size();
        nd->current_shape_idx = new_idx;
        nd->width = shapes[new_idx].width;
        nd->height = shapes[new_idx].height;
    }

    void swap_nodes()
    {
        int n = nodes.size();
        if (n < 2)
            return; // 至少需要兩個節點才能交換
        std::uniform_int_distribution<int> distrib(0, n - 1);
        int i = distrib(rng);
        int j = distrib(rng);
        if (i == j)
            return;

        Node *A = nodes[i], *B = nodes[j];

        // 交換所有與模組身分相關的資訊
        std::swap(A->module_info, B->module_info);
        std::swap(A->name, B->name);
        std::swap(A->id, B->id);
        std::swap(A->current_shape_idx, B->current_shape_idx);

        // 重新獲取對應 shape 的寬高
        const auto &sa = A->module_info->possible_shapes[A->current_shape_idx];
        A->width = sa.width;
        A->height = sa.height;

        const auto &sb = B->module_info->possible_shapes[B->current_shape_idx];
        B->width = sb.width;
        B->height = sb.height;
    }
    // 檢查 node_to_check 是否為 ancestor 的子孫
    bool is_descendant(const Node *node_to_check, const Node *ancestor)
    {
        if (ancestor == nullptr || node_to_check == nullptr)
        {
            return false;
        }

        // 使用非遞迴的廣度優先搜尋 (BFS) 或深度優先搜尋 (DFS) 來避免堆疊溢位
        std::queue<const Node *> q;
        if (ancestor->left_child)
            q.push(ancestor->left_child);
        if (ancestor->right_child)
            q.push(ancestor->right_child);

        while (!q.empty())
        {
            const Node *current = q.front();
            q.pop();

            if (current == node_to_check)
            {
                return true; // 找到了，是子孫
            }

            if (current->left_child)
                q.push(current->left_child);
            if (current->right_child)
                q.push(current->right_child);
        }

        return false; // 遍歷完畢，不是子孫
    }
    void delete_and_insert_node()
    {
        if (nodes.size() < 2)
            return;

        // 1. 選一個非 root 節點 A (邏輯不變)
        Node *A = nullptr;
        while (true)
        {
            // 使用新的隨機數引擎以獲得更好的隨機性
            std::uniform_int_distribution<int> node_dist(0, nodes.size() - 1);
            Node *cand = nodes[node_dist(rng)];
            if (cand != root)
            {
                A = cand;
                break;
            }
        }

        // 2.  使用隨機抽樣尋找節點 B
        Node *B = nullptr;
        const int MAX_ATTEMPTS = nodes.size(); // 設定一個嘗試次數上限，例如節點總數

        for (int i = 0; i < MAX_ATTEMPTS; ++i)
        {
            std::uniform_int_distribution<int> node_dist(0, nodes.size() - 1);
            Node *candidate_B = nodes[node_dist(rng)];

            // 檢查候選者 B 是否合法
            if (candidate_B != A &&                                                            // B 不能是 A
                (candidate_B->left_child == nullptr || candidate_B->right_child == nullptr) && // B 必須有空位
                !is_descendant(candidate_B, A))                                                // B 不能是 A 的子孫
            {
                B = candidate_B; // 找到了！
                break;           // 立刻跳出迴圈
            }
        }

        // 如果在限定次數內找不到合適的 B，就放棄這次擾動
        if (B == nullptr)
        {
            return;
        }

        // 3. 從舊 parent 拔掉 A (邏輯不變)
        if (A->parent)
        {
            if (A->parent->left_child == A)
                A->parent->left_child = nullptr;
            else
                A->parent->right_child = nullptr;
        }
        A->parent = nullptr;

        // 4. 插到 B 底下 (邏輯不變)
        // 為了更好的隨機性，也使用新的隨機數引擎
        std::uniform_int_distribution<int> side_dist(0, 1);
        bool place_left;

        if (B->left_child == nullptr && B->right_child == nullptr)
            place_left = (side_dist(rng) == 0);
        else if (B->left_child == nullptr)
            place_left = true;
        else // B->right_child must be nullptr
            place_left = false;

        if (place_left)
            B->left_child = A;
        else
            B->right_child = A;
        A->parent = B;
    }
    // --- 需要實作 (8): 輔助函式 ---
    // --- 修正版: 輔助函式 ---
    // 函式簽名改變，明確傳遞 from_root 和 to_root
    void copy_tree(const std::vector<Node *> &from_nodes, Node *from_root, std::vector<Node *> &to_nodes, Node *&to_root)
    {
        for (Node *node : to_nodes)
        {
            delete node;
        }
        to_nodes.clear();
        to_root = nullptr;

        if (from_nodes.empty())
            return;

        std::map<const Node *, Node *> node_map;

        for (const auto *old_node : from_nodes)
        {
            Node *new_node = new Node(old_node->name, old_node->id, old_node->width, old_node->height);
            new_node->current_shape_idx = old_node->current_shape_idx;
            new_node->module_info = old_node->module_info;
            new_node->x = old_node->x;
            new_node->y = old_node->y;
            to_nodes.push_back(new_node);
            node_map[old_node] = new_node;
        }

        for (const auto *old_node : from_nodes)
        {
            Node *new_node = node_map.at(old_node);
            new_node->parent = old_node->parent ? node_map.at(old_node->parent) : nullptr;
            new_node->left_child = old_node->left_child ? node_map.at(old_node->left_child) : nullptr;
            new_node->right_child = old_node->right_child ? node_map.at(old_node->right_child) : nullptr;
        }

        // 使用傳入的 from_root 來安全地設定新的 to_root
        if (from_root)
        {
            to_root = node_map.at(from_root);
        }
    }

    // 函式簽名改變，明確傳遞 from_root 和 to_root
    void restore_tree(const std::vector<Node *> &from_nodes, Node *from_root, std::vector<Node *> &to_nodes, Node *&to_root)
    {
        // 先釋放 to_nodes (即將被覆蓋的當前狀態) 的記憶體
        for (Node *node : to_nodes)
        {
            delete node;
        }
        to_nodes.clear();
        to_root = nullptr;

        // 重新從 from_nodes 深度複製一份
        // 注意：這裡直接呼叫 copy_tree 會造成重複刪除 to_nodes，所以直接把邏輯寫在這裡
        if (from_nodes.empty())
            return;
        std::map<const Node *, Node *> node_map;
        for (const auto *old_node : from_nodes)
        {
            Node *new_node = new Node(old_node->name, old_node->id, old_node->width, old_node->height);
            new_node->current_shape_idx = old_node->current_shape_idx;
            new_node->module_info = old_node->module_info;
            new_node->x = old_node->x;
            new_node->y = old_node->y;
            to_nodes.push_back(new_node);
            node_map[old_node] = new_node;
        }
        for (const auto *old_node : from_nodes)
        {
            Node *new_node = node_map.at(old_node);
            new_node->parent = old_node->parent ? node_map.at(old_node->parent) : nullptr;
            new_node->left_child = old_node->left_child ? node_map.at(old_node->left_child) : nullptr;
            new_node->right_child = old_node->right_child ? node_map.at(old_node->right_child) : nullptr;
        }
        if (from_root)
        {
            to_root = node_map.at(from_root);
        }
    }
    void generate_edge_list_recursive(const Node *node, ofstream &out)
    {
        if (node == nullptr)
        {
            return;
        }

        // 如果有左子節點，寫入一條邊
        if (node->left_child != nullptr)
        {
            out << "\"" << node->name << "\" -> \"" << node->left_child->name << "\" [label=\"L\"];" << endl;
        }
        // 如果有右子節點，寫入一條邊
        if (node->right_child != nullptr)
        {
            out << "\"" << node->name << "\" -> \"" << node->right_child->name << "\" [label=\"R\"];" << endl;
        }

        // 遞迴處理
        generate_edge_list_recursive(node->left_child, out);
        generate_edge_list_recursive(node->right_child, out);
    }
    void copy_data_only(const std::vector<Node *> &from_nodes, Node *from_root,
                        std::vector<Node *> &to_nodes, Node *&to_root)
    {
        // 如果來源是空的，清空目標後直接返回
        if (from_nodes.empty())
        {
            to_nodes.clear(); // 雖然理論上大小應相同，但為求穩健
            to_root = nullptr;
            return;
        }

        std::map<const Node *, Node *> node_map;

        // 1. 複製每個節點的資料成員，並建立新舊節點指標的對應
        for (size_t i = 0; i < from_nodes.size(); ++i)
        {
            const Node *src_node = from_nodes[i];
            Node *dst_node = to_nodes[i]; // 直接使用已存在的目標節點，不 new！

            // --- 複製資料 ---
            dst_node->name = src_node->name;
            dst_node->id = src_node->id;
            dst_node->width = src_node->width;
            dst_node->height = src_node->height;
            dst_node->current_shape_idx = src_node->current_shape_idx;
            dst_node->module_info = src_node->module_info;
            dst_node->x = src_node->x;
            dst_node->y = src_node->y;

            node_map[src_node] = dst_node;
        }

        // 2. 根據對應表，重新連接目標樹的指標 (parent, left_child, right_child)
        for (size_t i = 0; i < from_nodes.size(); ++i)
        {
            const Node *src_node = from_nodes[i];
            Node *dst_node = to_nodes[i];

            dst_node->parent = src_node->parent ? node_map.at(src_node->parent) : nullptr;
            dst_node->left_child = src_node->left_child ? node_map.at(src_node->left_child) : nullptr;
            dst_node->right_child = src_node->right_child ? node_map.at(src_node->right_child) : nullptr;
        }

        // 3. 設定目標樹的根節點
        to_root = from_root ? node_map.at(from_root) : nullptr;
    }
};

// 讀檔
std::vector<Module> parse_block_file(const std::string &filename)
{
    std::vector<Module> modules;
    std::ifstream input_file(filename);

    if (!input_file.is_open())
    {
        std::cerr << "Error: 無法開啟檔案 " << filename << std::endl;
        return modules;
    }

    std::string line;
    while (std::getline(input_file, line))
    {
        if (line.empty())
        {
            continue;
        }

        std::stringstream line_stream(line);
        Module current_module;

        line_stream >> current_module.name;
        std::string num_part;
        for (char c : current_module.name)
        {
            if (isdigit(c))
                num_part += c;
        }
        current_module.id = num_part.empty() ? 0 : std::stoi(num_part);
        char parenthesis;
        while (line_stream >> parenthesis && parenthesis == '(')
        {
            Shape shape;
            line_stream >> shape.width >> shape.height >> shape.col_multiple >> shape.row_multiple;
            line_stream >> parenthesis;
            current_module.possible_shapes.push_back(shape);
        }
        modules.push_back(current_module);
    }

    input_file.close();
    return modules;
}
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "用法: " << argv[0] << " <input_file.block> <output_file.output>" << std::endl;
        return 1;
    }
    std::string input_filename = argv[1];
    std::string output_filename = argv[2];
    std::vector<Module> my_modules = parse_block_file(input_filename);
    std::cout << "成功解析 " << my_modules.size() << " 個模組。" << std::endl;
    double ans_cost = std::numeric_limits<double>::max();
    Placer my_placer(my_modules, output_filename);

    while (my_placer.get_elapsed_time() < 580)
    {
        my_placer.solve();
        if (ans_cost > my_placer.get_best_cost())
        {
            cout << "找到更好的解，更新最佳成本: " << my_placer.get_best_cost() << endl;
            ans_cost = my_placer.get_best_cost();
            my_placer.write_output(output_filename);
            // 畫b*tree的資料
            // my_placer.write_best_tree("best_tree_structure.txt");
        }
    }
    return 0;
}