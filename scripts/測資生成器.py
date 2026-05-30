import random
H_mix=2
W_mix=2
H_max=80
W_max=150
max_options=15
def generate_block_file(filename="large_test.block", num_blocks=200,
                        width_range=(W_mix, W_max), height_range=(H_mix, H_max),
                        allow_multiple_options=True, max_options=max_options):
    with open(filename, 'w') as f:
        for i in range(num_blocks):
            name = f"B{i}"
            num_options = random.randint(1, max_options) if allow_multiple_options else 1
            f.write(name)
            for _ in range(num_options):
                w = round(random.uniform(*width_range), 2)
                h = round(random.uniform(*height_range), 2)
                cm = random.randint(1, 3)
                rm = random.randint(1, 3)
                f.write(f" ({w} {h} {cm} {rm})")
            f.write("\n")
    print(f"[✓] 產生完成: {filename} 共 {num_blocks} blocks")

# 呼叫範例
generate_block_file("cases/case20000.block", num_blocks=2000)
