# Used to generate figure 10 in the paper
import pandas as pd
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
import sys
import seaborn as sns

print(sys.argv[1])

if (sys.argv[1] == "1"):
  df = pd.read_csv('/tmp/saleae_traces/cont_pwrd_runtimes.csv');
else:
  df = pd.read_csv('/tmp/saleae_traces/harv_pwr_runtimes.csv');


print(df)
#print(df['atomics_avg'][:])

print(df.columns)


#apps = ['BC','AR','RSA','CEM','BF','CF']
apps=df.index
n_groups = len(apps)

sns.set(style="white", font_scale=1.9)
sns.plotting_context=("talk")

fig, ax = plt.subplots()

index = np.arange(n_groups)
bar_width = 0.35

opacity = .95
error_config = {'ecolor': '0.3'}



rects3 = plt.bar(index + 0*bar_width,df['buffi_avg'][:], bar_width,
                 alpha=opacity,
                 color='#99d594',
                 label='Buffi')

rects4 = plt.bar(index + 1*bar_width,df['coati_avg'][:], bar_width,
                 alpha=opacity,
                 color='#3288bd',
                 label='Coati')


plt.ylabel('Runtime(s)')
ax.set_aspect(.15)
if (sys.argv[1] == "1"):
  plt.title('Continuous Power')
  plot_name = "cont_pwrd_runtimes.pdf"
else:
  plt.title('Harvested Energy')
  plot_name = "harv_pwr_runtimes.pdf"

plt.xticks(index + bar_width, apps)
plt.legend(loc='lower center', ncol=2, bbox_to_anchor=(.5,-.6))

plt.tight_layout()
fig.savefig("/tmp/saleae_traces/" + plot_name,format='pdf',bbox_inches='tight')


