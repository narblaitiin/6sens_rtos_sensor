import glob
import os
import regex
import numpy as np
import json, csv
from datetime import datetime

CLUSTER_THRESOLD_MS=5000
data = []
nodes_id = []

def get_data():
    global nodes_id
    for dir in glob.glob("node*"):
        node_id = regex.match("node(.*)", dir).group(1)
        nodes_id.append(node_id)
        for file in os.listdir(os.path.join(dir, "data")):
            timestamp = regex.match("signal_(.*).dat", file).group(1)
            data.append((timestamp, node_id))
    
    data.sort(key=lambda x : x[0])
    return data

data = get_data()
nodes_id.sort()

timestamps = np.array([x[0] for x in data], dtype=np.uint64)
delta = np.diff(timestamps)

print("Len timestamps", len(timestamps))
print("Len delta", len(delta))

clusters_ids = []

current_vals = [0]
for i,  delta in enumerate(delta, start=1):
    if delta > CLUSTER_THRESOLD_MS:
        clusters_ids.append(current_vals)
        current_vals = []
    current_vals.append(i)

clusters = {}

for cluster in clusters_ids:
    nodes_timestamps = []
    for id in cluster:
       nodes_timestamps.append(data[id])
    avg = np.average(np.array([x[0] for x in nodes_timestamps], np.uint64))
    nodes = [x[1] for x in nodes_timestamps]

    clusters[str(avg)] = nodes


correlations = {}
nb_pos = { id : 0 for id in nodes_id}
with open("clusters.csv", "w") as f:
    w = csv.writer(f)
    w.writerow(["Timestamp"] + nodes_id)
    
    nb_alone = 0
    for timestamp, nodes in clusters.items():
        if len(nodes) == 1:
            nb_alone += 1
        if len(nodes) == len(nodes_id):
            print(timestamp)
        for i in nodes_id:
            nb_pos[i] += 1 if (i in nodes) else 0
            for j in nodes_id:
                nb_corr = 0
                if i not in correlations:
                    correlations[i] = {}
                if i in correlations and j in correlations[i]:
                    nb_corr = correlations[i][j]
        
                corr = (i in nodes) and (j in nodes)
                if corr is True:
                    correlations[i][j] = nb_corr + 1 
        
        row = [datetime.fromtimestamp(float(timestamp)/1000).strftime('%Y-%m-%d %H:%M:%S')]
        row += ["❌✅"[id in nodes] for id in nodes_id]
        w.writerow(row)

print("Nb cluster alone :", nb_alone)
print(nb_pos)   
print(correlations)

nb_clusters = len(clusters)

with open("correlation.csv", "w") as f:
    w = csv.writer(f)
    w.writerow(["Node"] + nodes_id)
    for i in nodes_id:
        row = [i]
        for j in nodes_id:
            row.append(correlations[i][j]/nb_pos[i])
        w.writerow(row)
    w.writerow(["Total détecté"] + list(nb_pos.values()))