import sys, csv, math
from preproc import FLOW_TYPES
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import mlab
from matplotlib.ticker import FuncFormatter
import matplotlib

RUNTIME = 3600.

LABELS = {
    'PMU_TOT' : "PMU",
    'AMI_TOT' : "AMI",
    'ERROR'   : "Urgent",
    'DATA'    : "D-R"
}

ORDER = {
    'PMU_TOT' : 0,
    'PMU_AGG' : 1,
    'PMU_COM' : 2,
    'AMI_TOT' : 3,
    'AMI_AGG' : 4,
    'AMI_COM' : 5,
    'ERROR'   : 6,
    'DATA'    : 7
}

STYLES = {
    'PMU_AGG' : ("teal", ":"),
    'PMU_COM' : ("darkslategray", ":"),
    'AMI_AGG' : ("lightgreen", "-."),
    'AMI_COM' : ("green", "-."),
    'ERROR'   : ("tomato", "-"),
    'DATA'    : ("darkorchid", "--")
}

def tobw(size):
    return 1. * float(size) / RUNTIME

def main(infile):

    fh = open(infile)
    reader = csv.reader(fh, delimiter=' ', skipinitialspace=True)
    reader.next() # skip header
    
    flows = {}
    
    for flowcls, recvcnt, recvsize, losscnt, losssize in reader:
        if flowcls in LABELS:
            flows[flowcls] = int(recvcnt), tobw(recvsize), int(losscnt), tobw(losssize)
    fh.close()
    
    matplotlib.rc('font', size=16)
    matplotlib.rcParams['xtick.major.pad'] = 4
    
    recvmeans = [data[1] for cls, data in sorted(flows.items(), key=lambda(x,_): ORDER[x])]
    lossrates = [100. * data[3] / (data[1] + data[3]) for cls, data in sorted(flows.items(), key=lambda(x,_): ORDER[x])]
    width = .4
    ind = np.arange(len(flows)) - width/2.

    fig, ax1 = plt.subplots()
    ax2 = ax1.twinx()
    
    ax1.set_yscale('log')
    ax2.set_yscale('linear')
    
    ax1.set_ylim([1e0, 1e6])
    ax2.set_ylim([0, 100.0])
    
    p1 = ax1.bar(ind - .2, recvmeans, width, color='darkslategray',linewidth=0, log=True)
    p2 = ax2.bar(ind + .2, lossrates, width, color='lightseagreen', label='Lost',linewidth=0, log=False)
    
    ax1.set_ylabel("Bytes / sec.")
    ax1.set_xticks(ind + width/2.)
    ax1.set_xticklabels([LABELS[cls] for cls in sorted(flows, key=lambda x: ORDER[x])])
    
    ticks = np.arange(6) / 5. * 100.0
    ax2.set_ylabel("Packet loss rate")
    ax2.set_yticks(ticks)
    ax2.set_yticklabels(map(lambda y: "%.2f%%" % y, ticks))

    fig.set_size_inches(4.5, 1.8)
    fig.subplots_adjust(left=0.17, right=0.77, top=0.83, bottom=0.185)
    fig.savefig("%s.pdf" % infile[:-4], dpi=120)
    
    return 0

if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
