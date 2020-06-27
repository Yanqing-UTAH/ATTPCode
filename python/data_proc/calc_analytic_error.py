#!/usr/bin/python3

from time_serial_matrix import *
import os
import math
from tqdm import tqdm
import numpy as np
import scipy.linalg as LA

def format_filename(s):
    i = s.rindex('_')
    ts = int(s[i+3:-4])
    sketch_name = s[:i]
    return (ts, sketch_name, s)

T = 100
d = 100
n = 1000
tsm = uniform_normal_tsm(T, n, d, 80, 7.5, 0)
X0, t0 = tsm.nextVectors(1000)
t0 = [int(math.ceil(t)) for t in t0]

A_F = np.empty(T + 21)
error = np.empty(T + 21)
XTX = np.zeros((d, d))
k = 0
for i in tqdm(range(T + 21)):
    try:
        k2 = t0.index(i + 1, k) 
    except:
        k2 = k
    X = X0[k:k2]
    t = t0[k:k2]
    if len(X) != 0:
        XTX += X.T @ X
    u,s,vh = LA.svd(XTX)
    A_F[i] = np.sum(s)
    error[i] = tsm.analytic_error(vh, np.sqrt(s), i+1)
    k = k2

file_list = sorted(map(lambda s: format_filename(s), os.listdir("../../output")))

for ts, sketch_name, file_name  in file_list:
    XTX = np.loadtxt("../../output/" + file_name)
    u, s, vh = LA.svd(XTX)
    err = tsm.analytic_error(vh, np.sqrt(s), ts + 1)
    print(ts, sketch_name, err / A_F[ts], error[ts] / A_F[ts])


