# this file processes the runtime for each of the apps on harvested energy

import pandas as pd
import numpy as np
import glob
import re
import sys


count = 0
diff = 0
average = 0

buffi_avg = []
buffi_std = []

coati_avg = []
coati_std = []

temp_counts = []
temp = []

labels =['buffi_avg',
        'coati_avg',]
single = 0
if (len(sys.argv) > 1):
  systems = [sys.argv[1]]
  single = 1
else:
  systems = ['buffi', 'coati']

print(systems)

if (len(sys.argv) > 2):
  apps = [sys.argv[2]]
else:
  apps = ['bc','ar','rsa','cem','blowfish','cuckoo']

print(apps)

direc_start = '/tmp/saleae_traces/'
direc = '/harv/'
runtime=0
for app in apps:
    print(app)
    for sys in systems:
        print(sys)
        print(app)
        log_files = direc_start  + sys + direc + sys+ "_" + app + "_*"
        print(log_files)
        count = 0
        diff = 0
        average = 0
        temp.clear()
        temp_counts.clear()
        for f in glob.glob(log_files):
            df_in = pd.read_csv(f);
            # Remove all the times that are less than 5
            df = df_in.loc[df_in["Time[s]"] > 5]
            print(f)
            #print(df)
            start = 0
            last = 0
            prev = 0

            df.columns = [c.replace(' ', '_') for c in df.columns]

            piece_df = df['_P3.7'][:]
            piece = piece_df.values
            print(piece)
            time_df = df['Time[s]'][:]
            time = time_df.values
            flag = 0
            if (app == 'rsa') or (app == "cuckoo") or (app == "ar" and
              sys!="buffi"):
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
            if(last > 0 and start < len(time)):
              print(time[last])
              print(time[start])
              temp.append(time[last] - time[start])
              runtime = time[last] - time[start]
              print(time[last] - time[start])
            else:
              print("no finish")
        #print(temp)

        if(sys == 'buffi'):
            buffi_avg.append(np.average(temp))
            buffi_std.append(np.std(temp))
        if(sys == 'coati'):
            coati_avg.append(np.average(temp))
            coati_std.append(np.std(temp))


if (single == 1):
  print("Runtime =  " + str(runtime))
else:
  df_all = pd.DataFrame(np.column_stack([buffi_avg,coati_avg,]),
  columns=labels,index=apps)
  df_all.to_csv(direc_start + 'harv_pwr_runtimes.csv')

