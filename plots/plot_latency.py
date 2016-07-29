import sys, csv
import matplotlib
import numpy as np
import matplotlib.pyplot as plt
import statsmodels.api as sm
from preproc import FLOW_TYPES

LABELS = {
#    'PMU_AGG' : "PMU agg.",
#    'PMU_COM' : "PMU deliv.",
#    'AMI_AGG' : "AMI agg.",
#    'AMI_COM' : "AMI deliv.",
    'PMU_TOT' : "PMU",
    'AMI_TOT' : "AMI",
    'ERROR'   : "Urgent",
    'DATA'    : "Demand-response"
}

ORDER = {
#    'PMU_AGG' : 0,
#    'PMU_COM' : 1,
#    'AMI_AGG' : 2,
#    'AMI_COM' : 3,
    'PMU_TOT' : 1,
    'AMI_TOT' : 3,
    'ERROR'   : 4,
    'DATA'    : 5
}

STYLES = {
#    'PMU_AGG' : ("teal", ":"),
#    'PMU_COM' : ("darkslategray", ":"),
#    'AMI_AGG' : ("lightgreen", "-."),
#    'AMI_COM' : ("green", "-."),
    'PMU_TOT' : ("darkslategray", ":"),
    'AMI_TOT' : ("green", "-."),
    'ERROR'   : ("tomato", "-"),
    'DATA'    : ("darkorchid", "--")
}

def main(infile):

    fh = open(infile)
    reader = csv.reader(fh, delimiter=' ', skipinitialspace=True)
    reader.next() # skip header
    
    lats = {cls: [] for cls in LABELS}
    
    for flowcls, latency in reader:
        if flowcls in lats:
            lats[flowcls].append(1000.*float(latency))
    fh.close()
    
    matplotlib.rc('font', size=16)
    matplotlib.rcParams['xtick.major.pad'] = 4
    
    for cls, samp in sorted(lats.items(), key=lambda(x,_): ORDER[x]):
        #n, bins, patches = plt.hist(x, N_BINS, normed=1, histtype='step', cumulative=True)
        print("plotting %s" % cls)
        color, style = STYLES[cls]
        ecdf = sm.distributions.ECDF(samp)
        x = np.linspace(min(samp), max(samp))
        y = ecdf(x)
        plt.step(x, y, label=LABELS[cls], color=color, linestyle=style, linewidth=3)
        
    plt.grid(True, color='.8')
    plt.xlim(0, 40)
    plt.ylim(0, 1.05)
    plt.xlabel("Latency (ms)")
    plt.ylabel("Cumulative freq.")
    #plt.xscale('log')
    #plt.legend()
    fig = plt.gcf()
    fig.set_size_inches(4.5, 2.8)
    plt.subplots_adjust(left=0.16, right=0.96, top=0.95, bottom=0.22)
    fig.savefig("%s.pdf" % infile[:-4], dpi=120)
    
    return 0

if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
