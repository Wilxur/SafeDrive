import os
base = 'C:\\Users\\ge138\\Documents\\疲劳驾驶'
print('Starting...')
os.makedirs(os.path.join(base, "database"), exist_ok=True)
with open(os.path.join(base, 'database', 'schema.sql'), 'w', encoding='utf-8') as f: f.write('-- test\n')
print("done")
