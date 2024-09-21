import sys

def process_directly_merged_file(file1, file2, out_path):
    ret = []
    with open(file1, 'r') as f:
        base_rep = {line.split(' --- ')[0]: line.split(' --- ')[1].strip() for line in f.readlines()}

    with open(file2, 'r') as f:
        for line in f.readlines():
            base_name, l1, l2 = line.strip().split(', ')
            if base_name in base_rep:
                apply_type = '+' if base_rep[base_name] == 'A' else '-'
                ret.append(f'@ {base_name}, {l1}, {l2}')
                ret.append(f'{apply_type}, 1, {l1}, = ,1, {l2}')
    return ret

if __name__ == "__main__":
    file1 = sys.argv[1]
    file2 = sys.argv[2]
    process_directly_merged_file(file1, file2)