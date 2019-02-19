import pandas as pd
import numpy as np
import glob


count = 0
diff = 0
average = 0

atomics_avg = []
atomics_std = []
coati_avg = []
coati_std = []
split_avg = []
split_std = []

temp = []
# Calculate BC runtimes
#'Final_Runs_Alpaca',
direcs = ['Final_Runs_Atomics',
        'Final_Runs_Coati',
        'Final_Runs_Split']

labels =['atomics_avg','atomics_std','fb_avg','fb_std','split_avg','split_std']
apps = ['bc','ar','rsa','cem','blowfish','cuckoo','cuckoo_2']
for app in apps:
    for direc in direcs:
        log_files = direc + "\\*" + app + "*"
        count = 0
        diff = 0
        average = 0
        temp.clear()
        for f in glob.glob(log_files):
            df = pd.read_csv(f);
            print(f)
            start = 0
            last = 0
            prev = 0

            df.columns = [c.replace(' ', '_') for c in df.columns]

            piece = df['_P3.7'][:]
            time = df['Time[s]'][:]
            flag = 0
            if((app == 'ar' and (direc != 'Final_Runs_Coati')) 
                    or (app == 'rsa') or (app == 'cuckoo')):
                for i in range(0, len(piece) - 2):
                    if piece[i] == 1:
                        if start == 0:
                            start = i;
                        else:
                            if (time[i] > 10):
                                last = i;
                                flag = 1
                                break;
                        prev = i
            else:
                for i in range(0, len(piece) - 2):
                    if piece[i] == 1:
                        if start == 0:
                            start = i;
                        else:
                            if ((piece[i + 1] == 1) and (piece[i + 2] == 1) and
                                    time[i] > 10):
                                #changed ^ those from time to piece
                                last = i;
                                flag = 1
                                break;
                        prev = i

            if(flag == 0):
                last = prev
            temp.append(time[last] - time[start])
            print(time[last] - time[start])
        if(direc == 'Final_Runs_Coati'):
            coati_avg.append(np.average(temp))
            coati_std.append(np.std(temp))
        if(direc == 'Final_Runs_Split'):
            split_avg.append(np.average(temp))
            split_std.append(np.std(temp))
        if(direc == 'Final_Runs_Atomics'):
            atomics_avg.append(np.average(temp))
            atomics_std.append(np.std(temp))

coati_avg[-2] = coati_avg[-1]
coati_std[-2] = coati_std[-1]

del(atomics_avg[-1])
del(atomics_std[-1])
del(split_avg[-1])
del(split_std[-1])
del(coati_avg[-1])
del(coati_std[-1])
print(atomics_std)
print(atomics_avg)
print(split_std)
print(split_avg)
print(coati_std)
print(coati_avg)
df_all = pd.DataFrame(np.column_stack([atomics_avg, atomics_std, coati_avg, coati_std,
    split_avg, split_std]), columns=labels,)

df_all.to_csv('coati_sys_runtimes.csv')


