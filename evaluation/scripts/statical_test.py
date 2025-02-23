import pandas as pd
import numpy as np
from scipy import stats

file_path = '../data/manual_time.csv'
data = pd.read_csv(file_path)

diff = data['B'] - data['A']

# 23.85% faster means 1 - 0.2385 = 0.7615
expected_difference_ratio = 0.7615
expected_difference = data['B'] * expected_difference_ratio - data['A']

t_stat, p_value = stats.ttest_1samp(diff, expected_difference.mean())


p_value_one_sided = p_value / 2 if t_stat > 0 else 1 - p_value / 2

print(t_stat, p_value_one_sided)